/**
 * @file UdpSocket.cpp
 * @brief UDP通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "UdpSocket.h"
#include "utils/Logger.h"
#include <QNetworkDatagram>

namespace ComAssistant {

UdpSocket::UdpSocket(const NetworkConfig& config, QObject* parent)
    : ICommunication(parent)
    , m_config(config)
{
    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpSocket::onReadyRead);
    connect(m_socket, &QUdpSocket::errorOccurred, this, &UdpSocket::onError);
}

UdpSocket::~UdpSocket()
{
    close();
}

bool UdpSocket::open()
{
    if (m_bound) {
        return true;
    }

    int localPort = m_config.listenPort;
    if (localPort <= 0) {
        localPort = 0;  // 让系统分配端口
    }

    if (!m_socket->bind(QHostAddress::Any, localPort)) {
        m_lastError = m_socket->errorString();
        LOG_ERROR(QString("Failed to bind UDP socket to port %1: %2")
                      .arg(localPort)
                      .arg(m_lastError));
        emit errorOccurred(m_lastError);
        return false;
    }

    m_bound = true;
    LOG_INFO(QString("UDP socket bound to port %1").arg(m_socket->localPort()));
    emit connectionStatusChanged(true);
    return true;
}

void UdpSocket::close()
{
    if (m_bound) {
        m_socket->close();
        m_bound = false;
        LOG_INFO("UDP socket closed");
        emit connectionStatusChanged(false);
    }
}

bool UdpSocket::isOpen() const
{
    return m_bound;
}

qint64 UdpSocket::write(const QByteArray& data)
{
    if (m_config.remoteIp.isEmpty() || m_config.remotePort <= 0) {
        m_lastError = tr("Remote address not set");
        return -1;
    }

    return writeTo(data, m_config.remoteIp, m_config.remotePort);
}

QByteArray UdpSocket::readAll()
{
    QByteArray data = m_readBuffer;
    m_readBuffer.clear();
    return data;
}

qint64 UdpSocket::bytesAvailable() const
{
    return m_readBuffer.size() + m_socket->pendingDatagramSize();
}

void UdpSocket::setBufferSize(int size)
{
    m_bufferSize = size;
    m_socket->setReadBufferSize(size);
}

int UdpSocket::bufferSize() const
{
    return m_bufferSize;
}

void UdpSocket::clearBuffer()
{
    m_readBuffer.clear();
    // 清空socket缓冲区
    while (m_socket->hasPendingDatagrams()) {
        m_socket->receiveDatagram();
    }
}

void UdpSocket::setReadTimeout(int ms)
{
    m_readTimeout = ms;
}

int UdpSocket::readTimeout() const
{
    return m_readTimeout;
}

void UdpSocket::setWriteTimeout(int ms)
{
    m_writeTimeout = ms;
}

int UdpSocket::writeTimeout() const
{
    return m_writeTimeout;
}

QString UdpSocket::statusString() const
{
    if (!m_bound) {
        return tr("Not bound");
    }

    QString status = tr("Local: %1").arg(m_socket->localPort());
    if (!m_config.remoteIp.isEmpty() && m_config.remotePort > 0) {
        status += tr(" -> %1:%2").arg(m_config.remoteIp).arg(m_config.remotePort);
    }
    return status;
}

void UdpSocket::setConfig(const NetworkConfig& config)
{
    m_config = config;
}

NetworkConfig UdpSocket::config() const
{
    return m_config;
}

void UdpSocket::setLocalPort(int port)
{
    m_config.listenPort = port;
}

int UdpSocket::localPort() const
{
    if (m_bound) {
        return m_socket->localPort();
    }
    return m_config.listenPort;
}

void UdpSocket::setRemoteAddress(const QString& ip, int port)
{
    m_config.remoteIp = ip;
    m_config.remotePort = port;
}

QString UdpSocket::remoteIp() const
{
    return m_config.remoteIp;
}

int UdpSocket::remotePort() const
{
    return m_config.remotePort;
}

qint64 UdpSocket::writeTo(const QByteArray& data, const QString& ip, int port)
{
    if (!m_bound) {
        // 自动绑定
        if (!open()) {
            return -1;
        }
    }

    QHostAddress address(ip);
    qint64 written = m_socket->writeDatagram(data, address, port);

    if (written > 0) {
        emit dataSent(data);
    } else if (written < 0) {
        m_lastError = m_socket->errorString();
        emit errorOccurred(m_lastError);
    }

    return written;
}

void UdpSocket::setBroadcastEnabled(bool enabled)
{
    m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, enabled ? 1 : 0);
}

bool UdpSocket::isBroadcastEnabled() const
{
    return m_socket->socketOption(QAbstractSocket::MulticastTtlOption).toInt() > 0;
}

bool UdpSocket::joinMulticastGroup(const QString& groupAddress)
{
    QHostAddress address(groupAddress);
    if (!address.isMulticast()) {
        m_lastError = tr("Invalid multicast address: %1").arg(groupAddress);
        return false;
    }

    if (!m_bound) {
        if (!open()) {
            return false;
        }
    }

    if (m_socket->joinMulticastGroup(address)) {
        LOG_INFO(QString("Joined multicast group: %1").arg(groupAddress));
        return true;
    }

    m_lastError = m_socket->errorString();
    LOG_ERROR(QString("Failed to join multicast group: %1").arg(m_lastError));
    return false;
}

bool UdpSocket::leaveMulticastGroup(const QString& groupAddress)
{
    QHostAddress address(groupAddress);
    if (m_socket->leaveMulticastGroup(address)) {
        LOG_INFO(QString("Left multicast group: %1").arg(groupAddress));
        return true;
    }

    m_lastError = m_socket->errorString();
    return false;
}

void UdpSocket::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_socket->receiveDatagram();
        QByteArray data = datagram.data();

        if (!data.isEmpty()) {
            m_lastSenderIp = datagram.senderAddress().toString();
            m_lastSenderPort = datagram.senderPort();

            m_readBuffer.append(data);
            emit datagramReceived(data, m_lastSenderIp, m_lastSenderPort);
            emit dataReceived(data);
        }
    }
}

void UdpSocket::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    m_lastError = m_socket->errorString();
    LOG_ERROR(QString("UDP socket error: %1").arg(m_lastError));
    emit errorOccurred(m_lastError);
}

} // namespace ComAssistant
