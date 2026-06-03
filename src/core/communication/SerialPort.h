/**
 * @file SerialPort.h
 * @brief 串口通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_SERIALPORT_H
#define COMASSISTANT_SERIALPORT_H

#include "ICommunication.h"
#include "config/AppConfig.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QList>

namespace ComAssistant {

/**
 * @brief 串口通信实现类
 */
class SerialPort : public ICommunication {
    Q_OBJECT

public:
    /**
     * @brief 常用波特率列表
     */
    static constexpr int CommonBaudRates[] = {
        300, 600, 1200, 2400, 4800, 9600, 14400, 19200,
        38400, 57600, 115200, 128000, 230400, 256000,
        460800, 500000, 512000, 600000, 750000, 921600,
        1000000, 1500000, 2000000, 3000000
    };

    /**
     * @brief 构造函数
     * @param config 串口配置
     * @param parent 父对象
     */
    explicit SerialPort(const SerialConfig& config, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~SerialPort() override;

    //=========================================================================
    // ICommunication 接口实现
    //=========================================================================

    bool open() override;
    void close() override;
    bool isOpen() const override;

    qint64 write(const QByteArray& data) override;

    /**
     * @brief 异步等待本次串口发送真正排空。
     * @param bytes 本次文件发送块的字节数，用于按波特率估算线路发送时间。
     * @return 成功接受等待请求返回 true；串口未打开或已有等待任务时返回 false。
     *
     * 该函数不会阻塞 UI 线程。它先等 Qt/系统写缓冲清空，再按当前串口
     * 数据位、校验位、停止位和波特率重新等待完整线路传输时间，最后发出
     * transmitDrained。这样文件传输进度条不会早于 CH340 实际发送完成。
     */
    bool waitForTransmitDrainedAsync(qint64 bytes) override;

    QByteArray readAll() override;
    qint64 bytesAvailable() const override;

    void setBufferSize(int size) override;
    int bufferSize() const override;
    void clearBuffer() override;

    void setReadTimeout(int ms) override;
    int readTimeout() const override;
    void setWriteTimeout(int ms) override;
    int writeTimeout() const override;

    CommType type() const override { return CommType::Serial; }
    QString typeName() const override { return QStringLiteral("Serial"); }
    QString statusString() const override;
    QString lastError() const override { return m_lastError; }

    //=========================================================================
    // 串口特有方法
    //=========================================================================

    /**
     * @brief 设置配置
     */
    void setConfig(const SerialConfig& config);

    /**
     * @brief 获取配置
     */
    SerialConfig config() const;

    /**
     * @brief 设置波特率
     */
    void setBaudRate(int baudRate);

    /**
     * @brief 获取波特率
     */
    int baudRate() const;

    /**
     * @brief 设置DTR信号
     */
    void setDTR(bool enabled);

    /**
     * @brief 设置RTS信号
     */
    void setRTS(bool enabled);

    /**
     * @brief 获取DTR状态
     */
    bool isDTR() const;

    /**
     * @brief 获取RTS状态
     */
    bool isRTS() const;

    /**
     * @brief 获取CTS状态
     */
    bool isCTS() const;

    /**
     * @brief 获取DSR状态
     */
    bool isDSR() const;

    //=========================================================================
    // 静态方法
    //=========================================================================

    /**
     * @brief 获取可用串口列表
     */
    static QList<QSerialPortInfo> availablePorts();

    /**
     * @brief 获取常用波特率列表
     */
    static QList<int> commonBaudRates();

    /**
     * @brief 验证波特率是否有效
     */
    static bool isValidBaudRate(int baudRate);

#ifdef COMASSISTANT_TESTS
    /**
     * @brief 测试专用：模拟已经进入串口发送排空等待状态。
     * @param pendingBytes 当前文件块需要按线路时间继续等待的字节数。
     *
     * 主要流程：设置 drain 状态标志并保存待估算字节数，供单元测试在不打开
     * 真实串口的情况下复现“Qt/系统写缓冲刚清空”的边界。该接口只在测试目标
     * 定义 COMASSISTANT_TESTS 时可见，避免测试用 private/public 宏改写 MSVC
     * 成员访问属性后产生链接符号不一致。
     */
    void prepareTransmitDrainForTest(qint64 pendingBytes);

    /**
     * @brief 测试专用：触发线路发送等待阶段。
     *
     * 主要流程：直接调用内部 startTransmitLineDelay()，用于验证写缓冲清空后
     * 是否重新等待完整线路时间。返回值通过定时器探针读取。
     */
    void startTransmitLineDelayForTest();

    /**
     * @brief 测试专用：估算指定字节数在线路上发送完成所需时间。
     * @param bytes 需要估算的字节数。
     * @return 按当前波特率、数据位、校验位和停止位估算出的毫秒数。
     */
    int estimateTransmitTimeMsForTest(qint64 bytes) const;

    /**
     * @brief 测试专用：判断线路等待定时器是否正在运行。
     * @return true 表示已经进入按线路时间等待发送完成的阶段。
     */
    bool transmitLineTimerActiveForTest() const;

    /**
     * @brief 测试专用：获取线路等待定时器当前间隔。
     * @return 定时器间隔毫秒数，用于验证没有扣减前一阶段等待耗时。
     */
    int transmitLineTimerIntervalForTest() const;
#endif

signals:
    /**
     * @brief 波特率变更信号
     */
    void baudRateChanged(int newBaudRate);

    /**
     * @brief 控制信号变化
     */
    void pinoutSignalsChanged();

private slots:
    void onReadyRead();
    void onBytesWritten(qint64 bytes);
    void onError(QSerialPort::SerialPortError error);
    void onTransmitDrainTimeout();
    void onTransmitLineDelayElapsed();

private:
    void applyConfig();
    void startTransmitLineDelay();
    void completeTransmitDrain(bool success, const QString& errorMessage);
    double configuredBitsPerByte() const;
    int estimateTransmitTimeMs(qint64 bytes) const;
    static QSerialPort::DataBits toQtDataBits(DataBits bits);
    static QSerialPort::StopBits toQtStopBits(StopBits bits);
    static QSerialPort::Parity toQtParity(Parity parity);
    static QSerialPort::FlowControl toQtFlowControl(FlowControl flow);

    QSerialPort* m_port = nullptr;
    SerialConfig m_config;
    bool m_dtr = false;
    bool m_rts = false;
    QTimer* m_transmitDrainWatchdog = nullptr; ///< 等待 Qt/系统发送缓冲清空的超时保护。
    QTimer* m_transmitLineTimer = nullptr;     ///< 按波特率等待线路上最后字节发完的定时器。
    qint64 m_pendingTransmitDrainBytes = 0;    ///< 当前等待发空的数据字节数。
    bool m_waitingTransmitDrain = false;       ///< 是否存在文件传输发空等待任务。
};

} // namespace ComAssistant

#endif // COMASSISTANT_SERIALPORT_H
