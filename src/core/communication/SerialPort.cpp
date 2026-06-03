/**
 * @file SerialPort.cpp
 * @brief 串口通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "SerialPort.h"
#include "utils/Logger.h"

namespace ComAssistant {

SerialPort::SerialPort(const SerialConfig& config, QObject* parent)
    : ICommunication(parent)
    , m_config(config)
{
    m_port = new QSerialPort(this);
    m_transmitDrainWatchdog = new QTimer(this);
    m_transmitDrainWatchdog->setSingleShot(true);
    m_transmitLineTimer = new QTimer(this);
    m_transmitLineTimer->setSingleShot(true);

    connect(m_port, &QSerialPort::readyRead, this, &SerialPort::onReadyRead);
    connect(m_port, &QSerialPort::bytesWritten, this, &SerialPort::onBytesWritten);
    connect(m_port, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(onError(QSerialPort::SerialPortError)));
    connect(m_transmitDrainWatchdog, &QTimer::timeout,
            this, &SerialPort::onTransmitDrainTimeout);
    connect(m_transmitLineTimer, &QTimer::timeout,
            this, &SerialPort::onTransmitLineDelayElapsed);
}

SerialPort::~SerialPort()
{
    if (isOpen()) {
        close();
    }
}

bool SerialPort::open()
{
    if (isOpen()) {
        return true;
    }

    if (m_config.portName.isEmpty()) {
        m_lastError = tr("Port name is empty");
        emit errorOccurred(m_lastError);
        return false;
    }

    m_port->setPortName(m_config.portName);
    applyConfig();

    // 检查端口是否存在
    bool portExists = false;
    for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
        if (info.portName() == m_config.portName ||
            info.portName() == "COM" + m_config.portName ||
            info.systemLocation().contains(m_config.portName)) {
            portExists = true;
            break;
        }
    }
    if (!portExists) {
        m_lastError = tr("Port not found: %1").arg(m_config.portName);
        LOG_ERROR(m_lastError);
        emit errorOccurred(m_lastError);
        return false;
    }

    // Qt5.12.9 兼容：抑制 QSerialPort 的内部警告
    // QIODevice::open: QSerialPort is not a sequential device 是无害的 Qt 内部警告
    if (!m_port->open(QIODevice::ReadWrite)) {
        m_lastError = m_port->errorString();
        // 过滤掉 Qt 内部的无害警告
        if (m_lastError.contains("sequential device")) {
            m_lastError = tr("Cannot open port %1. Port may be in use or access denied.")
                              .arg(m_config.portName);
        }
        LOG_ERROR(QString("Failed to open serial port %1: %2")
                      .arg(m_config.portName, m_lastError));
        emit errorOccurred(m_lastError);
        return false;
    }

    // 设置DTR/RTS
    m_port->setDataTerminalReady(m_dtr);
    m_port->setRequestToSend(m_rts);

    LOG_INFO(QString("Serial port %1 opened at %2 bps")
                 .arg(m_config.portName)
                 .arg(m_config.baudRate));

    emit connectionStatusChanged(true);
    return true;
}

void SerialPort::close()
{
    /*
     * 如果文件传输正在等待串口发空，关闭串口意味着该等待必然失败。
     * 先发失败信号再关闭底层端口，避免 UI 继续停在“传输中”。
     */
    if (m_waitingTransmitDrain) {
        completeTransmitDrain(false, tr("串口已关闭，发送未完成"));
    }

    if (m_port->isOpen()) {
        m_port->close();
        LOG_INFO(QString("Serial port %1 closed").arg(m_config.portName));
        emit connectionStatusChanged(false);
    }
}

bool SerialPort::isOpen() const
{
    return m_port->isOpen();
}

qint64 SerialPort::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = tr("Port is not open");
        return -1;
    }

    qint64 written = m_port->write(data);
    if (written > 0) {
        m_port->flush();
        emit dataSent(data.left(written));
    } else if (written < 0) {
        m_lastError = m_port->errorString();
        emit errorOccurred(m_lastError);
    }

    return written;
}

