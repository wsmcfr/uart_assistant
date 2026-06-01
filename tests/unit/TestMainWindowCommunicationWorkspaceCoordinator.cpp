/**
 * @file TestMainWindowCommunicationWorkspaceCoordinator.cpp
 * @brief 主窗口通信工作台协调器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMainWindowCommunicationWorkspaceCoordinator.h"

#include "ui/MainWindowCommunicationWorkspaceCoordinator.h"
#include "ui/widgets/HidReportWorkspaceWidget.h"
#include "ui/widgets/TcpClientWorkspaceWidget.h"
#include "ui/widgets/TcpServerWorkspaceWidget.h"
#include "ui/widgets/UdpWorkspaceWidget.h"

#include <QSpinBox>

using namespace ComAssistant;

namespace {

/**
 * @brief 按 objectName 查找子控件，并在缺失时让测试立即失败。
 * @param root 父控件。
 * @param objectName 子控件 objectName。
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

/**
 * @brief 构造绑定了四个专用工作台的协调器。
 * @param tcpClient TCP Client 工作台。
 * @param tcpServer TCP Server 工作台。
 * @param udp UDP 工作台。
 * @param hid HID Report 工作台。
 * @return 已设置工作台指针的协调器。
 */
MainWindowCommunicationWorkspaceCoordinator makeCoordinator(TcpClientWorkspaceWidget& tcpClient,
                                                           TcpServerWorkspaceWidget& tcpServer,
                                                           UdpWorkspaceWidget& udp,
                                                           HidReportWorkspaceWidget& hid)
{
    MainWindowCommunicationWorkspaceCoordinator coordinator;
    coordinator.setWorkspaces(&tcpClient, &tcpServer, &udp, &hid);
    return coordinator;
}

} // namespace

void TestMainWindowCommunicationWorkspaceCoordinator::testCurrentWorkspaceMatchesCommunicationType()
{
    TcpClientWorkspaceWidget tcpClient;
    TcpServerWorkspaceWidget tcpServer;
    UdpWorkspaceWidget udp;
    HidReportWorkspaceWidget hid;
    MainWindowCommunicationWorkspaceCoordinator coordinator =
        makeCoordinator(tcpClient, tcpServer, udp, hid);

    QCOMPARE(coordinator.currentWorkspace(CommType::TcpClient),
             static_cast<CommunicationWorkspaceWidget*>(&tcpClient));
    QCOMPARE(coordinator.currentWorkspace(CommType::TcpServer),
             static_cast<CommunicationWorkspaceWidget*>(&tcpServer));
    QCOMPARE(coordinator.currentWorkspace(CommType::Udp),
             static_cast<CommunicationWorkspaceWidget*>(&udp));
    QCOMPARE(coordinator.currentWorkspace(CommType::Hid),
             static_cast<CommunicationWorkspaceWidget*>(&hid));
    QVERIFY(coordinator.currentWorkspace(CommType::Serial) == nullptr);
}

void TestMainWindowCommunicationWorkspaceCoordinator::testSyncTcpClientWorkspaceToNetworkConfig()
{
    TcpClientWorkspaceWidget tcpClient;
    TcpServerWorkspaceWidget tcpServer;
    UdpWorkspaceWidget udp;
    HidReportWorkspaceWidget hid;
    MainWindowCommunicationWorkspaceCoordinator coordinator =
        makeCoordinator(tcpClient, tcpServer, udp, hid);

    NetworkConfig workspaceConfig;
    workspaceConfig.mode = NetworkMode::TcpClient;
    workspaceConfig.serverIp = QStringLiteral("192.168.1.25");
    workspaceConfig.serverPort = 1883;
    workspaceConfig.connectTimeout = 4500;
    tcpClient.setConfig(workspaceConfig);

    NetworkConfig networkConfig;
    HidConfig hidConfig;
    coordinator.syncWorkspaceToConfig(CommType::TcpClient, networkConfig, hidConfig);

    QCOMPARE(networkConfig.mode, NetworkMode::TcpClient);
    QCOMPARE(networkConfig.serverIp, workspaceConfig.serverIp);
    QCOMPARE(networkConfig.serverPort, workspaceConfig.serverPort);
    QCOMPARE(networkConfig.connectTimeout, workspaceConfig.connectTimeout);
}

