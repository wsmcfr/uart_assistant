/**
 * @file TestMainWindowCommunicationController.h
 * @brief 主窗口通信控制器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMAINWINDOWCOMMUNICATIONCONTROLLER_H
#define TESTMAINWINDOWCOMMUNICATIONCONTROLLER_H

#include <QObject>
#include <QTest>

/**
 * @brief 主窗口通信控制器回归测试
 *
 * 该测试验证通信后端生命周期从 MainWindow 拆出后，仍能保持打开、
 * 关闭、发送和错误状态等核心行为稳定。
 */
class TestMainWindowCommunicationController : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 未连接时发送应被拒绝，避免误写入空通信对象。
     */
    void testSendDataIsRejectedWhenDisconnected();

    /**
     * @brief 打开成功后应进入连接状态，并把发送数据写入底层通信对象。
     */
    void testOpenSuccessAllowsSendingData();

    /**
     * @brief 打开失败时应保留错误信息，并保持未连接状态。
     */
    void testOpenFailureKeepsDisconnectedStateAndError();

    /**
     * @brief 关闭当前连接时应关闭底层通信对象并清理连接状态。
     */
    void testCloseCurrentClosesCommunicationAndClearsState();
};

#endif // TESTMAINWINDOWCOMMUNICATIONCONTROLLER_H
