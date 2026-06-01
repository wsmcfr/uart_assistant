/**
 * @file FileTransfer.h
 * @brief 文件传输协议（XMODEM/YMODEM/ZMODEM）
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_FILETRANSFER_H
#define COMASSISTANT_FILETRANSFER_H

#include <QObject>
#include <QByteArray>
#include <QFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace ComAssistant {

/**
 * @brief 文件传输协议类型
 */
enum class TransferProtocol {
    XModem,         ///< XMODEM (128字节块)
    XModemCRC,      ///< XMODEM-CRC (128字节块，CRC校验)
    XModem1K,       ///< XMODEM-1K (1024字节块)
    YModem,         ///< YMODEM (支持批量传输)
    YModemG,        ///< YMODEM-G (无ACK流式传输)
    RawStream,      ///< 裸数据流分块发送
    CustomOta       ///< 自定义 OTA 分块协议
};

/**
 * @brief 传输方向
 */
enum class TransferDirection {
    Send,           ///< 发送文件
    Receive         ///< 接收文件
};

/**
 * @brief 传输状态
 */
enum class TransferState {
    Idle,           ///< 空闲
    WaitingStart,   ///< 等待开始
    Transferring,   ///< 传输中
    Completing,     ///< 完成中
    Completed,      ///< 已完成
    Cancelled,      ///< 已取消
    Error           ///< 错误
};

/**
 * @brief 传输进度信息
 */
struct TransferProgress {
    QString fileName;
    qint64 fileSize = 0;
    qint64 bytesTransferred = 0;
    int currentPacket = 0;
    int totalPackets = 0;
    int retryCount = 0;
    double speed = 0;  // bytes/s
    int elapsedMs = 0;
    TransferState state = TransferState::Idle;
    QString errorMessage;

    double percentage() const {
        return fileSize > 0 ? (bytesTransferred * 100.0 / fileSize) : 0;
    }
};

/**
 * @brief 文件传输基类
 */
class FileTransfer : public QObject {
    Q_OBJECT

public:
    explicit FileTransfer(QObject* parent = nullptr);
    virtual ~FileTransfer() = default;

    /**
     * @brief 开始发送文件
     */
    virtual bool startSend(const QString& filePath) = 0;

    /**
     * @brief 开始接收文件
     */
    virtual bool startReceive(const QString& savePath) = 0;

    /**
     * @brief 取消传输
     */
    virtual void cancel() = 0;

    /**
     * @brief 处理接收的数据
     */
    virtual void processReceivedData(const QByteArray& data) = 0;

    /**
     * @brief 获取当前状态
     */
    TransferState state() const { return m_state; }

    /**
     * @brief 获取传输进度
     */
    TransferProgress progress() const { return m_progress; }

    /**
     * @brief 获取协议类型
     */
    virtual TransferProtocol protocol() const = 0;

    /**
     * @brief 设置超时时间
     */
    void setTimeout(int ms) { m_timeoutMs = ms; }

    /**
     * @brief 设置最大重试次数
     */
    void setMaxRetries(int count) { m_maxRetries = count; }

signals:
    /**
     * @brief 请求发送数据
     */
    void sendData(const QByteArray& data);

    /**
     * @brief 进度更新
     */
    void progressUpdated(const TransferProgress& progress);

    /**
     * @brief 传输完成
     */
    void transferCompleted(bool success, const QString& message);

    /**
     * @brief 状态改变
     */
    void stateChanged(TransferState state);

protected:
    void setState(TransferState state);
    void updateProgress();

protected:
    TransferState m_state = TransferState::Idle;
    TransferProgress m_progress;
    QTimer* m_timeoutTimer = nullptr;
    int m_timeoutMs = 10000;
    int m_maxRetries = 10;

    // 协议常量
    static constexpr char SOH = 0x01;   // 128字节块开始
    static constexpr char STX = 0x02;   // 1024字节块开始
    static constexpr char EOT = 0x04;   // 传输结束
    static constexpr char ACK = 0x06;   // 确认
    static constexpr char NAK = 0x15;   // 否认
    static constexpr char CAN = 0x18;   // 取消
    static constexpr char CPMEOF = 0x1A; // 填充字符
    static constexpr char CRC_START = 'C';  // CRC模式请求
};

