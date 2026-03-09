/**
 * @file ICommunication.h
 * @brief 通信接口基类
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_ICOMMUNICATION_H
#define COMASSISTANT_ICOMMUNICATION_H

#include <QObject>
#include <QByteArray>
#include <QString>

namespace ComAssistant {

/**
 * @brief 通信类型枚举
 */
enum class CommType {
    Serial,     ///< 串口
    TcpClient,  ///< TCP客户端
    TcpServer,  ///< TCP服务端
    Udp,        ///< UDP
    Hid         ///< USB HID
};

/**
 * @brief 通信接口基类
 *
 * 所有通信实现（串口、TCP、UDP、HID）都继承此接口
 */
class ICommunication : public QObject {
    Q_OBJECT

public:
    explicit ICommunication(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ICommunication() = default;

    //=========================================================================
    // 连接管理
    //=========================================================================

    /**
     * @brief 打开连接
     * @return 是否成功
     */
    virtual bool open() = 0;

    /**
     * @brief 关闭连接
     */
    virtual void close() = 0;

    /**
     * @brief 检查是否已打开
     */
    virtual bool isOpen() const = 0;

    //=========================================================================
    // 数据传输
    //=========================================================================

    /**
     * @brief 发送数据
     * @param data 要发送的数据
     * @return 实际发送的字节数，-1表示错误
     */
    virtual qint64 write(const QByteArray& data) = 0;

    /**
     * @brief 读取所有可用数据
     * @return 读取到的数据
     */
    virtual QByteArray readAll() = 0;

    /**
     * @brief 获取可读取的字节数
     */
    virtual qint64 bytesAvailable() const = 0;

    //=========================================================================
    // 缓冲区管理
    //=========================================================================

    /**
     * @brief 设置缓冲区大小
     */
    virtual void setBufferSize(int size) = 0;

    /**
     * @brief 获取缓冲区大小
     */
    virtual int bufferSize() const = 0;

    /**
     * @brief 清空缓冲区
     */
    virtual void clearBuffer() = 0;

    //=========================================================================
    // 超时设置
    //=========================================================================

    /**
     * @brief 设置读超时
     * @param ms 超时时间（毫秒）
     */
    virtual void setReadTimeout(int ms) = 0;

    /**
     * @brief 获取读超时
     */
    virtual int readTimeout() const = 0;

    /**
     * @brief 设置写超时
     * @param ms 超时时间（毫秒）
     */
    virtual void setWriteTimeout(int ms) = 0;

    /**
     * @brief 获取写超时
     */
    virtual int writeTimeout() const = 0;

    //=========================================================================
    // 状态信息
    //=========================================================================

    /**
     * @brief 获取通信类型
     */
    virtual CommType type() const = 0;

    /**
     * @brief 获取类型名称
     */
    virtual QString typeName() const = 0;

    /**
     * @brief 获取状态描述字符串
     */
    virtual QString statusString() const = 0;

    /**
     * @brief 获取最后的错误信息
     */
    virtual QString lastError() const = 0;

signals:
    /**
     * @brief 收到数据信号
     */
    void dataReceived(const QByteArray& data);

    /**
     * @brief 数据已发送信号
     */
    void dataSent(const QByteArray& data);

    /**
     * @brief 错误发生信号
     */
    void errorOccurred(const QString& error);

    /**
     * @brief 连接状态变化信号
     */
    void connectionStatusChanged(bool connected);

protected:
    QString m_lastError;
    int m_readTimeout = 100;
    int m_writeTimeout = 100;
    int m_bufferSize = 65536;
};

} // namespace ComAssistant

#endif // COMASSISTANT_ICOMMUNICATION_H
