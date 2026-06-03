/**
 * @file TestMainWindowCommunicationController.cpp
 * @brief 主窗口通信控制器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMainWindowCommunicationController.h"

#include "ui/MainWindowCommunicationController.h"

#include <QSignalSpy>

#include <memory>

using namespace ComAssistant;

namespace {

/**
 * @brief 用于测试的假通信对象。
 *
 * 该对象不访问真实串口、网络或 HID 设备，只记录 open/close/write 调用，
 * 让控制器测试可以稳定验证生命周期和发送行为。
 */
class FakeCommunication : public ICommunication
{
    Q_OBJECT

public:
    /**
     * @brief 构造假通信对象。
     * @param openResult open() 需要返回的结果。
     * @param errorText open() 失败时暴露给控制器的错误文本。
     * @param externalCloseCallCount 可选外部关闭计数器，用于对象释放后断言 close() 调用。
     */
    explicit FakeCommunication(bool openResult = true,
                               const QString& errorText = QString(),
                               int* externalCloseCallCount = nullptr,
                               QObject* parent = nullptr)
        : ICommunication(parent)
        , m_openResult(openResult)
        , m_externalCloseCallCount(externalCloseCallCount)
    {
        m_lastError = errorText;
    }

    /**
     * @brief 模拟打开连接。
     * @return 构造时指定的打开结果。
     */
    bool open() override
    {
        ++openCallCount;
        m_open = m_openResult;
        emit connectionStatusChanged(m_open);
        return m_openResult;
    }

    /**
     * @brief 模拟关闭连接。
     */
    void close() override
    {
        ++closeCallCount;
        if (m_externalCloseCallCount) {
            ++(*m_externalCloseCallCount);
        }
        m_open = false;
        emit connectionStatusChanged(false);
    }

    /**
     * @brief 返回假通信对象当前打开状态。
     */
    bool isOpen() const override
    {
        return m_open;
    }

    /**
     * @brief 记录发送数据。
     * @param data 待发送数据。
     * @return 写入字节数。
     */
    qint64 write(const QByteArray& data) override
    {
        ++writeCallCount;
        lastWrittenData = data;
        if (failNextWrite) {
            failNextWrite = false;
            m_lastError = writeErrorText.isEmpty()
                ? QStringLiteral("write failed")
                : writeErrorText;
            return -1;
        }
        emit dataSent(data);
        return data.size();
    }

    /**
     * @brief 模拟异步等待发送缓冲排空。
     * @param bytes 本次需要等待硬件发空的字节数。
     * @return true 表示已接受 drain 请求。
     *
     * 文件传输测试用该函数区分“write 已经收下数据”和“串口真正发完”。
     * autoCompleteDrain 为 false 时，测试可手动调用 completePendingDrain()
     * 触发最终结果，从而验证控制器不会提前回调成功。
     */
    bool waitForTransmitDrainedAsync(qint64 bytes) override
    {
        ++drainRequestCount;
        lastDrainBytes = bytes;
        if (failNextDrainRequest) {
            failNextDrainRequest = false;
            m_lastError = drainErrorText.isEmpty()
                ? QStringLiteral("drain request failed")
                : drainErrorText;
            return false;
        }

        if (autoCompleteDrain) {
            emit transmitDrained(drainSuccess, drainErrorText);
        } else {
            pendingDrain = true;
        }
        return true;
    }

    /**
     * @brief 手动完成一次挂起的 drain 请求。
     * @param success drain 是否成功。
     * @param errorText 失败原因。
     */
    void completePendingDrain(bool success, const QString& errorText = QString())
    {
        pendingDrain = false;
        emit transmitDrained(success, errorText);
    }

    /**
     * @brief 模拟读取全部缓存数据。
     * @return 控制器测试不覆盖读取路径，因此始终返回空数据。
     */
    QByteArray readAll() override { return QByteArray(); }

    /**
     * @brief 模拟可读字节数。
     * @return 控制器测试不依赖接收缓存，因此始终返回 0。
     */
    qint64 bytesAvailable() const override { return 0; }

    /**
     * @brief 记录控制器设置的缓冲区大小。
     * @param size 新缓冲区大小。
     */
    void setBufferSize(int size) override { m_bufferSize = size; }

    /**
     * @brief 返回当前记录的缓冲区大小。
     * @return 最近一次 setBufferSize() 写入的值。
     */
    int bufferSize() const override { return m_bufferSize; }