/**
 * @brief 裸流分块发送参数
 *
 * 裸流模式不增加协议头，适合下位机只按顺序接收字节流的场景。
 * blockSize 控制每次写入串口的最大字节数，intervalMs 控制相邻块之间
 * 的等待时间，用于给 MCU 留出 Flash 写入或缓冲区处理时间。
 */
struct RawTransferOptions {
    int blockSize = 256;     ///< 每包发送字节数
    int intervalMs = 10;     ///< 包间隔，单位毫秒
};

/**
 * @brief 自定义 OTA 发送参数
 *
 * OTA 模式会发送文件头、数据块和结束包。开启 waitAck 后，每个包都要
 * 等待下位机返回 ackToken 才继续，适合需要可靠写 Flash 的 Bootloader。
 */
struct OtaTransferOptions {
    QString magic = "OTA1";      ///< 文件头 magic，默认取前 4 字节
    int blockSize = 256;         ///< 每个 OTA 数据块最大 payload 字节数
    int intervalMs = 10;         ///< 未启用 ACK 或 ACK 后发送下一包的间隔
    bool waitAck = false;        ///< 是否等待下位机 ACK
    QByteArray ackToken = "ACK"; ///< 下位机确认文本，默认 ASCII ACK
    int timeoutMs = 1000;        ///< ACK 超时时间
    int maxRetries = 3;          ///< ACK 超时后的最大重试次数
};

/**
 * @brief 裸数据流文件发送器
 *
 * 该类只负责把文件按块读出并通过 sendData 信号交给主窗口统一发送。
 * 它不关心串口是否真实写入成功，串口状态由 MainWindow::onSendData()
 * 统一处理，从而保持与普通发送、快捷发送相同的通信出口。
 */
class RawFileTransfer : public FileTransfer {
    Q_OBJECT

public:
    explicit RawFileTransfer(QObject* parent = nullptr);
    ~RawFileTransfer() override;

    /**
     * @brief 设置裸流发送参数
     * @param options 用户在界面中选择的块大小和发送间隔。
     */
    void setOptions(const RawTransferOptions& options);

    /**
     * @brief 开始按块发送文件
     * @param filePath 待发送文件路径。
     * @return 成功启动返回 true，文件不可读或状态不允许时返回 false。
     */
    bool startSend(const QString& filePath) override;

    /**
     * @brief 裸流接收暂不在第一版实现
     * @param savePath 预留参数，当前不会使用。
     * @return 始终返回 false，并通过完成信号给出说明。
     */
    bool startReceive(const QString& savePath) override;

    /**
     * @brief 取消当前裸流发送并释放文件句柄
     */
    void cancel() override;

    /**
     * @brief 裸流模式不处理接收数据
     * @param data 下位机返回的数据，当前仅保留接口一致性。
     */
    void processReceivedData(const QByteArray& data) override;

    /**
     * @brief 获取协议类型
     * @return TransferProtocol::RawStream。
     */
    TransferProtocol protocol() const override { return TransferProtocol::RawStream; }

    /**
     * @brief 暂停定时发送
     */
    void pause();

    /**
     * @brief 从暂停位置继续发送
     */
    void resume();

    /**
     * @brief 判断当前是否处于暂停状态
     * @return 暂停中返回 true。
     */
    bool isPaused() const { return m_paused; }

    /**
     * @brief 测试辅助：按块切分内存数据
     * @param data 待切分数据。
     * @param blockSize 每块最大字节数。
     * @return 切分后的数据块列表。
     */
    static QVector<QByteArray> splitForTest(const QByteArray& data, int blockSize);

private:
    void sendNextChunk();
    void finishSuccessfully();
    void failWithMessage(const QString& message);
    void refreshSpeed();

private:
    RawTransferOptions m_options;
    QFile* m_file = nullptr;
    QTimer* m_sendTimer = nullptr;
    QElapsedTimer m_elapsedTimer;
    bool m_paused = false;
};

