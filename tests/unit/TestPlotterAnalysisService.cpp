/**
 * @file TestPlotterAnalysisService.cpp
 * @brief 绘图分析服务单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestPlotterAnalysisService.h"

#include "ui/PlotterAnalysisService.h"
#include "qcustomplot/qcustomplot.h"

using namespace ComAssistant;

namespace {

/**
 * @brief 构造包含指定曲线数据的绘图对象。
 *
 * 主要流程：
 * 1. 创建 QCustomPlot 和一条 QCPGraph；
 * 2. 按样本序号写入 X 值；
 * 3. 把输入数组写入曲线 Y 值，便于测试分析服务的数据抽取。
 *
 * @param values 曲线 Y 值，X 轴按样本序号自动递增。
 * @return 带一条曲线的 QCustomPlot 对象。
 */
QCustomPlot* makePlotWithValues(const QVector<double>& values)
{
    QCustomPlot* plot = new QCustomPlot;
    QCPGraph* graph = plot->addGraph();
    for (int i = 0; i < values.size(); ++i) {
        graph->addData(static_cast<double>(i), values[i]);
    }
    return plot;
}

} // namespace

/**
 * @brief 验证分析服务可以提取 Y 值并计算基础统计。
 *
 * 主要流程：
 * 1. 构造一条 1~4 的测试曲线；
 * 2. 调用 extractYValues 获取原始 Y 值；
 * 3. 调用 calculateBasic 验证平均值、最小值和最大值。
 */
void TestPlotterAnalysisService::testExtractYValuesAndBasicStatistics()
{
    QScopedPointer<QCustomPlot> plot(makePlotWithValues({1.0, 2.0, 3.0, 4.0}));

    const QVector<double> values = PlotterAnalysisService::extractYValues(plot->graph(0));
    const PlotterStatisticsCalculator::BasicStatistics stats =
        PlotterAnalysisService::calculateBasic(plot.data(), 0);

    QCOMPARE(values.size(), 4);
    QCOMPARE(values.first(), 1.0);
    QCOMPARE(values.last(), 4.0);
    QCOMPARE(stats.count, 4);
    QCOMPARE(stats.average, 2.5);
    QCOMPARE(stats.minValue, 1.0);
    QCOMPARE(stats.maxValue, 4.0);
}

/**
 * @brief 验证增强统计保留曲线 X 轴 key 并按区间筛选。
 *
 * 主要流程：
 * 1. 构造非零起点的 X/Y 数据；
 * 2. 传入 10.5~12.5 的统计区间；
 * 3. 确认只统计区间内两个采样点，并保留区间标记。
 */
void TestPlotterAnalysisService::testAdvancedStatisticsUsesGraphKeys()
{
    QScopedPointer<QCustomPlot> plot(new QCustomPlot);
    QCPGraph* graph = plot->addGraph();
    graph->addData(10.0, 1.0);
    graph->addData(11.0, 3.0);
    graph->addData(12.0, 5.0);
    graph->addData(13.0, 99.0);

    const CurveStatistics stats =
        PlotterAnalysisService::calculateAdvanced(plot.data(), 0, 10.5, 12.5);

    QCOMPARE(stats.dataCount, 2);
    QCOMPARE(stats.minValue, 3.0);
    QCOMPARE(stats.maxValue, 5.0);
    QCOMPARE(stats.average, 4.0);
    QCOMPARE(stats.rangeStart, 10.5);
    QCOMPARE(stats.rangeEnd, 12.5);
    QVERIFY(stats.isRangeStats);
}

/**
 * @brief 验证 FFT 输入准备会拒绝样本点不足的曲线。
 *
 * 主要流程：
 * 1. 构造 4 点曲线；
 * 2. 准备 FFT 输入；
 * 3. 断言返回 TooFewSamples，同时保留已提取样本和采样率配置。
 */
void TestPlotterAnalysisService::testPrepareFftInputRejectsShortCurves()
{
    QScopedPointer<QCustomPlot> plot(makePlotWithValues({1.0, 2.0, 3.0, 4.0}));

    const PlotterAnalysisService::FftInput input =
        PlotterAnalysisService::prepareFftInput(plot.data(), 0, 1000.0);

    QVERIFY(!input.valid);
    QCOMPARE(input.error, PlotterAnalysisService::FftInputError::TooFewSamples);
    QCOMPARE(input.samples.size(), 4);
    QCOMPARE(input.config.sampleRate, 1000.0);
}

/**
 * @brief 验证 PID 分析服务复用统计计算器的既有指标口径。
 *
 * 主要流程：
 * 1. 构造包含超调和稳定段的响应曲线；
 * 2. 调用 calculatePid；
 * 3. 对超调、上升时间、调节时间和稳态误差做回归断言。
 */
void TestPlotterAnalysisService::testPidMetricsDelegatesToCalculator()
{
    QScopedPointer<QCustomPlot> plot(makePlotWithValues({
        0.0, 0.05, 0.2, 0.55, 0.92, 1.08,
        1.12, 1.03, 1.01, 1.0, 1.0, 1.0
    }));

    const PlotterStatisticsCalculator::PidMetrics metrics =
        PlotterAnalysisService::calculatePid(plot.data(), 0, 1.0);

    QVERIFY(qAbs(metrics.overshoot - 12.0) < 0.000001);
    QCOMPARE(metrics.riseTime, 2);
    QCOMPARE(metrics.settlingTime, 8);
    QVERIFY(qAbs(metrics.steadyError) < 0.000001);
}
