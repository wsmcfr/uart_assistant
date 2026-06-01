/**
 * @file TestPlotterStatisticsCalculator.h
 * @brief 绘图统计计算器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTPLOTTERSTATISTICSCALCULATOR_H
#define TESTPLOTTERSTATISTICSCALCULATOR_H

#include <QObject>
#include <QTest>

/**
 * @brief 绘图统计计算器回归测试
 *
 * 该测试验证 PlotterWindow 中可独立计算的统计和 PID 指标已经下沉到
 * 纯逻辑计算器，后续改 UI 或 QCustomPlot 集成时不影响计算正确性。
 */
class TestPlotterStatisticsCalculator : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 基础统计应计算最小值、最大值、均值和标准差。
     */
    void testBasicStatistics();

    /**
     * @brief 增强统计应计算中位数、RMS、峰峰值和采样率。
     */
    void testAdvancedStatistics();

    /**
     * @brief 区间统计应只采纳落在 X 轴范围内的数据点。
     */
    void testAdvancedStatisticsWithRange();

    /**
     * @brief PID 指标应计算超调、上升时间、调节时间和稳态误差。
     */
    void testPidMetrics();
};

#endif // TESTPLOTTERSTATISTICSCALCULATOR_H
