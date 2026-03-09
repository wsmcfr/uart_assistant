/**
 * @file FileTransfer.cpp
 * @brief 文件传输协议实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "FileTransfer.h"
#include "utils/Logger.h"
#include <QFileInfo>
#include <QElapsedTimer>

namespace ComAssistant {

// ============== FileTransfer 基类实现 ==============

FileTransfer::FileTransfer(QObject* parent)
    : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
}

void FileTransfer::setState(TransferState state)
{
    if (m_state != state) {
        m_state = state;
        m_progress.state = state;
        emit stateChanged(state);
    }
}

void FileTransfer::updateProgress()
{
    emit progressUpdated(m_progress);
}

// ============== XModemTransfer 实现 ==============

XModemTransfer::XModemTransfer(bool useCRC, bool use1K, QObject* parent)
    : FileTransfer(parent)
    , m_useCRC(useCRC)
    , m_use1K(use1K)
    , m_blockSize(use1K ? 1024 : 128)
{
    connect(m_timeoutTimer, &QTimer::timeout, this, &XModemTransfer::onTimeout);
}

XModemTransfer::~XModemTransfer()
{
    if (m_file) {
        m_file->close();
        delete m_file;
    }
}

TransferProtocol XModemTransfer::protocol() const
{
    if (m_use1K) return TransferProtocol::XModem1K;
    if (m_useCRC) return TransferProtocol::XModemCRC;
    return TransferProtocol::XModem;
}

bool XModemTransfer::startSend(const QString& filePath)
{
    if (m_state != TransferState::Idle) {
        LOG_WARN("Transfer already in progress");
        return false;
    }

    m_file = new QFile(filePath);
    if (!m_file->open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open file: %1").arg(filePath));
        m_progress.errorMessage = tr("无法打开文件");
        setState(TransferState::Error);
        emit transferCompleted(false, m_progress.errorMessage);
        return false;
    }

    m_fileData = m_file->readAll();
    m_file->close();

    QFileInfo fileInfo(filePath);
    m_progress.fileName = fileInfo.fileName();
    m_progress.fileSize = m_fileData.size();
    m_progress.bytesTransferred = 0;
    m_progress.totalPackets = (m_fileData.size() + m_blockSize - 1) / m_blockSize;
    m_progress.currentPacket = 0;

    m_packetNumber = 1;
    m_retryCount = 0;
    m_direction = TransferDirection::Send;
    m_sendState = SendState::WaitingC;

    setState(TransferState::WaitingStart);
    m_timeoutTimer->start(m_timeoutMs);

    LOG_INFO(QString("XMODEM send started: %1 (%2 bytes, %3 packets)")
        .arg(m_progress.fileName)
        .arg(m_progress.fileSize)
        .arg(m_progress.totalPackets));

    return true;
}

bool XModemTransfer::startReceive(const QString& savePath)
{
    if (m_state != TransferState::Idle) {
        LOG_WARN("Transfer already in progress");
        return false;
    }

    m_file = new QFile(savePath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Cannot create file: %1").arg(savePath));
        m_progress.errorMessage = tr("无法创建文件");
        setState(TransferState::Error);
        emit transferCompleted(false, m_progress.errorMessage);
        return false;
    }

    m_fileData.clear();
    m_receiveBuffer.clear();
    m_progress.fileName = QFileInfo(savePath).fileName();
    m_progress.fileSize = 0;
    m_progress.bytesTransferred = 0;

    m_packetNumber = 1;
    m_retryCount = 0;
    m_direction = TransferDirection::Receive;
    m_receiveState = ReceiveState::Starting;

    setState(TransferState::WaitingStart);

    // 发送开始信号
    QByteArray startChar;
    startChar.append(m_useCRC ? CRC_START : NAK);
    emit sendData(startChar);

    m_timeoutTimer->start(m_timeoutMs);

    LOG_INFO("XMODEM receive started");
    return true;
}

void XModemTransfer::cancel()
{
    m_timeoutTimer->stop();

    // 发送取消信号（连续3个CAN）
    QByteArray canData;
    canData.append(CAN);
    canData.append(CAN);
    canData.append(CAN);
    emit sendData(canData);

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    setState(TransferState::Cancelled);
    emit transferCompleted(false, tr("传输已取消"));

    LOG_INFO("XMODEM transfer cancelled");
}

void XModemTransfer::processReceivedData(const QByteArray& data)
{
    m_receiveBuffer.append(data);
    m_timeoutTimer->stop();

    if (m_direction == TransferDirection::Send) {
        // 发送模式
        for (char c : m_receiveBuffer) {
            switch (m_sendState) {
            case SendState::WaitingC:
                if (c == CRC_START || c == NAK) {
                    m_useCRC = (c == CRC_START);
                    m_sendState = SendState::SendingData;
                    setState(TransferState::Transferring);
                    sendNextPacket();
                } else if (c == CAN) {
                    setState(TransferState::Cancelled);
                    emit transferCompleted(false, tr("接收方取消"));
                    return;
                }
                break;

            case SendState::WaitingAck:
                if (c == ACK) {
                    m_retryCount = 0;
                    m_packetNumber++;
                    m_sendState = SendState::SendingData;
                    sendNextPacket();
                } else if (c == NAK) {
                    // 重发当前包
                    m_retryCount++;
                    if (m_retryCount > m_maxRetries) {
                        setState(TransferState::Error);
                        emit transferCompleted(false, tr("重试次数超限"));
                        return;
                    }
                    emit sendData(m_lastPacket);
                    m_timeoutTimer->start(m_timeoutMs);
                } else if (c == CAN) {
                    setState(TransferState::Cancelled);
                    emit transferCompleted(false, tr("接收方取消"));
                    return;
                }
                break;

            case SendState::SendingEOT:
                if (c == ACK) {
                    setState(TransferState::Completed);
                    emit transferCompleted(true, tr("传输完成"));
                    return;
                } else if (c == NAK) {
                    // 重发EOT
                    QByteArray eot;
                    eot.append(EOT);
                    emit sendData(eot);
                    m_timeoutTimer->start(m_timeoutMs);
                }
                break;

            default:
                break;
            }
        }
    } else {
        // 接收模式
        processReceiveData();
    }

    m_receiveBuffer.clear();
}

void XModemTransfer::processReceiveData()
{
    int expectedPacketSize = 3 + m_blockSize + (m_useCRC ? 2 : 1);

    while (!m_receiveBuffer.isEmpty()) {
        char header = m_receiveBuffer[0];

        if (header == EOT) {
            // 传输结束
            QByteArray ack;
            ack.append(ACK);
            emit sendData(ack);

            // 写入文件
            m_file->write(m_fileData);
            m_file->close();

            setState(TransferState::Completed);
            emit transferCompleted(true, tr("接收完成"));
            return;
        }

        if (header == CAN) {
            setState(TransferState::Cancelled);
            emit transferCompleted(false, tr("发送方取消"));
            return;
        }

        if (header != SOH && header != STX) {
            m_receiveBuffer.remove(0, 1);
            continue;
        }

        int blockSize = (header == STX) ? 1024 : 128;
        expectedPacketSize = 3 + blockSize + (m_useCRC ? 2 : 1);

        if (m_receiveBuffer.size() < expectedPacketSize) {
            // 数据不完整，等待更多数据
            m_timeoutTimer->start(m_timeoutMs);
            return;
        }

        QByteArray packet = m_receiveBuffer.left(expectedPacketSize);
        m_receiveBuffer.remove(0, expectedPacketSize);

        if (verifyPacket(packet, m_packetNumber)) {
            // 提取数据
            QByteArray blockData = packet.mid(3, blockSize);
            m_fileData.append(blockData);

            m_progress.bytesTransferred += blockSize;
            m_progress.currentPacket = m_packetNumber;
            updateProgress();

            // 发送ACK
            QByteArray ack;
            ack.append(ACK);
            emit sendData(ack);

            m_packetNumber = (m_packetNumber + 1) & 0xFF;
            m_retryCount = 0;

            setState(TransferState::Transferring);
        } else {
            // 校验失败，发送NAK
            m_retryCount++;
            if (m_retryCount > m_maxRetries) {
                setState(TransferState::Error);
                emit transferCompleted(false, tr("校验错误次数超限"));
                return;
            }

            QByteArray nak;
            nak.append(NAK);
            emit sendData(nak);
        }

        m_timeoutTimer->start(m_timeoutMs);
    }
}

void XModemTransfer::sendNextPacket()
{
    int offset = (m_packetNumber - 1) * m_blockSize;

    if (offset >= m_fileData.size()) {
        // 所有数据发送完成，发送EOT
        m_sendState = SendState::SendingEOT;
        QByteArray eot;
        eot.append(EOT);
        emit sendData(eot);
        m_timeoutTimer->start(m_timeoutMs);
        return;
    }

    QByteArray blockData = m_fileData.mid(offset, m_blockSize);

    // 填充到块大小
    while (blockData.size() < m_blockSize) {
        blockData.append(CPMEOF);
    }

    m_lastPacket = buildPacket(m_packetNumber, blockData);
    emit sendData(m_lastPacket);

    m_progress.bytesTransferred = qMin((qint64)offset + m_blockSize, m_progress.fileSize);
    m_progress.currentPacket = m_packetNumber;
    updateProgress();

    m_sendState = SendState::WaitingAck;
    m_timeoutTimer->start(m_timeoutMs);
}

QByteArray XModemTransfer::buildPacket(int packetNum, const QByteArray& data)
{
    QByteArray packet;

    // 帧头
    packet.append(m_use1K ? STX : SOH);

    // 包号
    packet.append(static_cast<char>(packetNum & 0xFF));
    packet.append(static_cast<char>(~packetNum & 0xFF));

    // 数据
    packet.append(data);

    // 校验
    if (m_useCRC) {
        quint16 crc = calculateCRC16(data);
        packet.append(static_cast<char>((crc >> 8) & 0xFF));
        packet.append(static_cast<char>(crc & 0xFF));
    } else {
        packet.append(static_cast<char>(calculateChecksum(data)));
    }

    return packet;
}

bool XModemTransfer::verifyPacket(const QByteArray& packet, int expectedNum)
{
    if (packet.size() < 4) return false;

    int packetNum = static_cast<quint8>(packet[1]);
    int packetNumComp = static_cast<quint8>(packet[2]);

    // 验证包号
    if ((packetNum ^ packetNumComp) != 0xFF) return false;
    if (packetNum != (expectedNum & 0xFF)) return false;

    int blockSize = (packet[0] == STX) ? 1024 : 128;
    QByteArray data = packet.mid(3, blockSize);

    // 验证校验
    if (m_useCRC) {
        int crcOffset = 3 + blockSize;
        quint16 receivedCRC = (static_cast<quint8>(packet[crcOffset]) << 8) |
                              static_cast<quint8>(packet[crcOffset + 1]);
        quint16 calculatedCRC = calculateCRC16(data);
        return receivedCRC == calculatedCRC;
    } else {
        quint8 receivedSum = static_cast<quint8>(packet[3 + blockSize]);
        quint8 calculatedSum = calculateChecksum(data);
        return receivedSum == calculatedSum;
    }
}

quint16 XModemTransfer::calculateCRC16(const QByteArray& data)
{
    quint16 crc = 0;
    for (char byte : data) {
        crc ^= (static_cast<quint8>(byte) << 8);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

quint8 XModemTransfer::calculateChecksum(const QByteArray& data)
{
    quint8 sum = 0;
    for (char byte : data) {
        sum += static_cast<quint8>(byte);
    }
    return sum;
}

void XModemTransfer::onTimeout()
{
    m_retryCount++;
    m_progress.retryCount = m_retryCount;

    if (m_retryCount > m_maxRetries) {
        setState(TransferState::Error);
        m_progress.errorMessage = tr("传输超时");
        emit transferCompleted(false, m_progress.errorMessage);
        return;
    }

    LOG_WARN(QString("XMODEM timeout, retry %1/%2").arg(m_retryCount).arg(m_maxRetries));

    if (m_direction == TransferDirection::Receive) {
        // 重发开始信号
        QByteArray startChar;
        startChar.append(m_useCRC ? CRC_START : NAK);
        emit sendData(startChar);
    } else if (!m_lastPacket.isEmpty()) {
        // 重发上一个包
        emit sendData(m_lastPacket);
    }

    m_timeoutTimer->start(m_timeoutMs);
    updateProgress();
}

// ============== YModemTransfer 实现 ==============

YModemTransfer::YModemTransfer(bool useG, QObject* parent)
    : FileTransfer(parent)
    , m_useG(useG)
{
    connect(m_timeoutTimer, &QTimer::timeout, this, &YModemTransfer::onTimeout);
}

YModemTransfer::~YModemTransfer()
{
    if (m_file) {
        m_file->close();
        delete m_file;
    }
}

TransferProtocol YModemTransfer::protocol() const
{
    return m_useG ? TransferProtocol::YModemG : TransferProtocol::YModem;
}

bool YModemTransfer::startSend(const QString& filePath)
{
    m_filesToSend.clear();
    m_filesToSend.append(filePath);
    m_currentFileIndex = 0;
    return startSendBatch(m_filesToSend);
}

bool YModemTransfer::startSendBatch(const QStringList& filePaths)
{
    if (m_state != TransferState::Idle) {
        return false;
    }

    m_filesToSend = filePaths;
    m_currentFileIndex = 0;
    m_direction = TransferDirection::Send;
    m_sendState = SendState::WaitingC;

    setState(TransferState::WaitingStart);
    m_timeoutTimer->start(m_timeoutMs);

    LOG_INFO(QString("YMODEM send started: %1 files").arg(filePaths.size()));
    return true;
}

bool YModemTransfer::startReceive(const QString& savePath)
{
    if (m_state != TransferState::Idle) {
        return false;
    }

    m_savePath = savePath;
    m_direction = TransferDirection::Receive;
    m_receiveState = ReceiveState::Starting;
    m_receiveBuffer.clear();

    setState(TransferState::WaitingStart);

    // 发送C开始
    QByteArray startChar;
    startChar.append(CRC_START);
    emit sendData(startChar);

    m_timeoutTimer->start(m_timeoutMs);

    LOG_INFO("YMODEM receive started");
    return true;
}

void YModemTransfer::cancel()
{
    m_timeoutTimer->stop();

    QByteArray canData;
    canData.append(CAN);
    canData.append(CAN);
    canData.append(CAN);
    emit sendData(canData);

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    setState(TransferState::Cancelled);
    emit transferCompleted(false, tr("传输已取消"));
}

void YModemTransfer::processReceivedData(const QByteArray& data)
{
    m_receiveBuffer.append(data);
    m_timeoutTimer->stop();

    // 处理发送模式响应
    if (m_direction == TransferDirection::Send) {
        for (char c : m_receiveBuffer) {
            switch (m_sendState) {
            case SendState::WaitingC:
                if (c == CRC_START) {
                    sendFileHeader();
                }
                break;

            case SendState::WaitingHeaderAck:
                if (c == ACK) {
                    // 等待C开始数据传输
                } else if (c == CRC_START) {
                    m_sendState = SendState::SendingData;
                    m_packetNumber = 1;
                    setState(TransferState::Transferring);
                    sendNextPacket();
                }
                break;

            case SendState::WaitingDataAck:
                if (c == ACK) {
                    m_packetNumber++;
                    m_sendState = SendState::SendingData;
                    sendNextPacket();
                } else if (c == NAK) {
                    // 重发
                }
                break;

            case SendState::WaitingEOTAck:
                if (c == NAK) {
                    // 发送第二个EOT
                    QByteArray eot;
                    eot.append(EOT);
                    emit sendData(eot);
                } else if (c == ACK) {
                    // 等待C准备下一个文件或结束
                } else if (c == CRC_START) {
                    m_currentFileIndex++;
                    if (m_currentFileIndex < m_filesToSend.size()) {
                        m_sendState = SendState::WaitingC;
                        sendFileHeader();
                    } else {
                        sendEndOfBatch();
                    }
                }
                break;

            default:
                break;
            }
        }
    }

    m_receiveBuffer.clear();
    m_timeoutTimer->start(m_timeoutMs);
}

void YModemTransfer::sendFileHeader()
{
    if (m_currentFileIndex >= m_filesToSend.size()) {
        sendEndOfBatch();
        return;
    }

    QString filePath = m_filesToSend[m_currentFileIndex];
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setState(TransferState::Error);
        emit transferCompleted(false, tr("无法打开文件"));
        return;
    }

    m_fileData = file.readAll();
    file.close();

    QFileInfo fileInfo(filePath);
    m_progress.fileName = fileInfo.fileName();
    m_progress.fileSize = m_fileData.size();
    m_progress.bytesTransferred = 0;
    m_progress.totalPackets = (m_fileData.size() + 1023) / 1024;

    QByteArray headerPacket = buildHeaderPacket(m_progress.fileName, m_progress.fileSize);
    emit sendData(headerPacket);

    m_sendState = SendState::WaitingHeaderAck;
    m_timeoutTimer->start(m_timeoutMs);

    LOG_INFO(QString("YMODEM sending header for: %1").arg(m_progress.fileName));
}

void YModemTransfer::sendNextPacket()
{
    int offset = (m_packetNumber - 1) * 1024;

    if (offset >= m_fileData.size()) {
        // 发送EOT
        m_sendState = SendState::WaitingEOTAck;
        QByteArray eot;
        eot.append(EOT);
        emit sendData(eot);
        m_timeoutTimer->start(m_timeoutMs);
        return;
    }

    QByteArray blockData = m_fileData.mid(offset, 1024);
    while (blockData.size() < 1024) {
        blockData.append(CPMEOF);
    }

    QByteArray packet = buildDataPacket(m_packetNumber, blockData);
    emit sendData(packet);

    m_progress.bytesTransferred = qMin((qint64)offset + 1024, m_progress.fileSize);
    m_progress.currentPacket = m_packetNumber;
    updateProgress();

    m_sendState = SendState::WaitingDataAck;
    m_timeoutTimer->start(m_timeoutMs);
}

void YModemTransfer::sendEndOfBatch()
{
    // 发送空文件头表示结束
    QByteArray endPacket = buildHeaderPacket("", 0);
    emit sendData(endPacket);

    setState(TransferState::Completed);
    emit transferCompleted(true, tr("所有文件传输完成"));
}

QByteArray YModemTransfer::buildHeaderPacket(const QString& fileName, qint64 fileSize)
{
    QByteArray data(128, 0);

    if (!fileName.isEmpty()) {
        QByteArray nameBytes = fileName.toUtf8();
        memcpy(data.data(), nameBytes.constData(), qMin(nameBytes.size(), 100));

        // 添加文件大小
        QString sizeStr = QString::number(fileSize);
        QByteArray sizeBytes = sizeStr.toLatin1();
        int nameLen = nameBytes.size();
        if (nameLen < 127) {
            memcpy(data.data() + nameLen + 1, sizeBytes.constData(),
                   qMin(sizeBytes.size(), 127 - nameLen - 1));
        }
    }

    QByteArray packet;
    packet.append(SOH);
    packet.append(char(0));
    packet.append(char(0xFF));
    packet.append(data);

    quint16 crc = calculateCRC16(data);
    packet.append(char((crc >> 8) & 0xFF));
    packet.append(char(crc & 0xFF));

    return packet;
}

QByteArray YModemTransfer::buildDataPacket(int packetNum, const QByteArray& data)
{
    QByteArray packet;
    packet.append(STX);
    packet.append(char(packetNum & 0xFF));
    packet.append(char(~packetNum & 0xFF));
    packet.append(data);

    quint16 crc = calculateCRC16(data);
    packet.append(char((crc >> 8) & 0xFF));
    packet.append(char(crc & 0xFF));

    return packet;
}

quint16 YModemTransfer::calculateCRC16(const QByteArray& data)
{
    quint16 crc = 0;
    for (char byte : data) {
        crc ^= (static_cast<quint8>(byte) << 8);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void YModemTransfer::onTimeout()
{
    m_retryCount++;
    if (m_retryCount > m_maxRetries) {
        setState(TransferState::Error);
        emit transferCompleted(false, tr("传输超时"));
        return;
    }

    LOG_WARN(QString("YMODEM timeout, retry %1").arg(m_retryCount));
    m_timeoutTimer->start(m_timeoutMs);
}

// ============== FileTransferFactory 实现 ==============

FileTransfer* FileTransferFactory::create(TransferProtocol protocol, QObject* parent)
{
    switch (protocol) {
    case TransferProtocol::XModem:
        return new XModemTransfer(false, false, parent);
    case TransferProtocol::XModemCRC:
        return new XModemTransfer(true, false, parent);
    case TransferProtocol::XModem1K:
        return new XModemTransfer(true, true, parent);
    case TransferProtocol::YModem:
        return new YModemTransfer(false, parent);
    case TransferProtocol::YModemG:
        return new YModemTransfer(true, parent);
    default:
        return nullptr;
    }
}

} // namespace ComAssistant
