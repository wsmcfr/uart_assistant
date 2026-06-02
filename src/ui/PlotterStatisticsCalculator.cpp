/**
 * @file PlotterStatisticsCalculator.cpp
 * @brief 绘图统计计算器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "PlotterStatisticsCalculator.h"

#include <QtMath>
#include <algorithm>

namespace ComAssistant {

PlotterStatisticsCalculator::BasicStatistics
PlotterStatisticsCalculator::calculateBasic(const QVector<double>& values)
{
    BasicStatistics stats;
    stats.count = values.size();
    if (values.isEmpty()) {
        return stats;
    }

    /*
     * 使用同一次遍历累计 min/max/sum/sumSq，保持原 PlotterWindow 的
     * 总体标准差算法，避免重构后统计口径发生变化。
     */
    double sum = 0.0;
    double sumSq = 0.0;
    stats.minValue = values.first();
    stats.maxValue = values.first();

    for (double value : values) {
        stats.minValue = qMin(stats.minValue, value);
        stats.maxValue = qMax(stats.maxValue, value);
        sum += value;
        sumSq += value * value;
    }

    stats.average = sum / stats.count;
    const double variance = (sumSq / stats.count) - (stats.average * stats.average);
    stats.stdDev = std::sqrt(qMax(0.0, variance));
    return stats;
}

CurveStatistics PlotterStatisticsCalculator::calculateAdvanced(
    const QVector<Sample>& samples,
    double rangeStart,
    double rangeEnd)
{
    CurveStatistics stats;

    QVector<double> yValues;
    yValues.reserve(samples.size());

    double firstX = 0.0;
    double lastX = 0.0;
    bool hasFirstX = false;
    const bool useRange = rangeStart >= 0.0 && rangeEnd > rangeStart;

    /*
     * 先按区间筛选采样点，再把后续统计都建立在筛选后的 yValues 上。
     * 这样“全量统计”和“选区统计”共用一套公式，不会出现两套口径。
     */
    for (const Sample& sample : samples) {
        if (useRange && (sample.x < rangeStart || sample.x > rangeEnd)) {
            continue;
        }

        yValues.append(sample.y);
        if (!hasFirstX) {
            firstX = sample.x;
            hasFirstX = true;
        }
        lastX = sample.x;
    }

    if (yValues.isEmpty()) {
        return stats;
    }

    stats.dataCount = yValues.size();
    if (useRange) {
        stats.isRangeStats = true;
        stats.rangeStart = rangeStart;
        stats.rangeEnd = rangeEnd;
    }

    const BasicStatistics basic = calculateBasic(yValues);
    stats.minValue = basic.minValue;
    stats.maxValue = basic.maxValue;
    stats.average = basic.average;
    stats.stdDev = basic.stdDev;

    double sumSq = 0.0;
    for (double value : yValues) {
        sumSq += value * value;
    }

    stats.peakToPeak = stats.maxValue - stats.minValue;
    stats.rms = std::sqrt(sumSq / stats.dataCount);

    const double peakAbs = qMax(qAbs(stats.maxValue), qAbs(stats.minValue));
    stats.crestFactor = stats.rms > 0.0 ? peakAbs / stats.rms : 0.0;

    QVector<double> sortedValues = yValues;
    std::sort(sortedValues.begin(), sortedValues.end());
    if (stats.dataCount % 2 == 0) {
        stats.median = (sortedValues[stats.dataCount / 2 - 1] +
                        sortedValues[stats.dataCount / 2]) / 2.0;
    } else {
        stats.median = sortedValues[stats.dataCount / 2];
    }

    stats.duration = lastX - firstX;
    if (stats.duration > 0.0 && stats.dataCount > 1) {
        stats.sampleRate = (stats.dataCount - 1) / stats.duration;
    }

    return stats;
}

PlotterStatisticsCalculator::PidMetrics
PlotterStatisticsCalculator::calculatePidMetrics(const QVector<double>& values,
                                                 double setpoint)
{
    PidMetrics metrics;
    if (values.size() < 10) {
        return metrics;
    }

    const double initialValue = values.first();
    const double responseRange = setpoint - initialValue;
    if (qAbs(responseRange) < 0.001) {
        metrics.steadyError = 0.0;
        return metrics;
    }

    /*
     * 上升时间沿用原窗口算法：寻找响应第一次到达 10% 和 90%
     * 阈值的位置，两者差值即为采样点单位的 rise time。
     */
    const double thresh10 = initialValue + responseRange * 0.1;
    const double thresh90 = initialValue + responseRange * 0.9;
    int idx10 = -1;
    int idx90 = -1;

    for (int i = 0; i < values.size(); ++i) {
        if (idx10 < 0 && ((responseRange > 0.0 && values[i] >= thresh10) ||
                          (responseRange < 0.0 && values[i] <= thresh10))) {
            idx10 = i;
        }
        if (idx90 < 0 && ((responseRange > 0.0 && values[i] >= thresh90) ||
                          (responseRange < 0.0 && values[i] <= thresh90))) {
            idx90 = i;
            break;
        }
    }

    if (idx10 >= 0 && idx90 >= 0) {
        metrics.riseTime = idx90 - idx10;
    }

    double peakValue = values.first();
    for (double value : values) {
        peakValue = responseRange > 0.0 ? qMax(peakValue, value)
                                        : qMin(peakValue, value);
    }

    metrics.overshoot = qAbs(peakValue - setpoint) / qAbs(responseRange) * 100.0;
    if ((responseRange > 0.0 && peakValue <= setpoint) ||
        (responseRange < 0.0 && peakValue >= setpoint)) {
        metrics.overshoot = 0.0;
    }

    const int steadyStart = values.size() * 9 / 10;
    double steadySum = 0.0;
    int steadyCount = 0;
    for (int i = steadyStart; i < values.size(); ++i) {
        steadySum += values[i];
        ++steadyCount;
    }
    const double steadyAverage = steadyCount > 0 ? steadySum / steadyCount : values.last();
    metrics.steadyError = setpoint - steadyAverage;

    /*
     * 调节时间定义为最后一个越出 ±2% 带宽样本后的下一个采样点位置。
     * 如果一开始就全程在带宽内，则保持默认 0。
     */
    const double tolerance = qAbs(responseRange) * 0.02;
    metrics.settlingTime = values.size() - 1;
    for (int i = values.size() - 1; i >= 0; --i) {
        if (qAbs(values[i] - setpoint) > tolerance) {
            metrics.settlingTime = i + 1;
            break;
        }
    }

    return metrics;
}

} // namespace ComAssistant
