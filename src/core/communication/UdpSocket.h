/**
 * @file UdpSocket.h
 * @brief UDP通信实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_UDPSOCKET_H
#define COMASSISTANT_UDPSOCKET_H

#include "ICommunication.h"
#include "config/AppConfig.h"
#include <QUdpSocket>
#include <QHostAddress>

namespace ComAssistant {

/**
 * @brief UDP通信实现类
 */
class UdpSocket : public ICommunication {
    Q_OBJECT

public:
    explicit UdpSocket(const NetworkConfig& config, QObject* parent = nullptr);
    ~UdpSocket() override;

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

    CommType type() const override { return CommType::Udp; }
    QString typeName() const override { return QStringLiteral("UDP"); }
    QString statusString() const override;
    QString lastError() const override { return m_lastError; }

    //=========================================================================
    // UDP特有方法
    //=========================================================================

    void setConfig(const NetworkConfig& config);
    NetworkConfig config() const;

    /**
     * @brief 设置本地绑定端口
     */
    void setLocalPort(int port);
    int localPort() const;

    /**
     * @brief 设置远程目标地址
     */
    void setRemoteAddress(const QString& ip, int port);
    QString remoteIp() const;
    int remotePort() const;

    /**
     * @brief 发送数据到指定地址
     */
    qint64 writeTo(const QByteArray& data, const QString& ip, int port);

    /**
     * @brief 启用/禁用广播
     */
    void setBroadcastEnabled(bool enabled);
    bool isBroadcastEnabled() const;

    /**
     * @brief 加入组播组
     */
    bool joinMulticastGroup(const QString& groupAddress);

    /**
     * @brief 离开组播组
     */
    bool leaveMulticastGroup(const QString& groupAddress);

signals:
    /**
     * @brief 收到数据报信号（包含发送者信息）
     */
    void datagramReceived(const QByteArray& data, const QString& senderIp, int senderPort);

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);

private:
    QUdpSocket* m_socket = nullptr;
    NetworkConfig m_config;
    QByteArray m_readBuffer;
    QString m_lastSenderIp;
    int m_lastSenderPort = 0;
    bool m_bound = false;
};

} // namespace ComAssistant

#endif // COMASSISTANT_UDPSOCKET_H
