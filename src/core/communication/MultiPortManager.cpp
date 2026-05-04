/**
 * @file MultiPortManager.cpp
 * @brief 多串口管理器实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "MultiPortManager.h"
#include "utils/Logger.h"
#include <QMutexLocker>
#include <QUuid>
#include <QVariant>

namespace ComAssistant {

MultiPortManager* MultiPortManager::instance()
{
    static MultiPortManager s_instance;
    return &s_instance;
}

MultiPortManager::MultiPortManager(QObject* parent)
    : QObject(parent)
{
}

MultiPortManager::~MultiPortManager()
{
    closeAllPorts();

    // 清理所有实例
    QMutexLocker locker(&m_mutex);
    qDeleteAll(m_instances);
    m_instances.clear();
}

QList<QSerialPortInfo> MultiPortManager::availablePorts() const
{
    return QSerialPortInfo::availablePorts();
}

QString MultiPortManager::generateInstanceId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString MultiPortManager::createPort(const QString& portName)
{
    QMutexLocker locker(&m_mutex);

    // 检查是否已有同名串口
    for (auto it = m_instances.constBegin(); it != m_instances.constEnd(); ++it) {
        if (it.value()->portName == portName && it.value()->isOpen) {
            LOG_WARN(QString("Port %1 is already in use").arg(portName));
            return QString();
        }
    }

    QString id = generateInstanceId();

    PortInstance* instance = new PortInstance();
    instance->id = id;
    instance->portName = portName;
    instance->alias = portName;
    instance->port = new QSerialPort(portName, this);

    // 连接信号
    connect(instance->port, &QSerialPort::readyRead,
            this, &MultiPortManager::onReadyRead);
    connect(instance->port, SIGNAL(error(QSerialPort::SerialPortError)),
            this, SLOT(onErrorOccurred(QSerialPort::SerialPortError)));

    // 存储实例ID到串口对象属性
    instance->port->setProperty("instanceId", id);

    m_instances[id] = instance;

    LOG_INFO(QString("Port instance created: %1 (%2)").arg(id, portName));

    locker.unlock();
    emit portCreated(id);

    return id;
}

bool MultiPortManager::removePort(const QString& instanceId)
{
    QMutexLocker locker(&m_mutex);

    auto it = m_instances.find(instanceId);
    if (it == m_instances.end()) {
        return false;
    }

    PortInstance* instance = it.value();

    // 先关闭串口
    if (instance->isOpen) {
        instance->port->close();
    }

    delete instance->port;
    delete instance;
    m_instances.erase(it);

    LOG_INFO(QString("Port instance removed: %1").arg(instanceId));

    locker.unlock();
    emit portRemoved(instanceId);

    return true;
}

QStringList MultiPortManager::instanceIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_instances.keys();
}

PortInstance* MultiPortManager::getInstance(const QString& instanceId)
{
    QMutexLocker locker(&m_mutex);
    return m_instances.value(instanceId, nullptr);
}

const PortInstance* MultiPortManager::getInstance(const QString& instanceId) const
{
    QMutexLocker locker(&m_mutex);
    return m_instances.value(instanceId, nullptr);
}

bool MultiPortManager::openPort(const QString& instanceId)
{
    QMutexLocker locker(&m_mutex);

    PortInstance* instance = m_instances.value(instanceId, nullptr);
    if (!instance) {
        LOG_ERROR(QString("Port instance not found: %1").arg(instanceId));
        return false;
    }

    if (instance->isOpen) {
        return true;
    }

    // 设置参数
    instance->port->setBaudRate(instance->baudRate);
    instance->port->setDataBits(instance->dataBits);
    instance->port->setParity(instance->parity);
    instance->port->setStopBits(instance->stopBits);
    instance->port->setFlowControl(instance->flowControl);

    if (!instance->port->open(QIODevice::ReadWrite)) {
        LOG_ERROR(QString("Failed to open port %1: %2")
            .arg(instance->portName, instance->port->errorString()));
        locker.unlock();
        emit portError(instanceId, instance->port->errorString());
        return false;
    }

    instance->isOpen = true;
    LOG_INFO(QString("Port opened: %1 (%2) @ %3")
        .arg(instanceId, instance->portName)
        .arg(instance->baudRate));

    locker.unlock();
    emit portOpened(instanceId);

    return true;
}

void MultiPortManager::closePort(const QString& instanceId)
{
    QMutexLocker locker(&m_mutex);

    PortInstance* instance = m_instances.value(instanceId, nullptr);
    if (!instance || !instance->isOpen) {
        return;
    }

    instance->port->close();
    instance->isOpen = false;

    LOG_INFO(QString("Port closed: %1 (%2)").arg(instanceId, instance->portName));

    locker.unlock();
    emit portClosed(instanceId);
}

void MultiPortManager::closeAllPorts()
{
    QMutexLocker locker(&m_mutex);
    QStringList ids = m_instances.keys();
    locker.unlock();

    for (const auto& id : ids) {
        closePort(id);
    }
}

qint64 MultiPortManager::sendData(const QString& instanceId, const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);

    PortInstance* instance = m_instances.value(instanceId, nullptr);
    if (!instance || !instance->isOpen) {
        return -1;
    }

    qint64 written = instance->port->write(data);
    if (written > 0) {
        instance->txBytes += written;
        instance->txCount++;

        locker.unlock();
        emit dataSent(instanceId, data);
        emit statisticsUpdated(instanceId);
    }

    return written;
}

void MultiPortManager::sendToAll(const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);
    QStringList ids = m_instances.keys();
    locker.unlock();

    for (const auto& id : ids) {
        QMutexLocker innerLocker(&m_mutex);
        PortInstance* instance = m_instances.value(id, nullptr);
        if (instance && instance->isOpen) {
            innerLocker.unlock();
            sendData(id, data);
        }
    }
}

bool MultiPortManager::setPortConfig(const QString& instanceId,
                                     qint32 baudRate,
                                     QSerialPort::DataBits dataBits,
                                     QSerialPort::Parity parity,
                                     QSerialPort::StopBits stopBits,
                                     QSerialPort::FlowControl flowControl)
{
    QMutexLocker locker(&m_mutex);

    PortInstance* instance = m_instances.value(instanceId, nullptr);
    if (!instance) {
        return false;
    }

    instance->baudRate = baudRate;
    instance->dataBits = dataBits;
    instance->parity = parity;
    instance->stopBits = stopBits;
    instance->flowControl = flowControl;

    // 如果已打开，则实时应用
    if (instance->isOpen) {
        instance->port->setBaudRate(baudRate);
        instance->port->setDataBits(dataBits);
        instance->port->setParity(parity);
        instance->port->setStopBits(stopBits);
        instance->port->setFlowControl(flowControl);
    }

    return true;
}

void MultiPortManager::setAlias(const QString& instanceId, const QString& alias)
{
    QMutexLocker locker(&m_mutex);

    PortInstance* instance = m_instances.value(instanceId, nullptr);
    if (instance) {
        instance->alias = alias;
    }
}

int MultiPortManager::openPortCount() const
{
    QMutexLocker locker(&m_mutex);

    int count = 0;
    for (auto it = m_instances.constBegin(); it != m_instances.constEnd(); ++it) {
        if (it.value()->isOpen) {
            count++;
        }
    }
    return count;
}

void MultiPortManager::resetStatistics(const QString& instanceId)
{
    QMutexLocker locker(&m_mutex);

    PortInstance* instance = m_instances.value(instanceId, nullptr);
    if (instance) {
        instance->txBytes = 0;
        instance->rxBytes = 0;
        instance->txCount = 0;
        instance->rxCount = 0;

        locker.unlock();
        emit statisticsUpdated(instanceId);
    }
}

void MultiPortManager::resetAllStatistics()
{
    QMutexLocker locker(&m_mutex);
    QStringList ids = m_instances.keys();
    locker.unlock();

    for (const auto& id : ids) {
        resetStatistics(id);
    }
}

void MultiPortManager::onReadyRead()
{
    QSerialPort* port = qobject_cast<QSerialPort*>(sender());
    if (!port) {
        return;
    }

    QString instanceId = port->property("instanceId").toString();

    QMutexLocker locker(&m_mutex);
    PortInstance* instance = m_instances.value(instanceId, nullptr);
    if (!instance) {
        return;
    }

    QByteArray data = port->readAll();
    if (!data.isEmpty()) {
        instance->rxBytes += data.size();
        instance->rxCount++;

        locker.unlock();
        emit dataReceived(instanceId, data);
        emit statisticsUpdated(instanceId);
    }
}

void MultiPortManager::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    QSerialPort* port = qobject_cast<QSerialPort*>(sender());
    if (!port) {
        return;
    }

    QString instanceId = port->property("instanceId").toString();
    QString errorString = port->errorString();

    LOG_ERROR(QString("Port error on %1: %2").arg(instanceId, errorString));
    emit portError(instanceId, errorString);

    // 资源错误通常意味着串口被拔出
    if (error == QSerialPort::ResourceError) {
        closePort(instanceId);
    }
}

} // namespace ComAssistant
