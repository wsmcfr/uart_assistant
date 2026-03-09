/**
 * @file TcpServer.cpp
 * @brief TCP服务端通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "TcpServer.h"
#include "utils/Logger.h"

namespace ComAssistant {

TcpServer::TcpServer(const NetworkConfig& config, QObject* parent)
    : ICommunication(parent)
    , m_config(config)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

TcpServer::~TcpServer()
{
    close();
}

bool TcpServer::open()
{
    if (isOpen()) {
        return true;
    }

    QHostAddress address = QHostAddress::Any;

    if (!m_server->listen(address, m_config.listenPort)) {
        m_lastError = m_server->errorString();
        LOG_ERROR(QString("Failed to start TCP server on port %1: %2")
                      .arg(m_config.listenPort)
                      .arg(m_lastError));
        emit errorOccurred(m_lastError);
        return false;
    }

    LOG_INFO(QString("TCP Server started on port %1").arg(m_config.listenPort));
    emit connectionStatusChanged(true);
    return true;
}

void TcpServer::close()
{
    disconnectAllClients();

    if (m_server->isListening()) {
        m_server->close();
        LOG_INFO("TCP Server stopped");
        emit connectionStatusChanged(false);
    }
}

bool TcpServer::isOpen() const
{
    return m_server->isListening();
}

qint64 TcpServer::write(const QByteArray& data)
{
    // 向所有客户端广播
    return broadcast(data);
}

QByteArray TcpServer::readAll()
{
    QByteArray data = m_readBuffer;
    m_readBuffer.clear();
    return data;
}

qint64 TcpServer::bytesAvailable() const
{
    return m_readBuffer.size();
}

void TcpServer::setBufferSize(int size)
{
    m_bufferSize = size;
    for (auto socket : m_clients) {
        socket->setReadBufferSize(size);
    }
}

int TcpServer::bufferSize() const
{
    return m_bufferSize;
}

void TcpServer::clearBuffer()
{
    m_readBuffer.clear();
}

void TcpServer::setReadTimeout(int ms)
{
    m_readTimeout = ms;
}

int TcpServer::readTimeout() const
{
    return m_readTimeout;
}

void TcpServer::setWriteTimeout(int ms)
{
    m_writeTimeout = ms;
}

int TcpServer::writeTimeout() const
{
    return m_writeTimeout;
}

QString TcpServer::statusString() const
{
    if (!isOpen()) {
        return tr("Not listening");
    }
    return tr("Listening on port %1 (%2 clients)")
        .arg(m_config.listenPort)
        .arg(m_clients.size());
}

void TcpServer::setConfig(const NetworkConfig& config)
{
    m_config = config;
}

NetworkConfig TcpServer::config() const
{
    return m_config;
}

void TcpServer::setListenPort(int port)
{
    m_config.listenPort = port;
}

int TcpServer::listenPort() const
{
    return m_config.listenPort;
}

void TcpServer::setMaxConnections(int max)
{
    m_config.maxConnections = max;
    m_server->setMaxPendingConnections(max);
}

int TcpServer::maxConnections() const
{
    return m_config.maxConnections;
}

QStringList TcpServer::connectedClients() const
{
    return m_clients.keys();
}

int TcpServer::connectionCount() const
{
    return m_clients.size();
}

void TcpServer::disconnectClient(const QString& clientId)
{
    if (m_clients.contains(clientId)) {
        QTcpSocket* socket = m_clients[clientId];
        socket->disconnectFromHost();
    }
}

void TcpServer::disconnectAllClients()
{
    QList<QString> clientIds = m_clients.keys();
    for (const QString& clientId : clientIds) {
        disconnectClient(clientId);
    }
}

qint64 TcpServer::writeToClient(const QString& clientId, const QByteArray& data)
{
    QTcpSocket* socket = getClientSocket(clientId);
    if (!socket) {
        m_lastError = tr("Client not found: %1").arg(clientId);
        return -1;
    }

    qint64 written = socket->write(data);
    if (written > 0) {
        socket->flush();
        emit dataSent(data.left(written));
    }
    return written;
}

qint64 TcpServer::broadcast(const QByteArray& data)
{
    qint64 totalWritten = 0;

    for (auto socket : m_clients) {
        qint64 written = socket->write(data);
        if (written > 0) {
            socket->flush();
            totalWritten += written;
        }
    }

    if (totalWritten > 0) {
        emit dataSent(data);
    }

    return totalWritten;
}

void TcpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket* socket = m_server->nextPendingConnection();

        // 检查连接数限制
        if (m_clients.size() >= m_config.maxConnections) {
            LOG_WARN("Max connections reached, rejecting new connection");
            socket->disconnectFromHost();
            socket->deleteLater();
            continue;
        }

        QString clientId = getClientId(socket);
        m_clients[clientId] = socket;

        connect(socket, &QTcpSocket::readyRead, this, &TcpServer::onClientReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TcpServer::onClientDisconnected);
        connect(socket, &QTcpSocket::errorOccurred, this, &TcpServer::onClientError);

        socket->setReadBufferSize(m_bufferSize);

        LOG_INFO(QString("Client connected: %1").arg(clientId));
        emit clientConnected(clientId);
    }
}

void TcpServer::onClientReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QString clientId = getClientId(socket);
    QByteArray data = socket->readAll();

    if (!data.isEmpty()) {
        m_readBuffer.append(data);
        emit clientDataReceived(clientId, data);
        emit dataReceived(data);
    }
}

void TcpServer::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    QString clientId = getClientId(socket);
    m_clients.remove(clientId);

    LOG_INFO(QString("Client disconnected: %1").arg(clientId));
    emit clientDisconnected(clientId);

    socket->deleteLater();
}

void TcpServer::onClientError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        LOG_ERROR(QString("Client error: %1").arg(socket->errorString()));
    }
}

QString TcpServer::getClientId(QTcpSocket* socket) const
{
    if (!socket) {
        return QString();
    }
    return QString("%1:%2")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());
}

QTcpSocket* TcpServer::getClientSocket(const QString& clientId) const
{
    return m_clients.value(clientId, nullptr);
}

} // namespace ComAssistant