    /**
     * @brief 模拟清空缓冲区。
     *
     * 假对象不维护真实接收缓存，因此这里保持空实现。
     */
    void clearBuffer() override {}

    /**
     * @brief 记录读取超时时间。
     * @param ms 读取超时，单位毫秒。
     */
    void setReadTimeout(int ms) override { m_readTimeout = ms; }

    /**
     * @brief 返回读取超时时间。
     * @return 最近一次 setReadTimeout() 写入的值。
     */
    int readTimeout() const override { return m_readTimeout; }

    /**
     * @brief 记录写入超时时间。
     * @param ms 写入超时，单位毫秒。
     */
    void setWriteTimeout(int ms) override { m_writeTimeout = ms; }

    /**
     * @brief 返回写入超时时间。
     * @return 最近一次 setWriteTimeout() 写入的值。
     */
    int writeTimeout() const override { return m_writeTimeout; }

    /**
     * @brief 返回假通信对象的类型。
     * @return 使用串口类型即可覆盖控制器通用生命周期路径。
     */
    CommType type() const override { return CommType::Serial; }

    /**
     * @brief 返回假通信对象类型名称。
     * @return 固定测试名称。
     */
    QString typeName() const override { return QStringLiteral("Fake"); }

    /**
     * @brief 返回假通信对象状态文本。
     * @return 固定测试状态文本。
     */
    QString statusString() const override { return QStringLiteral("Fake status"); }

    /**
     * @brief 返回最近一次模拟错误。
     * @return 构造函数注入或测试过程中设置的错误文本。
     */
    QString lastError() const override { return m_lastError; }

    bool m_open = false;           ///< 当前模拟打开状态。
    bool m_openResult = true;      ///< open() 返回值。
    int openCallCount = 0;         ///< open() 调用次数。
    int closeCallCount = 0;        ///< close() 调用次数。
    int writeCallCount = 0;        ///< write() 调用次数。
    int drainRequestCount = 0;     ///< 异步 drain 请求次数。
    QByteArray lastWrittenData;    ///< 最近一次写入数据。
    qint64 lastDrainBytes = 0;     ///< 最近一次 drain 等待的字节数。
    bool failNextWrite = false;    ///< 下一次 write() 是否模拟失败。
    QString writeErrorText;        ///< 模拟写入失败时返回的错误文本。
    bool autoCompleteDrain = true; ///< drain 请求是否立即发出完成信号。
    bool pendingDrain = false;     ///< 是否存在待手动完成的 drain 请求。
    bool drainSuccess = true;      ///< 自动 drain 完成时使用的结果。
    bool failNextDrainRequest = false; ///< 下一次 drain 请求是否直接失败。
    QString drainErrorText;        ///< 模拟 drain 失败时返回的错误文本。
    int* m_externalCloseCallCount = nullptr; ///< 对象释放后仍可读取的关闭调用计数器。
};

/**
 * @brief 保存假通信对象和所有权指针。
 */
struct FakeCommunicationHandle
{
    FakeCommunication* raw = nullptr;               ///< 测试断言使用的裸指针。
    std::unique_ptr<ICommunication> owned;          ///< 交给控制器接管的所有权。
};

/**
 * @brief 创建假通信对象句柄。
 * @param openResult open() 返回结果。
 * @param errorText 失败错误文本。
 * @param externalCloseCallCount 可选外部关闭计数器。
 * @return 包含裸指针和 unique_ptr 的句柄。
 */
FakeCommunicationHandle makeFakeCommunication(bool openResult = true,
                                              const QString& errorText = QString(),
                                              int* externalCloseCallCount = nullptr)
{
    FakeCommunicationHandle handle;
    auto fake = std::make_unique<FakeCommunication>(openResult,
                                                    errorText,
                                                    externalCloseCallCount);
    handle.raw = fake.get();
    handle.owned = std::move(fake);
    return handle;
}

} // namespace

void TestMainWindowCommunicationController::testSendDataIsRejectedWhenDisconnected()
{
    MainWindowCommunicationController controller;

    QVERIFY(!controller.sendData(QByteArray("ping")));
    QVERIFY(!controller.isConnected());
    QVERIFY(controller.lastError().contains(QStringLiteral("未连接")));
}

