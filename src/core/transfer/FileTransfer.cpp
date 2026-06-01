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
#include <QElapsedTimer>
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

/**
 * @brief 构造固定 4 字节 magic
 * @param magic 用户输入的 magic 字符串。
 * @return 长度固定为 4 字节的 magic，过长截断，过短补零。
 */
QByteArray normalizedMagic(const QString& magic)
{
    QByteArray result = magic.toLatin1();
    if (result.size() > 4) {
        result = result.left(4);
    }
    while (result.size() < 4) {
        result.append('\0');
    }
    return result;
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
     * 裸流发送没有握手阶段，启动成功后马上进入 Transferring。
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
    setState(TransferState::Transferring);
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
    if (m_state != TransferState::Transferring || m_paused) {
        return;
    }

    m_paused = true;
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    updateProgress();
}

void RawFileTransfer::resume()
{
    if (m_state != TransferState::Transferring || !m_paused) {
        return;
    }

    m_paused = false;
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
    if (!m_file || m_state != TransferState::Transferring || m_paused || m_waitingLocalSendResult) {
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
    if (!m_file || m_state != TransferState::Transferring) {
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
    setState(TransferState::Error);
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
    if (m_options.magic.isEmpty()) {
        m_options.magic = "OTA1";
    }
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

    setState(TransferState::Transferring);
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
    if (m_state != TransferState::Transferring || m_paused) {
        return;
    }

    m_paused = true;
    if (m_sendTimer) {
        m_sendTimer->stop();
    }
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    updateProgress();
}

void OtaFileTransfer::resume()
{
    if (m_state != TransferState::Transferring || !m_paused) {
        return;
    }

    m_paused = false;

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

QByteArray OtaFileTransfer::buildHeaderPacketForTest(const QString& magic,
                                                     const QString& fileName,
                                                     qint64 fileSize,
                                                     quint32 crc32,
                                                     int blockSize)
{
    QByteArray packet;
    packet.append(normalizedMagic(magic));
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
    if (!m_file || m_state != TransferState::Transferring || m_paused || m_waitingLocalSendResult) {
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
    if (!m_file || m_state != TransferState::Transferring || m_paused || m_waitingLocalSendResult) {
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
    if (m_state != TransferState::Transferring || m_paused || m_waitingLocalSendResult) {
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
    if (m_sendTimer && m_state == TransferState::Transferring && !m_paused) {
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
    setState(TransferState::Error);
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
    case TransferProtocol::RawStream:
        return new RawFileTransfer(parent);
    case TransferProtocol::CustomOta:
        return new OtaFileTransfer(parent);
    default:
        return nullptr;
    }
}

} // namespace ComAssistant
