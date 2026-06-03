/**
 * @file TestCommunicationReceiveBuffers.cpp
 * @brief 通信接收兼容缓存内存边界回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestCommunicationReceiveBuffers.h"

#include "communication/HidDevice.h"
#include "communication/TcpServer.h"
#include "communication/UdpSocket.h"

#include <QMetaObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

using namespace ComAssistant;

namespace {

/**
 * @brief 拼接 QSignalSpy 捕获到的第一个 QByteArray 参数。
 * @param spy 已连接到 dataReceived(QByteArray) 的信号探针。
 * @return 按信号到达顺序拼接后的完整 payload。
 *
 * TCP 可能把一次 write 拆成多次 readyRead，也可能把多次 write 合并成一次
 * readyRead。测试只关心上层 dataReceived 最终收到的字节流是否完整，因此
 * 用该函数消除分包方式差异。
 */
QByteArray concatenateByteArraySignal(const QSignalSpy& spy)
{
    QByteArray payload;
    for (const QList<QVariant>& arguments : spy) {
        payload.append(arguments.at(0).toByteArray());
    }
    return payload;
}

/**
 * @brief 为本机 TCP 测试申请一个当前空闲端口。
 * @return 可用于短时间内启动测试 TCP Server 的端口号；失败时返回 0。
 *
 * Qt 的 TcpServer 包装类不暴露监听 0 端口后的真实端口，因此测试先用探针
 * 绑定 0 端口获取系统分配值，再释放给被测对象使用。
 */
quint16 reserveFreeTcpPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }

    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

/**
 * @brief 为本机 UDP 测试申请一个当前空闲端口。
 * @return 可用于短时间内绑定测试 UDP Socket 的端口号；失败时返回 0。
 *
 * UDP 同样需要避免固定端口造成并发测试冲突，因此通过临时 QUdpSocket 获取
 * 系统分配端口后释放。
 */
quint16 reserveFreeUdpPort()
{
    QUdpSocket probe;
    if (!probe.bind(QHostAddress(QHostAddress::LocalHost), static_cast<quint16>(0))) {
        return 0;
    }

    const quint16 port = probe.localPort();
    probe.close();
    return port;
}

} // namespace

void TestCommunicationReceiveBuffers::testTcpServerReceiveBufferIsBoundedWhileSignalGetsFullPayload()
{
    /*
     * 主接收链路通过 dataReceived 消费 TCP Server 数据；readAll() 只是旧接口
     * 兼容缓存。该测试验证大于 bufferSize 的输入不会让兼容缓存保存整包。
     */
    const quint16 port = reserveFreeTcpPort();
    QVERIFY2(port > 0, "测试需要可用的本机 TCP 端口。");

    NetworkConfig config;
    config.mode = NetworkMode::TcpServer;
    config.listenPort = port;
    config.maxConnections = 1;

    TcpServer server(config);
    server.setBufferSize(8);
    QVERIFY2(server.open(), qPrintable(server.lastError()));

    QSignalSpy dataSpy(&server, SIGNAL(dataReceived(QByteArray)));

    QTcpSocket client;
    client.connectToHost(QHostAddress::LocalHost, port);
    QVERIFY2(client.waitForConnected(1000), qPrintable(client.errorString()));
    QTRY_COMPARE_WITH_TIMEOUT(server.connectionCount(), 1, 1000);

    const QByteArray payload("123456789ABCDEF");
    QCOMPARE(client.write(payload), static_cast<qint64>(payload.size()));
    QVERIFY2(client.waitForBytesWritten(1000), qPrintable(client.errorString()));

    QTRY_VERIFY_WITH_TIMEOUT(concatenateByteArraySignal(dataSpy).size() >= payload.size(), 1000);
    QCOMPARE(concatenateByteArraySignal(dataSpy), payload);
    QCOMPARE(server.bytesAvailable(), static_cast<qint64>(8));
    QCOMPARE(server.readAll(), payload.right(8));
    QCOMPARE(server.bytesAvailable(), static_cast<qint64>(0));
}

void TestCommunicationReceiveBuffers::testUdpReceiveBufferIsBoundedWhileSignalGetsFullDatagram()
{
    /*
     * UDP 数据报已经通过 dataReceived/datagramReceived 向主流程分发；内部
     * readAll() 缓存只用于兼容旧调用者，必须跟随 bufferSize 做尾部保留。
     */
    const quint16 port = reserveFreeUdpPort();
    QVERIFY2(port > 0, "测试需要可用的本机 UDP 端口。");

    NetworkConfig config;
    config.mode = NetworkMode::Udp;
    config.listenPort = port;

    UdpSocket receiver(config);
    receiver.setBufferSize(8);
    QVERIFY2(receiver.open(), qPrintable(receiver.lastError()));

    QSignalSpy dataSpy(&receiver, SIGNAL(dataReceived(QByteArray)));

    QUdpSocket sender;
    const QByteArray payload("123456789ABCDEF");
    QCOMPARE(sender.writeDatagram(payload, QHostAddress::LocalHost, port),
             static_cast<qint64>(payload.size()));

    QTRY_VERIFY_WITH_TIMEOUT(concatenateByteArraySignal(dataSpy).size() >= payload.size(), 1000);
    QCOMPARE(concatenateByteArraySignal(dataSpy), payload);
    QCOMPARE(receiver.bytesAvailable(), static_cast<qint64>(8));
    QCOMPARE(receiver.readAll(), payload.right(8));
    QCOMPARE(receiver.bytesAvailable(), static_cast<qint64>(0));
}

