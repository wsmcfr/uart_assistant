/**
 * @file TestCommunicationWorkspaces.h
 * @brief 通信类型专用工作台回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef TESTCOMMUNICATIONWORKSPACES_H
#define TESTCOMMUNICATIONWORKSPACES_H

#include <QObject>
#include <QTest>

class TestCommunicationWorkspaces : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证通信工作台共享 HEX 规范化、文本预览和日志格式能力。
     */
    void testCommunicationWorkspaceSharedHelpers();

    /**
     * @brief 验证 TCP 客户端工作台能读写配置并发出发送请求。
     */
    void testTcpClientWorkspaceConfigAndSendSignal();

    /**
     * @brief 验证 TCP 客户端发送辅助、快捷键和日志工具。
     */
    void testTcpClientWorkspaceSendHelpersAndLogTools();

    /**
     * @brief 验证 TCP 服务器工作台能管理客户端目标并区分指定发送和广播发送。
     */
    void testTcpServerWorkspaceClientTargetAndBroadcast();

    /**
     * @brief 验证 TCP 服务器客户端数量、断开入口和发送辅助状态。
     */
    void testTcpServerWorkspaceClientManagementHelpers();

    /**
     * @brief 验证 UDP 工作台能读写本地/远端配置并按选中远端发送。
     */
    void testUdpWorkspaceConfigRecentRemoteAndSendSignal();

    /**
     * @brief 验证 UDP 工作台最近远端清空、HEX 格式化和快捷发送。
     */
    void testUdpWorkspaceRecentRemoteAndSendHelpers();

    /**
     * @brief 验证 HID Report 工作台能读写 Report 参数并发出 Output/Feature 请求。
     */
    void testHidReportWorkspaceConfigAndFeatureSignals();

    /**
     * @brief 验证 HID Report 预览、截断提示、HEX 格式化和历史工具。
     */
    void testHidReportWorkspacePreviewAndHistoryTools();
};

#endif // TESTCOMMUNICATIONWORKSPACES_H
