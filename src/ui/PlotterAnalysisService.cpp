/**
 * @file PlotterAnalysisService.cpp
 * @brief 绘图分析服务实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "PlotterAnalysisService.h"

namespace ComAssistant {

/**
 * @brief 从 QCPGraph 中提取 Y 轴数据序列。
 *
 * 主要流程：
 * 1. 校验曲线是否为空或无数据；
 * 2. 按 QCustomPlot 数据容器顺序遍历；
 * 3. 只复制 value 字段，供基础统计、PID 和 FFT 使用。
 *
 * @param graph 待读取曲线。
 * @return Y 值数组；无效曲线返回空数组。
 */
QVector<double> PlotterAnalysisService::extractYValues(const QCPGraph* graph)
{
    QVector<double> values;
    if (!graph || graph->data()->isEmpty()) {
        return values;
    }

    values.reserve(graph->data()->size());
    for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
        values.append(it->value);
    }
    return values;
}

/**
 * @brief 从 QCPGraph 中提取带 X/Y 的采样点。
 *
 * 主要流程：
 * 1. 校验曲线是否为空或无数据；
 * 2. 保留 key/value 两列数据；
 * 3. 返回给增强统计用于区间筛选、采样率和时长计算。
 *
 * @param graph 待读取曲线。
 * @return 采样点数组；无效曲线返回空数组。
 */
QVector<PlotterStatisticsCalculator::Sample>
PlotterAnalysisService::extractSamples(const QCPGraph* graph)
{
    QVector<PlotterStatisticsCalculator::Sample> samples;
    if (!graph || graph->data()->isEmpty()) {
        return samples;
    }

    samples.reserve(graph->data()->size());
    for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
        PlotterStatisticsCalculator::Sample sample;
        sample.x = it->key;
        sample.y = it->value;
        samples.append(sample);
    }
    return samples;
}

/**
 * @brief 计算指定曲线的基础统计。
 *
 * 主要流程：
 * 1. 通过 graphAt 安全获取曲线；
 * 2. 提取 Y 值序列；
 * 3. 交给 PlotterStatisticsCalculator 计算 min/max/avg/stddev。
 *
 * @param plot 绘图控件。
 * @param curveIndex 曲线索引。
 * @return 基础统计结果；无效曲线返回全 0 结果。
 */
PlotterStatisticsCalculator::BasicStatistics
PlotterAnalysisService::calculateBasic(const QCustomPlot* plot, int curveIndex)
{
    return PlotterStatisticsCalculator::calculateBasic(
        extractYValues(graphAt(plot, curveIndex)));
}

/**
 * @brief 计算指定曲线的 PID 响应指标。
 *
 * 主要流程：
 * 1. 安全获取响应曲线；
 * 2. 提取 Y 值序列；
 * 3. 交给统计计算器按既有口径计算超调、上升时间、调节时间和稳态误差。
 *
 * @param plot 绘图控件。
 * @param curveIndex 响应曲线索引。
 * @param setpoint PID 设定值。
 * @return PID 指标；无效曲线或样本不足返回全 0 结果。
 */
PlotterStatisticsCalculator::PidMetrics
PlotterAnalysisService::calculatePid(const QCustomPlot* plot, int curveIndex, double setpoint)
{
    return PlotterStatisticsCalculator::calculatePidMetrics(
        extractYValues(graphAt(plot, curveIndex)),
        setpoint);
}

/**
 * @brief 计算指定曲线的增强统计。
 *
 * 主要流程：
 * 1. 安全获取曲线并提取 X/Y 采样点；
 * 2. 把选区起止值传给统计计算器；
 * 3. 返回峰峰值、RMS、中位数、采样率等增强指标。
 *
 * @param plot 绘图控件。
 * @param curveIndex 曲线索引。
 * @param rangeStart 统计区间起点；负数表示全量统计。
 * @param rangeEnd 统计区间终点；必须大于起点才生效。
 * @return 增强统计结果；无效曲线返回全 0 结果。
 */
CurveStatistics PlotterAnalysisService::calculateAdvanced(const QCustomPlot* plot,
                                                          int curveIndex,
                                                          double rangeStart,
                                                          double rangeEnd)
{
    return PlotterStatisticsCalculator::calculateAdvanced(
        extractSamples(graphAt(plot, curveIndex)),
        rangeStart,
        rangeEnd);
}

/**
 * @brief 准备 FFT 分析输入。
 *
 * 主要流程：
 * 1. 复制调用方传入的 FFTConfig，并写入当前采样率；
 * 2. 校验曲线索引、空数据和最少 8 点样本要求；
 * 3. 返回曲线名称、样本数组和明确错误码，供 UI 层显示对应提示。
 *
 * @param plot 绘图控件。
 * @param curveIndex 曲线索引。
 * @param sampleRate 当前采样率。
 * @param config FFT 配置模板。
 * @return FFT 输入状态；valid 为 true 时可直接调用 FFTUtils。
 */
PlotterAnalysisService::FftInput PlotterAnalysisService::prepareFftInput(
    const QCustomPlot* plot,
    int curveIndex,
    double sampleRate,
    const FFTConfig& config)
{
    FftInput input;
    input.config = config;
    input.config.sampleRate = sampleRate;

    QCPGraph* graph = graphAt(plot, curveIndex);
    if (!graph) {
        input.error = FftInputError::InvalidCurve;
        return input;
    }

    input.curveName = graph->name();
    input.samples = extractYValues(graph);

    if (input.samples.isEmpty()) {
        input.error = FftInputError::EmptyCurve;
        return input;
    }

    /*
     * FFTUtils 的有效分析至少需要 8 个采样点。这里提前给 UI 层明确原因，
     * 避免窗口对话框自己重复判断曲线数据量。
     */
    if (input.samples.size() < 8) {
        input.error = FftInputError::TooFewSamples;
        return input;
    }

    input.valid = true;
    input.error = FftInputError::None;
    return input;
}

/**
 * @brief 按索引安全获取 QCPGraph。
 *
 * 主要流程：
 * 1. 校验绘图控件是否存在；
 * 2. 校验曲线索引边界；
 * 3. 调用 QCustomPlot::graph 返回曲线指针。
 *
 * @param plot 绘图控件。
 * @param curveIndex 曲线索引。
 * @return 有效曲线指针；无效输入返回 nullptr。
 */
QCPGraph* PlotterAnalysisService::graphAt(const QCustomPlot* plot, int curveIndex)
{
    if (!plot || curveIndex < 0 || curveIndex >= plot->graphCount()) {
        return nullptr;
    }

    return plot->graph(curveIndex);
}

} // namespace ComAssistant