void TestCommunicationReceiveBuffers::testHidReceiveBufferIsBoundedWhileSignalGetsFullReports()
{
    /*
     * HID 输入报告由 worker 线程投递到 handleInputReport()。这里用元对象直接
     * 调用该槽函数，覆盖真实归一化和缓存路径，同时避免测试依赖实际 HID 硬件。
     */
    HidConfig config;
    config.removeInReportId = false;

    HidDevice device(config);
    device.setBufferSize(8);

    QSignalSpy dataSpy(&device, SIGNAL(dataReceived(QByteArray)));

    const QByteArray firstReport("123456789");
    const QByteArray secondReport("ABCDEF");
    QVERIFY(QMetaObject::invokeMethod(&device,
                                      "handleInputReport",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, firstReport)));
    QVERIFY(QMetaObject::invokeMethod(&device,
                                      "handleInputReport",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, secondReport)));

    const QByteArray payload = firstReport + secondReport;
    QCOMPARE(concatenateByteArraySignal(dataSpy), payload);
    QCOMPARE(device.bytesAvailable(), static_cast<qint64>(8));
    QCOMPARE(device.readAll(), payload.right(8));
    QCOMPARE(device.bytesAvailable(), static_cast<qint64>(0));
}

void TestCommunicationReceiveBuffers::testSetBufferSizeTrimsExistingReceiveBuffers()
{
    /*
     * 用户可能在运行中调整接收缓冲区大小。调小 bufferSize 时，已有
     * readAll() 兼容缓存也必须立即裁剪，不能等下一包数据到达后才处理。
     */
    const QByteArray payload("123456789ABCDEF");

    const quint16 tcpPort = reserveFreeTcpPort();
    QVERIFY2(tcpPort > 0, "测试需要可用的本机 TCP 端口。");

    NetworkConfig tcpConfig;
    tcpConfig.mode = NetworkMode::TcpServer;
    tcpConfig.listenPort = tcpPort;
    tcpConfig.maxConnections = 1;

    TcpServer tcpServer(tcpConfig);
    tcpServer.setBufferSize(64);
    QVERIFY2(tcpServer.open(), qPrintable(tcpServer.lastError()));
    QSignalSpy tcpSpy(&tcpServer, SIGNAL(dataReceived(QByteArray)));

    QTcpSocket tcpClient;
    tcpClient.connectToHost(QHostAddress::LocalHost, tcpPort);
    QVERIFY2(tcpClient.waitForConnected(1000), qPrintable(tcpClient.errorString()));
    QTRY_COMPARE_WITH_TIMEOUT(tcpServer.connectionCount(), 1, 1000);
    QCOMPARE(tcpClient.write(payload), static_cast<qint64>(payload.size()));
    QVERIFY2(tcpClient.waitForBytesWritten(1000), qPrintable(tcpClient.errorString()));
    QTRY_VERIFY_WITH_TIMEOUT(concatenateByteArraySignal(tcpSpy).size() >= payload.size(), 1000);
    QCOMPARE(tcpServer.bytesAvailable(), static_cast<qint64>(payload.size()));

    tcpServer.setBufferSize(5);
    QCOMPARE(tcpServer.bytesAvailable(), static_cast<qint64>(5));
    QCOMPARE(tcpServer.readAll(), payload.right(5));

    const quint16 udpPort = reserveFreeUdpPort();
    QVERIFY2(udpPort > 0, "测试需要可用的本机 UDP 端口。");

    NetworkConfig udpConfig;
    udpConfig.mode = NetworkMode::Udp;
    udpConfig.listenPort = udpPort;

    UdpSocket udpReceiver(udpConfig);
    udpReceiver.setBufferSize(64);
    QVERIFY2(udpReceiver.open(), qPrintable(udpReceiver.lastError()));
    QSignalSpy udpSpy(&udpReceiver, SIGNAL(dataReceived(QByteArray)));

    QUdpSocket udpSender;
    QCOMPARE(udpSender.writeDatagram(payload, QHostAddress::LocalHost, udpPort),
             static_cast<qint64>(payload.size()));
    QTRY_VERIFY_WITH_TIMEOUT(concatenateByteArraySignal(udpSpy).size() >= payload.size(), 1000);
    QCOMPARE(udpReceiver.bytesAvailable(), static_cast<qint64>(payload.size()));

    udpReceiver.setBufferSize(5);
    QCOMPARE(udpReceiver.bytesAvailable(), static_cast<qint64>(5));
    QCOMPARE(udpReceiver.readAll(), payload.right(5));

    HidDevice hidDevice{HidConfig()};
    hidDevice.setBufferSize(64);
    QSignalSpy hidSpy(&hidDevice, SIGNAL(dataReceived(QByteArray)));
    QVERIFY(QMetaObject::invokeMethod(&hidDevice,
                                      "handleInputReport",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, payload)));
    QCOMPARE(concatenateByteArraySignal(hidSpy), payload);
    QCOMPARE(hidDevice.bytesAvailable(), static_cast<qint64>(payload.size()));

    hidDevice.setBufferSize(5);
    QCOMPARE(hidDevice.bytesAvailable(), static_cast<qint64>(5));
    QCOMPARE(hidDevice.readAll(), payload.right(5));
}
