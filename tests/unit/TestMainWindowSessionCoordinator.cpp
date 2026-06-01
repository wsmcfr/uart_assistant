/**
 * @file TestMainWindowSessionCoordinator.cpp
 * @brief 主窗口会话协调器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMainWindowSessionCoordinator.h"

#include "ui/MainWindowSessionCoordinator.h"
#include "ui/widgets/HidReportWorkspaceWidget.h"
#include "ui/widgets/NetworkSettingsWidget.h"
#include "ui/widgets/QuickSendWidget.h"
#include "ui/widgets/SerialSettingsWidget.h"
#include "ui/widgets/TcpClientWorkspaceWidget.h"
#include "ui/widgets/TcpServerWorkspaceWidget.h"
#include "ui/widgets/UdpWorkspaceWidget.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>

using namespace ComAssistant;

namespace {

/**
 * @brief 构造绑定了常用控件的会话协调器。
 */
struct SessionCoordinatorFixture
{
    SerialSettingsWidget serialSettings;      ///< 串口配置页。
    NetworkSettingsWidget networkSettings;    ///< 网络配置页。
    TcpClientWorkspaceWidget tcpClient;       ///< TCP Client 工作台。
    TcpServerWorkspaceWidget tcpServer;       ///< TCP Server 工作台。
    UdpWorkspaceWidget udp;                   ///< UDP 工作台。
    HidReportWorkspaceWidget hid;             ///< HID 工作台。
    QuickSendWidget quickSend;                ///< 快捷发送控件。
    QComboBox commTypeCombo;                  ///< 通信类型下拉框。
    QComboBox portCombo;                      ///< 串口/HID 设备下拉框。
    QComboBox baudCombo;                      ///< 波特率下拉框。
    QLineEdit ipEdit;                         ///< 顶部网络 IP 输入框。
    QSpinBox portSpin;                        ///< 顶部网络端口输入框。
    QComboBox displayModeCombo;               ///< 显示模式下拉框。
    MainWindowSessionCoordinator coordinator; ///< 被测会话协调器。

    /**
     * @brief 初始化测试控件和协调器绑定关系。
     */
    SessionCoordinatorFixture()
    {
        commTypeCombo.addItem(QStringLiteral("Serial"), static_cast<int>(CommType::Serial));
        commTypeCombo.addItem(QStringLiteral("TCP Client"), static_cast<int>(CommType::TcpClient));
        commTypeCombo.addItem(QStringLiteral("TCP Server"), static_cast<int>(CommType::TcpServer));
        commTypeCombo.addItem(QStringLiteral("UDP"), static_cast<int>(CommType::Udp));
        commTypeCombo.addItem(QStringLiteral("HID"), static_cast<int>(CommType::Hid));

        baudCombo.addItem(QStringLiteral("9600"), 9600);
        baudCombo.addItem(QStringLiteral("115200"), 115200);

        portSpin.setRange(1, 65535);
        displayModeCombo.addItem(QStringLiteral("Serial"), 0);
        displayModeCombo.addItem(QStringLiteral("Terminal"), 1);
        displayModeCombo.addItem(QStringLiteral("Frame"), 2);
        displayModeCombo.addItem(QStringLiteral("Debug"), 3);

        coordinator.setConfigurationWidgets(&serialSettings,
                                            &networkSettings,
                                            &tcpClient,
                                            &tcpServer,
                                            &udp,
                                            &hid);
        coordinator.setToolbarWidgets(&commTypeCombo,
                                      &portCombo,
                                      &baudCombo,
                                      &ipEdit,
                                      &portSpin,
                                      &displayModeCombo);
        coordinator.setQuickSendWidget(&quickSend);
    }
};

/**
 * @brief 创建基础会话数据。
 * @return 带常用串口和网络默认值的会话。
 */
SessionData makeBaseSession()
{
    SessionData session;
    session.commType = CommType::Serial;
    session.serialConfig.portName = QStringLiteral("COM7");
    session.serialConfig.baudRate = 115200;
    session.networkConfig.serverIp = QStringLiteral("192.168.10.5");
    session.networkConfig.serverPort = 9001;
    session.networkConfig.listenPort = 7000;
    session.networkConfig.remoteIp = QStringLiteral("239.1.1.1");
    session.networkConfig.remotePort = 7001;
    session.hidConfig.path = QStringLiteral("hid-path");
    session.hidConfig.name = QStringLiteral("Demo HID");
    session.protocolType = static_cast<int>(ProtocolType::TextPlot);
    session.displayMode = 2;
    return session;
}

} // namespace