void TestMainWindowCommunicationController::testOpenSuccessAllowsSendingData()
{
    FakeCommunication* fake = nullptr;
    MainWindowCommunicationController controller;
    controller.setCommunicationFactory([&fake](CommType,
                                               const SerialConfig&,
                                               const NetworkConfig&,
                                               const HidConfig&) {
        FakeCommunicationHandle handle = makeFakeCommunication(true);
        fake = handle.raw;
        return std::move(handle.owned);
    });

    QSignalSpy statusSpy(&controller, SIGNAL(connectionStatusChanged(bool)));

    QVERIFY(controller.openCurrent(CommType::Serial,
                                  SerialConfig(),
                                  NetworkConfig(),
                                  HidConfig()));
    QVERIFY(controller.isConnected());
    QCOMPARE(statusSpy.count(), 1);
    QCOMPARE(statusSpy.takeFirst().at(0).toBool(), true);

    QVERIFY(controller.sendData(QByteArray("hello")));
    QVERIFY(fake != nullptr);
    QCOMPARE(fake->writeCallCount, 1);
    QCOMPARE(fake->lastWrittenData, QByteArray("hello"));
}

void TestMainWindowCommunicationController::testFailedSendIsKeptForRetry()
{
    /*
     * 第三阶段要求写入失败时不要丢弃队首数据。这里先让假通信对象失败
     * 一次，再触发控制器重试，验证同一个 payload 会重新写入。
     */
    FakeCommunication* fake = nullptr;
    MainWindowCommunicationController controller;
    controller.setCommunicationFactory([&fake](CommType,
                                               const SerialConfig&,
                                               const NetworkConfig&,
                                               const HidConfig&) {
        FakeCommunicationHandle handle = makeFakeCommunication(true);
        fake = handle.raw;
        return std::move(handle.owned);
    });

    QVERIFY(controller.openCurrent(CommType::Serial,
                                  SerialConfig(),
                                  NetworkConfig(),
                                  HidConfig()));
    QVERIFY(fake != nullptr);

    fake->failNextWrite = true;
    fake->writeErrorText = QStringLiteral("temporary write failure");

    QVERIFY(!controller.sendData(QByteArray("retry-me")));
    QCOMPARE(fake->writeCallCount, 1);
    QCOMPARE(controller.pendingSendCount(), 1);
    QVERIFY(controller.lastError().contains(QStringLiteral("temporary write failure")));

    QVERIFY(controller.retryPendingSends());
    QCOMPARE(fake->writeCallCount, 2);
    QCOMPARE(fake->lastWrittenData, QByteArray("retry-me"));
    QCOMPARE(controller.pendingSendCount(), 0);
}

void TestMainWindowCommunicationController::testCloseCurrentCancelsPendingSends()
{
    /*
     * 用户断开连接时，旧连接中排队但尚未成功写出的数据不能泄漏到下次
     * 连接。通过一次失败制造待发送任务，再关闭连接验证队列被取消。
     */
    FakeCommunication* fake = nullptr;
    MainWindowCommunicationController controller;
    controller.setCommunicationFactory([&fake](CommType,
                                               const SerialConfig&,
                                               const NetworkConfig&,
                                               const HidConfig&) {
        FakeCommunicationHandle handle = makeFakeCommunication(true);
        fake = handle.raw;
        return std::move(handle.owned);
    });

    QVERIFY(controller.openCurrent(CommType::Serial,
                                  SerialConfig(),
                                  NetworkConfig(),
                                  HidConfig()));
    QVERIFY(fake != nullptr);

    fake->failNextWrite = true;
    QVERIFY(!controller.sendData(QByteArray("old-connection-data")));
    QCOMPARE(controller.pendingSendCount(), 1);

    controller.closeCurrent();

    QCOMPARE(controller.pendingSendCount(), 0);
    QVERIFY(!controller.isConnected());
}

void TestMainWindowCommunicationController::testOpenFailureKeepsDisconnectedStateAndError()
{
    MainWindowCommunicationController controller;
    controller.setCommunicationFactory([](CommType,
                                          const SerialConfig&,
                                          const NetworkConfig&,
                                          const HidConfig&) {
        return makeFakeCommunication(false, QStringLiteral("open failed")).owned;
    });

    QVERIFY(!controller.openCurrent(CommType::Serial,
                                   SerialConfig(),
                                   NetworkConfig(),
                                   HidConfig()));
    QVERIFY(!controller.isConnected());
    QVERIFY(controller.lastError().contains(QStringLiteral("open failed")));
}

