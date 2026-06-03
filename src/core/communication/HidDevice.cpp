/**
 * @file HidDevice.cpp
 * @brief USB HID 通信实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "HidDevice.h"
#include "HidReportCodec.h"
#include "utils/Logger.h"

#include <QMetaObject>
#include <QtGlobal>

#ifdef COMASSISTANT_ENABLE_HIDAPI
#include <hidapi/hidapi.h>
#endif

namespace ComAssistant {

namespace {

#ifdef COMASSISTANT_ENABLE_HIDAPI
/**
 * @brief 从 hidapi 返回的宽字符串构造 QString。
 * @param text hidapi 宽字符串指针，可为空。
 * @return 转换后的 QString。
 */
QString hidWideStringToQString(const wchar_t* text)
{
    return text ? QString::fromWCharArray(text) : QString();
}
#endif

} // namespace

HidDevice::HidDevice(const HidConfig& config, QObject* parent)
    : ICommunication(parent)
    , m_config(config)
{
    m_bufferSize = 65536;
    startWorkerThread();
}

HidDevice::~HidDevice()
{
    stopWorkerThread();
}

bool HidDevice::open()
{
    if (isOpen()) {
        return true;
    }

    if (m_config.path.isEmpty() && (m_config.vendorId == 0 || m_config.productId == 0)) {
        m_lastError = tr("HID device path or VID/PID is required.");
        emit errorOccurred(m_lastError);
        return false;
    }

    bool opened = false;
    QString errorMessage;
    invokeWorkerBlocking([&]() {
        opened = m_worker && m_worker->open(m_config, errorMessage);
    });
    if (!opened) {
        m_lastError = errorMessage.isEmpty()
            ? tr("Failed to open HID device: %1").arg(displayName())
            : errorMessage;
        emit errorOccurred(m_lastError);
        return false;
    }

    m_open = true;
    m_readBuffer.clear();
    m_readBuffer.squeeze();
    LOG_INFO(QString("HID device opened: %1").arg(displayName()));
    emit connectionStatusChanged(true);
    return true;
}

void HidDevice::close()
{
    /*
     * 先释放 readAll() 兼容缓存，再判断 worker/open 状态。这样即使设备
     * 没有成功打开、或测试/异常路径只积累了输入报告缓存，调用 close()
     * 仍能兑现“关闭后释放接收缓存”的语义。
     */
    m_readBuffer.clear();
    m_readBuffer.squeeze();

    if (!m_worker || !m_open) {
        return;
    }

    invokeWorkerBlocking([&]() {
        m_worker->close();
    });
    m_open = false;
    LOG_INFO(QString("HID device closed: %1").arg(displayName()));
    emit connectionStatusChanged(false);
}

bool HidDevice::isOpen() const
{
    return m_open;
}

qint64 HidDevice::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = tr("HID device is not open.");
        return -1;
    }

    const QByteArray report = buildOutputReport(data);
    int written = -1;
    QString errorMessage;
    invokeWorkerBlocking([&]() {
        written = m_worker ? m_worker->writeOutputReport(report, errorMessage) : -1;
    });

    if (written < 0) {
        m_lastError = errorMessage.isEmpty() ? tr("Failed to write HID report.") : errorMessage;
        emit errorOccurred(m_lastError);
        return -1;
    }

    /*
     * 对上层而言，发送成功的数据应是调用者提供的 payload，而不是为了
     * 满足 HID 报告格式补齐的 Report ID 或尾部填充字节。
     */
    emit dataSent(data);
    return data.size();
}

QByteArray HidDevice::readAll()
{
    const QByteArray data = m_readBuffer;
    m_readBuffer.clear();
    m_readBuffer.squeeze();
    return data;
}

qint64 HidDevice::bytesAvailable() const
{
    return m_readBuffer.size();
}

void HidDevice::setBufferSize(int size)
{
    m_bufferSize = qMax(0, size);
    trimReceiveBuffer(m_readBuffer);
}

int HidDevice::bufferSize() const
{
    return m_bufferSize;
}

void HidDevice::clearBuffer()
{
    m_readBuffer.clear();
    m_readBuffer.squeeze();
}

void HidDevice::setReadTimeout(int ms)
{
    m_readTimeout = qMax(0, ms);
}

int HidDevice::readTimeout() const
{
    return m_readTimeout;
}

void HidDevice::setWriteTimeout(int ms)
{
    m_writeTimeout = qMax(0, ms);
}

int HidDevice::writeTimeout() const
{
    return m_writeTimeout;
}

QString HidDevice::statusString() const
{
    if (!isOpen()) {
        return tr("Disconnected");
    }
    return tr("HID: %1").arg(displayName());
}

void HidDevice::setConfig(const HidConfig& config)
{
    m_config = config;
}

HidConfig HidDevice::config() const
{
    return m_config;
}

bool HidDevice::sendFeatureReport(const QByteArray& report)
{
    if (!isOpen()) {
        m_lastError = tr("HID device is not open.");
        return false;
    }

    bool sent = false;
    QString errorMessage;
    invokeWorkerBlocking([&]() {
        sent = m_worker && m_worker->sendFeatureReport(report, errorMessage);
    });
    if (!sent) {
        m_lastError = errorMessage.isEmpty()
            ? tr("Failed to send HID feature report.")
            : errorMessage;
        emit errorOccurred(m_lastError);
        return false;
    }
    return true;
}

