/**
 * @file PlotterAnalysisService.h
 * @brief 绘图分析服务
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_PLOTTERANALYSISSERVICE_H
#define COMASSISTANT_PLOTTERANALYSISSERVICE_H

#include "PlotterStatisticsCalculator.h"
#include "core/utils/FFTUtils.h"
#include "qcustomplot/qcustomplot.h"

#include <QVector>

namespace ComAssistant {

/**
 * @brief 绘图分析服务。
 *
 * 该类负责把 QCustomPlot 曲线数据转换为分析层需要的纯数据结构，
 * 并统一调用统计、PID 与 FFT 输入准备逻辑。窗口层因此只负责对话框和结果展示。
 */
class PlotterAnalysisService
{
public:
    /**
     * @brief FFT 输入准备失败原因。
     */
    enum class FftInputError
    {
        None,          ///< 输入有效，没有错误。
        InvalidCurve,  ///< 曲线索引无效或曲线不存在。
        EmptyCurve,    ///< 曲线没有数据。
        TooFewSamples  ///< 样本点少于 FFT 分析最低要求。
    };

    /**
     * @brief FFT 分析输入。
     */
    struct FftInput
    {
        bool valid = false;                 ///< 是否可直接进入 FFTUtils 分析。
        FftInputError error = FftInputError::None; ///< 输入状态。
        QVector<double> samples;           ///< 从曲线提取出的 Y 值序列。
        FFTConfig config;                  ///< 带采样率的 FFT 配置。
        QString curveName;                 ///< 曲线名称，用于结果窗口标题和日志。
    };

    /**
     * @brief 从曲线中提取 Y 值。
     * @param graph 曲线对象。
     * @return Y 值序列；曲线为空时返回空列表。
     */
    static QVector<double> extractYValues(const QCPGraph* graph);

    /**
     * @brief 从曲线中提取 X/Y 采样点。
     * @param graph 曲线对象。
     * @return 采样点序列；曲线为空时返回空列表。
     */
    static QVector<PlotterStatisticsCalculator::Sample> extractSamples(const QCPGraph* graph);

    /**
     * @brief 计算基础统计。
     * @param plot 绘图控件。
     * @param curveIndex 曲线索引。
     * @return 基础统计结果；无效曲线返回全 0。
     */
    static PlotterStatisticsCalculator::BasicStatistics calculateBasic(const QCustomPlot* plot,
                                                                       int curveIndex);

    /**
     * @brief 计算 PID 指标。
     * @param plot 绘图控件。
     * @param curveIndex 曲线索引。
     * @param setpoint 目标设定值。
     * @return PID 指标；无效曲线或样本不足返回全 0。
     */
    static PlotterStatisticsCalculator::PidMetrics calculatePid(const QCustomPlot* plot,
                                                                int curveIndex,
                                                                double setpoint);

    /**
     * @brief 计算增强统计。
     * @param plot 绘图控件。
     * @param curveIndex 曲线索引。
     * @param rangeStart 统计区间起点。
     * @param rangeEnd 统计区间终点。
     * @return 增强统计结果；无效曲线返回全 0。
     */
    static CurveStatistics calculateAdvanced(const QCustomPlot* plot,
                                             int curveIndex,
                                             double rangeStart = -1.0,
                                             double rangeEnd = -1.0);

    /**
     * @brief 准备 FFT 分析输入。
     * @param plot 绘图控件。
     * @param curveIndex 曲线索引。
     * @param sampleRate 当前采样率。
     * @param config 输入 FFT 配置；函数会覆盖其中的 sampleRate。
     * @return FFT 输入状态和样本数据。
     */
    static FftInput prepareFftInput(const QCustomPlot* plot,
                                    int curveIndex,
                                    double sampleRate,
                                    const FFTConfig& config = FFTConfig());

private:
    /**
     * @brief 按索引安全获取曲线。
     * @param plot 绘图控件。
     * @param curveIndex 曲线索引。
     * @return 曲线指针；无效时返回 nullptr。
     */
    static QCPGraph* graphAt(const QCustomPlot* plot, int curveIndex);
};

} // namespace ComAssistant

#endif // COMASSISTANT_PLOTTERANALYSISSERVICE_H
