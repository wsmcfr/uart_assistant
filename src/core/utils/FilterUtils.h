/**
 * @file FilterUtils.h
 * @brief 数字滤波工具类
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef FILTERUTILS_H
#define FILTERUTILS_H

#include <QVector>
#include <QString>

namespace ComAssistant {

/**
 * @brief 数字滤波器类型枚举
 */
enum class DigitalFilterType {
    MovingAverage,      ///< 滑动平均滤波
    Median,             ///< 中值滤波
    ExponentialMA,      ///< 指数移动平均（一阶滞后）
    LowPass,            ///< 低通滤波（Butterworth一阶）
    HighPass,           ///< 高通滤波
    BandPass,           ///< 带通滤波
    BandStop,           ///< 带阻滤波（陷波）
    Kalman              ///< 卡尔曼滤波
};

/**
 * @brief 滤波器配置
 */
struct FilterConfig {
    DigitalFilterType type = DigitalFilterType::MovingAverage;
    int windowSize = 5;           ///< 窗口大小（滑动平均、中值）
    double alpha = 0.3;           ///< 平滑系数（指数移动平均，0-1）
    double cutoffFrequency = 100; ///< 截止频率 (Hz)
    double sampleRate = 1000;     ///< 采样率 (Hz)
    double lowCutoff = 50;        ///< 低截止频率（带通/带阻）
    double highCutoff = 200;      ///< 高截止频率（带通/带阻）
    double processNoise = 0.01;   ///< 卡尔曼过程噪声
    double measureNoise = 0.1;    ///< 卡尔曼测量噪声
};

/**
 * @brief 实时滤波器状态（用于逐点滤波）
 */
struct FilterState {
    QVector<double> buffer;       ///< 数据缓冲区
    double runningSum = 0;        ///< 滑动平均运行和，用于实时路径避免每点重新遍历窗口
    double lastOutput = 0;        ///< 上次输出值
    double kalmanEstimate = 0;    ///< 卡尔曼估计值
    double kalmanError = 1.0;     ///< 卡尔曼误差协方差
    bool initialized = false;     ///< 是否已初始化
};

/**
 * @brief 数字滤波工具类
 *
 * 提供多种数字滤波算法，支持批量处理和实时逐点滤波
 */
class FilterUtils {
public:
    // ===================== 批量滤波 =====================

    /**
     * @brief 使用配置进行批量滤波
     * @param data 输入数据
     * @param config 滤波配置
     * @return 滤波后的数据
     */
    static QVector<double> filter(const QVector<double>& data, const FilterConfig& config);

    /**
     * @brief 滑动平均滤波
     * @param data 输入数据
     * @param windowSize 窗口大小
     * @return 滤波后的数据
     */
    static QVector<double> movingAverage(const QVector<double>& data, int windowSize);

    /**
     * @brief 中值滤波
     * @param data 输入数据
     * @param windowSize 窗口大小（建议奇数）
     * @return 滤波后的数据
     */
    static QVector<double> medianFilter(const QVector<double>& data, int windowSize);

    /**
     * @brief 指数移动平均滤波（一阶滞后滤波）
     * @param data 输入数据
     * @param alpha 平滑系数 (0 < alpha <= 1)，值越小滤波越强
     * @return 滤波后的数据
     */
    static QVector<double> exponentialMA(const QVector<double>& data, double alpha);

    /**
     * @brief 一阶Butterworth低通滤波
     * @param data 输入数据
     * @param cutoffFreq 截止频率 (Hz)
     * @param sampleRate 采样率 (Hz)
     * @return 滤波后的数据
     */
    static QVector<double> lowPassFilter(const QVector<double>& data,
                                         double cutoffFreq, double sampleRate);

    /**
     * @brief 一阶高通滤波
     * @param data 输入数据
     * @param cutoffFreq 截止频率 (Hz)
     * @param sampleRate 采样率 (Hz)
     * @return 滤波后的数据
     */
    static QVector<double> highPassFilter(const QVector<double>& data,
                                          double cutoffFreq, double sampleRate);

    /**
     * @brief 带通滤波
     * @param data 输入数据
     * @param lowCutoff 低截止频率 (Hz)
     * @param highCutoff 高截止频率 (Hz)
     * @param sampleRate 采样率 (Hz)
     * @return 滤波后的数据
     */
    static QVector<double> bandPassFilter(const QVector<double>& data,
                                          double lowCutoff, double highCutoff,
                                          double sampleRate);

    /**
     * @brief 带阻滤波（陷波）
     * @param data 输入数据
     * @param lowCutoff 低截止频率 (Hz)
     * @param highCutoff 高截止频率 (Hz)
     * @param sampleRate 采样率 (Hz)
     * @return 滤波后的数据
     */
    static QVector<double> bandStopFilter(const QVector<double>& data,
                                          double lowCutoff, double highCutoff,
                                          double sampleRate);

    /**
     * @brief 卡尔曼滤波
     * @param data 输入数据
     * @param processNoise 过程噪声协方差 Q
     * @param measureNoise 测量噪声协方差 R
     * @return 滤波后的数据
     */
    static QVector<double> kalmanFilter(const QVector<double>& data,
                                        double processNoise, double measureNoise);

    // ===================== 实时滤波（逐点） =====================

    /**
     * @brief 实时滤波单个数据点
     * @param value 输入值
     * @param config 滤波配置
     * @param state 滤波器状态（需要保持）
     * @return 滤波后的值
     */
    static double filterPoint(double value, const FilterConfig& config, FilterState& state);

    /**
     * @brief 重置滤波器状态
     */
    static void resetState(FilterState& state);

    // ===================== 工具方法 =====================

    /**
     * @brief 获取滤波器类型名称
     * @param type 滤波器类型
     * @return 名称字符串
     */
    static QString getFilterName(DigitalFilterType type);

    /**
     * @brief 获取所有支持的滤波器类型
     * @return 滤波器类型列表
     */
    static QVector<DigitalFilterType> supportedFilters();
};

} // namespace ComAssistant

#endif // FILTERUTILS_H
