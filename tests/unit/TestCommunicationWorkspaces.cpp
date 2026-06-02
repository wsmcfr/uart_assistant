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
#include "ui/widgets/CommunicationWorkspaceWidget.h"

#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>
#include <QTextDocument>

using namespace ComAssistant;

namespace {

/**
 * @brief 暴露通信工作台基类的受保护辅助函数，便于单元测试直接验证格式规则。
 *
 * 测试只通过该探针读取共享格式化结果，不参与生产界面的事件流程。
 */
class CommunicationWorkspaceProbe : public CommunicationWorkspaceWidget
{
public:
    using CommunicationWorkspaceWidget::formatLogLine;
    using CommunicationWorkspaceWidget::normalizeHexText;
    using CommunicationWorkspaceWidget::payloadPreview;
};

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
    if (!child) {
        qFatal("Missing child widget: %s", objectName);
    }
    return child;
}

} // namespace

void TestCommunicationWorkspaces::testCommunicationWorkspaceSharedHelpers()
{
    CommunicationWorkspaceProbe widget;

    QCOMPARE(widget.normalizeHexText(QStringLiteral("0x0a bb c")),
             QStringLiteral("0A BB 0C"));

    const QByteArray previewBytes("line1\nline2\t0123456789");
    QCOMPARE(widget.payloadPreview(previewBytes, 12), QStringLiteral("line1.line2...."));

    const QString logLine = widget.formatLogLine(QStringLiteral("TX"),
                                                 QStringLiteral("UDP"),
                                                 QByteArray("Hi\n"));
    QVERIFY(logLine.contains(QStringLiteral("TX")));
    QVERIFY(logLine.contains(QStringLiteral("UDP")));
    QVERIFY(logLine.contains(QStringLiteral("3 字节")));
    QVERIFY(logLine.contains(QStringLiteral("48 69 0A")));
    QVERIFY(logLine.contains(QStringLiteral("Hi.")));
}

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

void TestCommunicationWorkspaces::testTcpClientWorkspaceSendHelpersAndLogTools()
{
    TcpClientWorkspaceWidget widget;

    QPlainTextEdit* sendEdit = requireChild<QPlainTextEdit>(widget, "tcpClientSendEdit");
    QPlainTextEdit* logEdit = requireChild<QPlainTextEdit>(widget, "tcpClientLogEdit");
    QPushButton* formatButton = requireChild<QPushButton>(widget, "tcpClientFormatHexButton");
    QPushButton* clearButton = requireChild<QPushButton>(widget, "tcpClientClearLogButton");
    QCheckBox* appendNewlineCheck = requireChild<QCheckBox>(widget, "tcpClientAppendNewlineCheck");
    QLabel* byteCountLabel = requireChild<QLabel>(widget, "tcpClientByteCountLabel");

    sendEdit->setPlainText(QStringLiteral("0x0a bb c"));
    formatButton->click();
    QCOMPARE(sendEdit->toPlainText(), QStringLiteral("0A BB 0C"));
    QVERIFY(byteCountLabel->text().contains(QStringLiteral("3")));

    QSignalSpy sendSpy(&widget, SIGNAL(sendDataRequested(QByteArray)));
    requireChild<QCheckBox>(widget, "tcpClientHexSendCheck")->setChecked(false);
    appendNewlineCheck->setChecked(true);
    sendEdit->setPlainText(QStringLiteral("ping"));
    QTest::keyClick(sendEdit, Qt::Key_Return, Qt::ControlModifier);
    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), QByteArray("ping\n"));

    widget.appendReceivedData(QByteArray("pong"));
    QVERIFY(logEdit->toPlainText().contains(QStringLiteral("TCP")));
    clearButton->click();
    QVERIFY(logEdit->toPlainText().isEmpty());
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