bool SerialPort::waitForTransmitDrainedAsync(qint64 bytes)
{
    /*
     * 文件传输每块只允许一个 drain 等待。若上层在上一块还没发完时又请求
     * 下一块，说明状态机已经失去背压，必须拒绝以避免进度提前推进。
     */
    if (!isOpen()) {
        m_lastError = tr("Port is not open");
        return false;
    }
    if (m_waitingTransmitDrain) {
        m_lastError = tr("串口发送缓冲仍在等待发空");
        return false;
    }

    m_waitingTransmitDrain = true;
    m_pendingTransmitDrainBytes = qMax<qint64>(0, bytes);

    /*
     * flush() 只尽量把 Qt 内部缓冲推给系统驱动，不保证硬件已经发完。
     * 因此这里用 bytesToWrite() 观察 Qt/系统写缓冲，再额外等待线路时间。
     */
    m_port->flush();
    /*
     * 第一阶段 watchdog 只保护 Qt/系统写缓冲迟迟不清空的情况。写缓冲清空后
     * 会在 startTransmitLineDelay() 中重启 watchdog，单独保护线路等待阶段。
     */
    const int watchdogMs = qMax(m_config.writeTimeout, 1000);
    m_transmitDrainWatchdog->start(watchdogMs);

    if (m_port->bytesToWrite() <= 0) {
        startTransmitLineDelay();
    }
    return true;
}

QByteArray SerialPort::readAll()
{
    return m_port->readAll();
}

qint64 SerialPort::bytesAvailable() const
{
    return m_port->bytesAvailable();
}

void SerialPort::setBufferSize(int size)
{
    m_bufferSize = size;
    m_port->setReadBufferSize(size);
}

int SerialPort::bufferSize() const
{
    return m_bufferSize;
}

void SerialPort::clearBuffer()
{
    m_port->clear();
}

void SerialPort::setReadTimeout(int ms)
{
    m_readTimeout = ms;
}

int SerialPort::readTimeout() const
{
    return m_readTimeout;
}

void SerialPort::setWriteTimeout(int ms)
{
    m_writeTimeout = ms;
}

int SerialPort::writeTimeout() const
{
    return m_writeTimeout;
}

QString SerialPort::statusString() const
{
    if (!isOpen()) {
        return tr("Disconnected");
    }

    return QString("%1 - %2,%3,%4,%5")
        .arg(m_config.portName)
        .arg(m_config.baudRate)
        .arg(static_cast<int>(m_config.dataBits))
        .arg(m_config.parity == Parity::None ? "N" :
             m_config.parity == Parity::Odd ? "O" :
             m_config.parity == Parity::Even ? "E" : "?")
        .arg(m_config.stopBits == StopBits::One ? "1" :
             m_config.stopBits == StopBits::OnePointFive ? "1.5" : "2");
}

void SerialPort::setConfig(const SerialConfig& config)
{
    m_config = config;
    if (isOpen()) {
        applyConfig();
    }
}

SerialConfig SerialPort::config() const
{
    return m_config;
}

void SerialPort::setBaudRate(int baudRate)
{
    if (m_config.baudRate != baudRate) {
        m_config.baudRate = baudRate;
        if (isOpen()) {
            m_port->setBaudRate(baudRate);
        }
        emit baudRateChanged(baudRate);
    }
}

int SerialPort::baudRate() const
{
    return m_config.baudRate;
}

void SerialPort::setDTR(bool enabled)
{
    m_dtr = enabled;
    if (isOpen()) {
        m_port->setDataTerminalReady(enabled);
        emit pinoutSignalsChanged();
    }
}

void SerialPort::setRTS(bool enabled)
{
    m_rts = enabled;
    if (isOpen()) {
        m_port->setRequestToSend(enabled);
        emit pinoutSignalsChanged();
    }
}

bool SerialPort::isDTR() const
{
    return m_dtr;
}

bool SerialPort::isRTS() const
{
    return m_rts;
}

bool SerialPort::isCTS() const
{
    if (isOpen()) {
        return m_port->pinoutSignals() & QSerialPort::ClearToSendSignal;
    }
    return false;
}

bool SerialPort::isDSR() const
{
    if (isOpen()) {
        return m_port->pinoutSignals() & QSerialPort::DataSetReadySignal;
    }
    return false;
}

