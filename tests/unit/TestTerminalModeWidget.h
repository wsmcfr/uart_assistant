/**
 * @file TestTerminalModeWidget.h
 * @brief 终端模式交互行为回归测试头文件
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#ifndef TESTTERMINALMODEWIDGET_H
#define TESTTERMINALMODEWIDGET_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证终端模式与主窗口发送/接收回调之间的显示边界。
 *
 * 终端模式的屏幕应保留用户已经在面板内输入的命令，并显示设备返回数据；
 * 主窗口的“发送成功”回调不能再次把发送内容写回屏幕造成重复回显。
 */
class TestTerminalModeWidget : public QObject {
    Q_OBJECT

private slots:
    /**
     * @brief 发送成功回调不应产生本地假回显，真实接收数据仍需显示。
     *
     * 测试流程：
     * 1. 构造终端模式并清屏；
     * 2. 模拟主窗口调用 appendSentData；
     * 3. 导出屏幕内容，确认发送内容没有被写入；
     * 4. 再模拟设备回包，确认真实接收内容仍能显示。
     */
    void testAppendSentDataDoesNotEchoToTerminal();

    /**
     * @brief 面板内输入按 Enter 后应保留当前输入行。
     *
     * 用户没有按清屏时，Enter 只是提交命令并把光标移动到下一行开头；
     * 如果设备没有回包，不应该把刚才输入的内容从屏幕上擦掉。
     */
    void testEnterKeepsTypedCommandVisible();

    /**
     * @brief 多候选 Tab 补全应像 Linux 终端一样列出匹配项。
     *
     * 输入 c 时列出 cd/chmod/chown，继续输入 h 变成 ch 后只列出 chmod/chown；
     * 多候选不能直接替换为某一个命令。
     */
    void testTabCompletionListsMatchingCommands();
};

#endif // TESTTERMINALMODEWIDGET_H
