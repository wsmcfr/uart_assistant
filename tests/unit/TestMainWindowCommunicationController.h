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
     * @brief 底层写入失败时应保留队首，恢复后可重试同一数据。
     */
    void testFailedSendIsKeptForRetry();

    /**
     * @brief 关闭连接时应取消所有待发送任务，避免旧连接数据泄漏到新连接。
     */
    void testCloseCurrentCancelsPendingSends();

    /**
     * @brief 打开失败时应保留错误信息，并保持未连接状态。
     */
    void testOpenFailureKeepsDisconnectedStateAndError();

    /**
     * @brief 关闭当前连接时应关闭底层通信对象并清理连接状态。
     */
    void testCloseCurrentClosesCommunicationAndClearsState();

    /**
     * @brief 文件传输发送应等待串口异步发空后才通知成功。
     *
     * 主要流程：假通信对象先让 write() 成功，再挂起 transmit drain；
     * 控制器不能在 drain 完成前回调文件传输状态机，避免 UI 提前显示完成。
     */
    void testFileTransferSendWaitsForAsyncDrainBeforeSuccessCallback();

    /**
     * @brief 文件传输发送在串口发空失败时应把失败原因回传给状态机。
     *
     * 主要流程：假通信对象模拟 drain 超时，控制器应回调失败并保存错误文本，
     * 让 Raw/OTA 传输停止，而不是继续读取下一块。
     */
    void testFileTransferSendReportsDrainFailure();
};

#endif // TESTMAINWINDOWCOMMUNICATIONCONTROLLER_H