QList<QSerialPortInfo> SerialPort::availablePorts()
{
    return QSerialPortInfo::availablePorts();
}

QList<int> SerialPort::commonBaudRates()
{
    QList<int> rates;
    for (int rate : CommonBaudRates) {
        rates.append(rate);
    }
    return rates;
}

bool SerialPort::isValidBaudRate(int baudRate)
{
    return baudRate > 0 && baudRate <= 10000000;
}

#ifdef COMASSISTANT_TESTS
void SerialPort::prepareTransmitDrainForTest(qint64 pendingBytes)
{
    /*
     * 测试只需要复现“文件发送 drain 已经开始、Qt 写缓冲刚清空”的内部状态。
     * 不打开真实串口可以让回归测试稳定运行在 CI 中，也避免占用用户机器 COM 口。
     */
    m_waitingTransmitDrain = true;
    m_pendingTransmitDrainBytes = qMax<qint64>(0, pendingBytes);
}

void SerialPort::startTransmitLineDelayForTest()
{
    /*
     * 通过测试专用公开包装调用私有状态机，避免使用 #define private public。
     * MSVC 会把成员访问权限编码进符号名，强行改写 private 会导致测试链接到
     * public 版符号，而实现文件生成 private 版符号，最终出现 LNK2019。
     */
    startTransmitLineDelay();
}

int SerialPort::estimateTransmitTimeMsForTest(qint64 bytes) const
{
    /*
     * 暴露纯估算结果给测试断言，确保测试不需要直接链接私有辅助函数。
     */
    return estimateTransmitTimeMs(bytes);
}

bool SerialPort::transmitLineTimerActiveForTest() const
{
    /*
     * 定时器对象仍由 SerialPort 管理，测试只读取状态，不接管其生命周期。
     */
    return m_transmitLineTimer->isActive();
}

int SerialPort::transmitLineTimerIntervalForTest() const
{
    /*
     * 返回当前 interval，用于确认线路等待没有扣减前一阶段等待写缓冲的耗时。
     */
    return m_transmitLineTimer->interval();
}
#endif

void SerialPort::onReadyRead()
{
    QByteArray data = m_port->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void SerialPort::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes)
    if (!m_waitingTransmitDrain) {
        return;
    }

    if (m_port->bytesToWrite() <= 0) {
        startTransmitLineDelay();
    }
}

void SerialPort::onError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    m_lastError = m_port->errorString();
    LOG_ERROR(QString("Serial port error: %1").arg(m_lastError));

    if (m_waitingTransmitDrain) {
        completeTransmitDrain(false, m_lastError);
    }

    if (error == QSerialPort::ResourceError) {
        // 设备断开
        close();
    }

    emit errorOccurred(m_lastError);
}

void SerialPort::onTransmitDrainTimeout()
{
    if (!m_waitingTransmitDrain) {
        return;
    }

    completeTransmitDrain(false, tr("串口发送缓冲等待发空超时"));
}

void SerialPort::onTransmitLineDelayElapsed()
{
    if (!m_waitingTransmitDrain) {
        return;
    }

    completeTransmitDrain(true, QString());
}

void SerialPort::applyConfig()
{
    m_port->setBaudRate(m_config.baudRate);
    m_port->setDataBits(toQtDataBits(m_config.dataBits));
    m_port->setStopBits(toQtStopBits(m_config.stopBits));
    m_port->setParity(toQtParity(m_config.parity));
    m_port->setFlowControl(toQtFlowControl(m_config.flowControl));
    m_port->setReadBufferSize(m_config.readBufferSize);
}

void SerialPort::startTransmitLineDelay()
{
    if (!m_waitingTransmitDrain || m_transmitLineTimer->isActive()) {
        return;
    }

    /*
     * bytesToWrite()==0 只表示 Qt 写缓冲已经交给驱动。USB-串口芯片仍可能
     * 正在按波特率把这些字节一个个移出 TX。为了让文件传输进度条绝不早于
     * 真实线路发送，进入该阶段后重新等待完整线路时间，而不是扣减前面等待
     * Qt/系统写缓冲清空所消耗的时间。
     */
    const int estimatedMs = estimateTransmitTimeMs(m_pendingTransmitDrainBytes);

    if (estimatedMs <= 0) {
        completeTransmitDrain(true, QString());
        return;
    }

    /*
     * 第二阶段 watchdog 随线路等待重启，避免低波特率或较大分块时，第一阶段
     * 的“等缓冲清空”超时保护在线路定时器还没结束前误判失败。
     */
    m_transmitDrainWatchdog->start(estimatedMs + 1000);
    m_transmitLineTimer->start(estimatedMs);
}

