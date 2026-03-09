/**
 * @file IAPUpgrader.h
 * @brief IAP固件升级器
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_IAPUPGRADER_H
#define COMASSISTANT_IAPUPGRADER_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <QFile>
#include <functional>

namespace ComAssistant {

/**
 * @brief IAP升级协议类型
 */
enum class IAPProtocol {
    Custom,         ///< 自定义协议
    STM32Bootloader,///< STM32 系统Bootloader
    XMODEM,         ///< 基于XMODEM
    YMODEM          ///< 基于YMODEM
};

/**
 * @brief 固件类型
 */
enum class FirmwareType {
    Binary,         ///< 二进制文件 (.bin)
    IntelHex,       ///< Intel HEX格式 (.hex)
    SRecord         ///< Motorola S-Record (.s19)
};

/**
 * @brief IAP升级进度
 */
struct IAPProgress {
    QString fileName;
    qint64 fileSize = 0;
    qint64 bytesWritten = 0;
    int currentBlock = 0;
    int totalBlocks = 0;
    QString status;
    bool hasError = false;
    QString errorMessage;

    double percentage() const {
        return fileSize > 0 ? (bytesWritten * 100.0 / fileSize) : 0;
    }
};

/**
 * @brief HEX记录结构
 */
struct HexRecord {
    quint8 byteCount = 0;
    quint16 address = 0;
    quint8 recordType = 0;
    QByteArray data;
    quint8 checksum = 0;
    bool valid = false;

    // 记录类型
    static constexpr quint8 DATA = 0x00;
    static constexpr quint8 END_OF_FILE = 0x01;
    static constexpr quint8 EXT_SEGMENT_ADDR = 0x02;
    static constexpr quint8 START_SEGMENT_ADDR = 0x03;
    static constexpr quint8 EXT_LINEAR_ADDR = 0x04;
    static constexpr quint8 START_LINEAR_ADDR = 0x05;
};

/**
 * @brief IAP固件升级器
 */
class IAPUpgrader : public QObject {
    Q_OBJECT

public:
    explicit IAPUpgrader(QObject* parent = nullptr);
    ~IAPUpgrader() override;

    /**
     * @brief 设置协议
     */
    void setProtocol(IAPProtocol protocol) { m_protocol = protocol; }

    /**
     * @brief 设置自定义协议参数
     */
    void setCustomProtocol(const QByteArray& header, const QByteArray& ack,
                          int blockSize = 256, int timeout = 1000);

    /**
     * @brief 加载固件文件
     */
    bool loadFirmware(const QString& filePath);

    /**
     * @brief 获取固件数据
     */
    QByteArray firmwareData() const { return m_firmwareData; }

    /**
     * @brief 获取起始地址
     */
    quint32 startAddress() const { return m_startAddress; }

    /**
     * @brief 开始升级
     */
    bool startUpgrade();

    /**
     * @brief 取消升级
     */
    void cancel();

    /**
     * @brief 处理接收数据
     */
    void processReceivedData(const QByteArray& data);

    /**
     * @brief 获取进度
     */
    IAPProgress progress() const { return m_progress; }

    /**
     * @brief 解析HEX文件
     */
    static bool parseHexFile(const QString& filePath, QByteArray& data,
                            quint32& startAddr, QString& errorMsg);

    /**
     * @brief 解析单行HEX记录
     */
    static HexRecord parseHexLine(const QString& line);

signals:
    void sendData(const QByteArray& data);
    void progressUpdated(const IAPProgress& progress);
    void upgradeCompleted(bool success, const QString& message);

private slots:
    void onTimeout();

private:
    void sendNextBlock();
    void sendEraseCommand();
    void sendWriteCommand(quint32 address, const QByteArray& data);
    void sendVerifyCommand();
    void sendResetCommand();
    QByteArray buildPacket(quint8 cmd, const QByteArray& data);

private:
    IAPProtocol m_protocol = IAPProtocol::Custom;

    // 自定义协议参数
    QByteArray m_packetHeader;
    QByteArray m_expectedAck;
    int m_blockSize = 256;
    int m_timeout = 1000;

    // 固件数据
    QByteArray m_firmwareData;
    quint32 m_startAddress = 0x08000000;  // STM32默认Flash起始地址
    FirmwareType m_firmwareType = FirmwareType::Binary;

    // 升级状态
    enum class State {
        Idle,
        Erasing,
        Writing,
        Verifying,
        Resetting,
        Completed,
        Error
    };
    State m_state = State::Idle;

    int m_currentBlock = 0;
    int m_retryCount = 0;
    int m_maxRetries = 3;
    QTimer* m_timeoutTimer = nullptr;
    IAPProgress m_progress;
    QByteArray m_receiveBuffer;
};

/**
 * @brief 自动波特率检测器
 */
class BaudRateDetector : public QObject {
    Q_OBJECT

public:
    explicit BaudRateDetector(QObject* parent = nullptr);
    ~BaudRateDetector() override;

    /**
     * @brief 开始检测
     */
    void startDetection(const QString& portName);

    /**
     * @brief 取消检测
     */
    void cancel();

    /**
     * @brief 设置测试波特率列表
     */
    void setBaudRates(const QList<int>& rates);

    /**
     * @brief 设置测试字符
     */
    void setTestPattern(const QByteArray& pattern);

    /**
     * @brief 处理接收数据
     */
    void processReceivedData(const QByteArray& data);

signals:
    /**
     * @brief 请求打开串口
     */
    void requestOpenPort(const QString& portName, int baudRate);

    /**
     * @brief 请求关闭串口
     */
    void requestClosePort();

    /**
     * @brief 请求发送数据
     */
    void sendData(const QByteArray& data);

    /**
     * @brief 检测完成
     */
    void detectionCompleted(int baudRate, bool success);

    /**
     * @brief 进度更新
     */
    void progressUpdated(int current, int total, int baudRate);

private slots:
    void testNextBaudRate();
    void onTimeout();

private:
    bool analyzeResponse(const QByteArray& response);

private:
    QString m_portName;
    QList<int> m_baudRates;
    int m_currentIndex = 0;
    QByteArray m_testPattern;
    QByteArray m_receiveBuffer;
    QTimer* m_timer = nullptr;
    int m_timeout = 500;  // 每个波特率测试超时
    bool m_cancelled = false;

    // 默认测试波特率
    static const QList<int> DEFAULT_BAUD_RATES;
};

} // namespace ComAssistant

#endif // COMASSISTANT_IAPUPGRADER_H
