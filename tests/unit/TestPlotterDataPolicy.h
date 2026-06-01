/**
 * @file TestPlotterDataPolicy.h
 * @brief 绘图数据策略单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTPLOTTERDATAPOLICY_H
#define TESTPLOTTERDATAPOLICY_H

#include <QObject>
#include <QTest>

/**
 * @brief 绘图数据策略回归测试
 *
 * 该测试覆盖抽稀状态机和实时数据裁剪计划，保证 PlotterWindow 后续
 * 调整绘图 UI 时不会误改数据保留策略。
 */
class TestPlotterDataPolicy : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 无抽稀模式应保留每个数据点。
     */
    void testNoDecimationKeepsEveryPoint();

    /**
     * @brief 1:N 抽稀应跳过前 N-1 个点并保留第 N 个点。
     */
    void testDecimationKeepsNthPoint();

    /**
     * @brief 重设抽稀比例应清零内部计数器。
     */
    void testResetDecimationClearsCounter();

    /**
     * @brief 超出最大点数时应裁剪到 90% 目标数量。
     */
    void testTrimPlanKeepsNinetyPercentOfLimit();
};

#endif // TESTPLOTTERDATAPOLICY_H