void TestMainWindowCommunicationController::testCloseCurrentClosesCommunicationAndClearsState()
{
    FakeCommunication* fake = nullptr;
    int closeCallCount = 0;
    MainWindowCommunicationController controller;
    controller.setCommunicationFactory([&fake, &closeCallCount](CommType,
                                                                const SerialConfig&,
                                                                const NetworkConfig&,
                                                                const HidConfig&) {
        FakeCommunicationHandle handle = makeFakeCommunication(true,
                                                               QString(),
                                                               &closeCallCount);
        fake = handle.raw;
        return std::move(handle.owned);
    });

    QVERIFY(controller.openCurrent(CommType::Serial,
                                  SerialConfig(),
                                  NetworkConfig(),
                                  HidConfig()));
    QVERIFY(fake != nullptr);

    controller.closeCurrent();

    QVERIFY(!controller.isConnected());
    QCOMPARE(closeCallCount, 1);
    QVERIFY(controller.communication() == nullptr);
}

void TestMainWindowCommunicationController::testFileTransferSendWaitsForAsyncDrainBeforeSuccessCallback()
{
    /*
     * 用户看到“文件发送完成”时，CH340 和 MCU 不应还在继续接收同一文件。
     * 该回归测试把 write() 成功和串口 drain 完成拆开，确保控制器只在
     * drain 信号到达后才把成功结果回调给文件传输状态机。
     */
    FakeCommunication* fake = nullptr;
    MainWindowCommunicationController controller;
    controller.setCommunicationFactory([&fake](CommType,
                                               const SerialConfig&,
                                               const NetworkConfig&,
                                               const HidConfig&) {
        FakeCommunicationHandle handle = makeFakeCommunication(true);
        fake = handle.raw;
        fake->autoCompleteDrain = false;
        return std::move(handle.owned);
    });

    QVERIFY(controller.openCurrent(CommType::Serial,
                                  SerialConfig(),
                                  NetworkConfig(),
                                  HidConfig()));
    QVERIFY(fake != nullptr);

    int callbackCount = 0;
    bool callbackSuccess = false;
    QString callbackError;
    QVERIFY(controller.sendFileTransferDataAsync(
        QByteArray("firmware"),
        [&callbackCount, &callbackSuccess, &callbackError](bool success, const QString& error) {
            ++callbackCount;
            callbackSuccess = success;
            callbackError = error;
        }));

    QCOMPARE(fake->writeCallCount, 1);
    QCOMPARE(fake->drainRequestCount, 1);
    QCOMPARE(fake->lastDrainBytes, static_cast<qint64>(8));
    QCOMPARE(callbackCount, 0);
    QVERIFY(fake->pendingDrain);

    fake->completePendingDrain(true);

    QCOMPARE(callbackCount, 1);
    QVERIFY(callbackSuccess);
    QVERIFY(callbackError.isEmpty());
}

void TestMainWindowCommunicationController::testFileTransferSendReportsDrainFailure()
{
    /*
     * drain 超时意味着数据可能仍停在系统或 USB-串口驱动缓冲里。文件传输
     * 必须把它当成本地发送失败，避免继续推进下一块或显示完成。
     */
    FakeCommunication* fake = nullptr;
    MainWindowCommunicationController controller;
    controller.setCommunicationFactory([&fake](CommType,
                                               const SerialConfig&,
                                               const NetworkConfig&,
                                               const HidConfig&) {
        FakeCommunicationHandle handle = makeFakeCommunication(true);
        fake = handle.raw;
        fake->autoCompleteDrain = false;
        return std::move(handle.owned);
    });

    QVERIFY(controller.openCurrent(CommType::Serial,
                                  SerialConfig(),
                                  NetworkConfig(),
                                  HidConfig()));
    QVERIFY(fake != nullptr);

    int callbackCount = 0;
    bool callbackSuccess = true;
    QString callbackError;
    QVERIFY(controller.sendFileTransferDataAsync(
        QByteArray("block"),
        [&callbackCount, &callbackSuccess, &callbackError](bool success, const QString& error) {
            ++callbackCount;
            callbackSuccess = success;
            callbackError = error;
        }));

    fake->completePendingDrain(false, QStringLiteral("串口发送缓冲等待发空超时"));

    QCOMPARE(callbackCount, 1);
    QVERIFY(!callbackSuccess);
    QVERIFY(callbackError.contains(QStringLiteral("发空超时")));
    QVERIFY(controller.lastError().contains(QStringLiteral("发空超时")));
}

#include "TestMainWindowCommunicationController.moc"
