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
    YModemG         ///< YMODEM-G (无ACK流式传输)
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
