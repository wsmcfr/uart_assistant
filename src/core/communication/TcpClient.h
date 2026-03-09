/**
 * @file TcpClient.h
 * @brief TCP客户端通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_TCPCLIENT_H
#define COMASSISTANT_TCPCLIENT_H

#include "ICommunication.h"
#include "config/AppConfig.h"
#include <QTcpSocket>
#include <QHostAddress>
#include <QTimer>

namespace ComAssistant {

/**
 * @brief TCP客户端通信实现类
 */
class TcpClient : public ICommunication {
    Q_OBJECT

public:
    explicit TcpClient(const NetworkConfig& config, QObject* parent = nullptr);
    ~TcpClient() override;

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

    CommType type() const override { return CommType::TcpClient; }
    QString typeName() const override { return QStringLiteral("TCP Client"); }
    QString statusString() const override;
    QString lastError() const override { return m_lastError; }

    //=========================================================================
    // TCP客户端特有方法
    //=========================================================================

    void setConfig(const NetworkConfig& config);
    NetworkConfig config() const;

    void setServerAddress(const QString& ip, int port);
    QString serverIp() const;
    int serverPort() const;

    void setConnectTimeout(int ms);
    int connectTimeout() const;

    void setAutoReconnect(bool enabled, int intervalMs = 3000);
    bool isAutoReconnect() const;

    QAbstractSocket::SocketState state() const;

signals:
    void connected();
    void disconnected();
    void stateChanged(QAbstractSocket::SocketState state);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onStateChanged(QAbstractSocket::SocketState state);
    void tryReconnect();

private:
    QTcpSocket* m_socket = nullptr;
    NetworkConfig m_config;
    QTimer* m_reconnectTimer = nullptr;
    bool m_autoReconnect = false;
    int m_reconnectInterval = 3000;
    bool m_manualDisconnect = false;
};

} // namespace ComAssistant

#endif // COMASSISTANT_TCPCLIENT_H
