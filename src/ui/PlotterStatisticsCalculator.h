/**
 * @file PlotterStatisticsCalculator.h
 * @brief 绘图统计计算器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_PLOTTERSTATISTICSCALCULATOR_H
#define COMASSISTANT_PLOTTERSTATISTICSCALCULATOR_H

#include "PlotterWindow.h"

#include <QVector>

namespace ComAssistant {

/**
 * @brief 绘图统计计算器。
 *
 * 该类承接 PlotterWindow 中不依赖 UI 的统计计算职责。调用方只需要
 * 提供采样值或采样点，计算器返回纯数据结果，避免统计公式散落在
 * 窗口类的对话框、菜单和绘图控件逻辑中。
 */
class PlotterStatisticsCalculator
{
public:
    /**
     * @brief 带 X/Y 的采样点。
     */
    struct Sample
    {
        double x = 0.0; ///< X 轴位置，通常是时间或样本序号。
        double y = 0.0; ///< Y 轴采样值。
    };

    /**
     * @brief 基础统计结果。
     */
    struct BasicStatistics
    {
        int count = 0;         ///< 数据点数量。
        double minValue = 0.0; ///< 最小值。
        double maxValue = 0.0; ///< 最大值。
        double average = 0.0;  ///< 平均值。
        double stdDev = 0.0;   ///< 标准差。
    };

    /**
     * @brief PID 响应指标。
     */
    struct PidMetrics
    {
        double overshoot = 0.0; ///< 超调量百分比。
        int settlingTime = 0;   ///< 调节时间，单位为采样点数。
        int riseTime = 0;       ///< 上升时间，单位为采样点数。
        double steadyError = 0.0; ///< 稳态误差。
    };

    /**
     * @brief 计算基础统计。
     * @param values Y 轴数据序列。
     * @return 基础统计结果；空输入返回全 0。
     */
    static BasicStatistics calculateBasic(const QVector<double>& values);

    /**
     * @brief 计算增强统计。
     * @param samples 带 X/Y 的采样点序列。
     * @param rangeStart 统计区间起点；负数表示不限制区间。
     * @param rangeEnd 统计区间终点；必须大于 rangeStart 才生效。
     * @return 增强统计结果；无有效样本时返回全 0。
     */
    static CurveStatistics calculateAdvanced(const QVector<Sample>& samples,
                                             double rangeStart = -1.0,
                                             double rangeEnd = -1.0);

    /**
     * @brief 计算 PID 响应指标。
     * @param values 响应曲线 Y 值序列。
     * @param setpoint 目标设定值。
     * @return PID 响应指标；样本不足时返回全 0。
     */
    static PidMetrics calculatePidMetrics(const QVector<double>& values,
                                          double setpoint);
};

} // namespace ComAssistant

#endif // COMASSISTANT_PLOTTERSTATISTICSCALCULATOR_H