QByteArray HidDevice::getFeatureReport(const QByteArray& requestReport)
{
    if (!isOpen()) {
        m_lastError = tr("HID device is not open.");
        return QByteArray();
    }

    QByteArray report = requestReport;
    if (report.isEmpty()) {
        report = QByteArray(qMax(1, m_config.featureReportLength), '\0');
    }

    QByteArray result;
    QString errorMessage;
    invokeWorkerBlocking([&]() {
        result = m_worker ? m_worker->getFeatureReport(report, errorMessage) : QByteArray();
    });
    if (result.isEmpty() && !errorMessage.isEmpty()) {
        m_lastError = errorMessage;
        emit errorOccurred(m_lastError);
        return QByteArray();
    }
    return result;
}

QList<HidDeviceInfo> HidDevice::availableDevices()
{
    QList<HidDeviceInfo> devices;

#ifdef COMASSISTANT_ENABLE_HIDAPI
    if (hid_init() != 0) {
        return devices;
    }

    hid_device_info* list = hid_enumerate(0x0, 0x0);
    for (hid_device_info* item = list; item != nullptr; item = item->next) {
        HidDeviceInfo info;
        info.path = QString::fromLocal8Bit(item->path ? item->path : "");
        info.vendorId = static_cast<quint16>(item->vendor_id);
        info.productId = static_cast<quint16>(item->product_id);
        info.interfaceNumber = static_cast<quint8>(qMax(0, item->interface_number));
        info.usagePage = static_cast<quint16>(item->usage_page);
        info.usage = static_cast<quint16>(item->usage);

        const QString manufacturer = hidWideStringToQString(item->manufacturer_string);
        const QString product = hidWideStringToQString(item->product_string);
        if (!manufacturer.isEmpty() || !product.isEmpty()) {
            info.name = QStringLiteral("%1 %2").arg(manufacturer, product).trimmed();
        }
        if (info.name.isEmpty()) {
            info.name = QStringLiteral("VID:%1 PID:%2")
                            .arg(info.vendorId, 4, 16, QLatin1Char('0'))
                            .arg(info.productId, 4, 16, QLatin1Char('0'))
                            .toUpper();
        }

        devices.append(info);
    }
    hid_free_enumeration(list);
#endif

    return devices;
}

bool HidDevice::backendAvailable()
{
#ifdef COMASSISTANT_ENABLE_HIDAPI
    return true;
#else
    return false;
#endif
}

QByteArray HidDevice::buildOutputReport(const QByteArray& data) const
{
    return HidReportCodec::buildOutputReport(m_config, data);
}

QByteArray HidDevice::normalizeInputReport(const QByteArray& report) const
{
    return HidReportCodec::normalizeInputReport(m_config, report);
}

QString HidDevice::displayName() const
{
    if (!m_config.name.isEmpty()) {
        return m_config.name;
    }
    if (m_config.vendorId != 0 || m_config.productId != 0) {
        return QStringLiteral("VID:%1 PID:%2")
            .arg(m_config.vendorId, 4, 16, QLatin1Char('0'))
            .arg(m_config.productId, 4, 16, QLatin1Char('0'))
            .toUpper();
    }
    return m_config.path.isEmpty() ? tr("Unselected HID device") : m_config.path;
}

void HidDevice::handleInputReport(const QByteArray& report)
{
    /*
     * worker 返回完整 Input Report，HidDevice 负责按用户配置移除 Report ID
     * 并维护 readAll() 缓冲，保持上层通信接口行为不变。
     */
    const QByteArray payload = normalizeInputReport(report);
    if (payload.isEmpty()) {
        return;
    }

    appendToReceiveBuffer(m_readBuffer, payload);
    emit dataReceived(payload);
}

void HidDevice::handleWorkerError(const QString& errorMessage)
{
    m_lastError = errorMessage;
    emit errorOccurred(errorMessage);
}

void HidDevice::startWorkerThread()
{
    /*
     * worker 没有设置 Qt 父对象，便于 moveToThread。线程结束时由
     * deleteLater 清理，HidDevice 只持有非拥有指针。
     */
    m_workerThread = new QThread(this);
    m_worker = new HidWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &HidWorker::inputReportReady,
            this, &HidDevice::handleInputReport, Qt::QueuedConnection);
    connect(m_worker, &HidWorker::errorOccurred,
            this, &HidDevice::handleWorkerError, Qt::QueuedConnection);

    m_workerThread->start();
}

void HidDevice::stopWorkerThread()
{
    if (!m_workerThread) {
        return;
    }

    if (m_worker && m_workerThread->isRunning()) {
        invokeWorkerBlocking([&]() {
            m_worker->close();
        });
    }
    m_open = false;
    m_readBuffer.clear();
    m_readBuffer.squeeze();

    m_workerThread->quit();
    m_workerThread->wait();
    delete m_workerThread;
    m_worker = nullptr;
    m_workerThread = nullptr;
}

void HidDevice::invokeWorkerBlocking(const std::function<void()>& operation) const
{
    if (!m_worker) {
        return;
    }

    if (QThread::currentThread() == m_workerThread) {
        operation();
        return;
    }

    QMetaObject::invokeMethod(
        m_worker,
        [operation]() {
            operation();
        },
        Qt::BlockingQueuedConnection);
}

} // namespace ComAssistant
