/**
 * @file TcpClient.cpp
 * @brief TCP客户端通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "TcpClient.h"
#include "utils/Logger.h"

namespace ComAssistant {

TcpClient::TcpClient(const NetworkConfig& config, QObject* parent)
    : ICommunication(parent)
    , m_config(config)
{
    m_socket = new QTcpSocket(this);
    m_reconnectTimer = new QTimer(this);

    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(m_socket, &QTcpSocket::stateChanged, this, &TcpClient::onStateChanged);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TcpClient::onError);
    connect(m_reconnectTimer, &QTimer::timeout, this, &TcpClient::tryReconnect);
}

TcpClient::~TcpClient()
{
    m_reconnectTimer->stop();
    if (isOpen()) {
        close();
    }
}

bool TcpClient::open()
{
    if (isOpen()) {
        return true;
    }

    m_manualDisconnect = false;

    if (m_config.serverIp.isEmpty()) {
        m_lastError = tr("Server IP is empty");
        emit errorOccurred(m_lastError);
        return false;
    }

    LOG_INFO(QString("Connecting to %1:%2...")
                 .arg(m_config.serverIp)
                 .arg(m_config.serverPort));

    m_socket->connectToHost(m_config.serverIp, m_config.serverPort);

    // 等待连接
    if (!m_socket->waitForConnected(m_config.connectTimeout)) {
        m_lastError = m_socket->errorString();
        LOG_ERROR(QString("Failed to connect: %1").arg(m_lastError));
        emit errorOccurred(m_lastError);

        // 启动自动重连
        if (m_autoReconnect && !m_manualDisconnect) {
            m_reconnectTimer->start(m_reconnectInterval);
        }
        return false;
    }

    return true;
}

void TcpClient::close()
{
    m_manualDisconnect = true;
    m_reconnectTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
}

bool TcpClient::isOpen() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

qint64 TcpClient::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = tr("Socket is not connected");
        return -1;
    }

    qint64 written = m_socket->write(data);
    if (written > 0) {
        m_socket->flush();
        emit dataSent(data.left(written));
    } else if (written < 0) {
        m_lastError = m_socket->errorString();
        emit errorOccurred(m_lastError);
    }

    return written;
}

QByteArray TcpClient::readAll()
{
    return m_socket->readAll();
}

qint64 TcpClient::bytesAvailable() const
{
    return m_socket->bytesAvailable();
}

void TcpClient::setBufferSize(int size)
{
    m_bufferSize = size;
    m_socket->setReadBufferSize(size);
}

int TcpClient::bufferSize() const
{
    return m_bufferSize;
}

void TcpClient::clearBuffer()
{
    m_socket->readAll();
}

void TcpClient::setReadTimeout(int ms)
{
    m_readTimeout = ms;
}

int TcpClient::readTimeout() const
{
    return m_readTimeout;
}

void TcpClient::setWriteTimeout(int ms)
{
    m_writeTimeout = ms;
}

int TcpClient::writeTimeout() const
{
    return m_writeTimeout;
}

QString TcpClient::statusString() const
{
    if (!isOpen()) {
        return tr("Disconnected");
    }
    return QString("%1:%2").arg(m_config.serverIp).arg(m_config.serverPort);
}

void TcpClient::setConfig(const NetworkConfig& config)
{
    m_config = config;
}

NetworkConfig TcpClient::config() const
{
    return m_config;
}

void TcpClient::setServerAddress(const QString& ip, int port)
{
    m_config.serverIp = ip;
    m_config.serverPort = port;
}

QString TcpClient::serverIp() const
{
    return m_config.serverIp;
}

int TcpClient::serverPort() const
{
    return m_config.serverPort;
}

void TcpClient::setConnectTimeout(int ms)
{
    m_config.connectTimeout = ms;
}

int TcpClient::connectTimeout() const
{
    return m_config.connectTimeout;
}

void TcpClient::setAutoReconnect(bool enabled, int intervalMs)
{
    m_autoReconnect = enabled;
    m_reconnectInterval = intervalMs;
}

bool TcpClient::isAutoReconnect() const
{
    return m_autoReconnect;
}

QAbstractSocket::SocketState TcpClient::state() const
{
    return m_socket->state();
}

void TcpClient::onConnected()
{
    LOG_INFO(QString("Connected to %1:%2")
                 .arg(m_config.serverIp)
                 .arg(m_config.serverPort));
    m_reconnectTimer->stop();
    emit connected();
    emit connectionStatusChanged(true);
}

void TcpClient::onDisconnected()
{
    LOG_INFO(QString("Disconnected from %1:%2")
                 .arg(m_config.serverIp)
                 .arg(m_config.serverPort));

    emit disconnected();
    emit connectionStatusChanged(false);

    // 自动重连
    if (m_autoReconnect && !m_manualDisconnect) {
        m_reconnectTimer->start(m_reconnectInterval);
    }
}

void TcpClient::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpClient::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    m_lastError = m_socket->errorString();
    LOG_ERROR(QString("TCP Client error: %1").arg(m_lastError));
    emit errorOccurred(m_lastError);
}

void TcpClient::onStateChanged(QAbstractSocket::SocketState state)
{
    emit stateChanged(state);
}

void TcpClient::tryReconnect()
{
    if (!m_manualDisconnect && !isOpen()) {
        LOG_INFO("Attempting to reconnect...");
        open();
    }
}

} // namespace ComAssistant
