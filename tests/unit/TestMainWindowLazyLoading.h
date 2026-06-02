/**
 * @file TestMainWindowLazyLoading.h
 * @brief 主窗口重型组件懒加载回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMAINWINDOWLAZYLOADING_H
#define TESTMAINWINDOWLAZYLOADING_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证主窗口不会在启动时一次性创建所有重型通信和显示模式组件。
 */
class TestMainWindowLazyLoading : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 默认启动只创建串口路径，网络/HID 和高级显示模式按需创建。
     */
    void testMainWindowCreatesHeavyWidgetsOnDemand();

    /**
     * @brief 离开高级显示模式或专用通信工作台后，应销毁不用的重型页面。
     */
    void testMainWindowReleasesUnusedHeavyWidgetsAfterSwitchingAway();

    /**
     * @brief 从串口高级模式切到网络工作台后，也应释放隐藏的高级模式组件。
     */
    void testMainWindowReleasesSerialAdvancedModeWhenLeavingSerialWorkspace();

    /**
     * @brief 离开串口工作台释放高级模式时，不应丢失用户上次选择的串口模式。
     */
    void testLeavingSerialWorkspaceKeepsLastSerialDisplaySelection();
};

#endif // TESTMAINWINDOWLAZYLOADING_H
