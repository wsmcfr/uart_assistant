/**
 * @file TestPlotterStatisticsCalculator.cpp
 * @brief 绘图统计计算器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestPlotterStatisticsCalculator.h"

#include "ui/PlotterStatisticsCalculator.h"

using namespace ComAssistant;

namespace {

/**
 * @brief 构造统计计算器使用的采样点。
 * @param x X 轴值。
 * @param y Y 轴值。
 * @return 采样点结构。
 */
PlotterStatisticsCalculator::Sample sample(double x, double y)
{
    PlotterStatisticsCalculator::Sample result;
    result.x = x;
    result.y = y;
    return result;
}

} // namespace

void TestPlotterStatisticsCalculator::testBasicStatistics()
{
    const QVector<double> values = {1.0, 2.0, 3.0, 4.0};

    const PlotterStatisticsCalculator::BasicStatistics stats =
        PlotterStatisticsCalculator::calculateBasic(values);

    QCOMPARE(stats.count, 4);
    QCOMPARE(stats.minValue, 1.0);
    QCOMPARE(stats.maxValue, 4.0);
    QCOMPARE(stats.average, 2.5);
    QVERIFY(qAbs(stats.stdDev - 1.11803398875) < 0.000001);
}

void TestPlotterStatisticsCalculator::testAdvancedStatistics()
{
    const QVector<PlotterStatisticsCalculator::Sample> samples = {
        sample(0.0, 1.0),
        sample(1.0, -2.0),
        sample(2.0, 3.0),
        sample(3.0, 4.0)
    };

    const CurveStatistics stats =
        PlotterStatisticsCalculator::calculateAdvanced(samples);

    QCOMPARE(stats.dataCount, 4);
    QCOMPARE(stats.minValue, -2.0);
    QCOMPARE(stats.maxValue, 4.0);
    QCOMPARE(stats.average, 1.5);
    QCOMPARE(stats.peakToPeak, 6.0);
    QCOMPARE(stats.median, 2.0);
    QVERIFY(qAbs(stats.rms - std::sqrt(7.5)) < 0.000001);
    QVERIFY(qAbs(stats.crestFactor - (4.0 / std::sqrt(7.5))) < 0.000001);
    QCOMPARE(stats.duration, 3.0);
    QCOMPARE(stats.sampleRate, 1.0);
    QVERIFY(!stats.isRangeStats);
}

void TestPlotterStatisticsCalculator::testAdvancedStatisticsWithRange()
{
    const QVector<PlotterStatisticsCalculator::Sample> samples = {
        sample(0.0, 10.0),
        sample(1.0, 2.0),
        sample(2.0, 4.0),
        sample(3.0, 6.0),
        sample(4.0, 100.0)
    };

    const CurveStatistics stats =
        PlotterStatisticsCalculator::calculateAdvanced(samples, 1.0, 3.0);

    QCOMPARE(stats.dataCount, 3);
    QCOMPARE(stats.minValue, 2.0);
    QCOMPARE(stats.maxValue, 6.0);
    QCOMPARE(stats.average, 4.0);
    QCOMPARE(stats.median, 4.0);
    QCOMPARE(stats.rangeStart, 1.0);
    QCOMPARE(stats.rangeEnd, 3.0);
    QCOMPARE(stats.duration, 2.0);
    QCOMPARE(stats.sampleRate, 1.0);
    QVERIFY(stats.isRangeStats);
}

void TestPlotterStatisticsCalculator::testPidMetrics()
{
    const QVector<double> response = {
        0.0, 0.05, 0.2, 0.55, 0.92, 1.08,
        1.12, 1.03, 1.01, 1.0, 1.0, 1.0
    };

    const PlotterStatisticsCalculator::PidMetrics metrics =
        PlotterStatisticsCalculator::calculatePidMetrics(response, 1.0);

    QVERIFY(qAbs(metrics.overshoot - 12.0) < 0.000001);
    QCOMPARE(metrics.riseTime, 2);
    QCOMPARE(metrics.settlingTime, 8);
    QVERIFY(qAbs(metrics.steadyError) < 0.000001);
}
