/**
 * @file SerialPortMonitor.h
 * @brief 串口热插拔监控器
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_SERIALPORTMONITOR_H
#define COMASSISTANT_SERIALPORTMONITOR_H

#include <QObject>
#include <QTimer>
#include <QSet>
#include <QSerialPortInfo>

namespace ComAssistant {

/**
 * @brief 串口热插拔监控器
 *
 * 监控系统串口变化，支持自动重连功能
 */
class SerialPortMonitor : public QObject {
    Q_OBJECT

public:
    static SerialPortMonitor* instance();

    /**
     * @brief 开始监控
     * @param intervalMs 检测间隔（毫秒）
     */
    void startMonitoring(int intervalMs = 1000);

    /**
     * @brief 停止监控
     */
    void stopMonitoring();

    /**
     * @brief 是否正在监控
     */
    bool isMonitoring() const { return m_timer && m_timer->isActive(); }

    /**
     * @brief 设置监控的目标串口
     * @param portName 串口名（如COM3）
     */
    void setTargetPort(const QString& portName);

    /**
     * @brief 获取目标串口
     */
    QString targetPort() const { return m_targetPort; }

    /**
     * @brief 设置自动重连
     */
    void setAutoReconnect(bool enabled) { m_autoReconnect = enabled; }

    /**
     * @brief 是否启用自动重连
     */
    bool autoReconnect() const { return m_autoReconnect; }

    /**
     * @brief 设置重连延迟
     */
    void setReconnectDelay(int ms) { m_reconnectDelay = ms; }

    /**
     * @brief 获取当前可用串口列表
     */
    QStringList availablePorts() const;

    /**
     * @brief 检查串口是否可用
     */
    bool isPortAvailable(const QString& portName) const;

signals:
    /**
     * @brief 串口列表变化
     */
    void portListChanged(const QStringList& ports);

    /**
     * @brief 串口断开
     */
    void portDisconnected(const QString& portName);

    /**
     * @brief 串口恢复（重新插入）
     */
    void portReconnected(const QString& portName);

    /**
     * @brief 新串口插入
     */
    void portAdded(const QString& portName);

    /**
     * @brief 串口移除
     */
    void portRemoved(const QString& portName);

    /**
     * @brief 请求自动重连
     */
    void requestReconnect(const QString& portName);

private slots:
    void checkPorts();
    void onReconnectTimer();

private:
    explicit SerialPortMonitor(QObject* parent = nullptr);
    ~SerialPortMonitor() override = default;

    static SerialPortMonitor* s_instance;

    QTimer* m_timer = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    QSet<QString> m_knownPorts;
    QString m_targetPort;
    bool m_autoReconnect = true;
    int m_reconnectDelay = 500;  // 重连延迟（毫秒）
    bool m_targetWasConnected = false;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SERIALPORTMONITOR_H
