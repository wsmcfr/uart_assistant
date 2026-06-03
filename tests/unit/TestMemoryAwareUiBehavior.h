/**
 * @file TestMemoryAwareUiBehavior.h
 * @brief 内存友好 UI 行为回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMEMORYAWAREUIBEHAVIOR_H
#define TESTMEMORYAWAREUIBEHAVIOR_H

#include <QObject>
#include <QTest>

/**
 * @brief 覆盖用户反馈的曲线命名入口和交互控件遮挡问题。
 */
class TestMemoryAwareUiBehavior : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 绘图控制面板应提供可直接点击的曲线重命名入口。
     */
    void testPlotControlPanelExposesCurveRenameAction();

    /**
     * @brief 滑动条控件应给数值、单位和范围文本预留足够布局空间。
     */
    void testSliderControlLayoutAvoidsTextOverlap();

    /**
     * @brief 数据表格清空后应释放已落屏记录和待刷新记录的历史容量。
     */
    void testDataTableClearReleasesRecordCapacities();
};

#endif // TESTMEMORYAWAREUIBEHAVIOR_H