void TestMainWindowCommunicationWorkspaceCoordinator::testSyncTcpServerWorkspaceToNetworkConfig()
{
    TcpClientWorkspaceWidget tcpClient;
    TcpServerWorkspaceWidget tcpServer;
    UdpWorkspaceWidget udp;
    HidReportWorkspaceWidget hid;
    MainWindowCommunicationWorkspaceCoordinator coordinator =
        makeCoordinator(tcpClient, tcpServer, udp, hid);

    NetworkConfig workspaceConfig;
    workspaceConfig.mode = NetworkMode::TcpServer;
    workspaceConfig.listenPort = 9009;
    workspaceConfig.maxConnections = 9;
    tcpServer.setConfig(workspaceConfig);

    NetworkConfig networkConfig;
    HidConfig hidConfig;
    coordinator.syncWorkspaceToConfig(CommType::TcpServer, networkConfig, hidConfig);

    QCOMPARE(networkConfig.mode, NetworkMode::TcpServer);
    QCOMPARE(networkConfig.listenPort, workspaceConfig.listenPort);
    QCOMPARE(networkConfig.maxConnections, workspaceConfig.maxConnections);
}

void TestMainWindowCommunicationWorkspaceCoordinator::testSyncUdpWorkspaceToNetworkConfig()
{
    TcpClientWorkspaceWidget tcpClient;
    TcpServerWorkspaceWidget tcpServer;
    UdpWorkspaceWidget udp;
    HidReportWorkspaceWidget hid;
    MainWindowCommunicationWorkspaceCoordinator coordinator =
        makeCoordinator(tcpClient, tcpServer, udp, hid);

    NetworkConfig workspaceConfig;
    workspaceConfig.mode = NetworkMode::Udp;
    workspaceConfig.listenPort = 7007;
    workspaceConfig.remoteIp = QStringLiteral("239.10.10.10");
    workspaceConfig.remotePort = 7008;
    udp.setConfig(workspaceConfig);

    NetworkConfig networkConfig;
    HidConfig hidConfig;
    coordinator.syncWorkspaceToConfig(CommType::Udp, networkConfig, hidConfig);

    QCOMPARE(networkConfig.mode, NetworkMode::Udp);
    QCOMPARE(networkConfig.listenPort, workspaceConfig.listenPort);
    QCOMPARE(networkConfig.remoteIp, workspaceConfig.remoteIp);
    QCOMPARE(networkConfig.remotePort, workspaceConfig.remotePort);
}

void TestMainWindowCommunicationWorkspaceCoordinator::testSyncHidWorkspaceKeepsDeviceIdentity()
{
    TcpClientWorkspaceWidget tcpClient;
    TcpServerWorkspaceWidget tcpServer;
    UdpWorkspaceWidget udp;
    HidReportWorkspaceWidget hid;
    MainWindowCommunicationWorkspaceCoordinator coordinator =
        makeCoordinator(tcpClient, tcpServer, udp, hid);

    HidConfig selectedDevice;
    selectedDevice.path = QStringLiteral("hid-path");
    selectedDevice.name = QStringLiteral("Selected HID");
    selectedDevice.vendorId = 0x1234;
    selectedDevice.productId = 0x5678;
    selectedDevice.interfaceNumber = 2;
    selectedDevice.usagePage = 0xFF00;
    selectedDevice.usage = 0x0001;
    selectedDevice.inputReportLength = 64;
    selectedDevice.outputReportLength = 64;
    selectedDevice.featureReportLength = 64;
    selectedDevice.outReportId = 1;
    selectedDevice.featureReportId = 2;
    selectedDevice.firstDataIsLength = true;
    selectedDevice.removeInReportId = true;
    hid.setConfig(selectedDevice);

    requireChild<QSpinBox>(hid, "hidInputLengthSpin")->setValue(32);
    requireChild<QSpinBox>(hid, "hidOutputLengthSpin")->setValue(16);
    requireChild<QSpinBox>(hid, "hidFeatureLengthSpin")->setValue(8);
    requireChild<QSpinBox>(hid, "hidOutputReportIdSpin")->setValue(3);
    requireChild<QSpinBox>(hid, "hidFeatureReportIdSpin")->setValue(4);

    NetworkConfig networkConfig;
    HidConfig hidConfig = selectedDevice;
    coordinator.syncWorkspaceToConfig(CommType::Hid, networkConfig, hidConfig);

    QCOMPARE(hidConfig.path, selectedDevice.path);
    QCOMPARE(hidConfig.name, selectedDevice.name);
    QCOMPARE(hidConfig.vendorId, selectedDevice.vendorId);
    QCOMPARE(hidConfig.productId, selectedDevice.productId);
    QCOMPARE(hidConfig.interfaceNumber, selectedDevice.interfaceNumber);
    QCOMPARE(hidConfig.usagePage, selectedDevice.usagePage);
    QCOMPARE(hidConfig.usage, selectedDevice.usage);
    QCOMPARE(hidConfig.inputReportLength, 32);
    QCOMPARE(hidConfig.outputReportLength, 16);
    QCOMPARE(hidConfig.featureReportLength, 8);
    QCOMPARE(hidConfig.outReportId, 3);
    QCOMPARE(hidConfig.featureReportId, 4);
    QVERIFY(hidConfig.firstDataIsLength);
    QVERIFY(hidConfig.removeInReportId);
}
