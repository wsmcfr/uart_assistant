/**
 * @file TestMainWindowSessionCoordinator.h
 * @brief 主窗口会话协调器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMAINWINDOWSESSIONCOORDINATOR_H
#define TESTMAINWINDOWSESSIONCOORDINATOR_H

#include <QObject>
#include <QTest>

/**
 * @brief 主窗口会话协调器回归测试
 *
 * 该测试覆盖会话恢复时配置对象、工具栏控件、专用工作台和协议安全
 * 回退逻辑，确保相关职责从 MainWindow 拆出后行为不漂移。
 */
class TestMainWindowSessionCoordinator : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief TCP Client 会话应回填网络配置和顶部网络工具栏。
     */
    void testApplyTcpClientSessionUpdatesNetworkToolbar();

    /**
     * @brief UDP 会话远端端口为空时应回退到监听端口。
     */
    void testApplyUdpSessionFallsBackToListenPort();

    /**
     * @brief 刷新端口列表后应能重新选中串口和 HID 设备。
     */
    void testSelectRestoredSerialAndHidDevice();

    /**
     * @brief 会话中的非法协议值应回退为 Raw。
     */
    void testInvalidProtocolFallsBackToRaw();
};

#endif // TESTMAINWINDOWSESSIONCOORDINATOR_H
