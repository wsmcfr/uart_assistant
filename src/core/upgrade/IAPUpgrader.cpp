/**
 * @file IAPUpgrader.cpp
 * @brief IAP固件升级器实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "IAPUpgrader.h"
#include "utils/Logger.h"
#include <QFileInfo>
#include <QTextStream>

namespace ComAssistant {

// ============== IAPUpgrader 实现 ==============

IAPUpgrader::IAPUpgrader(QObject* parent)
    : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &IAPUpgrader::onTimeout);

    // 默认自定义协议参数
    m_packetHeader = QByteArray::fromHex("AA55");
    m_expectedAck = QByteArray::fromHex("55AA00");
}

IAPUpgrader::~IAPUpgrader()
{
}

void IAPUpgrader::setCustomProtocol(const QByteArray& header, const QByteArray& ack,
                                   int blockSize, int timeout)
{
    m_packetHeader = header;
    m_expectedAck = ack;
    m_blockSize = blockSize;
    m_timeout = timeout;
}

bool IAPUpgrader::loadFirmware(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    QString ext = fileInfo.suffix().toLower();

    m_progress.fileName = fileInfo.fileName();

    if (ext == "hex" || ext == "ihex") {
        m_firmwareType = FirmwareType::IntelHex;
        QString errorMsg;
        if (!parseHexFile(filePath, m_firmwareData, m_startAddress, errorMsg)) {
            LOG_ERROR(QString("Failed to parse HEX file: %1").arg(errorMsg));
            m_progress.errorMessage = errorMsg;
            return false;
        }
    } else if (ext == "bin") {
        m_firmwareType = FirmwareType::Binary;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            LOG_ERROR(QString("Cannot open file: %1").arg(filePath));
            m_progress.errorMessage = tr("无法打开文件");
            return false;
        }
        m_firmwareData = file.readAll();
        file.close();
    } else {
        LOG_ERROR(QString("Unsupported file format: %1").arg(ext));
        m_progress.errorMessage = tr("不支持的文件格式");
        return false;
    }

    m_progress.fileSize = m_firmwareData.size();
    m_progress.totalBlocks = (m_firmwareData.size() + m_blockSize - 1) / m_blockSize;

    LOG_INFO(QString("Firmware loaded: %1, size=%2 bytes, blocks=%3, startAddr=0x%4")
        .arg(m_progress.fileName)
        .arg(m_progress.fileSize)
        .arg(m_progress.totalBlocks)
        .arg(m_startAddress, 8, 16, QChar('0')));

    return true;
}

bool IAPUpgrader::parseHexFile(const QString& filePath, QByteArray& data,
                              quint32& startAddr, QString& errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMsg = "Cannot open file";
        return false;
    }

    QTextStream stream(&file);
    QMap<quint32, quint8> memoryMap;
    quint32 extendedAddress = 0;
    quint32 minAddr = 0xFFFFFFFF;
    quint32 maxAddr = 0;
    bool hasData = false;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (line.isEmpty()) continue;

        HexRecord record = parseHexLine(line);
        if (!record.valid) {
            errorMsg = QString("Invalid HEX record: %1").arg(line);
            return false;
        }

        switch (record.recordType) {
        case HexRecord::DATA:
            {
                quint32 addr = extendedAddress + record.address;
                for (int i = 0; i < record.data.size(); ++i) {
                    memoryMap[addr + i] = static_cast<quint8>(record.data[i]);
                }
                minAddr = qMin(minAddr, addr);
                maxAddr = qMax(maxAddr, addr + static_cast<quint32>(record.data.size()) - 1);
                hasData = true;
            }
            break;

        case HexRecord::END_OF_FILE:
            break;

        case HexRecord::EXT_SEGMENT_ADDR:
            if (record.data.size() >= 2) {
                extendedAddress = ((static_cast<quint8>(record.data[0]) << 8) |
                                   static_cast<quint8>(record.data[1])) << 4;
            }
            break;

        case HexRecord::EXT_LINEAR_ADDR:
            if (record.data.size() >= 2) {
                extendedAddress = ((static_cast<quint8>(record.data[0]) << 8) |
                                   static_cast<quint8>(record.data[1])) << 16;
            }
            break;

        case HexRecord::START_LINEAR_ADDR:
            if (record.data.size() >= 4) {
                startAddr = (static_cast<quint8>(record.data[0]) << 24) |
                           (static_cast<quint8>(record.data[1]) << 16) |
                           (static_cast<quint8>(record.data[2]) << 8) |
                           static_cast<quint8>(record.data[3]);
            }
            break;

        default:
            break;
        }
    }

    if (!hasData) {
        errorMsg = "No data in HEX file";
        return false;
    }

    // 转换为连续数据
    startAddr = minAddr;
    qint64 size = maxAddr - minAddr + 1;
    data.resize(size);
    data.fill(0xFF);  // 默认填充0xFF

    for (auto it = memoryMap.constBegin(); it != memoryMap.constEnd(); ++it) {
        data[it.key() - minAddr] = it.value();
    }

    return true;
}

HexRecord IAPUpgrader::parseHexLine(const QString& line)
{
    HexRecord record;
    record.valid = false;

    if (!line.startsWith(':') || line.length() < 11) {
        return record;
    }

    QString hex = line.mid(1);
    QByteArray bytes = QByteArray::fromHex(hex.toLatin1());

    if (bytes.size() < 5) {
        return record;
    }

    record.byteCount = static_cast<quint8>(bytes[0]);
    record.address = (static_cast<quint8>(bytes[1]) << 8) | static_cast<quint8>(bytes[2]);
    record.recordType = static_cast<quint8>(bytes[3]);

    if (bytes.size() < record.byteCount + 5) {
        return record;
    }

    record.data = bytes.mid(4, record.byteCount);
    record.checksum = static_cast<quint8>(bytes[4 + record.byteCount]);

    // 校验
    quint8 sum = 0;
    for (int i = 0; i < bytes.size() - 1; ++i) {
        sum += static_cast<quint8>(bytes[i]);
    }
    sum = (~sum + 1) & 0xFF;

    record.valid = (sum == record.checksum);
    return record;
}

bool IAPUpgrader::startUpgrade()
{
    if (m_firmwareData.isEmpty()) {
        m_progress.errorMessage = tr("未加载固件");
        emit upgradeCompleted(false, m_progress.errorMessage);
        return false;
    }

    m_state = State::Erasing;
    m_currentBlock = 0;
    m_retryCount = 0;
    m_receiveBuffer.clear();

    m_progress.bytesWritten = 0;
    m_progress.currentBlock = 0;
    m_progress.status = tr("擦除中...");
    emit progressUpdated(m_progress);

    sendEraseCommand();
    m_timeoutTimer->start(m_timeout * 10);  // 擦除超时更长

    LOG_INFO("IAP upgrade started");
    return true;
}

void IAPUpgrader::cancel()
{
    m_timeoutTimer->stop();
    m_state = State::Idle;

    m_progress.status = tr("已取消");
    emit progressUpdated(m_progress);
    emit upgradeCompleted(false, tr("升级已取消"));

    LOG_INFO("IAP upgrade cancelled");
}

void IAPUpgrader::processReceivedData(const QByteArray& data)
{
    m_receiveBuffer.append(data);
    m_timeoutTimer->stop();

    // 检查是否收到期望的响应
    if (m_receiveBuffer.contains(m_expectedAck)) {
        m_receiveBuffer.clear();
        m_retryCount = 0;

        switch (m_state) {
        case State::Erasing:
            m_state = State::Writing;
            m_progress.status = tr("写入中...");
            emit progressUpdated(m_progress);
            sendNextBlock();
            break;

        case State::Writing:
            m_currentBlock++;
            m_progress.currentBlock = m_currentBlock;
            m_progress.bytesWritten = qMin((qint64)m_currentBlock * m_blockSize,
                                          m_progress.fileSize);
            emit progressUpdated(m_progress);

            if (m_currentBlock >= m_progress.totalBlocks) {
                m_state = State::Verifying;
                m_progress.status = tr("校验中...");
                emit progressUpdated(m_progress);
                sendVerifyCommand();
            } else {
                sendNextBlock();
            }
            break;

        case State::Verifying:
            m_state = State::Resetting;
            m_progress.status = tr("重启中...");
            emit progressUpdated(m_progress);
            sendResetCommand();
            break;

        case State::Resetting:
            m_state = State::Completed;
            m_progress.status = tr("完成");
            emit progressUpdated(m_progress);
            emit upgradeCompleted(true, tr("固件升级成功"));
            LOG_INFO("IAP upgrade completed successfully");
            break;

        default:
            break;
        }
    } else {
        // 等待更多数据
        m_timeoutTimer->start(m_timeout);
    }
}

void IAPUpgrader::sendNextBlock()
{
    int offset = m_currentBlock * m_blockSize;
    QByteArray blockData = m_firmwareData.mid(offset, m_blockSize);

    // 填充到块大小
    while (blockData.size() < m_blockSize) {
        blockData.append('\xFF');
    }

    quint32 address = m_startAddress + offset;
    sendWriteCommand(address, blockData);
    m_timeoutTimer->start(m_timeout);
}

void IAPUpgrader::sendEraseCommand()
{
    // 自定义协议: Header + CMD(0x01) + StartAddr(4) + Size(4) + Checksum
    QByteArray data;
    data.append(char(0x01));  // 擦除命令

    // 起始地址
    data.append(char((m_startAddress >> 24) & 0xFF));
    data.append(char((m_startAddress >> 16) & 0xFF));
    data.append(char((m_startAddress >> 8) & 0xFF));
    data.append(char(m_startAddress & 0xFF));

    // 擦除大小
    quint32 size = m_firmwareData.size();
    data.append(char((size >> 24) & 0xFF));
    data.append(char((size >> 16) & 0xFF));
    data.append(char((size >> 8) & 0xFF));
    data.append(char(size & 0xFF));

    emit sendData(buildPacket(0x01, data));
}

void IAPUpgrader::sendWriteCommand(quint32 address, const QByteArray& data)
{
    // 自定义协议: Header + CMD(0x02) + Address(4) + Length(2) + Data + Checksum
    QByteArray packet;
    packet.append(char(0x02));  // 写入命令

    // 地址
    packet.append(char((address >> 24) & 0xFF));
    packet.append(char((address >> 16) & 0xFF));
    packet.append(char((address >> 8) & 0xFF));
    packet.append(char(address & 0xFF));

    // 长度
    quint16 len = data.size();
    packet.append(char((len >> 8) & 0xFF));
    packet.append(char(len & 0xFF));

    // 数据
    packet.append(data);

    emit sendData(buildPacket(0x02, packet));
}

void IAPUpgrader::sendVerifyCommand()
{
    // 发送校验命令
    QByteArray data;
    data.append(char(0x03));  // 校验命令

    // CRC32
    quint32 crc = qChecksum(m_firmwareData.constData(), m_firmwareData.size());
    data.append(char((crc >> 24) & 0xFF));
    data.append(char((crc >> 16) & 0xFF));
    data.append(char((crc >> 8) & 0xFF));
    data.append(char(crc & 0xFF));

    emit sendData(buildPacket(0x03, data));
    m_timeoutTimer->start(m_timeout);
}

void IAPUpgrader::sendResetCommand()
{
    QByteArray data;
    data.append(char(0x04));  // 重启命令

    emit sendData(buildPacket(0x04, data));
    m_timeoutTimer->start(m_timeout);
}

QByteArray IAPUpgrader::buildPacket(quint8 cmd, const QByteArray& data)
{
    Q_UNUSED(cmd)
    QByteArray packet;
    packet.append(m_packetHeader);
    packet.append(data);

    // 计算校验和
    quint8 sum = 0;
    for (char c : data) {
        sum += static_cast<quint8>(c);
    }
    packet.append(char(sum));

    return packet;
}

void IAPUpgrader::onTimeout()
{
    m_retryCount++;

    if (m_retryCount > m_maxRetries) {
        m_state = State::Error;
        m_progress.hasError = true;
        m_progress.errorMessage = tr("通信超时");
        m_progress.status = tr("失败");
        emit progressUpdated(m_progress);
        emit upgradeCompleted(false, m_progress.errorMessage);
        LOG_ERROR("IAP upgrade timeout");
        return;
    }

    LOG_WARN(QString("IAP timeout, retry %1/%2").arg(m_retryCount).arg(m_maxRetries));

    // 重试当前操作
    switch (m_state) {
    case State::Erasing:
        sendEraseCommand();
        m_timeoutTimer->start(m_timeout * 10);
        break;
    case State::Writing:
        sendNextBlock();
        break;
    case State::Verifying:
        sendVerifyCommand();
        break;
    case State::Resetting:
        sendResetCommand();
        break;
    default:
        break;
    }
}

// ============== BaudRateDetector 实现 ==============

const QList<int> BaudRateDetector::DEFAULT_BAUD_RATES = {
    115200, 9600, 19200, 38400, 57600, 230400, 460800, 921600, 1500000
};

BaudRateDetector::BaudRateDetector(QObject* parent)
    : QObject(parent)
    , m_baudRates(DEFAULT_BAUD_RATES)
    , m_testPattern("AT\r\n")
{
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &BaudRateDetector::onTimeout);
}

BaudRateDetector::~BaudRateDetector()
{
}

void BaudRateDetector::setBaudRates(const QList<int>& rates)
{
    m_baudRates = rates;
}

void BaudRateDetector::setTestPattern(const QByteArray& pattern)
{
    m_testPattern = pattern;
}

void BaudRateDetector::startDetection(const QString& portName)
{
    m_portName = portName;
    m_currentIndex = 0;
    m_cancelled = false;
    m_receiveBuffer.clear();

    LOG_INFO(QString("Starting baud rate detection on %1").arg(portName));
    testNextBaudRate();
}

void BaudRateDetector::cancel()
{
    m_cancelled = true;
    m_timer->stop();
    emit requestClosePort();
}

void BaudRateDetector::testNextBaudRate()
{
    if (m_cancelled) return;

    if (m_currentIndex >= m_baudRates.size()) {
        // 所有波特率都测试完毕，未找到匹配
        LOG_WARN("Baud rate detection failed - no matching rate found");
        emit detectionCompleted(0, false);
        return;
    }

    int baudRate = m_baudRates[m_currentIndex];

    emit progressUpdated(m_currentIndex + 1, m_baudRates.size(), baudRate);
    LOG_DEBUG(QString("Testing baud rate: %1").arg(baudRate));

    // 请求打开串口
    emit requestOpenPort(m_portName, baudRate);

    // 短暂延迟后发送测试数据
    QTimer::singleShot(100, this, [this]() {
        if (!m_cancelled) {
            m_receiveBuffer.clear();
            emit sendData(m_testPattern);
            m_timer->start(m_timeout);
        }
    });
}

void BaudRateDetector::processReceivedData(const QByteArray& data)
{
    m_receiveBuffer.append(data);

    if (analyzeResponse(m_receiveBuffer)) {
        m_timer->stop();
        int detectedBaud = m_baudRates[m_currentIndex];
        LOG_INFO(QString("Baud rate detected: %1").arg(detectedBaud));
        emit detectionCompleted(detectedBaud, true);
    }
}

void BaudRateDetector::onTimeout()
{
    // 当前波特率测试失败，尝试下一个
    emit requestClosePort();

    m_currentIndex++;
    QTimer::singleShot(200, this, &BaudRateDetector::testNextBaudRate);
}

bool BaudRateDetector::analyzeResponse(const QByteArray& response)
{
    if (response.isEmpty()) return false;

    // 检查是否收到可识别的响应
    // 1. 检查AT命令响应 "OK"
    if (response.contains("OK") || response.contains("ok")) {
        return true;
    }

    // 2. 检查是否收到回显
    if (response.contains(m_testPattern.left(2))) {
        return true;
    }

    // 3. 检查是否收到可打印字符（非乱码）
    int printableCount = 0;
    for (char c : response) {
        if ((c >= 0x20 && c <= 0x7E) || c == '\r' || c == '\n') {
            printableCount++;
        }
    }

    // 如果超过80%是可打印字符，认为波特率正确
    if (response.size() >= 3 && printableCount * 100 / response.size() >= 80) {
        return true;
    }

    return false;
}

} // namespace ComAssistant