void TestMainWindowSessionCoordinator::testApplyTcpClientSessionUpdatesNetworkToolbar()
{
    SessionCoordinatorFixture fixture;
    SessionData session = makeBaseSession();
    session.commType = CommType::TcpClient;

    CommType commType = CommType::Serial;
    SerialConfig serialConfig;
    NetworkConfig networkConfig;
    HidConfig hidConfig;

    const MainWindowSessionCoordinator::ApplyResult result =
        fixture.coordinator.applySession(session,
                                         commType,
                                         serialConfig,
                                         networkConfig,
                                         hidConfig);

    QCOMPARE(commType, CommType::TcpClient);
    QCOMPARE(networkConfig.serverIp, session.networkConfig.serverIp);
    QCOMPARE(networkConfig.serverPort, session.networkConfig.serverPort);
    QCOMPARE(fixture.commTypeCombo.currentData().toInt(), static_cast<int>(CommType::TcpClient));
    QCOMPARE(fixture.ipEdit.text(), session.networkConfig.serverIp);
    QCOMPARE(fixture.portSpin.value(), session.networkConfig.serverPort);
    QCOMPARE(fixture.displayModeCombo.currentData().toInt(), session.displayMode);
    QCOMPARE(result.restoredProtocolType, ProtocolType::TextPlot);
}

void TestMainWindowSessionCoordinator::testApplyUdpSessionFallsBackToListenPort()
{
    SessionCoordinatorFixture fixture;
    SessionData session = makeBaseSession();
    session.commType = CommType::Udp;
    session.networkConfig.remoteIp.clear();
    session.networkConfig.remotePort = 0;
    session.networkConfig.listenPort = 7654;

    CommType commType = CommType::Serial;
    SerialConfig serialConfig;
    NetworkConfig networkConfig;
    HidConfig hidConfig;

    fixture.coordinator.applySession(session,
                                     commType,
                                     serialConfig,
                                     networkConfig,
                                     hidConfig);

    QCOMPARE(fixture.ipEdit.text(), QStringLiteral("127.0.0.1"));
    QCOMPARE(fixture.portSpin.value(), session.networkConfig.listenPort);
}

void TestMainWindowSessionCoordinator::testSelectRestoredSerialAndHidDevice()
{
    SessionCoordinatorFixture fixture;
    SessionData serialSession = makeBaseSession();
    serialSession.commType = CommType::Serial;

    CommType commType = CommType::Serial;
    SerialConfig serialConfig;
    NetworkConfig networkConfig;
    HidConfig hidConfig;

    fixture.coordinator.applySession(serialSession,
                                     commType,
                                     serialConfig,
                                     networkConfig,
                                     hidConfig);

    fixture.portCombo.addItem(QStringLiteral("COM1"), QStringLiteral("COM1"));
    fixture.portCombo.addItem(QStringLiteral("COM7"), QStringLiteral("COM7"));
    QVERIFY(fixture.coordinator.selectRestoredPort(commType, serialConfig, hidConfig));
    QCOMPARE(fixture.portCombo.currentData().toString(), QStringLiteral("COM7"));

    SessionData hidSession = makeBaseSession();
    hidSession.commType = CommType::Hid;
    fixture.coordinator.applySession(hidSession,
                                     commType,
                                     serialConfig,
                                     networkConfig,
                                     hidConfig);

    fixture.portCombo.clear();
    fixture.portCombo.addItem(QStringLiteral("Other HID"), QStringLiteral("other-path"));
    fixture.portCombo.addItem(hidSession.hidConfig.name, hidSession.hidConfig.path);
    QVERIFY(fixture.coordinator.selectRestoredPort(commType, serialConfig, hidConfig));
    QCOMPARE(fixture.portCombo.currentData().toString(), hidSession.hidConfig.path);
}

void TestMainWindowSessionCoordinator::testInvalidProtocolFallsBackToRaw()
{
    SessionCoordinatorFixture fixture;
    SessionData session = makeBaseSession();
    session.protocolType = 9999;

    CommType commType = CommType::Serial;
    SerialConfig serialConfig;
    NetworkConfig networkConfig;
    HidConfig hidConfig;

    const MainWindowSessionCoordinator::ApplyResult result =
        fixture.coordinator.applySession(session,
                                         commType,
                                         serialConfig,
                                         networkConfig,
                                         hidConfig);

    QCOMPARE(result.restoredProtocolType, ProtocolType::Raw);
}
