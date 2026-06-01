/**
 * @file HidWorker.cpp
 * @brief HID 设备后台工作器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "HidWorker.h"

#include <QMutexLocker>
#include <QtGlobal>

#ifdef COMASSISTANT_ENABLE_HIDAPI
#include <hidapi/hidapi.h>
#endif

namespace ComAssistant {

namespace {

/**
 * @brief HID 轮询间隔，放在 worker 线程内执行，避免阻塞 UI。
 */
constexpr int kWorkerPollIntervalMs = 10;

/**
 * @brief HID 报告长度兜底值。
 */
constexpr int kDefaultReportLength = 64;

#ifdef COMASSISTANT_ENABLE_HIDAPI
/**
 * @brief 生产 hidapi 后端。
 *
 * 该类只在 worker 线程互斥保护下被调用，内部不再额外加锁。
 */
class HidApiWorkerBackend : public HidWorkerBackend
{
public:
    ~HidApiWorkerBackend() override
    {
        close();
    }

    bool open(const HidConfig& config, QString& errorMessage) override
    {
        if (m_device) {
            return true;
        }

        if (config.path.isEmpty() && (config.vendorId == 0 || config.productId == 0)) {
            errorMessage = QObject::tr("HID device path or VID/PID is required.");
            return false;
        }

        if (hid_init() != 0) {
            errorMessage = QObject::tr("Failed to initialize HID backend.");
            return false;
        }

        if (!config.path.isEmpty()) {
            const QByteArray pathBytes = config.path.toLocal8Bit();
            m_device = hid_open_path(pathBytes.constData());
        } else {
            m_device = hid_open(config.vendorId, config.productId, nullptr);
        }

        if (!m_device) {
            errorMessage = QObject::tr("Failed to open HID device.");
            return false;
        }

        hid_set_nonblocking(m_device, 1);
        return true;
    }

    void close() override
    {
        if (m_device) {
            hid_close(m_device);
            m_device = nullptr;
        }
    }

    bool isOpen() const override
    {
        return m_device != nullptr;
    }

    int writeOutputReport(const QByteArray& report, QString& errorMessage) override
    {
        if (!m_device) {
            errorMessage = QObject::tr("HID device is not open.");
            return -1;
        }

        const int written = hid_write(
            m_device,
            reinterpret_cast<const unsigned char*>(report.constData()),
            static_cast<size_t>(report.size()));

        if (written < 0) {
            errorMessage = QObject::tr("Failed to write HID report.");
        }
        return written;
    }

    int sendFeatureReport(const QByteArray& report, QString& errorMessage) override
    {
        if (!m_device) {
            errorMessage = QObject::tr("HID device is not open.");
            return -1;
        }

        const int written = hid_send_feature_report(
            m_device,
            reinterpret_cast<const unsigned char*>(report.constData()),
            static_cast<size_t>(report.size()));

        if (written < 0) {
            errorMessage = QObject::tr("Failed to send HID feature report.");
        }
        return written;
    }

    QByteArray getFeatureReport(const QByteArray& requestReport, QString& errorMessage) override
    {
        if (!m_device) {
            errorMessage = QObject::tr("HID device is not open.");
            return QByteArray();
        }

        QByteArray report = requestReport;
        if (report.isEmpty()) {
            report = QByteArray(1, '\0');
        }

        const int bytesRead = hid_get_feature_report(
            m_device,
            reinterpret_cast<unsigned char*>(report.data()),
            static_cast<size_t>(report.size()));

        if (bytesRead < 0) {
            errorMessage = QObject::tr("Failed to get HID feature report.");
            return QByteArray();
        }
        return report.left(bytesRead);
    }

    QList<QByteArray> readInputReports(int maxReportLength, QString& errorMessage) override
    {
        QList<QByteArray> reports;
        if (!m_device) {
            return reports;
        }

        QByteArray report(qMax(1, maxReportLength), Qt::Uninitialized);
        while (true) {
            const int bytesRead = hid_read_timeout(
                m_device,
                reinterpret_cast<unsigned char*>(report.data()),
                static_cast<size_t>(report.size()),
                0);

            if (bytesRead > 0) {
                reports.append(report.left(bytesRead));
                continue;
            }

            if (bytesRead < 0) {
                errorMessage = QObject::tr("Failed to read HID report.");
            }
            break;
        }

        return reports;
    }

private:
    hid_device* m_device = nullptr; ///< hidapi 设备句柄，仅在 worker 互斥区访问
};
#else
/**
 * @brief 未启用 hidapi 时的占位后端。
 *
 * 该后端保留清晰错误，保证无 hidapi 构建仍能运行并解释能力缺失。
 */
