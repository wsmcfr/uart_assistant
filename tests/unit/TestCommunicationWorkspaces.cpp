/**
 * @file TestCommunicationWorkspaces.cpp
 * @brief 通信类型专用工作台回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TestCommunicationWorkspaces.h"

#include "ui/widgets/TcpClientWorkspaceWidget.h"
#include "ui/widgets/TcpServerWorkspaceWidget.h"
#include "ui/widgets/UdpWorkspaceWidget.h"
#include "ui/widgets/HidReportWorkspaceWidget.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>

using namespace ComAssistant;

namespace {

/**
 * @brief 按 objectName 查找子控件，并在缺失时让测试给出明确错误。
 * @param root 父控件。
 * @param objectName 目标 objectName。
 * @return 找到的子控件指针。
 */
template <typename T>
T* requireChild(QWidget& root, const char* objectName)
{
    T* child = root.findChild<T*>(QString::fromLatin1(objectName));
    Q_ASSERT(child);
    return child;
}

} // namespace

void TestCommunicationWorkspaces::testTcpClientWorkspaceConfigAndSendSignal()
{
    TcpClientWorkspaceWidget widget;

    NetworkConfig config;
    config.mode = NetworkMode::TcpClient;
    config.serverIp = QStringLiteral("192.168.10.8");
    config.serverPort = 1883;
    config.connectTimeout = 2500;
    widget.setConfig(config);

    QCOMPARE(requireChild<QLineEdit>(widget, "tcpClientServerIpEdit")->text(), config.serverIp);
    QCOMPARE(requireChild<QSpinBox>(widget, "tcpClientServerPortSpin")->value(), config.serverPort);
    QCOMPARE(requireChild<QSpinBox>(widget, "tcpClientTimeoutSpin")->value(), config.connectTimeout);

    const NetworkConfig restored = widget.config();
    QCOMPARE(restored.mode, NetworkMode::TcpClient);
    QCOMPARE(restored.serverIp, config.serverIp);
    QCOMPARE(restored.serverPort, config.serverPort);
    QCOMPARE(restored.connectTimeout, config.connectTimeout);

    QSignalSpy sendSpy(&widget, SIGNAL(sendDataRequested(QByteArray)));
    requireChild<QPlainTextEdit>(widget, "tcpClientSendEdit")->setPlainText(QStringLiteral("ping"));
    requireChild<QPushButton>(widget, "tcpClientSendButton")->click();

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), QByteArray("ping"));
}

void TestCommunicationWorkspaces::testTcpServerWorkspaceClientTargetAndBroadcast()
{
    TcpServerWorkspaceWidget widget;

    NetworkConfig config;
    config.mode = NetworkMode::TcpServer;
    config.listenPort = 9001;
    config.maxConnections = 7;
    widget.setConfig(config);
    widget.setClients(QStringList() << QStringLiteral("127.0.0.1:52000"));

    QCOMPARE(requireChild<QSpinBox>(widget, "tcpServerListenPortSpin")->value(), config.listenPort);
    QCOMPARE(requireChild<QSpinBox>(widget, "tcpServerMaxConnectionsSpin")->value(), config.maxConnections);
    QCOMPARE(requireChild<QComboBox>(widget, "tcpServerClientCombo")->currentText(),
             QStringLiteral("127.0.0.1:52000"));

    QSignalSpy clientSpy(&widget, SIGNAL(sendToClientRequested(QString,QByteArray)));
    QSignalSpy broadcastSpy(&widget, SIGNAL(broadcastDataRequested(QByteArray)));
    requireChild<QPlainTextEdit>(widget, "tcpServerSendEdit")->setPlainText(QStringLiteral("hello"));

    requireChild<QPushButton>(widget, "tcpServerSendClientButton")->click();
    QCOMPARE(clientSpy.count(), 1);
    QCOMPARE(clientSpy.takeFirst().at(0).toString(), QStringLiteral("127.0.0.1:52000"));

    requireChild<QPushButton>(widget, "tcpServerBroadcastButton")->click();
    QCOMPARE(broadcastSpy.count(), 1);
    QCOMPARE(broadcastSpy.takeFirst().at(0).toByteArray(), QByteArray("hello"));
}

