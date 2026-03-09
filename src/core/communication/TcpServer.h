/**
 * @file TcpServer.h
 * @brief TCP服务端通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_TCPSERVER_H
#define COMASSISTANT_TCPSERVER_H

#include "ICommunication.h"
#include "config/AppConfig.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>

namespace ComAssistant {

/**
 * @brief TCP服务端通信实现类
 */
class TcpServer : public ICommunication {
    Q_OBJECT

public:
    explicit TcpServer(const NetworkConfig& config, QObject* parent = nullptr);
    ~TcpServer() override;

    //=========================================================================
    // ICommunication 接口实现
    //=========================================================================

    bool open() override;
    void close() override;
    bool isOpen() const override;

    qint64 write(const QByteArray& data) override;
    QByteArray readAll() override;
    qint64 bytesAvailable() const override;

    void setBufferSize(int size) override;
    int bufferSize() const override;
    void clearBuffer() override;

    void setReadTimeout(int ms) override;
    int readTimeout() const override;
    void setWriteTimeout(int ms) override;
    int writeTimeout() const override;

    CommType type() const override { return CommType::TcpServer; }
    QString typeName() const override { return QStringLiteral("TCP Server"); }
    QString statusString() const override;
    QString lastError() const override { return m_lastError; }

    //=========================================================================
    // TCP服务端特有方法
    //=========================================================================

    void setConfig(const NetworkConfig& config);
    NetworkConfig config() const;

    void setListenPort(int port);
    int listenPort() const;

    void setMaxConnections(int max);
    int maxConnections() const;

    /**
     * @brief 获取已连接的客户端列表
     * @return 客户端ID列表（格式：ip:port）
     */
    QStringList connectedClients() const;

    /**
     * @brief 获取连接数
     */
    int connectionCount() const;

    /**
     * @brief 断开指定客户端
     */
    void disconnectClient(const QString& clientId);

    /**
     * @brief 断开所有客户端
     */
    void disconnectAllClients();

    /**
     * @brief 向指定客户端发送数据
     */
    qint64 writeToClient(const QString& clientId, const QByteArray& data);

    /**
     * @brief 向所有客户端广播数据
     */
    qint64 broadcast(const QByteArray& data);

signals:
    void clientConnected(const QString& clientId);
    void clientDisconnected(const QString& clientId);
    void clientDataReceived(const QString& clientId, const QByteArray& data);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();
    void onClientError(QAbstractSocket::SocketError error);

private:
    QString getClientId(QTcpSocket* socket) const;
    QTcpSocket* getClientSocket(const QString& clientId) const;

    QTcpServer* m_server = nullptr;
    NetworkConfig m_config;
    QMap<QString, QTcpSocket*> m_clients;
    QByteArray m_readBuffer;
};

} // namespace ComAssistant

#endif // COMASSISTANT_TCPSERVER_H