class DisabledHidWorkerBackend : public HidWorkerBackend
{
public:
    bool open(const HidConfig& config, QString& errorMessage) override
    {
        Q_UNUSED(config)
        errorMessage = QObject::tr("HID backend is not enabled in this build.");
        return false;
    }

    void close() override {}
    bool isOpen() const override { return false; }

    int writeOutputReport(const QByteArray& report, QString& errorMessage) override
    {
        Q_UNUSED(report)
        errorMessage = QObject::tr("HID backend is not enabled in this build.");
        return -1;
    }

    int sendFeatureReport(const QByteArray& report, QString& errorMessage) override
    {
        Q_UNUSED(report)
        errorMessage = QObject::tr("HID backend is not enabled in this build.");
        return -1;
    }

    QByteArray getFeatureReport(const QByteArray& requestReport, QString& errorMessage) override
    {
        Q_UNUSED(requestReport)
        errorMessage = QObject::tr("HID backend is not enabled in this build.");
        return QByteArray();
    }

    QList<QByteArray> readInputReports(int maxReportLength, QString& errorMessage) override
    {
        Q_UNUSED(maxReportLength)
        Q_UNUSED(errorMessage)
        return {};
    }
};
#endif

} // namespace

HidWorker::HidWorker(std::unique_ptr<HidWorkerBackend> backend, QObject* parent)
    : QObject(parent)
    , m_backend(backend ? std::move(backend) : createDefaultBackend())
{
    /*
     * 定时器属于 worker 对象。HidDevice 会先把 worker 移动到后台线程，
     * 再调用 open()，因此轮询回调也会在后台线程执行。
     */
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kWorkerPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &HidWorker::pollInputReports);
}

HidWorker::~HidWorker()
{
    close();
}

bool HidWorker::open(const HidConfig& config, QString& errorMessage)
{
    QMutexLocker locker(&m_mutex);
    if (m_backend->isOpen()) {
        return true;
    }

    if (!m_backend->open(config, errorMessage)) {
        return false;
    }

    m_config = config;
    if (m_pollTimer) {
        m_pollTimer->start();
    }
    return true;
}

void HidWorker::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    m_backend->close();
}

bool HidWorker::isOpen() const
{
    QMutexLocker locker(&m_mutex);
    return m_backend->isOpen();
}

int HidWorker::writeOutputReport(const QByteArray& report, QString& errorMessage)
{
    QMutexLocker locker(&m_mutex);
    return m_backend->writeOutputReport(report, errorMessage);
}

bool HidWorker::sendFeatureReport(const QByteArray& report, QString& errorMessage)
{
    QMutexLocker locker(&m_mutex);
    return m_backend->sendFeatureReport(report, errorMessage) >= 0;
}

QByteArray HidWorker::getFeatureReport(const QByteArray& requestReport, QString& errorMessage)
{
    QMutexLocker locker(&m_mutex);
    return m_backend->getFeatureReport(requestReport, errorMessage);
}

void HidWorker::pollInputReports()
{
    QList<QByteArray> reports;
    QString errorMessage;
    {
        /*
         * 轮询也进入同一把锁，确保 read、write、Feature Report 不会同时
         * 访问 hidapi 句柄。读取使用非阻塞模式，因此锁持有时间很短。
         */
        QMutexLocker locker(&m_mutex);
        if (!m_backend->isOpen()) {
            return;
        }

        const int reportLength = qMax(kDefaultReportLength, m_config.inputReportLength);
        reports = m_backend->readInputReports(reportLength, errorMessage);
    }

    for (const QByteArray& report : reports) {
        emit inputReportReady(report);
    }
    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }
}

std::unique_ptr<HidWorkerBackend> HidWorker::createDefaultBackend()
{
#ifdef COMASSISTANT_ENABLE_HIDAPI
    return std::make_unique<HidApiWorkerBackend>();
#else
    return std::make_unique<DisabledHidWorkerBackend>();
#endif
}

} // namespace ComAssistant
