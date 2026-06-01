/**
 * @file HidDevice.cpp
 * @brief USB HID 通信实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "HidDevice.h"
#include "HidReportCodec.h"
#include "utils/Logger.h"

#include <QtGlobal>

namespace ComAssistant {

namespace {

/**
 * @brief HID 轮询间隔，兼顾实时性和 UI 线程负载。
 */
constexpr int kPollIntervalMs = 10;

/**
 * @brief HID 报告长度兜底值。
 */
constexpr int kDefaultReportLength = 64;

} // namespace

HidDevice::HidDevice(const HidConfig& config, QObject* parent)
    : ICommunication(parent)
    , m_config(config)
{
    m_bufferSize = 65536;
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &HidDevice::pollInputReport);
}

HidDevice::~HidDevice()
{
    close();
}

bool HidDevice::open()
{
    if (isOpen()) {
        return true;
    }

#ifndef COMASSISTANT_ENABLE_HIDAPI
    m_lastError = tr("HID backend is not enabled in this build.");
    emit errorOccurred(m_lastError);
    return false;
#else
    if (m_config.path.isEmpty() && (m_config.vendorId == 0 || m_config.productId == 0)) {
        m_lastError = tr("HID device path or VID/PID is required.");
        emit errorOccurred(m_lastError);
        return false;
    }

    if (hid_init() != 0) {
        m_lastError = tr("Failed to initialize HID backend.");
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!m_config.path.isEmpty()) {
        const QByteArray pathBytes = m_config.path.toLocal8Bit();
        m_device = hid_open_path(pathBytes.constData());
    } else {
        m_device = hid_open(m_config.vendorId, m_config.productId, nullptr);
    }

    if (!m_device) {
        m_lastError = tr("Failed to open HID device: %1")
                          .arg(displayName());
        emit errorOccurred(m_lastError);
        return false;
    }

    hid_set_nonblocking(m_device, 1);
    m_readBuffer.clear();
    m_pollTimer->start();

    LOG_INFO(QString("HID device opened: %1").arg(displayName()));
    emit connectionStatusChanged(true);
    return true;
#endif
}

void HidDevice::close()
{
#ifdef COMASSISTANT_ENABLE_HIDAPI
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    if (m_device) {
        hid_close(m_device);
        m_device = nullptr;
        LOG_INFO(QString("HID device closed: %1").arg(displayName()));
        emit connectionStatusChanged(false);
    }
#else
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
#endif
}

bool HidDevice::isOpen() const
{
#ifdef COMASSISTANT_ENABLE_HIDAPI
    return m_device != nullptr;
#else
    return false;
#endif
}

qint64 HidDevice::write(const QByteArray& data)
{
    if (!isOpen()) {
        m_lastError = tr("HID device is not open.");
        return -1;
    }

#ifndef COMASSISTANT_ENABLE_HIDAPI
    Q_UNUSED(data)
    return -1;
#else
    const QByteArray report = buildOutputReport(data);
    const int written = hid_write(
        m_device,
        reinterpret_cast<const unsigned char*>(report.constData()),
        static_cast<size_t>(report.size()));

    if (written < 0) {
        m_lastError = tr("Failed to write HID report.");
        emit errorOccurred(m_lastError);
        return -1;
    }

    /*
     * 对上层而言，发送成功的数据应是调用者提供的 payload，而不是为了
     * 满足 HID 报告格式补齐的 Report ID 或尾部填充字节。
     */
    emit dataSent(data);
    return data.size();
#endif
}

QByteArray HidDevice::readAll()
{
    const QByteArray data = m_readBuffer;
    m_readBuffer.clear();
    return data;
}

qint64 HidDevice::bytesAvailable() const
{
    return m_readBuffer.size();
}

void HidDevice::setBufferSize(int size)
{
    m_bufferSize = qMax(0, size);
    if (m_bufferSize > 0 && m_readBuffer.size() > m_bufferSize) {
        m_readBuffer = m_readBuffer.right(m_bufferSize);
    }
}

int HidDevice::bufferSize() const
{
    return m_bufferSize;
}

void HidDevice::clearBuffer()
{
    m_readBuffer.clear();
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

        const QString manufacturer = fromWideString(item->manufacturer_string);
        const QString product = fromWideString(item->product_string);
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

void HidDevice::pollInputReport()
{
#ifdef COMASSISTANT_ENABLE_HIDAPI
    if (!m_device) {
        return;
    }

    const int reportLength = qMax(kDefaultReportLength, m_config.inputReportLength);
    QByteArray report(reportLength, Qt::Uninitialized);

    while (true) {
        const int bytesRead = hid_read_timeout(
            m_device,
            reinterpret_cast<unsigned char*>(report.data()),
            static_cast<size_t>(report.size()),
            0);

        if (bytesRead > 0) {
            const QByteArray payload = normalizeInputReport(report.left(bytesRead));
            if (!payload.isEmpty()) {
                m_readBuffer.append(payload);
                if (m_bufferSize > 0 && m_readBuffer.size() > m_bufferSize) {
                    m_readBuffer = m_readBuffer.right(m_bufferSize);
                }
                emit dataReceived(payload);
            }
            continue;
        }

        if (bytesRead < 0) {
            m_lastError = tr("Failed to read HID report.");
            emit errorOccurred(m_lastError);
        }
        break;
    }
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

#ifdef COMASSISTANT_ENABLE_HIDAPI
QString HidDevice::fromWideString(const wchar_t* text)
{
    return text ? QString::fromWCharArray(text) : QString();
}
#endif

} // namespace ComAssistant
