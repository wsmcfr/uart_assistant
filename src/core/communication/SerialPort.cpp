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

    connect(m_port, &QSerialPort::readyRead, this, &SerialPort::onReadyRead);
    connect(m_port, SIGNAL(error(QSerialPort::SerialPortError)), this, SLOT(onError(QSerialPort::SerialPortError)));
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

void SerialPort::onReadyRead()
{
    QByteArray data = m_port->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void SerialPort::onError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    m_lastError = m_port->errorString();
    LOG_ERROR(QString("Serial port error: %1").arg(m_lastError));

    if (error == QSerialPort::ResourceError) {
        // 设备断开
        close();
    }

    emit errorOccurred(m_lastError);
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