void SerialPort::completeTransmitDrain(bool success, const QString& errorMessage)
{
    if (!m_waitingTransmitDrain) {
        return;
    }

    m_transmitDrainWatchdog->stop();
    m_transmitLineTimer->stop();
    m_pendingTransmitDrainBytes = 0;
    m_waitingTransmitDrain = false;

    if (!success) {
        m_lastError = errorMessage.isEmpty()
            ? tr("串口发送缓冲等待发空失败")
            : errorMessage;
    }

    emit transmitDrained(success, success ? QString() : m_lastError);
}

double SerialPort::configuredBitsPerByte() const
{
    /*
     * 串口线路发送一个字节通常包含 1 个起始位、N 个数据位、可选校验位
     * 和停止位。OneAndHalfStop 按 1.5 位估算，最终向上取整到毫秒。
     */
    double bits = 1.0; // start bit
    switch (m_config.dataBits) {
    case DataBits::Five:
        bits += 5.0;
        break;
    case DataBits::Six:
        bits += 6.0;
        break;
    case DataBits::Seven:
        bits += 7.0;
        break;
    case DataBits::Eight:
    default:
        bits += 8.0;
        break;
    }

    if (m_config.parity != Parity::None) {
        bits += 1.0;
    }

    switch (m_config.stopBits) {
    case StopBits::OnePointFive:
        bits += 1.5;
        break;
    case StopBits::Two:
        bits += 2.0;
        break;
    case StopBits::One:
    default:
        bits += 1.0;
        break;
    }
    return bits;
}

int SerialPort::estimateTransmitTimeMs(qint64 bytes) const
{
    if (bytes <= 0) {
        return 0;
    }

    /*
     * 为了避免依赖额外数学头，这里把半位停止位放大 2 倍后做整数向上取整。
     * 公式等价于 ceil(bytes * bitsPerByte * 1000 / baudRate)。
     */
    const qint64 baudRate = qMax(1, m_config.baudRate);
    const qint64 scaledBits =
        static_cast<qint64>(bytes * configuredBitsPerByte() * 2.0 + 0.5);
    const qint64 numerator = scaledBits * 1000;
    const qint64 denominator = baudRate * 2;
    return static_cast<int>((numerator + denominator - 1) / denominator);
}

QSerialPort::DataBits SerialPort::toQtDataBits(DataBits bits)
{
    switch (bits) {
        case DataBits::Five:  return QSerialPort::Data5;
        case DataBits::Six:   return QSerialPort::Data6;
        case DataBits::Seven: return QSerialPort::Data7;
        case DataBits::Eight:
        default:              return QSerialPort::Data8;
    }
}

QSerialPort::StopBits SerialPort::toQtStopBits(StopBits bits)
{
    switch (bits) {
        case StopBits::OnePointFive: return QSerialPort::OneAndHalfStop;
        case StopBits::Two:          return QSerialPort::TwoStop;
        case StopBits::One:
        default:                     return QSerialPort::OneStop;
    }
}

QSerialPort::Parity SerialPort::toQtParity(Parity parity)
{
    switch (parity) {
        case Parity::Odd:   return QSerialPort::OddParity;
        case Parity::Even:  return QSerialPort::EvenParity;
        case Parity::Mark:  return QSerialPort::MarkParity;
        case Parity::Space: return QSerialPort::SpaceParity;
        case Parity::None:
        default:            return QSerialPort::NoParity;
    }
}

QSerialPort::FlowControl SerialPort::toQtFlowControl(FlowControl flow)
{
    switch (flow) {
        case FlowControl::Hardware: return QSerialPort::HardwareControl;
        case FlowControl::Software: return QSerialPort::SoftwareControl;
        case FlowControl::None:
        default:                    return QSerialPort::NoFlowControl;
    }
}

} // namespace ComAssistant
