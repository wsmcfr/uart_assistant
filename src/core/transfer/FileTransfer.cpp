/**
 * @file FileTransfer.cpp
 * @brief 文件传输协议实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "FileTransfer.h"
#include "utils/Logger.h"
#include "utils/ChecksumUtils.h"
#include <QFileInfo>
#include <QDir>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QtGlobal>

namespace ComAssistant {

namespace {

/**
 * @brief 将 16 位整数按小端序追加到字节数组
 * @param data 目标字节数组。
 * @param value 待写入的 16 位无符号整数。
 *
 * 串口 Bootloader 通常按字节解析数据包，小端序便于 Cortex-M 等 MCU
 * 直接还原整数。这里集中封装，避免多个协议包手写位移导致顺序不一致。
 */
void appendLe16(QByteArray& data, quint16 value)
{
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
}

/**
 * @brief 将 32 位整数按小端序追加到字节数组
 * @param data 目标字节数组。
 * @param value 待写入的 32 位无符号整数。
 */
void appendLe32(QByteArray& data, quint32 value)
{
    data.append(static_cast<char>(value & 0xFF));
    data.append(static_cast<char>((value >> 8) & 0xFF));
    data.append(static_cast<char>((value >> 16) & 0xFF));
    data.append(static_cast<char>((value >> 24) & 0xFF));
}

/**
 * @brief 规范化发送块大小
 * @param value 用户输入或配置中的块大小。
 * @return 限制在 1 到 4096 字节之间的安全块大小。
 *
 * 块太小会导致效率极低，块太大又容易压满 MCU 串口缓冲区。第一版
 * 先用 4096 作为上限，既能覆盖常见 128/256/512/1024 字节场景，
 * 也避免用户误填过大数值导致 UI 卡顿或设备丢包。
 */
int sanitizeTransferBlockSize(int value)
{
    return qBound(1, value, 4096);
}

/**
 * @brief 规范化包间隔
 * @param value 用户输入或配置中的间隔毫秒数。
 * @return 不小于 0 的间隔毫秒数。
 */
int sanitizeIntervalMs(int value)
{
    return qMax(0, value);
}

/**
 * @brief 计算文件 CRC32
 * @param file 已打开的文件对象，函数会临时读取文件并恢复到开头。
 * @param crc32 输出整文件 CRC32。
 * @param errorMessage 失败时输出错误文本。
 * @return 成功计算返回 true。
 *
 * 这里使用分块读取，避免为了计算 CRC 一次性把大固件读进内存。
 * 读取完成后会 seek(0)，保证后续发送仍从文件开头开始。
 */
bool calculateFileCrc32(QFile& file, quint32& crc32, QString& errorMessage)
{
    if (!file.seek(0)) {
        errorMessage = QObject::tr("无法定位到文件开头");
        return false;
    }

    quint32 runningCrc = 0xFFFFFFFF;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(64 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            errorMessage = file.errorString();
            return false;
        }

        /*
         * 采用 IEEE 802.3 CRC32 多项式 0xEDB88320，与 ChecksumUtils::crc32
         * 的结果保持一致。这里做增量计算，是为了支持大文件流式预扫描。
         */
        for (char ch : chunk) {
            runningCrc ^= static_cast<quint8>(ch);
            for (int bit = 0; bit < 8; ++bit) {
                if (runningCrc & 1U) {
                    runningCrc = (runningCrc >> 1) ^ 0xEDB88320U;
                } else {
                    runningCrc >>= 1;
                }
            }
        }
    }

    crc32 = runningCrc ^ 0xFFFFFFFFU;
    if (!file.seek(0)) {
        errorMessage = QObject::tr("无法重新定位到文件开头");
        return false;
    }
    return true;
}

} // namespace

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

void FileTransfer::notifyLocalSendResult(bool success, const QString& errorMessage)
{
    Q_UNUSED(success)
    Q_UNUSED(errorMessage)
    /*
     * 基类默认不处理本地发送确认。XMODEM/YMODEM 目前仍由设备端 ACK/NAK
     * 推动状态机；Raw/OTA 会覆盖该函数，等待主窗口发送队列确认后再推进。
     */
}

// ============== RawFileTransfer 实现 ==============

RawFileTransfer::RawFileTransfer(QObject* parent)
    : FileTransfer(parent)
{
    m_sendTimer = new QTimer(this);
    m_sendTimer->setSingleShot(true);
    connect(m_sendTimer, &QTimer::timeout, this, &RawFileTransfer::sendNextChunk);
}