void TestCommunicationWorkspaces::testTcpServerWorkspaceClientManagementHelpers()
{
    TcpServerWorkspaceWidget widget;

    QLabel* clientCountLabel = requireChild<QLabel>(widget, "tcpServerClientCountLabel");
    QPushButton* sendClientButton = requireChild<QPushButton>(widget, "tcpServerSendClientButton");
    QPushButton* disconnectButton = requireChild<QPushButton>(widget, "tcpServerDisconnectClientButton");
    QPushButton* formatButton = requireChild<QPushButton>(widget, "tcpServerFormatHexButton");
    QPlainTextEdit* sendEdit = requireChild<QPlainTextEdit>(widget, "tcpServerSendEdit");

    QVERIFY(!sendClientButton->isEnabled());
    QVERIFY(!disconnectButton->isEnabled());

    widget.setClients(QStringList() << QStringLiteral("127.0.0.1:52000")
                                    << QStringLiteral("127.0.0.1:52001"));
    QVERIFY(clientCountLabel->text().contains(QStringLiteral("2")));
    QVERIFY(sendClientButton->isEnabled());
    QVERIFY(disconnectButton->isEnabled());

    QSignalSpy disconnectSpy(&widget, SIGNAL(disconnectClientRequested(QString)));
    disconnectButton->click();
    QCOMPARE(disconnectSpy.count(), 1);
    QCOMPARE(disconnectSpy.takeFirst().at(0).toString(), QStringLiteral("127.0.0.1:52000"));

    sendEdit->setPlainText(QStringLiteral("1 2 0xff"));
    formatButton->click();
    QCOMPARE(sendEdit->toPlainText(), QStringLiteral("01 02 FF"));
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

void TestCommunicationWorkspaces::testUdpWorkspaceRecentRemoteAndSendHelpers()
{
    UdpWorkspaceWidget widget;

    QComboBox* recentCombo = requireChild<QComboBox>(widget, "udpRecentRemoteCombo");
    QPushButton* clearRecentButton = requireChild<QPushButton>(widget, "udpClearRecentRemoteButton");
    QPushButton* formatButton = requireChild<QPushButton>(widget, "udpFormatHexButton");
    QPlainTextEdit* sendEdit = requireChild<QPlainTextEdit>(widget, "udpSendEdit");
    QLabel* statusLabel = requireChild<QLabel>(widget, "udpSummaryLabel");

    widget.addRecentRemote(QStringLiteral("10.0.0.12"), 6000);
    widget.addRecentRemote(QStringLiteral("10.0.0.13"), 6001);
    QCOMPARE(recentCombo->count(), 2);
    clearRecentButton->click();
    QCOMPARE(recentCombo->count(), 0);

    sendEdit->setPlainText(QStringLiteral("aa b c"));
    formatButton->click();
    QCOMPARE(sendEdit->toPlainText(), QStringLiteral("AA 0B 0C"));

    QSignalSpy datagramSpy(&widget, SIGNAL(sendDatagramRequested(QByteArray,QString,int)));
    requireChild<QCheckBox>(widget, "udpHexSendCheck")->setChecked(false);
    sendEdit->setPlainText(QStringLiteral("quick"));
    QTest::keyClick(sendEdit, Qt::Key_Return, Qt::ControlModifier);
    QCOMPARE(datagramSpy.count(), 1);
    QCOMPARE(datagramSpy.takeFirst().at(0).toByteArray(), QByteArray("quick"));

    widget.setConnected(true);
    QVERIFY(statusLabel->text().contains(QStringLiteral("目标")));
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

void TestCommunicationWorkspaces::testHidReportWorkspacePreviewAndHistoryTools()
{
    HidReportWorkspaceWidget widget;

    HidConfig config;
    config.name = QStringLiteral("Preview HID");
    config.outputReportLength = 4;
    config.featureReportLength = 5;
    config.outReportId = 2;
    config.featureReportId = 3;
    widget.setConfig(config);

    QPlainTextEdit* outputEdit = requireChild<QPlainTextEdit>(widget, "hidOutputPayloadEdit");
    QPlainTextEdit* featureEdit = requireChild<QPlainTextEdit>(widget, "hidFeaturePayloadEdit");
    QLabel* outputPreviewLabel = requireChild<QLabel>(widget, "hidOutputPreviewLabel");
    QLabel* featurePreviewLabel = requireChild<QLabel>(widget, "hidFeaturePreviewLabel");
    QLabel* outputCountLabel = requireChild<QLabel>(widget, "hidOutputByteCountLabel");
    QLabel* warningLabel = requireChild<QLabel>(widget, "hidTruncationWarningLabel");
    QPlainTextEdit* historyEdit = requireChild<QPlainTextEdit>(widget, "hidHistoryEdit");

    outputEdit->setPlainText(QStringLiteral("a b c d"));
    QVERIFY(outputPreviewLabel->text().contains(QStringLiteral("02 0A 0B 0C")));
    QVERIFY(outputCountLabel->text().contains(QStringLiteral("4")));
    QVERIFY(!warningLabel->isHidden());
    QVERIFY(warningLabel->text().contains(QStringLiteral("截断")));

    requireChild<QPushButton>(widget, "hidOutputFormatHexButton")->click();
    QCOMPARE(outputEdit->toPlainText(), QStringLiteral("0A 0B 0C 0D"));

    featureEdit->setPlainText(QStringLiteral("1 2"));
    QVERIFY(featurePreviewLabel->text().contains(QStringLiteral("03 01 02 00 00")));

    requireChild<QPushButton>(widget, "hidFeatureFormatHexButton")->click();
    QCOMPARE(featureEdit->toPlainText(), QStringLiteral("01 02"));

    widget.appendReceivedData(QByteArray::fromHex("010203"));
    QVERIFY(historyEdit->toPlainText().contains(QStringLiteral("Input")));
    requireChild<QPushButton>(widget, "hidClearHistoryButton")->click();
    QVERIFY(historyEdit->toPlainText().isEmpty());
}

void TestCommunicationWorkspaces::testCommunicationWorkspaceLogsHaveBoundedHistory()
{
    /*
     * 通信工作台日志属于常驻运行链路。如果这里没有 maximumBlockCount，
     * TCP/UDP/HID 长时间收发时 QTextDocument 会持续增长，表现得像内存泄露。
     */
    TcpClientWorkspaceWidget tcpClient;
    TcpServerWorkspaceWidget tcpServer;
    UdpWorkspaceWidget udp;
    HidReportWorkspaceWidget hid;

    QPlainTextEdit* tcpClientLog = requireChild<QPlainTextEdit>(tcpClient, "tcpClientLogEdit");
    QPlainTextEdit* tcpServerLog = requireChild<QPlainTextEdit>(tcpServer, "tcpServerLogEdit");
    QPlainTextEdit* udpLog = requireChild<QPlainTextEdit>(udp, "udpLogEdit");
    QPlainTextEdit* hidLog = requireChild<QPlainTextEdit>(hid, "hidHistoryEdit");

    const QList<QPlainTextEdit*> logs = {tcpClientLog, tcpServerLog, udpLog, hidLog};
    for (QPlainTextEdit* logEdit : logs) {
        QVERIFY(logEdit != nullptr);
        QVERIFY(logEdit->document() != nullptr);
        QVERIFY2(logEdit->document()->maximumBlockCount() > 0,
                 "通信日志必须设置最大块数，不能无限保留历史。");
        QVERIFY2(logEdit->document()->maximumBlockCount() <= 5000,
                 "通信日志上限应保持轻量，避免专用工作台复制主接收区的大历史。");
    }
}
