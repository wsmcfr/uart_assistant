/**
 * @file TestMainWindowCommunicationWorkspaceCoordinator.h
 * @brief 主窗口通信工作台协调器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMAINWINDOWCOMMUNICATIONWORKSPACECOORDINATOR_H
#define TESTMAINWINDOWCOMMUNICATIONWORKSPACECOORDINATOR_H

#include <QObject>
#include <QTest>

/**
 * @brief 主窗口通信工作台协调器回归测试。
 *
 * 该测试验证 MainWindow 的工作台选择和配置同步逻辑被抽出后，
 * TCP/UDP/HID 配置仍按原有规则回写到通信配置对象。
 */
class TestMainWindowCommunicationWorkspaceCoordinator : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 根据当前通信类型返回对应专用工作台；串口模式不返回专用工作台。
     */
    void testCurrentWorkspaceMatchesCommunicationType();

    /**
     * @brief TCP Client 工作台配置应同步为网络配置并保留客户端模式。
     */
    void testSyncTcpClientWorkspaceToNetworkConfig();

    /**
     * @brief TCP Server 工作台配置应同步为网络配置并保留服务端模式。
     */
    void testSyncTcpServerWorkspaceToNetworkConfig();

    /**
     * @brief UDP 工作台配置应同步为网络配置并保留 UDP 模式。
     */
    void testSyncUdpWorkspaceToNetworkConfig();

    /**
     * @brief HID 工作台只同步 Report 参数，不覆盖设备身份信息。
     */
    void testSyncHidWorkspaceKeepsDeviceIdentity();
};

#endif // TESTMAINWINDOWCOMMUNICATIONWORKSPACECOORDINATOR_H