/**
 * @brief 自定义 OTA 文件发送器
 *
 * 该类实现一个通用、可配置的 OTA 包格式：文件头携带总大小和 CRC32，
 * 数据包携带块序号、长度、payload 和 CRC16，结束包携带 DONE 与整文件
 * CRC32。它可选择等待下位机 ACK，以适配简单 Bootloader。
 */
class OtaFileTransfer : public FileTransfer {
    Q_OBJECT

public:
    explicit OtaFileTransfer(QObject* parent = nullptr);
    ~OtaFileTransfer() override;

    /**
     * @brief 设置 OTA 发送参数
     * @param options magic、块大小、间隔、ACK、超时和重试配置。
     */
    void setOptions(const OtaTransferOptions& options);

    /**
     * @brief 开始 OTA 文件发送
     * @param filePath 待发送文件路径。
     * @return 成功启动返回 true，文件不可读或文件过大时返回 false。
     */
    bool startSend(const QString& filePath) override;

    /**
     * @brief OTA 接收暂不在第一版实现
     * @param savePath 预留参数，当前不会使用。
     * @return 始终返回 false，并通过完成信号给出说明。
     */
    bool startReceive(const QString& savePath) override;

    /**
     * @brief 取消当前 OTA 发送并释放资源
     */
    void cancel() override;

    /**
     * @brief 处理下位机返回数据，ACK 模式下用于推进状态机
     * @param data 串口接收到的数据。
     */
    void processReceivedData(const QByteArray& data) override;

    /**
     * @brief 获取协议类型
     * @return TransferProtocol::CustomOta。
     */
    TransferProtocol protocol() const override { return TransferProtocol::CustomOta; }

    /**
     * @brief 暂停 OTA 发送
     */
    void pause();

    /**
     * @brief 从暂停位置继续 OTA 发送
     */
    void resume();

    /**
     * @brief 判断当前是否处于暂停状态
     * @return 暂停中返回 true。
     */
    bool isPaused() const { return m_paused; }

    /**
     * @brief 测试辅助：构造 OTA 文件头包
     * @param magic 4 字节 magic，不足 4 字节会补 0，超过会截断。
     * @param fileName 文件名。
     * @param fileSize 文件大小，第一版协议使用 32 位长度字段。
     * @param crc32 整文件 CRC32。
     * @param blockSize 数据块大小。
     * @return 可直接发送给下位机的文件头包。
     */
    static QByteArray buildHeaderPacketForTest(const QString& magic,
                                               const QString& fileName,
                                               qint64 fileSize,
                                               quint32 crc32,
                                               int blockSize);

    /**
     * @brief 测试辅助：构造 OTA 数据包
     * @param blockIndex 从 0 开始的数据块序号。
     * @param payload 当前数据块内容。
     * @return 数据包，尾部包含 CRC16-XMODEM。
     */
    static QByteArray buildDataPacketForTest(quint32 blockIndex, const QByteArray& payload);

    /**
     * @brief 测试辅助：构造 OTA 结束包
     * @param crc32 整文件 CRC32。
     * @return DONE + 小端 CRC32。
     */
    static QByteArray buildEndPacketForTest(quint32 crc32);

private:
    enum class OtaStage {
        Idle,
        Header,
        Data,
        End
    };

    void sendHeaderPacket();
    void sendNextDataPacket();
    void sendEndPacket();
    void scheduleNextDataPacket();
    void waitForAck(OtaStage stage);
    void handleAck(OtaStage stage);
    void onAckTimeout();
    void finishSuccessfully();
    void failWithMessage(const QString& message);
    void refreshSpeed();

private:
    OtaTransferOptions m_options;
    QFile* m_file = nullptr;
    QTimer* m_sendTimer = nullptr;
    QElapsedTimer m_elapsedTimer;
    QByteArray m_receiveBuffer;
    QByteArray m_pendingPacket;
    quint32 m_fileCrc32 = 0;
    quint32 m_currentBlock = 0;
    int m_retryCount = 0;
    bool m_paused = false;
    OtaStage m_waitingAckFor = OtaStage::Idle;
};

