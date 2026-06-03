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
#include <QtGlobal>

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
     * @brief 异步等待最近一次发送的数据真正排空。
     * @param bytes 本次需要等待发空的字节数，用于串口按波特率估算线路耗时。
     * @return 成功接受等待请求返回 true；当前连接不可用或已有等待任务返回 false。
     *
     * 默认实现用于 TCP/UDP/HID 等没有串口线路“按波特率发空”概念的后端，
     * 会立即发出成功信号。SerialPort 会覆盖该函数：先等待 Qt 写缓冲变空，
     * 再根据串口帧格式和波特率等待线路传输时间，最后才发出 transmitDrained。
     */
    virtual bool waitForTransmitDrainedAsync(qint64 bytes)
    {
        Q_UNUSED(bytes)
        emit transmitDrained(true, QString());
        return true;
    }

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
     * @brief 最近一次发送请求已经排空或失败。
     * @param success true 表示可认为本地发送链路已经排空。
     * @param errorMessage success 为 false 时的失败原因。
     *
     * 该信号专门服务文件传输等需要“最后一个字节发完后再推进进度”的场景。
     * 普通手动发送仍只依赖 dataSent，避免影响日常调试输入的即时反馈。
     */
    void transmitDrained(bool success, const QString& errorMessage);

    /**
     * @brief 错误发生信号
     */
    void errorOccurred(const QString& error);

    /**
     * @brief 连接状态变化信号
     */
    void connectionStatusChanged(bool connected);

protected:
    /**
     * @brief 追加数据到 readAll() 兼容缓存，并按缓冲区上限裁剪。
     * @param buffer 要维护的兼容缓存。
     * @param data 本次刚收到、已经通过 dataReceived 信号分发的数据。
     *
     * 主窗口主要通过 dataReceived 信号消费数据，但旧调用者仍可能依赖
     * readAll()。因此各后端保留一份兼容缓存；为了避免信号路径已经消费
     * 后缓存仍长期增长，这里只保留 bufferSize() 指定的尾部数据。
     * m_bufferSize <= 0 表示不限制，沿用现有 HID 行为，避免破坏旧配置。
     */
    void appendToReceiveBuffer(QByteArray& buffer, const QByteArray& data) const
    {
        buffer.append(data);
        trimReceiveBuffer(buffer);
    }

    /**
     * @brief 按当前 m_bufferSize 裁剪 readAll() 兼容缓存。
     * @param buffer 要裁剪的兼容缓存。
     *
     * 该函数用于 setBufferSize() 调小上限时立即释放旧数据，避免已经积累的
     * 缓存必须等下一包到达才被裁剪。
     */
    void trimReceiveBuffer(QByteArray& buffer) const
    {
        if (m_bufferSize > 0 && buffer.size() > m_bufferSize) {
            buffer = buffer.right(m_bufferSize);
        }
    }

    QString m_lastError;
    int m_readTimeout = 100;
    int m_writeTimeout = 100;
    int m_bufferSize = 65536;
};

} // namespace ComAssistant

#endif // COMASSISTANT_ICOMMUNICATION_H
