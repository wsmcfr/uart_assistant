/**
 * @file MultiPortManager.h
 * @brief 多串口管理器
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_MULTIPORTMANAGER_H
#define COMASSISTANT_MULTIPORTMANAGER_H

#include <QObject>
#include <QMap>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QMutex>

namespace ComAssistant {

/**
 * @brief 串口实例信息
 */
struct PortInstance {
    QString id;                     ///< 唯一标识符
    QString portName;               ///< 串口名称 (如 COM3)
    QString alias;                  ///< 用户别名
    QSerialPort* port = nullptr;    ///< 串口对象
    bool isOpen = false;            ///< 是否已打开

    // 配置
    qint32 baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;

    // 统计
    qint64 txBytes = 0;
    qint64 rxBytes = 0;
    qint64 txCount = 0;
    qint64 rxCount = 0;
};

/**
 * @brief 多串口管理器
 */
class MultiPortManager : public QObject {
    Q_OBJECT

public:
    static MultiPortManager* instance();

    /**
     * @brief 获取所有可用串口
     */
    QList<QSerialPortInfo> availablePorts() const;

    /**
     * @brief 创建串口实例
     * @param portName 串口名称
     * @return 实例ID，失败返回空字符串
     */
    QString createPort(const QString& portName);

    /**
     * @brief 移除串口实例
     */
    bool removePort(const QString& instanceId);

    /**
     * @brief 获取所有实例ID
     */
    QStringList instanceIds() const;

    /**
     * @brief 获取实例信息
     */
    PortInstance* getInstance(const QString& instanceId);
    const PortInstance* getInstance(const QString& instanceId) const;

    /**
     * @brief 打开串口
     */
    bool openPort(const QString& instanceId);

    /**
     * @brief 关闭串口
     */
    void closePort(const QString& instanceId);

    /**
     * @brief 关闭所有串口
     */
    void closeAllPorts();

    /**
     * @brief 发送数据
     */
    qint64 sendData(const QString& instanceId, const QByteArray& data);

    /**
     * @brief 发送数据到所有打开的串口
     */
    void sendToAll(const QByteArray& data);

    /**
     * @brief 设置串口参数
     */
    bool setPortConfig(const QString& instanceId,
                       qint32 baudRate,
                       QSerialPort::DataBits dataBits = QSerialPort::Data8,
                       QSerialPort::Parity parity = QSerialPort::NoParity,
                       QSerialPort::StopBits stopBits = QSerialPort::OneStop,
                       QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl);

    /**
     * @brief 设置别名
     */
    void setAlias(const QString& instanceId, const QString& alias);

    /**
     * @brief 获取活动串口数量
     */
    int openPortCount() const;

    /**
     * @brief 重置统计
     */
    void resetStatistics(const QString& instanceId);
    void resetAllStatistics();

signals:
    /**
     * @brief 串口已创建
     */
    void portCreated(const QString& instanceId);

    /**
     * @brief 串口已移除
     */
    void portRemoved(const QString& instanceId);

    /**
     * @brief 串口已打开
     */
    void portOpened(const QString& instanceId);

    /**
     * @brief 串口已关闭
     */
    void portClosed(const QString& instanceId);

    /**
     * @brief 接收到数据
     */
    void dataReceived(const QString& instanceId, const QByteArray& data);

    /**
     * @brief 数据已发送
     */
    void dataSent(const QString& instanceId, const QByteArray& data);

    /**
     * @brief 串口错误
     */
    void portError(const QString& instanceId, const QString& errorString);

    /**
     * @brief 统计更新
     */
    void statisticsUpdated(const QString& instanceId);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    MultiPortManager(QObject* parent = nullptr);
    ~MultiPortManager() override;

    QString generateInstanceId() const;

private:
    QMap<QString, PortInstance*> m_instances;
    mutable QMutex m_mutex;
    int m_instanceCounter = 0;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MULTIPORTMANAGER_H
