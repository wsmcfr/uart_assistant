/**
 * @file SerialPortMonitor.cpp
 * @brief 串口热插拔监控器实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "SerialPortMonitor.h"
#include "utils/Logger.h"

namespace ComAssistant {

SerialPortMonitor* SerialPortMonitor::s_instance = nullptr;

SerialPortMonitor* SerialPortMonitor::instance()
{
    if (!s_instance) {
        s_instance = new SerialPortMonitor();
    }
    return s_instance;
}

SerialPortMonitor::SerialPortMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SerialPortMonitor::checkPorts);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &SerialPortMonitor::onReconnectTimer);

    // 初始化已知串口列表
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        m_knownPorts.insert(info.portName());
    }
}

void SerialPortMonitor::startMonitoring(int intervalMs)
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    m_timer->start(intervalMs);
    LOG_INFO(QString("Serial port monitor started with interval %1ms").arg(intervalMs));
}

void SerialPortMonitor::stopMonitoring()
{
    m_timer->stop();
    m_reconnectTimer->stop();
    LOG_INFO("Serial port monitor stopped");
}

void SerialPortMonitor::setTargetPort(const QString& portName)
{
    m_targetPort = portName;
    m_targetWasConnected = isPortAvailable(portName);
    LOG_DEBUG(QString("Target port set to %1, available: %2")
        .arg(portName).arg(m_targetWasConnected));
}

QStringList SerialPortMonitor::availablePorts() const
{
    QStringList ports;
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        ports.append(info.portName());
    }
    ports.sort();
    return ports;
}

bool SerialPortMonitor::isPortAvailable(const QString& portName) const
{
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        if (info.portName() == portName) {
            return true;
        }
    }
    return false;
}

void SerialPortMonitor::checkPorts()
{
    QSet<QString> currentPorts;
    for (const auto& info : QSerialPortInfo::availablePorts()) {
        currentPorts.insert(info.portName());
    }

    // 检查新增的串口
    QSet<QString> addedPorts = currentPorts - m_knownPorts;
    for (const QString& port : addedPorts) {
        LOG_INFO(QString("Serial port added: %1").arg(port));
        emit portAdded(port);

        // 如果是目标串口恢复
        if (port == m_targetPort && !m_targetWasConnected) {
            LOG_INFO(QString("Target port %1 reconnected").arg(port));
            emit portReconnected(port);

            if (m_autoReconnect) {
                m_reconnectTimer->start(m_reconnectDelay);
            }
        }
    }

    // 检查移除的串口
    QSet<QString> removedPorts = m_knownPorts - currentPorts;
    for (const QString& port : removedPorts) {
        LOG_INFO(QString("Serial port removed: %1").arg(port));
        emit portRemoved(port);

        // 如果是目标串口断开
        if (port == m_targetPort && m_targetWasConnected) {
            LOG_WARN(QString("Target port %1 disconnected").arg(port));
            emit portDisconnected(port);
        }
    }

    // 如果有变化，发送列表更新信号
    if (!addedPorts.isEmpty() || !removedPorts.isEmpty()) {
        QStringList portList = currentPorts.values();
        portList.sort();
        emit portListChanged(portList);
    }

    // 更新状态
    m_knownPorts = currentPorts;
    m_targetWasConnected = currentPorts.contains(m_targetPort);
}

void SerialPortMonitor::onReconnectTimer()
{
    if (!m_targetPort.isEmpty() && isPortAvailable(m_targetPort)) {
        LOG_INFO(QString("Requesting auto-reconnect for %1").arg(m_targetPort));
        emit requestReconnect(m_targetPort);
    }
}

} // namespace ComAssistant
