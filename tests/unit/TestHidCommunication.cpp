/**
 * @file TestHidCommunication.cpp
 * @brief HID 通信工厂与会话配置回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TestHidCommunication.h"

#include "communication/CommunicationFactory.h"
#include "communication/HidDevice.h"
#include "communication/HidReportCodec.h"
#include "communication/HidWorker.h"
#include "session/SessionData.h"

#include <QPointer>
#include <QThread>
#include <atomic>

using namespace ComAssistant;

namespace {

/**
 * @brief 测试用 HID 后端。
 *
 * 该后端不访问真实设备，只记录 Feature Report 的并发进入数量。若 worker
 * 没有把请求串行化，两个并发请求会让 maxConcurrent 大于 1。
 */
class FakeHidWorkerBackend : public HidWorkerBackend
{
public:
    bool open(const HidConfig& config, QString& errorMessage) override
    {
        Q_UNUSED(config)
        Q_UNUSED(errorMessage)
        opened = true;
        return true;
    }

    void close() override
    {
        opened = false;
    }

    bool isOpen() const override
    {
        return opened;
    }

    int writeOutputReport(const QByteArray& report, QString& errorMessage) override
    {
        Q_UNUSED(errorMessage)
        lastOutputReport = report;
        return report.size();
    }

    int sendFeatureReport(const QByteArray& report, QString& errorMessage) override
    {
        Q_UNUSED(errorMessage)
        enterOperation();
        lastFeatureReport = report;
        QThread::msleep(30);
        leaveOperation();
        return report.size();
    }

    QByteArray getFeatureReport(const QByteArray& requestReport, QString& errorMessage) override
    {
        Q_UNUSED(errorMessage)
        enterOperation();
        lastFeatureRequest = requestReport;
        QThread::msleep(30);
        leaveOperation();
        return requestReport;
    }

    QList<QByteArray> readInputReports(int maxReportLength, QString& errorMessage) override
    {
        Q_UNUSED(maxReportLength)
        Q_UNUSED(errorMessage)
        return {};
    }

    bool opened = false;                    ///< 是否处于打开状态
    QByteArray lastOutputReport;            ///< 最近一次 Output Report
    QByteArray lastFeatureReport;           ///< 最近一次 Feature Set 报告
    QByteArray lastFeatureRequest;          ///< 最近一次 Feature Get 请求
    std::atomic<int> activeOperations {0};   ///< 当前进入后端的 Feature 操作数
    std::atomic<int> maxConcurrent {0};      ///< 观测到的最大并发操作数

private:
    /**
     * @brief 记录一次 Feature 操作进入。
     *
     * 通过原子计数捕捉并发重入，避免测试本身依赖固定执行顺序。
     */
    void enterOperation()
    {
        const int active = activeOperations.fetch_add(1) + 1;
        int currentMax = maxConcurrent.load();
        while (active > currentMax &&
               !maxConcurrent.compare_exchange_weak(currentMax, active)) {
        }
    }

    /**
     * @brief 记录一次 Feature 操作离开。
     */
    void leaveOperation()
    {
        activeOperations.fetch_sub(1);
    }
};

} // namespace

void TestHidCommunication::testFactoryCreatesHidCommunication()
{
    HidConfig config;
    config.vendorId = 0x1234;
    config.productId = 0x5678;

    const QList<CommType> supportedTypes = CommunicationFactory::supportedTypes();
    QVERIFY2(supportedTypes.contains(CommType::Hid),
             "HID 必须出现在受支持通信类型列表中，避免 UI 与工厂能力不一致。");

    std::unique_ptr<ICommunication> communication =
        CommunicationFactory::create(CommType::Hid, SerialConfig(), NetworkConfig(), config);

    QVERIFY2(communication != nullptr,
             "HID 工厂必须返回通信实例，不能再返回 nullptr。");
    QCOMPARE(communication->type(), CommType::Hid);
    QCOMPARE(communication->typeName(), QStringLiteral("HID"));
}

void TestHidCommunication::testOpenWithoutDeviceIdentityFailsClearly()
{
    std::unique_ptr<ICommunication> communication =
        CommunicationFactory::create(CommType::Hid, SerialConfig(), NetworkConfig(), HidConfig());

    QVERIFY2(communication != nullptr,
             "即使没有选择设备，工厂也应返回对象，由 open() 给出明确错误。");
    QVERIFY2(!communication->open(),
             "没有 path 或 VID/PID 时不能打开 HID 设备。");
    QVERIFY2(communication->lastError().contains(QStringLiteral("HID")),
             qPrintable(QStringLiteral("错误信息应明确指出 HID 配置问题，实际为: %1")
                            .arg(communication->lastError())));
}

