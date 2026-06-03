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
    /*
     * TCP Server 的 m_readBuffer 只是 readAll() 兼容缓存。关闭监听时用户
     * 已不再需要旧数据，释放容量可以避免大流量后关闭服务仍保留峰值内存。
     */
    m_readBuffer.clear();
    m_readBuffer.squeeze();

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
    m_readBuffer.squeeze();
    return data;
}

qint64 TcpServer::bytesAvailable() const
{
    return m_readBuffer.size();
}

void TcpServer::setBufferSize(int size)
{
    /*
     * Qt 底层 readBufferSize 和 readAll() 兼容缓存使用同一配置上限。
     * 负数没有明确业务含义，统一归零表示“不限制”，避免传给 Qt 后端。
     */
    m_bufferSize = qMax(0, size);
    trimReceiveBuffer(m_readBuffer);
    for (auto socket : m_clients) {
        socket->setReadBufferSize(m_bufferSize);
    }
}

int TcpServer::bufferSize() const
{
    return m_bufferSize;
}

void TcpServer::clearBuffer()
{
    m_readBuffer.clear();
    m_readBuffer.squeeze();
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
    /*
     * close()/析构要求返回时不再持有客户端 socket 引用。单纯调用
     * disconnectFromHost() 需要等待异步 disconnected 信号，期间 m_clients
     * 仍会保留旧指针。这里先从映射中取出全部 socket，再断开本对象上的
     * socket 信号并安排删除，使“关闭服务”具备同步释放引用的语义。
     */
    const QMap<QString, QTcpSocket*> clients = m_clients;
    m_clients.clear();
    for (QTcpSocket* socket : clients) {
        if (!socket) {
            continue;
        }
        socket->disconnect(this);
        socket->disconnectFromHost();
        socket->close();
        socket->deleteLater();
    }

    /*
     * 主窗口工作台依赖 clientDisconnected 更新目标客户端列表。批量关闭时
     * 已经屏蔽了 socket 的异步 disconnected 信号，因此这里主动广播一次。
     */
    for (const QString& clientId : clients.keys()) {
        emit clientDisconnected(clientId);
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
        connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onClientError(QAbstractSocket::SocketError)));

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
        /*
         * dataReceived 是主接收路径；m_readBuffer 只为旧 readAll() 调用保留
         * 最近数据。追加后立即按 bufferSize 裁剪，避免未调用 readAll() 时
         * TCP Server 隐藏缓存长期增长。
         */
        appendToReceiveBuffer(m_readBuffer, data);
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