RawFileTransfer::~RawFileTransfer()
{
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void RawFileTransfer::setOptions(const RawTransferOptions& options)
{
    /*
     * 这里立即清洗参数，而不是等到 startSend() 再处理，保证 UI 或测试
     * 多次读取 options 时看到的都是可执行范围内的值。
     */
    m_options.blockSize = sanitizeTransferBlockSize(options.blockSize);
    m_options.intervalMs = sanitizeIntervalMs(options.intervalMs);
}

bool RawFileTransfer::startSend(const QString& filePath)
{
    /*
     * 裸流发送没有握手阶段，启动成功后马上进入 Running。
     * 若当前对象还在发送中，直接拒绝，避免同一个 QFile 指针被重复使用。
     */
    if (m_state != TransferState::Idle) {
        LOG_WARN("Raw transfer already in progress");
        return false;
    }

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    m_file = new QFile(filePath);
    if (!m_file->open(QIODevice::ReadOnly)) {
        failWithMessage(tr("无法打开文件: %1").arg(m_file->errorString()));
        return false;
    }

    const QFileInfo fileInfo(filePath);
    m_progress = TransferProgress();
    m_progress.fileName = fileInfo.fileName();
    m_progress.fileSize = fileInfo.size();
    m_progress.totalPackets = m_options.blockSize > 0
        ? static_cast<int>((m_progress.fileSize + m_options.blockSize - 1) / m_options.blockSize)
        : 0;
    m_progress.currentPacket = 0;
    m_progress.bytesTransferred = 0;

    m_paused = false;
    m_elapsedTimer.restart();
    setState(TransferState::Running);
    updateProgress();

    /*
     * 用 0ms 定时器启动第一包，而不是直接同步发送。这样 UI 有机会先
     * 刷新“开始传输”的状态，也保持所有块都由同一个定时路径发出。
     */
    m_sendTimer->start(0);
    LOG_INFO(QString("Raw file transfer started: %1").arg(filePath));
    return true;
}

bool RawFileTransfer::startReceive(const QString& savePath)
{
    Q_UNUSED(savePath)
    failWithMessage(tr("裸流接收暂未实现，请使用发送模式。"));
    return false;
}

void RawFileTransfer::cancel()
{
    /*
     * 取消必须停止定时器并关闭文件句柄，否则下一次打开同一文件时
     * Windows 可能仍认为文件被当前进程占用。
     */
    /*
     * Cancelling 是对 UI 和诊断日志可见的资源释放阶段。即使释放过程很快，
     * 也先发出该状态，防止用户重复点击取消或误判为普通失败。
     */
    setState(TransferState::Cancelling);

    if (m_sendTimer) {
        m_sendTimer->stop();
    }

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    m_paused = false;
    m_waitingLocalSendResult = false;
    m_pendingChunk.clear();
    setState(TransferState::Cancelled);
    emit transferCompleted(false, tr("传输已取消"));
}

void RawFileTransfer::processReceivedData(const QByteArray& data)
{
    Q_UNUSED(data)
    /*
     * 裸流模式没有 ACK/NAK 语义，下位机返回的数据仍会显示在主接收区，
     * 这里不消费它，避免误把普通日志当作传输控制字符。
     */
}

void RawFileTransfer::notifyLocalSendResult(bool success, const QString& errorMessage)
{
    if (!m_waitingLocalSendResult) {
        return;
    }

    if (!success) {
        failWithMessage(errorMessage.isEmpty()
            ? tr("本地发送队列处理裸流数据失败")
            : errorMessage);
        return;
    }

    continueAfterCurrentChunkAccepted();
}

void RawFileTransfer::pause()
{
    if (m_state != TransferState::Running || m_paused) {
        return;
    }

    m_paused = true;
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    setState(TransferState::Paused);
    updateProgress();
}

void RawFileTransfer::resume()
{
    if (m_state != TransferState::Paused || !m_paused) {
        return;
    }

    m_paused = false;
    setState(TransferState::Running);
    if (m_sendTimer && !m_waitingLocalSendResult) {
        m_sendTimer->start(0);
    }
}

QVector<QByteArray> RawFileTransfer::splitForTest(const QByteArray& data, int blockSize)
{
    QVector<QByteArray> chunks;
    const int safeBlockSize = sanitizeTransferBlockSize(blockSize);

    for (int offset = 0; offset < data.size(); offset += safeBlockSize) {
        chunks.append(data.mid(offset, safeBlockSize));
    }
    return chunks;
}

void RawFileTransfer::sendNextChunk()
{
    if (!m_file || m_state != TransferState::Running || m_paused || m_waitingLocalSendResult) {
        return;
    }

    const QByteArray chunk = m_file->read(m_options.blockSize);
    if (chunk.isEmpty()) {
        if (m_file->error() != QFileDevice::NoError) {
            failWithMessage(tr("读取文件失败: %1").arg(m_file->errorString()));
            return;
        }
        finishSuccessfully();
        return;
    }

    m_pendingChunk = chunk;
    m_waitingLocalSendResult = true;
    emit sendData(chunk);
}

void RawFileTransfer::continueAfterCurrentChunkAccepted()
{
    if (!m_file || m_state != TransferState::Running) {
        return;
    }

    /*
     * 只有主窗口发送队列确认当前块被本地发送管道接受后，才推进进度和
     * 文件读取位置。这样连接断开或队列拒绝不会让 UI 提前显示已发送。
     */
    const int acceptedSize = m_pendingChunk.size();
    m_pendingChunk.clear();
    m_waitingLocalSendResult = false;

    m_progress.bytesTransferred += acceptedSize;
    m_progress.currentPacket++;
    refreshSpeed();
    updateProgress();

    if (m_progress.bytesTransferred >= m_progress.fileSize) {
        finishSuccessfully();
        return;
    }

    if (!m_paused) {
        m_sendTimer->start(m_options.intervalMs);
    }
}

void RawFileTransfer::finishSuccessfully()
{
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    m_pendingChunk.clear();
    m_waitingLocalSendResult = false;

    refreshSpeed();
    setState(TransferState::Completed);
    updateProgress();
    emit transferCompleted(true, tr("裸流文件发送完成"));
}

void RawFileTransfer::failWithMessage(const QString& message)
{
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    m_pendingChunk.clear();
    m_waitingLocalSendResult = false;

    m_progress.errorMessage = message;
    setState(TransferState::Failed);
    updateProgress();
    emit transferCompleted(false, message);
}

void RawFileTransfer::refreshSpeed()
{
    const qint64 elapsed = m_elapsedTimer.isValid() ? m_elapsedTimer.elapsed() : 0;
    m_progress.elapsedMs = static_cast<int>(elapsed);
    m_progress.speed = elapsed > 0
        ? (m_progress.bytesTransferred * 1000.0 / elapsed)
        : 0.0;
}

// ============== OtaFileTransfer 实现 ==============

OtaFileTransfer::OtaFileTransfer(QObject* parent)
    : FileTransfer(parent)
{
    m_sendTimer = new QTimer(this);
    m_sendTimer->setSingleShot(true);
    connect(m_sendTimer, &QTimer::timeout, this, &OtaFileTransfer::sendNextDataPacket);
    connect(m_timeoutTimer, &QTimer::timeout, this, &OtaFileTransfer::onAckTimeout);
}

OtaFileTransfer::~OtaFileTransfer()
{
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void OtaFileTransfer::setOptions(const OtaTransferOptions& options)
{
    /*
     * OTA 的 ACK token 允许用户配置，但空 token 会让状态机无法判断确认，
     * 因此空值回退到默认 "ACK"。
     */
    m_options = options;
    m_options.blockSize = sanitizeTransferBlockSize(options.blockSize);
    m_options.intervalMs = sanitizeIntervalMs(options.intervalMs);
    m_options.timeoutMs = qMax(1, options.timeoutMs);
    m_options.maxRetries = qMax(0, options.maxRetries);
    if (m_options.ackToken.isEmpty()) {
        m_options.ackToken = "ACK";
    }
}

bool OtaFileTransfer::startSend(const QString& filePath)
{
    if (m_state != TransferState::Idle) {
        LOG_WARN("OTA transfer already in progress");
        return false;
    }

    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    m_file = new QFile(filePath);
    if (!m_file->open(QIODevice::ReadOnly)) {
        failWithMessage(tr("无法打开文件: %1").arg(m_file->errorString()));
        return false;
    }

    const QFileInfo fileInfo(filePath);
    if (fileInfo.size() > 0xFFFFFFFFLL) {
        failWithMessage(tr("自定义 OTA 第一版仅支持不超过 4GB 的文件。"));
        return false;
    }

    QString crcError;
    if (!calculateFileCrc32(*m_file, m_fileCrc32, crcError)) {
        failWithMessage(tr("计算文件 CRC32 失败: %1").arg(crcError));
        return false;
    }

    m_progress = TransferProgress();
    m_progress.fileName = fileInfo.fileName();
    m_progress.fileSize = fileInfo.size();
    m_progress.totalPackets = m_options.blockSize > 0
        ? static_cast<int>((m_progress.fileSize + m_options.blockSize - 1) / m_options.blockSize)
        : 0;
    m_progress.currentPacket = 0;
    m_progress.bytesTransferred = 0;

    m_currentBlock = 0;
    m_retryCount = 0;
    m_receiveBuffer.clear();
    m_pendingPacket.clear();
    m_pendingPayloadBytes = 0;
    m_paused = false;
    m_waitingLocalSendResult = false;
    m_pendingLocalSendStage = OtaStage::Idle;
    m_waitingAckFor = OtaStage::Idle;
    m_elapsedTimer.restart();

    setState(TransferState::Running);
    updateProgress();
    sendHeaderPacket();
    return true;
}

bool OtaFileTransfer::startReceive(const QString& savePath)
{
    Q_UNUSED(savePath)
    failWithMessage(tr("自定义 OTA 接收暂未实现，请使用发送模式。"));
    return false;
}

void OtaFileTransfer::cancel()
{
    /*
     * OTA 取消同样先进入 Cancelling，随后停止发送/ACK 定时器并释放文件。
     * 这样 UI 能把“正在释放资源”和“已经取消”两个阶段区分开。
     */
    setState(TransferState::Cancelling);

    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    m_paused = false;
    m_waitingLocalSendResult = false;
    m_pendingLocalSendStage = OtaStage::Idle;
    m_pendingPayloadBytes = 0;
    m_waitingAckFor = OtaStage::Idle;
    setState(TransferState::Cancelled);
    emit transferCompleted(false, tr("传输已取消"));
}

void OtaFileTransfer::processReceivedData(const QByteArray& data)
{
    if (!m_options.waitAck || m_waitingAckFor == OtaStage::Idle) {
        return;
    }

    /*
     * ACK 可能与设备日志一起返回，也可能被串口分片。因此先累积缓冲区，
     * 再查找用户配置的 token；找到后清空缓冲，推进当前等待的阶段。
     */
    m_receiveBuffer.append(data);
    if (m_receiveBuffer.contains(m_options.ackToken)) {
        const OtaStage ackStage = m_waitingAckFor;
        m_receiveBuffer.clear();
        handleAck(ackStage);
    }
}

void OtaFileTransfer::notifyLocalSendResult(bool success, const QString& errorMessage)
{
    if (!m_waitingLocalSendResult) {
        return;
    }

    if (!success) {
        failWithMessage(errorMessage.isEmpty()
            ? tr("本地发送队列处理 OTA 数据失败")
            : errorMessage);
        return;
    }

    continueAfterLocalSendAccepted();
}

void OtaFileTransfer::pause()
{
    if (m_state != TransferState::Running || m_paused) {
        return;
    }

    m_paused = true;
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    setState(TransferState::Paused);
    updateProgress();
}

void OtaFileTransfer::resume()
{
    if (m_state != TransferState::Paused || !m_paused) {
        return;
    }

    m_paused = false;
    setState(TransferState::Running);

    /*
     * 暂停时若正等待 ACK，恢复后继续等待同一包的 ACK；否则从当前文件
     * 偏移继续发送下一块。这样不会因为暂停/继续导致块序号跳变。
     */
    if (m_waitingLocalSendResult) {
        return;
    }

    if (m_options.waitAck && m_waitingAckFor != OtaStage::Idle && !m_pendingPacket.isEmpty()) {
        m_timeoutTimer->start(m_options.timeoutMs);
    } else {
        scheduleNextDataPacket();
    }
}

bool OtaFileTransfer::parseMagicText(const QString& text, quint32& magic, QString* errorMessage)
{
    /*
     * Magic 同时面向两类用户：旧版界面中的 4 字节 ASCII（例如 OTA1），
     * 以及固件代码中常见的 uint32_t 常量（例如 0x474F5441UL）。
     * 解析集中放在核心层，避免 UI、测试和后续配置文件各自维护一套规则。
     */
    const QString trimmed = text.trimmed();
    auto setError = [errorMessage](const QString& message) {
        if (errorMessage) {
            *errorMessage = message;
        }
    };

    if (trimmed.isEmpty()) {
        setError(QObject::tr("OTA Magic 不能为空"));
        return false;
    }

    QString numericText = trimmed;
    while (numericText.endsWith(QLatin1Char('u'), Qt::CaseInsensitive) ||
           numericText.endsWith(QLatin1Char('l'), Qt::CaseInsensitive)) {
        numericText.chop(1);
    }

    static const QRegularExpression hexPattern(QStringLiteral("^0[xX][0-9a-fA-F]+$"));
    if (hexPattern.match(numericText).hasMatch()) {
        bool ok = false;
        const quint64 value = numericText.mid(2).toULongLong(&ok, 16);
        if (!ok || value > 0xFFFFFFFFULL) {
            setError(QObject::tr("OTA Magic 十六进制值必须在 uint32_t 范围内"));
            return false;
        }

        magic = static_cast<quint32>(value);
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    const QByteArray ascii = trimmed.toLatin1();
    if (ascii.size() > 4) {
        setError(QObject::tr("OTA Magic ASCII 模式最多 4 字节；十六进制请使用 0x12345678UL"));
        return false;
    }

    magic = 0;
    for (int index = 0; index < ascii.size(); ++index) {
        /*
         * ASCII 输入按线上字节顺序写入头包。由于发包统一使用 appendLe32()，
         * 这里反向还原成小端 uint32_t，确保 "OTA1" 仍输出 4F 54 41 31。
         */
        magic |= static_cast<quint32>(static_cast<quint8>(ascii.at(index))) << (8 * index);
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QByteArray OtaFileTransfer::buildHeaderPacketForTest(quint32 magic,
                                                     const QString& fileName,
                                                     qint64 fileSize,
                                                     quint32 crc32,
                                                     int blockSize)
{
    QByteArray packet;
    appendLe32(packet, magic);
    appendLe32(packet, static_cast<quint32>(fileSize));
    appendLe32(packet, crc32);
    appendLe16(packet, static_cast<quint16>(sanitizeTransferBlockSize(blockSize)));

    /*
     * 文件名长度使用 1 字节，便于小 MCU 分配固定接收缓冲区。超过 255
     * 字节时截断，用户仍可通过文件大小和 CRC 确认固件内容。
     */
    QByteArray fileNameBytes = fileName.toUtf8();
    if (fileNameBytes.size() > 255) {
        fileNameBytes = fileNameBytes.left(255);
    }
    packet.append(static_cast<char>(fileNameBytes.size() & 0xFF));
    packet.append(fileNameBytes);
    return packet;
}

QByteArray OtaFileTransfer::buildDataPacketForTest(quint32 blockIndex, const QByteArray& payload)
{
    QByteArray packet;
    appendLe32(packet, blockIndex);
    appendLe16(packet, static_cast<quint16>(payload.size()));
    packet.append(payload);

    const quint16 crc = ChecksumUtils::crc16XMODEM(packet);
    appendLe16(packet, crc);
    return packet;
}

QByteArray OtaFileTransfer::buildEndPacketForTest(quint32 crc32)
{
    QByteArray packet("DONE", 4);
    appendLe32(packet, crc32);
    return packet;
}

void OtaFileTransfer::sendHeaderPacket()
{
    if (!m_file || m_state != TransferState::Running || m_paused || m_waitingLocalSendResult) {
        return;
    }

    m_pendingPacket = buildHeaderPacketForTest(
        m_options.magic,
        m_progress.fileName,
        m_progress.fileSize,
        m_fileCrc32,
        m_options.blockSize);
    m_pendingPayloadBytes = 0;
    m_pendingLocalSendStage = OtaStage::Header;
    m_waitingLocalSendResult = true;
    emit sendData(m_pendingPacket);
}

void OtaFileTransfer::sendNextDataPacket()
{
    if (!m_file || m_state != TransferState::Running || m_paused || m_waitingLocalSendResult) {
        return;
    }

    const QByteArray payload = m_file->read(m_options.blockSize);
    if (payload.isEmpty()) {
        if (m_file->error() != QFileDevice::NoError) {
            failWithMessage(tr("读取文件失败: %1").arg(m_file->errorString()));
            return;
        }
        sendEndPacket();
        return;
    }

    m_pendingPacket = buildDataPacketForTest(m_currentBlock, payload);
    m_pendingPayloadBytes = payload.size();
    m_pendingLocalSendStage = OtaStage::Data;
    m_waitingLocalSendResult = true;
    emit sendData(m_pendingPacket);
}

void OtaFileTransfer::sendEndPacket()
{
    if (m_state != TransferState::Running || m_paused || m_waitingLocalSendResult) {
        return;
    }

    m_pendingPacket = buildEndPacketForTest(m_fileCrc32);
    m_pendingPayloadBytes = 0;
    m_pendingLocalSendStage = OtaStage::End;
    m_waitingLocalSendResult = true;
    emit sendData(m_pendingPacket);
}

void OtaFileTransfer::continueAfterLocalSendAccepted()
{
    const OtaStage stage = m_pendingLocalSendStage;
    m_waitingLocalSendResult = false;
    m_pendingLocalSendStage = OtaStage::Idle;

    /*
     * 本地发送队列确认后再更新 OTA 阶段。对数据块而言，payload 已经从
     * QFile 读取并缓存在 m_pendingPacket；如果后续等待设备 ACK 超时，
     * 仍会重发同一个 pendingPacket，不会移动文件指针。
     */
    if (stage == OtaStage::Data) {
        m_progress.bytesTransferred += m_pendingPayloadBytes;
        m_progress.currentPacket = static_cast<int>(m_currentBlock + 1);
        refreshSpeed();
        updateProgress();
    }

    if (m_options.waitAck) {
        waitForAck(stage);
        return;
    }

    switch (stage) {
    case OtaStage::Header:
        scheduleNextDataPacket();
        break;
    case OtaStage::Data:
        m_currentBlock++;
        scheduleNextDataPacket();
        break;
    case OtaStage::End:
        finishSuccessfully();
        break;
    default:
        break;
    }

    m_pendingPayloadBytes = 0;
}

void OtaFileTransfer::scheduleNextDataPacket()
{
    if (m_sendTimer && m_state == TransferState::Running && !m_paused) {
        m_sendTimer->start(m_options.intervalMs);
    }
}

void OtaFileTransfer::waitForAck(OtaStage stage)
{
    m_waitingAckFor = stage;
    m_retryCount = 0;
    m_progress.retryCount = 0;
    m_receiveBuffer.clear();
    if (m_timeoutTimer) {
        m_timeoutTimer->start(m_options.timeoutMs);
    }
}

void OtaFileTransfer::handleAck(OtaStage stage)
{
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    m_waitingAckFor = OtaStage::Idle;
    m_retryCount = 0;
    m_progress.retryCount = 0;

    switch (stage) {
    case OtaStage::Header:
        scheduleNextDataPacket();
        break;
    case OtaStage::Data:
        m_currentBlock++;
        scheduleNextDataPacket();
        break;
    case OtaStage::End:
        finishSuccessfully();
        break;
    default:
        break;
    }
}

void OtaFileTransfer::onAckTimeout()
{
    if (m_paused || m_waitingAckFor == OtaStage::Idle || m_pendingPacket.isEmpty()) {
        return;
    }

    m_retryCount++;
    m_progress.retryCount = m_retryCount;
    if (m_retryCount > m_options.maxRetries) {
        failWithMessage(tr("等待 OTA ACK 超时"));
        return;
    }

    /*
     * ACK 超时后重发同一个 pendingPacket，不移动文件指针和块序号。
     * 对数据块而言，payload 在首次发送时已经从 QFile 读出，因此必须缓存
     * 完整包，不能重新 read()，否则会跳过当前块。
     */
    emit sendData(m_pendingPacket);
    updateProgress();
    m_timeoutTimer->start(m_options.timeoutMs);
}

void OtaFileTransfer::finishSuccessfully()
{
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    m_waitingLocalSendResult = false;
    m_pendingLocalSendStage = OtaStage::Idle;
    m_pendingPayloadBytes = 0;

    refreshSpeed();
    setState(TransferState::Completed);
    updateProgress();
    emit transferCompleted(true, tr("OTA 文件发送完成"));
}

void OtaFileTransfer::failWithMessage(const QString& message)
{
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    m_waitingLocalSendResult = false;
    m_pendingLocalSendStage = OtaStage::Idle;
    m_pendingPayloadBytes = 0;

    m_progress.errorMessage = message;
    setState(TransferState::Failed);
    updateProgress();
    emit transferCompleted(false, message);
}

void OtaFileTransfer::refreshSpeed()
{
    const qint64 elapsed = m_elapsedTimer.isValid() ? m_elapsedTimer.elapsed() : 0;
    m_progress.elapsedMs = static_cast<int>(elapsed);
    m_progress.speed = elapsed > 0
        ? (m_progress.bytesTransferred * 1000.0 / elapsed)
        : 0.0;
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
    closeActiveFile();
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
        failTransferWithMessage(tr("无法打开文件"));
        return false;
    }

    QFileInfo fileInfo(filePath);
    m_progress.fileName = fileInfo.fileName();
    m_progress.fileSize = fileInfo.size();
    m_progress.bytesTransferred = 0;
    m_progress.totalPackets = (m_progress.fileSize + m_blockSize - 1) / m_blockSize;
    m_progress.currentPacket = 0;
    m_progress.retryCount = 0;
    m_progress.errorMessage.clear();

    m_packetNumber = 1;
    m_retryCount = 0;
    m_direction = TransferDirection::Send;
    m_sendState = SendState::WaitingC;
    m_lastPacket.clear();
    m_fileData.clear();

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
        failTransferWithMessage(tr("无法创建文件"));
        return false;
    }

    m_fileData.clear();
    m_receiveBuffer.clear();
    m_progress.fileName = QFileInfo(savePath).fileName();
    m_progress.fileSize = 0;
    m_progress.bytesTransferred = 0;
    m_progress.currentPacket = 0;
    m_progress.retryCount = 0;
    m_progress.errorMessage.clear();

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

    closeActiveFile();

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
                    closeActiveFile();
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
                        failTransferWithMessage(tr("重试次数超限"));
                        return;
                    }
                    emit sendData(m_lastPacket);
                    m_timeoutTimer->start(m_timeoutMs);
                } else if (c == CAN) {
                    closeActiveFile();
                    setState(TransferState::Cancelled);
                    emit transferCompleted(false, tr("接收方取消"));
                    return;
                }
                break;

            case SendState::SendingEOT:
                if (c == ACK) {
                    closeActiveFile();
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

    if (m_direction == TransferDirection::Send) {
        /*
         * 发送模式逐字节处理对端 ACK/NAK/CAN，处理完即可清空响应缓存。
         * 接收模式必须保留半包，不能在这里无条件 clear。
         */
        m_receiveBuffer.clear();
    }
}

void XModemTransfer::processReceiveData()
{
    int expectedPacketSize = 3 + m_blockSize + (m_useCRC ? 2 : 1);

    while (!m_receiveBuffer.isEmpty()) {
        char header = m_receiveBuffer[0];

        if (header == EOT) {
            // 传输结束
            m_receiveBuffer.remove(0, 1);
            QByteArray ack;
            ack.append(ACK);
            emit sendData(ack);

            /*
             * 合法数据包已经在接收过程中直接写入文件。EOT 阶段只负责
             * 刷盘并关闭句柄，不再把 m_fileData 再写一遍。
             */
            if (m_file) {
                m_file->flush();
            }
            closeActiveFile();

            setState(TransferState::Completed);
            updateProgress();
            emit transferCompleted(true, tr("接收完成"));
            return;
        }

        if (header == CAN) {
            m_receiveBuffer.remove(0, 1);
            closeActiveFile();
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
            if (!m_file || !m_file->isOpen()) {
                failTransferWithMessage(tr("文件未打开"));
                return;
            }
            if (m_file->write(blockData) != static_cast<qint64>(blockData.size())) {
                failTransferWithMessage(tr("写入文件失败"));
                return;
            }

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
                failTransferWithMessage(tr("校验错误次数超限"));
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
    /*
     * 标准 XMODEM 发送按 ACK 推进。这里只读取当前包对应的文件片段，
     * 并把完整协议包保存到 m_lastPacket 供 NAK/超时重发；不再把整个
     * 固件读入 m_fileData，降低大文件发送峰值内存。
     */
    const qint64 offset = static_cast<qint64>(m_packetNumber - 1) * m_blockSize;

    if (offset >= m_progress.fileSize) {
        /*
         * 文件内容已经全部读完。EOT 阶段只需要控制字节和 m_lastPacket
         * 不再需要源文件，立即关闭句柄，方便用户删除或覆盖源文件。
         */
        closeActiveFile();
        m_sendState = SendState::SendingEOT;
        QByteArray eot;
        eot.append(EOT);
        emit sendData(eot);
        m_timeoutTimer->start(m_timeoutMs);
        return;
    }

    if (!m_file || !m_file->isOpen()) {
        failTransferWithMessage(tr("文件未打开"));
        return;
    }

    if (!m_file->seek(offset)) {
        failTransferWithMessage(tr("读取文件失败"));
        return;
    }

    QByteArray blockData = m_file->read(m_blockSize);
    if (blockData.isEmpty() && offset < m_progress.fileSize) {
        failTransferWithMessage(tr("读取文件失败"));
        return;
    }

    // 填充到块大小
    while (blockData.size() < m_blockSize) {
        blockData.append(CPMEOF);
    }

    m_lastPacket = buildPacket(m_packetNumber, blockData);
    emit sendData(m_lastPacket);

    m_progress.bytesTransferred = qMin(offset + static_cast<qint64>(m_blockSize),
                                       m_progress.fileSize);
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

void XModemTransfer::closeActiveFile()
{
    /*
     * QFile 指针在发送和接收路径共用。集中释放可以保证所有终止路径都把
     * 句柄置空，后续 cancel()/析构再次调用时不会重复 delete。
     */
    if (!m_file) {
        return;
    }

    m_file->close();
    delete m_file;
    m_file = nullptr;
}

void XModemTransfer::failTransferWithMessage(const QString& message)
{
    /*
     * 失败路径必须同时停止计时器、释放文件句柄并写入 progress，确保 UI
     * 看到稳定错误状态，且超时回调不会在失败后继续重发旧包。
     */
    m_timeoutTimer->stop();
    closeActiveFile();
    m_progress.errorMessage = message;
    setState(TransferState::Failed);
    updateProgress();
    emit transferCompleted(false, message);
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
    /*
     * 超时重发必须按当前发送阶段选择内容：数据阶段重发 m_lastPacket，
     * EOT 阶段重发 EOT 控制字节。旧实现只看 m_lastPacket，导致文件
     * 数据发送完后 EOT 超时会错误重发最后一个数据包。
     */
    m_retryCount++;
    m_progress.retryCount = m_retryCount;

    if (m_retryCount > m_maxRetries) {
        failTransferWithMessage(tr("传输超时"));
        return;
    }

    LOG_WARN(QString("XMODEM timeout, retry %1/%2").arg(m_retryCount).arg(m_maxRetries));

    if (m_direction == TransferDirection::Receive) {
        // 重发开始信号
        QByteArray startChar;
        startChar.append(m_useCRC ? CRC_START : NAK);
        emit sendData(startChar);
    } else if (m_sendState == SendState::SendingEOT) {
        QByteArray eot;
        eot.append(EOT);
        emit sendData(eot);
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
    /*
     * YMODEM-G 数据阶段不等待逐包 ACK，但仍要避免同步循环一次性构造并
     * 发送所有数据包。0ms 单次定时器可以在事件循环中逐包推进，降低
     * UI 卡顿和发送队列瞬时内存峰值。
     */
    m_streamTimer = new QTimer(this);
    m_streamTimer->setSingleShot(true);
    connect(m_streamTimer, &QTimer::timeout,
            this, &YModemTransfer::sendNextYModemGStreamPacket);
}

YModemTransfer::~YModemTransfer()
{
    if (m_streamTimer) {
        m_streamTimer->stop();
    }
    closeActiveFile();
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
    m_retryCount = 0;
    m_receiveBuffer.clear();
    if (m_streamTimer) {
        m_streamTimer->stop();
    }

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
    const QFileInfo saveInfo(savePath);
    const bool receiveToDirectory = saveInfo.exists() && saveInfo.isDir();
    const bool receiveToExplicitFile = !receiveToDirectory && !savePath.trimmed().isEmpty();
    if (!receiveToDirectory && !receiveToExplicitFile) {
        m_progress.errorMessage = tr("保存路径无效");
        setState(TransferState::Error);
        emit transferCompleted(false, m_progress.errorMessage);
        return false;
    }

    m_direction = TransferDirection::Receive;
    m_receiveState = ReceiveState::Starting;
    m_receiveBuffer.clear();
    if (m_streamTimer) {
        m_streamTimer->stop();
    }
    m_currentReceiveFilePath.clear();
    m_receiveFileSize = 0;
    m_receiveBytes = 0;
    m_receivedFileCount = 0;
    m_retryCount = 0;
    m_packetNumber = 0;
    m_progress = TransferProgress();
    m_progress.fileName = receiveToExplicitFile ? saveInfo.fileName() : QString();
    m_progress.state = TransferState::WaitingStart;

    setState(TransferState::WaitingStart);

    /*
     * 普通 YMODEM 通过 'C' 请求 CRC 模式；YMODEM-G 通过 'G' 告诉发送端
     * 进入无逐包 ACK 的流式模式。
     */
    sendControlByte(m_useG ? 'G' : CRC_START);

    m_timeoutTimer->start(m_timeoutMs);

    LOG_INFO("YMODEM receive started");
    return true;
}

void YModemTransfer::cancel()
{
    m_timeoutTimer->stop();
    if (m_streamTimer) {
        m_streamTimer->stop();
    }

    QByteArray canData;
    canData.append(CAN);
    canData.append(CAN);
    canData.append(CAN);
    emit sendData(canData);

    closeActiveFile();

    setState(TransferState::Cancelled);
    emit transferCompleted(false, tr("传输已取消"));
}

void YModemTransfer::processReceivedData(const QByteArray& data)
{
    if (isTerminalState()) {
        return;
    }

    m_receiveBuffer.append(data);
    m_timeoutTimer->stop();

    // 处理发送模式响应
    if (m_direction == TransferDirection::Send) {
        for (char c : m_receiveBuffer) {
            switch (m_sendState) {
            case SendState::WaitingC:
                if ((!m_useG && c == CRC_START) || (m_useG && c == 'G')) {
                    sendFileHeader();
                } else if (c == CAN) {
                    closeActiveFile();
                    setState(TransferState::Cancelled);
                    emit transferCompleted(false, tr("接收方取消"));
                    return;
                }
                break;

            case SendState::WaitingHeaderAck:
                if (c == ACK) {
                    // 等待C开始数据传输
                } else if ((!m_useG && c == CRC_START) || (m_useG && c == 'G')) {
                    m_sendState = SendState::SendingData;
                    m_packetNumber = 1;
                    setState(TransferState::Transferring);
                    if (m_useG) {
                        startYModemGStream();
                    } else {
                        sendNextPacket();
                    }
                } else if (c == CAN) {
                    closeActiveFile();
                    setState(TransferState::Cancelled);
                    emit transferCompleted(false, tr("接收方取消"));
                    return;
                }
                break;

            case SendState::WaitingDataAck:
                if (m_useG) {
                    /*
                     * G 模式正常情况下不会进入该分支；保留 CAN 处理是为了
                     * 兼容接收端发现错误后立即中止的场景。
                     */
                    if (c == CAN) {
                        closeActiveFile();
                        if (m_streamTimer) {
                            m_streamTimer->stop();
                        }
                        setState(TransferState::Cancelled);
                        emit transferCompleted(false, tr("接收方取消"));
                        return;
                    }
                    break;
                }
                if (c == ACK) {
                    /*
                     * 重试次数属于当前待确认包。一个包最终 ACK 后必须清零，
                     * 否则多个不同包的偶发 NAK 会累加，导致后续包过早失败。
                     */
                    m_retryCount = 0;
                    m_progress.retryCount = 0;
                    m_packetNumber++;
                    m_sendState = SendState::SendingData;
                    sendNextPacket();
                } else if (c == NAK) {
                    /*
                     * 数据包可能因线路噪声损坏。流式发送只缓存当前待 ACK
                     * 的完整协议包，收到 NAK 时直接重发，不重新读下一块。
                     */
                    if (!m_lastPacket.isEmpty()) {
                        ++m_retryCount;
                        m_progress.retryCount = m_retryCount;
                        if (m_retryCount > m_maxRetries) {
                            failTransferWithMessage(tr("重试次数超限"));
                            return;
                        }
                        emit sendData(m_lastPacket);
                    }
                } else if (c == CAN) {
                    closeActiveFile();
                    setState(TransferState::Cancelled);
                    emit transferCompleted(false, tr("接收方取消"));
                    return;
                }
                break;

            case SendState::WaitingEOTAck:
                if (c == NAK) {
                    // G 模式没有双 EOT 流程；若对端返回 NAK，按普通模式重发 EOT。
                    QByteArray eot;
                    eot.append(EOT);
                    emit sendData(eot);
                } else if (c == ACK) {
                    // 等待C准备下一个文件或结束
                } else if ((!m_useG && c == CRC_START) || (m_useG && c == 'G')) {
                    m_currentFileIndex++;
                    if (m_currentFileIndex < m_filesToSend.size()) {
                        m_sendState = SendState::WaitingC;
                        sendFileHeader();
                    } else {
                        sendEndOfBatch();
                    }
                } else if (c == CAN) {
                    closeActiveFile();
                    setState(TransferState::Cancelled);
                    emit transferCompleted(false, tr("接收方取消"));
                    return;
                }
                break;

            default:
                break;
            }
        }
    } else {
        processReceiveData();
    }

    if (m_direction == TransferDirection::Send) {
        m_receiveBuffer.clear();
    }

    if (!isTerminalState() &&
        !(m_direction == TransferDirection::Send &&
          m_useG &&
          m_sendState == SendState::SendingData)) {
        m_timeoutTimer->start(m_timeoutMs);
    }
}

void YModemTransfer::sendFileHeader()
{
    if (m_currentFileIndex >= m_filesToSend.size()) {
        sendEndOfBatch();
        return;
    }

    /*
     * 每个文件发送前都关闭上一轮可能遗留的句柄，再打开当前源文件。头包
     * 只需要文件名和大小，不能 readAll() 整个文件；数据包阶段再按 1K
     * 从 QFile 读取。
     */
    closeActiveFile();

    const QString filePath = m_filesToSend[m_currentFileIndex];
    m_file = new QFile(filePath);
    if (!m_file->open(QIODevice::ReadOnly)) {
        delete m_file;
        m_file = nullptr;
        failTransferWithMessage(tr("无法打开文件"));
        return;
    }

    QFileInfo fileInfo(filePath);
    m_progress.fileName = fileInfo.fileName();
    m_progress.fileSize = fileInfo.size();
    m_progress.bytesTransferred = 0;
    m_progress.totalPackets = (m_progress.fileSize + 1023) / 1024;
    m_progress.currentPacket = 0;
    m_progress.retryCount = 0;
    m_progress.errorMessage.clear();
    m_retryCount = 0;
    m_lastPacket.clear();
    m_fileData.clear();

    QByteArray headerPacket = buildHeaderPacket(m_progress.fileName, m_progress.fileSize);
    emit sendData(headerPacket);

    m_sendState = SendState::WaitingHeaderAck;
    m_timeoutTimer->start(m_timeoutMs);

    LOG_INFO(QString("YMODEM sending header for: %1").arg(m_progress.fileName));
}

void YModemTransfer::sendNextPacket()
{
    /*
     * YMODEM 数据包固定 1024 字节。这里仅读取当前包对应的文件片段，并把
     * 完整协议包保存到 m_lastPacket 供 NAK 或超时重发，避免整文件常驻
     * m_fileData。
     */
    const qint64 offset = static_cast<qint64>(m_packetNumber - 1) * 1024;

    if (offset >= m_progress.fileSize) {
        /*
         * 文件数据已经全部发送完，EOT 只需要控制字节。此时关闭源文件，
         * 后续 NAK 重发 EOT 不需要再读取文件。
         */
        closeActiveFile();
        m_sendState = SendState::WaitingEOTAck;
        QByteArray eot;
        eot.append(EOT);
        emit sendData(eot);
        m_timeoutTimer->start(m_timeoutMs);
        return;
    }

    if (!m_file || !m_file->isOpen()) {
        failTransferWithMessage(tr("文件未打开"));
        return;
    }

    if (!m_file->seek(offset)) {
        failTransferWithMessage(tr("读取文件失败"));
        return;
    }

    QByteArray blockData = m_file->read(1024);
    if (blockData.isEmpty() && offset < m_progress.fileSize) {
        failTransferWithMessage(tr("读取文件失败"));
        return;
    }

    while (blockData.size() < 1024) {
        blockData.append(CPMEOF);
    }

    m_lastPacket = buildDataPacket(m_packetNumber, blockData);
    emit sendData(m_lastPacket);

    m_progress.bytesTransferred = qMin(offset + static_cast<qint64>(1024), m_progress.fileSize);
    m_progress.currentPacket = m_packetNumber;
    updateProgress();

    m_sendState = SendState::WaitingDataAck;
    m_timeoutTimer->start(m_timeoutMs);
}

void YModemTransfer::startYModemGStream()
{
    /*
     * 收到第二个 G 后进入数据流。包号从 1 开始，后续由定时器逐包读文件
     * 并发送；不设置 WaitingDataAck，避免误把 ACK 当成推进条件。
     */
    if (!m_useG) {
        sendNextPacket();
        return;
    }

    m_sendState = SendState::SendingData;
    m_packetNumber = 1;
    m_retryCount = 0;
    m_progress.retryCount = 0;
    m_timeoutTimer->stop();
    if (m_streamTimer) {
        m_streamTimer->start(0);
    } else {
        sendNextYModemGStreamPacket();
    }
}

void YModemTransfer::sendNextYModemGStreamPacket()
{
    /*
     * 终止状态或非发送方向下忽略定时器残留回调，避免完成/取消后继续读
     * 文件。普通 YMODEM 不走这里。
     */
    if (!m_useG || m_direction != TransferDirection::Send || isTerminalState()) {
        return;
    }

    const qint64 offset = static_cast<qint64>(m_packetNumber - 1) * 1024;
    if (offset >= m_progress.fileSize) {
        closeActiveFile();
        m_lastPacket.clear();
        m_sendState = SendState::WaitingEOTAck;
        sendControlByte(EOT);
        m_timeoutTimer->start(m_timeoutMs);
        return;
    }

    if (!m_file || !m_file->isOpen()) {
        failTransferWithMessage(tr("文件未打开"));
        return;
    }

    if (!m_file->seek(offset)) {
        failTransferWithMessage(tr("读取文件失败"));
        return;
    }

    QByteArray blockData = m_file->read(1024);
    if (blockData.isEmpty() && offset < m_progress.fileSize) {
        failTransferWithMessage(tr("读取文件失败"));
        return;
    }

    while (blockData.size() < 1024) {
        blockData.append(CPMEOF);
    }

    m_lastPacket = buildDataPacket(m_packetNumber, blockData);
    emit sendData(m_lastPacket);

    m_progress.bytesTransferred = qMin(offset + static_cast<qint64>(1024), m_progress.fileSize);
    m_progress.currentPacket = m_packetNumber;
    updateProgress();

    ++m_packetNumber;
    if (m_streamTimer) {
        m_streamTimer->start(0);
    } else {
        sendNextYModemGStreamPacket();
    }
}

void YModemTransfer::sendEndOfBatch()
{
    /*
     * 空 0 号头包表示批次结束。发送前确保没有源文件句柄残留；完成后
     * 清空当前重发包，避免后续串口残留字节误触发旧包重发。
     */
    closeActiveFile();
    if (m_streamTimer) {
        m_streamTimer->stop();
    }
    m_lastPacket.clear();
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

quint16 YModemTransfer::calculateCRC16(const QByteArray& data) const
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

void YModemTransfer::processReceiveData()
{
    /*
     * YMODEM 接收采用以下阶段：
     * 1. 主动发送 'C' 请求 CRC 模式；
     * 2. 等待 0 号文件头，解析文件名和文件大小；
     * 3. ACK 头包并再次发送 'C' 请求数据包；
     * 4. 接收 1..N 号 1024 字节数据包，按头包声明大小写入文件；
     * 5. 第一个 EOT 返回 NAK，第二个 EOT 返回 ACK + 'C'；
     * 6. 收到空 0 号头包后 ACK 并完成批次。
     *
     * YMODEM-G 在此基础上把起始字符改为 'G'，数据包不逐包 ACK，
     * 并在发现错误时立即 CAN 中止，因为 G 模式没有 NAK 重传语义。
     */
    while (!m_receiveBuffer.isEmpty()) {
        const char header = m_receiveBuffer.at(0);

        if (header == CAN) {
            /*
             * 对端取消时可能已经打开目标文件。必须释放文件句柄后再进入
             * Cancelled，避免 Windows 下残留文件被本进程锁住。
             */
            m_timeoutTimer->stop();
            if (m_file) {
                m_file->close();
                delete m_file;
                m_file = nullptr;
            }
            setState(TransferState::Cancelled);
            emit transferCompleted(false, tr("发送方取消"));
            return;
        }

        if (header == EOT) {
            m_receiveBuffer.remove(0, 1);
            if (m_useG) {
                /*
                 * G 模式单 EOT 后直接确认，并发送 G 请求下一文件头。普通
                 * YMODEM 的双 EOT/NAK 流程不适用于无逐包 ACK 的高速模式。
                 */
                if (m_receiveState == ReceiveState::ReceivingData ||
                    m_receiveState == ReceiveState::WaitingHeader) {
                    finishCurrentReceiveFile();
                    sendControlByte(ACK);
                    sendControlByte('G');
                    m_receiveState = ReceiveState::WaitingHeader;
                    m_packetNumber = 0;
                    m_retryCount = 0;
                } else {
                    abortYModemGReceiveWithMessage(tr("YMODEM-G 收到异常 EOT"));
                    return;
                }
                continue;
            }

            if (m_receiveState == ReceiveState::ReceivingData) {
                m_receiveState = ReceiveState::WaitingSecondEot;
                sendControlByte(NAK);
            } else if (m_receiveState == ReceiveState::WaitingSecondEot) {
                finishCurrentReceiveFile();
                sendControlByte(ACK);
                sendControlByte(CRC_START);
                m_receiveState = ReceiveState::WaitingHeader;
                m_packetNumber = 0;
                m_retryCount = 0;
            } else {
                sendControlByte(NAK);
            }
            continue;
        }

        if (header != SOH && header != STX) {
            /*
             * 线路上可能混入状态字符或噪声。丢弃非包头字节并继续寻找
             * 下一个合法包头，避免一个无关字节阻塞整个接收状态机。
             */
            m_receiveBuffer.remove(0, 1);
            continue;
        }

        const int blockSize = (header == STX) ? 1024 : 128;
        const int packetSize = 3 + blockSize + 2;
        if (m_receiveBuffer.size() < packetSize) {
            return;
        }

        const QByteArray packet = m_receiveBuffer.left(packetSize);
        m_receiveBuffer.remove(0, packetSize);

        if (!verifyReceivePacket(packet)) {
            if (m_useG) {
                abortYModemGReceiveWithMessage(tr("YMODEM-G 数据包校验失败，已中止传输"));
                return;
            }
            ++m_retryCount;
            m_progress.retryCount = m_retryCount;
            updateProgress();
            if (m_retryCount > m_maxRetries) {
                failTransferWithMessage(tr("校验错误次数超限"));
                return;
            }
            sendControlByte(NAK);
            continue;
        }

        const int packetNum = static_cast<quint8>(packet.at(1));
        const QByteArray payload = packet.mid(3, blockSize);
        m_retryCount = 0;
        m_progress.retryCount = 0;

        if (packetNum == 0) {
            QString fileName;
            qint64 fileSize = 0;
            if (!parseReceiveHeader(payload, fileName, fileSize)) {
                if (m_useG) {
                    abortYModemGReceiveWithMessage(tr("YMODEM-G 文件头无效，已中止传输"));
                } else {
                    failTransferWithMessage(tr("YMODEM文件头无效"));
                }
                return;
            }

            if (fileName.isEmpty()) {
                if (m_file) {
                    finishCurrentReceiveFile();
                }
                sendControlByte(ACK);
                completeReceiveBatch(tr("接收完成"));
                return;
            }

            if (!openReceiveFile(fileName, fileSize)) {
                return;
            }

            sendControlByte(ACK);
            sendControlByte(m_useG ? 'G' : CRC_START);
            m_receiveState = ReceiveState::ReceivingData;
            m_packetNumber = 1;
            setState(TransferState::Running);
            updateProgress();
            continue;
        }

        if (m_receiveState != ReceiveState::ReceivingData) {
            if (m_useG) {
                abortYModemGReceiveWithMessage(tr("YMODEM-G 在非数据阶段收到数据包"));
                return;
            }
            sendControlByte(NAK);
            continue;
        }

        const int expectedPacketNumber = m_packetNumber & 0xFF;
        const int previousPacketNumber = (m_packetNumber - 1) & 0xFF;
        if (packetNum == previousPacketNumber) {
            /*
             * 发送端可能因为没有收到 ACK 而重发上一包。上一包已经写入时
             * 只补 ACK，不能重复写入文件，否则接收内容会被放大。
             */
            sendControlByte(ACK);
            continue;
        }

        if (packetNum != expectedPacketNumber) {
            if (m_useG) {
                abortYModemGReceiveWithMessage(tr("YMODEM-G 数据包序号错误，已中止传输"));
                return;
            }
            ++m_retryCount;
            m_progress.retryCount = m_retryCount;
            updateProgress();
            if (m_retryCount > m_maxRetries) {
                failTransferWithMessage(tr("包序号错误次数超限"));
                return;
            }
            sendControlByte(NAK);
            continue;
        }

        if (!writeReceivePayload(payload)) {
            return;
        }

        m_packetNumber = (m_packetNumber + 1) & 0xFF;
        if (!m_useG) {
            sendControlByte(ACK);
        }
        setState(TransferState::Running);
        updateProgress();
    }
}

bool YModemTransfer::verifyReceivePacket(const QByteArray& packet) const
{
    /*
     * YMODEM 包格式为 header + packet number + number complement +
     * payload + CRC16。接收端必须同时验证包号反码和 CRC，否则错误包
     * 可能被当成数据写入文件。
     */
    if (packet.size() < 3 + 128 + 2) {
        return false;
    }

    const char header = packet.at(0);
    const int blockSize = (header == STX) ? 1024 : 128;
    if (header != SOH && header != STX) {
        return false;
    }
    if (packet.size() != 3 + blockSize + 2) {
        return false;
    }

    const int packetNum = static_cast<quint8>(packet.at(1));
    const int packetNumComp = static_cast<quint8>(packet.at(2));
    if ((packetNum ^ packetNumComp) != 0xFF) {
        return false;
    }

    const QByteArray payload = packet.mid(3, blockSize);
    const int crcOffset = 3 + blockSize;
    const quint16 receivedCrc =
        static_cast<quint16>(static_cast<quint8>(packet.at(crcOffset)) << 8) |
        static_cast<quint16>(static_cast<quint8>(packet.at(crcOffset + 1)));
    return receivedCrc == calculateCRC16(payload);
}

bool YModemTransfer::parseReceiveHeader(const QByteArray& payload,
                                        QString& fileName,
                                        qint64& fileSize) const
{
    /*
     * YMODEM 0 号包 payload 形如：
     *   filename\0filesize [mtime mode serial]\0...
     * 本实现只强制解析文件名和大小，其余元数据保留兼容但不使用。
     */
    const int nameEnd = payload.indexOf('\0');
    if (nameEnd < 0) {
        return false;
    }

    fileName = QString::fromUtf8(payload.left(nameEnd)).trimmed();
    if (fileName.isEmpty()) {
        fileSize = 0;
        return true;
    }

    const QByteArray rest = payload.mid(nameEnd + 1);
    const QList<QByteArray> fields = rest.split(' ');
    if (fields.isEmpty() || fields.first().isEmpty()) {
        return false;
    }

    bool ok = false;
    fileSize = fields.first().toLongLong(&ok, 10);
    return ok && fileSize >= 0;
}

bool YModemTransfer::isSafeReceiveFileName(const QString& fileName) const
{
    /*
     * 文件名来自对端设备，必须视为不可信输入。这里只允许单个文件名，
     * 不接受绝对路径、盘符、目录分隔符或 ".."，防止写出用户选择目录。
     */
    if (fileName.isEmpty()) {
        return false;
    }
    if (fileName.contains(QLatin1Char('/')) ||
        fileName.contains(QLatin1Char('\\')) ||
        fileName.contains(QStringLiteral("..")) ||
        fileName.contains(QLatin1Char(':'))) {
        return false;
    }

    const QFileInfo info(fileName);
    return info.fileName() == fileName;
}

QString YModemTransfer::resolveReceiveFilePath(const QString& fileName) const
{
    /*
     * 用户可能选择保存目录，也可能通过保存对话框指定完整文件路径。
     * 选择目录时使用对端文件名；选择文件时尊重用户显式路径。
     */
    const QFileInfo saveInfo(m_savePath);
    if (saveInfo.exists() && saveInfo.isDir()) {
        return QDir(saveInfo.absoluteFilePath()).filePath(fileName);
    }
    return saveInfo.absoluteFilePath();
}

bool YModemTransfer::openReceiveFile(const QString& fileName, qint64 fileSize)
{
    /*
     * 收到 0 号头包后才知道真实文件名和大小，此时打开目标文件并初始化
     * 进度。若正在接收上一文件，先完成上一文件，支持后续批量扩展。
     */
    if (!isSafeReceiveFileName(fileName)) {
        failTransferWithMessage(tr("YMODEM文件名不安全"));
        return false;
    }

    if (m_file) {
        finishCurrentReceiveFile();
    }

    const QString targetPath = resolveReceiveFilePath(fileName);
    const QDir targetDir = QFileInfo(targetPath).absoluteDir();
    if (!targetDir.exists() && !targetDir.mkpath(QStringLiteral("."))) {
        failTransferWithMessage(tr("无法创建保存目录"));
        return false;
    }

    m_file = new QFile(targetPath);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = tr("无法创建文件: %1").arg(m_file->errorString());
        delete m_file;
        m_file = nullptr;
        failTransferWithMessage(error);
        return false;
    }

    m_currentReceiveFilePath = targetPath;
    m_receiveFileSize = fileSize;
    m_receiveBytes = 0;
    m_progress.fileName = QFileInfo(targetPath).fileName();
    m_progress.fileSize = fileSize;
    m_progress.bytesTransferred = 0;
    m_progress.currentPacket = 0;
    m_progress.totalPackets = static_cast<int>((fileSize + 1023) / 1024);
    m_progress.errorMessage.clear();
    return true;
}

bool YModemTransfer::writeReceivePayload(const QByteArray& payload)
{
    /*
     * YMODEM 最后一包会用 CPMEOF 填充到 1024 字节。这里按头包声明的
     * 文件大小截断写入，避免把填充字节保存到目标文件。
     */
    if (!m_file || !m_file->isOpen()) {
        failTransferWithMessage(tr("接收文件未打开"));
        return false;
    }

    const qint64 remaining = qMax<qint64>(0, m_receiveFileSize - m_receiveBytes);
    const int bytesToWrite = static_cast<int>(qMin<qint64>(remaining, payload.size()));
    if (bytesToWrite > 0) {
        const QByteArray chunk = payload.left(bytesToWrite);
        if (m_file->write(chunk) != chunk.size()) {
            failTransferWithMessage(tr("写入文件失败: %1").arg(m_file->errorString()));
            return false;
        }
        m_receiveBytes += bytesToWrite;
    }

    m_progress.bytesTransferred = m_receiveBytes;
    m_progress.currentPacket = (m_packetNumber & 0xFF);
    return true;
}

void YModemTransfer::finishCurrentReceiveFile()
{
    /*
     * 关闭文件集中放在这里，保证成功完成、批量下一个文件、取消和析构
     * 都不会重复关闭或泄漏 QFile。
     */
    if (!m_file) {
        return;
    }

    m_file->flush();
    m_file->close();
    delete m_file;
    m_file = nullptr;
    ++m_receivedFileCount;
}

void YModemTransfer::completeReceiveBatch(const QString& message)
{
    /*
     * 空 0 号头包表示 YMODEM 批次结束。完成时确保文件已关闭，状态与
     * progress 同步，然后只发一次完成信号。
     */
    m_timeoutTimer->stop();
    if (m_file) {
        finishCurrentReceiveFile();
    }
    m_receiveState = ReceiveState::Done;
    setState(TransferState::Completed);
    updateProgress();
    emit transferCompleted(true, message);
}

void YModemTransfer::failTransferWithMessage(const QString& message)
{
    /*
     * 失败路径必须停止超时计时器并释放当前文件句柄，避免状态已经失败
     * 但后台仍继续写入或超时回调再次触发。
     */
    m_timeoutTimer->stop();
    if (m_streamTimer) {
        m_streamTimer->stop();
    }
    closeActiveFile();
    m_progress.errorMessage = message;
    setState(TransferState::Failed);
    updateProgress();
    emit transferCompleted(false, message);
}

void YModemTransfer::abortYModemGReceiveWithMessage(const QString& message)
{
    /*
     * YMODEM-G 无逐包重传，一旦接收端发现错误，继续接收只会让目标
     * 文件处于不可恢复状态。因此发送多个 CAN 字节，提升对端在高速
     * 流中识别取消的概率，然后按失败路径释放本地文件。
     */
    QByteArray cancelPacket;
    cancelPacket.append(CAN);
    cancelPacket.append(CAN);
    cancelPacket.append(CAN);
    emit sendData(cancelPacket);
    failTransferWithMessage(message);
}

void YModemTransfer::sendControlByte(char value)
{
    /*
     * 控制字节也通过 sendData 信号交给主窗口统一发送，保持与协议数据包
     * 相同的出口和日志链路。
     */
    QByteArray packet;
    packet.append(value);
    emit sendData(packet);
}

bool YModemTransfer::isTerminalState() const
{
    /*
     * 终止状态下忽略后续串口残留字节，避免完成/失败后又被旧数据推进。
     */
    return m_state == TransferState::Completed ||
           m_state == TransferState::Cancelled ||
           m_state == TransferState::Failed;
}

void YModemTransfer::closeActiveFile()
{
    /*
     * YMODEM 发送和接收共用 m_file：发送时它是源文件，接收时它是目标
     * 文件。统一释放可以让批量切换、完成、失败、取消和析构都走同一条
     * 资源路径，避免 Windows 文件句柄滞留。
     */
    if (!m_file) {
        return;
    }

    m_file->close();
    delete m_file;
    m_file = nullptr;
}

void YModemTransfer::onTimeout()
{
    m_retryCount++;
    m_progress.retryCount = m_retryCount;
    updateProgress();

    if (m_retryCount > m_maxRetries) {
        failTransferWithMessage(tr("传输超时"));
        return;
    }

    LOG_WARN(QString("YMODEM timeout, retry %1").arg(m_retryCount));
    if (m_direction == TransferDirection::Receive) {
        if (m_receiveState == ReceiveState::Starting ||
            m_receiveState == ReceiveState::WaitingHeader) {
            sendControlByte(m_useG ? 'G' : CRC_START);
        } else if (m_receiveState == ReceiveState::ReceivingData) {
            if (m_useG) {
                abortYModemGReceiveWithMessage(tr("YMODEM-G 接收超时，已中止传输"));
                return;
            }
            sendControlByte(NAK);
        } else if (m_receiveState == ReceiveState::WaitingSecondEot) {
            sendControlByte(NAK);
        }
    } else {
        /*
         * 发送模式超时按当前阶段重发最后一个协议包或 EOT。数据包重发
         * 使用 m_lastPacket，不访问源文件，因此与流式读取兼容。
         */
        if (m_sendState == SendState::SendingData && m_useG) {
            /*
             * G 模式数据阶段由 m_streamTimer 推进，不依赖超时重发；若这里
             * 触发，说明接收端长期没有 EOT ACK 或连接异常，按失败处理。
             */
            failTransferWithMessage(tr("YMODEM-G 发送超时"));
            return;
        } else if (m_sendState == SendState::WaitingHeaderAck) {
            emit sendData(buildHeaderPacket(m_progress.fileName, m_progress.fileSize));
        } else if (m_sendState == SendState::WaitingDataAck && !m_lastPacket.isEmpty()) {
            emit sendData(m_lastPacket);
        } else if (m_sendState == SendState::WaitingEOTAck) {
            sendControlByte(EOT);
        }
    }
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
    case TransferProtocol::RawStream:
        return new RawFileTransfer(parent);
    case TransferProtocol::CustomOta:
        return new OtaFileTransfer(parent);
    default:
        return nullptr;
    }
}

} // namespace ComAssistant