void TestCommunicationWorkspaces::testUdpWorkspaceConfigRecentRemoteAndSendSignal()
{
    UdpWorkspaceWidget widget;

    NetworkConfig config;
    config.mode = NetworkMode::Udp;
    config.listenPort = 7000;
    config.remoteIp = QStringLiteral("239.1.1.1");
    config.remotePort = 7001;
    widget.setConfig(config);
    widget.addRecentRemote(QStringLiteral("10.0.0.12"), 6000);

    QCOMPARE(requireChild<QSpinBox>(widget, "udpLocalPortSpin")->value(), config.listenPort);
    QCOMPARE(requireChild<QLineEdit>(widget, "udpRemoteIpEdit")->text(), QStringLiteral("10.0.0.12"));
    QCOMPARE(requireChild<QSpinBox>(widget, "udpRemotePortSpin")->value(), 6000);

    QSignalSpy datagramSpy(&widget, SIGNAL(sendDatagramRequested(QByteArray,QString,int)));
    requireChild<QPlainTextEdit>(widget, "udpSendEdit")->setPlainText(QStringLiteral("udp-data"));
    requireChild<QPushButton>(widget, "udpSendButton")->click();

    QCOMPARE(datagramSpy.count(), 1);
    const QList<QVariant> args = datagramSpy.takeFirst();
    QCOMPARE(args.at(0).toByteArray(), QByteArray("udp-data"));
    QCOMPARE(args.at(1).toString(), QStringLiteral("10.0.0.12"));
    QCOMPARE(args.at(2).toInt(), 6000);
}

void TestCommunicationWorkspaces::testHidReportWorkspaceConfigAndFeatureSignals()
{
    HidReportWorkspaceWidget widget;

    HidConfig config;
    config.name = QStringLiteral("Demo HID");
    config.vendorId = 0x1234;
    config.productId = 0x5678;
    config.inputReportLength = 32;
    config.outputReportLength = 16;
    config.featureReportLength = 8;
    config.outReportId = 2;
    config.featureReportId = 3;
    widget.setConfig(config);

    QCOMPARE(requireChild<QSpinBox>(widget, "hidInputLengthSpin")->value(), config.inputReportLength);
    QCOMPARE(requireChild<QSpinBox>(widget, "hidOutputLengthSpin")->value(), config.outputReportLength);
    QCOMPARE(requireChild<QSpinBox>(widget, "hidFeatureLengthSpin")->value(), config.featureReportLength);
    QCOMPARE(requireChild<QSpinBox>(widget, "hidOutputReportIdSpin")->value(), static_cast<int>(config.outReportId));
    QCOMPARE(requireChild<QSpinBox>(widget, "hidFeatureReportIdSpin")->value(), static_cast<int>(config.featureReportId));

    QSignalSpy outputSpy(&widget, SIGNAL(outputReportRequested(QByteArray)));
    QSignalSpy featureSetSpy(&widget, SIGNAL(featureReportSetRequested(QByteArray)));
    QSignalSpy featureGetSpy(&widget, SIGNAL(featureReportGetRequested(QByteArray)));

    requireChild<QPlainTextEdit>(widget, "hidOutputPayloadEdit")->setPlainText(QStringLiteral("A1 B2"));
    requireChild<QPushButton>(widget, "hidOutputSendButton")->click();
    QCOMPARE(outputSpy.count(), 1);
    QCOMPARE(outputSpy.takeFirst().at(0).toByteArray(), QByteArray::fromHex("A1B2"));

    requireChild<QPlainTextEdit>(widget, "hidFeaturePayloadEdit")->setPlainText(QStringLiteral("01 02 03"));
    requireChild<QPushButton>(widget, "hidFeatureSetButton")->click();
    QCOMPARE(featureSetSpy.count(), 1);
    QCOMPARE(featureSetSpy.takeFirst().at(0).toByteArray(), QByteArray::fromHex("0301020300000000"));

    requireChild<QPushButton>(widget, "hidFeatureGetButton")->click();
    QCOMPARE(featureGetSpy.count(), 1);
    QCOMPARE(featureGetSpy.takeFirst().at(0).toByteArray(), QByteArray::fromHex("0300000000000000"));
}