void TestHidCommunication::testSessionPersistsHidConfig()
{
    SessionData session;
    session.commType = CommType::Hid;
    session.hidConfig.name = QStringLiteral("Demo HID");
    session.hidConfig.path = QStringLiteral("hid-path-001");
    session.hidConfig.vendorId = 0x1234;
    session.hidConfig.productId = 0x5678;
    session.hidConfig.interfaceNumber = 2;
    session.hidConfig.usagePage = 0xFF00;
    session.hidConfig.usage = 0x0001;
    session.hidConfig.inputReportLength = 64;
    session.hidConfig.outputReportLength = 65;
    session.hidConfig.outReportId = 3;
    session.hidConfig.featureReportLength = 33;
    session.hidConfig.featureReportId = 7;
    session.hidConfig.firstDataIsLength = true;
    session.hidConfig.removeInReportId = true;

    const QJsonObject json = session.toJson();
    const SessionData restored = SessionData::fromJson(json);

    QCOMPARE(restored.commType, CommType::Hid);
    QCOMPARE(restored.hidConfig.name, session.hidConfig.name);
    QCOMPARE(restored.hidConfig.path, session.hidConfig.path);
    QCOMPARE(restored.hidConfig.vendorId, session.hidConfig.vendorId);
    QCOMPARE(restored.hidConfig.productId, session.hidConfig.productId);
    QCOMPARE(restored.hidConfig.interfaceNumber, session.hidConfig.interfaceNumber);
    QCOMPARE(restored.hidConfig.usagePage, session.hidConfig.usagePage);
    QCOMPARE(restored.hidConfig.usage, session.hidConfig.usage);
    QCOMPARE(restored.hidConfig.inputReportLength, session.hidConfig.inputReportLength);
    QCOMPARE(restored.hidConfig.outputReportLength, session.hidConfig.outputReportLength);
    QCOMPARE(restored.hidConfig.outReportId, session.hidConfig.outReportId);
    QCOMPARE(restored.hidConfig.featureReportLength, session.hidConfig.featureReportLength);
    QCOMPARE(restored.hidConfig.featureReportId, session.hidConfig.featureReportId);
    QCOMPARE(restored.hidConfig.firstDataIsLength, session.hidConfig.firstDataIsLength);
    QCOMPARE(restored.hidConfig.removeInReportId, session.hidConfig.removeInReportId);
}

void TestHidCommunication::testOutputReportCodecBuildsPaddedReport()
{
    HidConfig config;
    config.outputReportLength = 8;
    config.outReportId = 0x05;
    config.firstDataIsLength = true;

    const QByteArray report = HidReportCodec::buildOutputReport(config, QByteArray::fromHex("A1B2C3"));

    QCOMPARE(report.size(), 8);
    QCOMPARE(static_cast<quint8>(report.at(0)), static_cast<quint8>(0x05));
    QCOMPARE(static_cast<quint8>(report.at(1)), static_cast<quint8>(3));
    QCOMPARE(report.mid(2, 3), QByteArray::fromHex("A1B2C3"));
    QCOMPARE(report.mid(5), QByteArray::fromHex("000000"));
}

void TestHidCommunication::testInputReportCodecRemovesReportId()
{
    HidConfig config;
    config.removeInReportId = true;

    const QByteArray payload =
        HidReportCodec::normalizeInputReport(config, QByteArray::fromHex("02010203"));

    QCOMPARE(payload, QByteArray::fromHex("010203"));
}

void TestHidCommunication::testFeatureReportCodecUsesFeatureSettings()
{
    HidConfig config;
    config.featureReportLength = 6;
    config.featureReportId = 0x09;

    const QByteArray report =
        HidReportCodec::buildFeatureReport(config, QByteArray::fromHex("112233445566"));

    QCOMPARE(report.size(), 6);
    QCOMPARE(static_cast<quint8>(report.at(0)), static_cast<quint8>(0x09));
    QCOMPARE(report.mid(1), QByteArray::fromHex("1122334455"));
}

void TestHidCommunication::testHidDeviceOwnsAndStopsWorkerThread()
{
    /*
     * HID 的真实读写可能阻塞，通信对象必须把 worker 放到独立线程并在
     * 析构时关闭线程。该测试不打开设备，只验证线程生命周期归属。
     */
    QPointer<QThread> workerThread;
    {
        HidDevice device{HidConfig()};
        workerThread = device.workerThreadForTest();

        QVERIFY2(workerThread,
                 "HidDevice 应创建独立 worker 线程，不能继续在 UI 线程轮询 HID。");
        QVERIFY2(workerThread != QThread::currentThread(),
                 "HID worker 必须运行在独立线程。");
        QVERIFY2(workerThread->isRunning(),
                 "HID worker 线程创建后应处于运行状态，便于 open/write/read 串行投递。");
    }

    QVERIFY2(workerThread.isNull(),
             "HidDevice 析构后 worker 线程对象应被删除，避免关闭应用时留下线程。");
}

void TestHidCommunication::testHidWorkerSerializesConcurrentFeatureReports()
{
    /*
     * Feature Set/Get 共用同一个 HID 设备句柄，必须通过 worker 队列串行化。
     * 测试通过同时排入两个 Feature 操作，检查假后端观测不到并发进入。
     */
    HidConfig config;
    config.featureReportLength = 4;

    auto backend = std::make_unique<FakeHidWorkerBackend>();
    FakeHidWorkerBackend* backendPtr = backend.get();
    HidWorker worker(std::move(backend));

    QString error;
    QVERIFY(worker.open(config, error));

    QThread firstCaller;
    QThread secondCaller;
    bool firstResult = false;
    QByteArray secondResult;

    QObject firstTask;
    QObject secondTask;
    firstTask.moveToThread(&firstCaller);
    secondTask.moveToThread(&secondCaller);

    QObject::connect(&firstCaller, &QThread::started, &firstTask, [&]() {
        QString localError;
        firstResult = worker.sendFeatureReport(QByteArray::fromHex("01020304"), localError);
        firstCaller.quit();
    });
    QObject::connect(&secondCaller, &QThread::started, &secondTask, [&]() {
        QString localError;
        secondResult = worker.getFeatureReport(QByteArray::fromHex("05000000"), localError);
        secondCaller.quit();
    });

    firstCaller.start();
    secondCaller.start();
    QVERIFY(firstCaller.wait(1000));
    QVERIFY(secondCaller.wait(1000));

    QVERIFY(firstResult);
    QCOMPARE(secondResult, QByteArray::fromHex("05000000"));
    QCOMPARE(backendPtr->maxConcurrent.load(), 1);
    worker.close();
}