/**
 * @brief XMODEM文件传输
 */
class XModemTransfer : public FileTransfer {
    Q_OBJECT

public:
    explicit XModemTransfer(bool useCRC = true, bool use1K = false, QObject* parent = nullptr);
    ~XModemTransfer() override;

    bool startSend(const QString& filePath) override;
    bool startReceive(const QString& savePath) override;
    void cancel() override;
    void processReceivedData(const QByteArray& data) override;
    TransferProtocol protocol() const override;

private slots:
    void onTimeout();

private:
    void sendNextPacket();
    void sendPacket(int packetNum, const QByteArray& data);
    QByteArray buildPacket(int packetNum, const QByteArray& data);
    bool verifyPacket(const QByteArray& packet, int expectedNum);
    quint16 calculateCRC16(const QByteArray& data);
    quint8 calculateChecksum(const QByteArray& data);
    void processReceiveData();  // 处理接收模式下的数据

private:
    bool m_useCRC;
    bool m_use1K;
    int m_blockSize;

    QFile* m_file = nullptr;
    QByteArray m_fileData;
    int m_packetNumber = 1;
    int m_retryCount = 0;
    QByteArray m_receiveBuffer;
    QByteArray m_lastPacket;

    enum class SendState {
        WaitingC,       // 等待C或NAK开始
        SendingData,    // 发送数据包
        WaitingAck,     // 等待ACK
        SendingEOT,     // 发送EOT
        Done
    };

    enum class ReceiveState {
        Starting,       // 发送C/NAK
        ReceivingData,  // 接收数据
        Done
    };

    SendState m_sendState = SendState::WaitingC;
    ReceiveState m_receiveState = ReceiveState::Starting;
    TransferDirection m_direction = TransferDirection::Send;
};

/**
 * @brief YMODEM文件传输
 */
class YModemTransfer : public FileTransfer {
    Q_OBJECT

public:
    explicit YModemTransfer(bool useG = false, QObject* parent = nullptr);
    ~YModemTransfer() override;

    bool startSend(const QString& filePath) override;
    bool startReceive(const QString& savePath) override;
    void cancel() override;
    void processReceivedData(const QByteArray& data) override;
    TransferProtocol protocol() const override;

    /**
     * @brief 批量发送多个文件
     */
    bool startSendBatch(const QStringList& filePaths);

private slots:
    void onTimeout();

private:
    void sendFileHeader();
    void sendNextPacket();
    void sendEndOfBatch();
    QByteArray buildHeaderPacket(const QString& fileName, qint64 fileSize);
    QByteArray buildDataPacket(int packetNum, const QByteArray& data);
    quint16 calculateCRC16(const QByteArray& data);

private:
    bool m_useG;
    QStringList m_filesToSend;
    int m_currentFileIndex = 0;

    QFile* m_file = nullptr;
    QByteArray m_fileData;
    int m_packetNumber = 0;
    int m_retryCount = 0;
    QByteArray m_receiveBuffer;
    QString m_savePath;

    enum class SendState {
        WaitingC,
        SendingHeader,
        WaitingHeaderAck,
        SendingData,
        WaitingDataAck,
        SendingEOT,
        WaitingEOTAck,
        SendingEndHeader,
        Done
    };

    enum class ReceiveState {
        Starting,
        WaitingHeader,
        ReceivingData,
        Done
    };

    SendState m_sendState = SendState::WaitingC;
    ReceiveState m_receiveState = ReceiveState::Starting;
    TransferDirection m_direction = TransferDirection::Send;
};

/**
 * @brief 文件传输工厂
 */
class FileTransferFactory {
public:
    static FileTransfer* create(TransferProtocol protocol, QObject* parent = nullptr);
};

} // namespace ComAssistant

#endif // COMASSISTANT_FILETRANSFER_H
