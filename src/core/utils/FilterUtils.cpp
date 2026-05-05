/**
 * @file FilterUtils.cpp
 * @brief 数字滤波工具类实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "FilterUtils.h"
#include <QObject>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ComAssistant {

QVector<double> FilterUtils::filter(const QVector<double>& data, const FilterConfig& config)
{
    switch (config.type) {
    case DigitalFilterType::MovingAverage:
        return movingAverage(data, config.windowSize);

    case DigitalFilterType::Median:
        return medianFilter(data, config.windowSize);

    case DigitalFilterType::ExponentialMA:
        return exponentialMA(data, config.alpha);

    case DigitalFilterType::LowPass:
        return lowPassFilter(data, config.cutoffFrequency, config.sampleRate);

    case DigitalFilterType::HighPass:
        return highPassFilter(data, config.cutoffFrequency, config.sampleRate);

    case DigitalFilterType::BandPass:
        return bandPassFilter(data, config.lowCutoff, config.highCutoff, config.sampleRate);

    case DigitalFilterType::BandStop:
        return bandStopFilter(data, config.lowCutoff, config.highCutoff, config.sampleRate);

    case DigitalFilterType::Kalman:
        return kalmanFilter(data, config.processNoise, config.measureNoise);

    default:
        return data;
    }
}

QVector<double> FilterUtils::movingAverage(const QVector<double>& data, int windowSize)
{
    if (data.isEmpty() || windowSize <= 0) {
        return data;
    }

    windowSize = qMin(windowSize, data.size());
    QVector<double> result(data.size());

    // 计算初始窗口和
    double sum = 0;
    int halfWindow = windowSize / 2;

    for (int i = 0; i < data.size(); ++i) {
        // 计算当前窗口范围
        int start = qMax(0, i - halfWindow);
        int end = qMin(data.size() - 1, i + halfWindow);
        int count = end - start + 1;

        // 计算窗口内平均值
        sum = 0;
        for (int j = start; j <= end; ++j) {
            sum += data[j];
        }
        result[i] = sum / count;
    }

    return result;
}

QVector<double> FilterUtils::medianFilter(const QVector<double>& data, int windowSize)
{
    if (data.isEmpty() || windowSize <= 0) {
        return data;
    }

    windowSize = qMin(windowSize, data.size());
    if (windowSize % 2 == 0) {
        windowSize++;  // 确保窗口大小为奇数
    }

    QVector<double> result(data.size());
    int halfWindow = windowSize / 2;

    for (int i = 0; i < data.size(); ++i) {
        // 计算当前窗口范围
        int start = qMax(0, i - halfWindow);
        int end = qMin(data.size() - 1, i + halfWindow);

        // 收集窗口内的值
        QVector<double> window;
        for (int j = start; j <= end; ++j) {
            window.append(data[j]);
        }

        // 排序并取中值
        std::sort(window.begin(), window.end());
        result[i] = window[window.size() / 2];
    }

    return result;
}

QVector<double> FilterUtils::exponentialMA(const QVector<double>& data, double alpha)
{
    if (data.isEmpty()) {
        return data;
    }

    alpha = qBound(0.001, alpha, 1.0);
    QVector<double> result(data.size());

    result[0] = data[0];
    for (int i = 1; i < data.size(); ++i) {
        // EMA: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
        result[i] = alpha * data[i] + (1 - alpha) * result[i - 1];
    }

    return result;
}

QVector<double> FilterUtils::lowPassFilter(const QVector<double>& data,
                                           double cutoffFreq, double sampleRate)
{
    if (data.isEmpty() || cutoffFreq <= 0 || sampleRate <= 0) {
        return data;
    }

    // 一阶Butterworth低通滤波器
    // 时间常数 RC = 1 / (2 * pi * fc)
    // alpha = dt / (RC + dt), dt = 1 / fs
    double dt = 1.0 / sampleRate;
    double RC = 1.0 / (2.0 * M_PI * cutoffFreq);
    double alpha = dt / (RC + dt);

    QVector<double> result(data.size());
    result[0] = data[0];

    for (int i = 1; i < data.size(); ++i) {
        result[i] = alpha * data[i] + (1 - alpha) * result[i - 1];
    }

    return result;
}

QVector<double> FilterUtils::highPassFilter(const QVector<double>& data,
                                            double cutoffFreq, double sampleRate)
{
    if (data.isEmpty() || cutoffFreq <= 0 || sampleRate <= 0) {
        return data;
    }

    // 一阶高通滤波器
    // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
    double dt = 1.0 / sampleRate;
    double RC = 1.0 / (2.0 * M_PI * cutoffFreq);
    double alpha = RC / (RC + dt);

    QVector<double> result(data.size());
    result[0] = data[0];

    for (int i = 1; i < data.size(); ++i) {
        result[i] = alpha * (result[i - 1] + data[i] - data[i - 1]);
    }

    return result;
}

QVector<double> FilterUtils::bandPassFilter(const QVector<double>& data,
                                            double lowCutoff, double highCutoff,
                                            double sampleRate)
{
    if (data.isEmpty() || lowCutoff >= highCutoff) {
        return data;
    }

    // 带通滤波 = 高通 + 低通
    QVector<double> highPassed = highPassFilter(data, lowCutoff, sampleRate);
    return lowPassFilter(highPassed, highCutoff, sampleRate);
}

QVector<double> FilterUtils::bandStopFilter(const QVector<double>& data,
                                            double lowCutoff, double highCutoff,
                                            double sampleRate)
{
    if (data.isEmpty() || lowCutoff >= highCutoff) {
        return data;
    }

    // 带阻滤波 = 原信号 - 带通信号
    QVector<double> bandPassed = bandPassFilter(data, lowCutoff, highCutoff, sampleRate);
    QVector<double> result(data.size());

    for (int i = 0; i < data.size(); ++i) {
        result[i] = data[i] - bandPassed[i];
    }

    return result;
}

QVector<double> FilterUtils::kalmanFilter(const QVector<double>& data,
                                          double processNoise, double measureNoise)
{
    if (data.isEmpty()) {
        return data;
    }

    QVector<double> result(data.size());

    // 初始化
    double estimate = data[0];
    double errorCovariance = 1.0;

    for (int i = 0; i < data.size(); ++i) {
        // 预测步骤
        // 预测值 = 上次估计值（假设状态不变）
        // 预测误差协方差 = 上次误差协方差 + 过程噪声
        double predictedError = errorCovariance + processNoise;

        // 更新步骤
        // 卡尔曼增益 K = P / (P + R)
        double kalmanGain = predictedError / (predictedError + measureNoise);

        // 更新估计值
        estimate = estimate + kalmanGain * (data[i] - estimate);

        // 更新误差协方差
        errorCovariance = (1 - kalmanGain) * predictedError;

        result[i] = estimate;
    }

    return result;
}

double FilterUtils::filterPoint(double value, const FilterConfig& config, FilterState& state)
{
    /*
     * 实时滤波配置可能来自旧会话、脚本或未来的自动恢复，不能假设窗口大小
     * 始终来自当前对话框范围。这里统一规整到可用窗口，避免空缓冲、除零和
     * removeFirst 在异常配置下反复触发。
     */
    const int safeWindowSize = qMax(1, config.windowSize);

    // 初始化
    if (!state.initialized) {
        state.lastOutput = value;
        state.kalmanEstimate = value;
        state.kalmanError = 1.0;
        state.runningSum = 0.0;
        state.initialized = true;

        if (config.type == DigitalFilterType::MovingAverage ||
            config.type == DigitalFilterType::Median) {
            state.buffer.resize(safeWindowSize);
            state.buffer.fill(value);
            state.runningSum = value * safeWindowSize;
        }

        return value;
    }

    double output = value;

    switch (config.type) {
    case DigitalFilterType::MovingAverage: {
        /*
         * 滑动平均处在绘图实时滤波高频路径。维护运行和可把每点复杂度从
         * O(windowSize) 降到 O(1)，窗口过大时不再拖慢 UI 刷新。
         */
        state.buffer.append(value);
        state.runningSum += value;
        const int overflow = state.buffer.size() - safeWindowSize;
        if (overflow > 0) {
            for (int i = 0; i < overflow; ++i) {
                state.runningSum -= state.buffer[i];
            }
            state.buffer.remove(0, overflow);
        }
        output = state.runningSum / state.buffer.size();
        break;
    }

    case DigitalFilterType::Median: {
        // 中值滤波同样使用安全窗口，并一次性裁剪溢出旧点。
        state.buffer.append(value);
        const int overflow = state.buffer.size() - safeWindowSize;
        if (overflow > 0) {
            state.buffer.remove(0, overflow);
        }
        QVector<double> sorted = state.buffer;
        std::sort(sorted.begin(), sorted.end());
        output = sorted[sorted.size() / 2];
        break;
    }

    case DigitalFilterType::ExponentialMA: {
        // 指数移动平均
        output = config.alpha * value + (1 - config.alpha) * state.lastOutput;
        break;
    }

    case DigitalFilterType::LowPass: {
        // 低通滤波
        double dt = 1.0 / config.sampleRate;
        double RC = 1.0 / (2.0 * M_PI * config.cutoffFrequency);
        double alpha = dt / (RC + dt);
        output = alpha * value + (1 - alpha) * state.lastOutput;
        break;
    }

    case DigitalFilterType::HighPass: {
        // 高通滤波
        double dt = 1.0 / config.sampleRate;
        double RC = 1.0 / (2.0 * M_PI * config.cutoffFrequency);
        double alpha = RC / (RC + dt);
        // 需要保存上一个输入值
        if (state.buffer.isEmpty()) {
            state.buffer.append(value);
            output = 0;
        } else {
            double lastInput = state.buffer.last();
            output = alpha * (state.lastOutput + value - lastInput);
            state.buffer[0] = value;
        }
        break;
    }

    case DigitalFilterType::Kalman: {
        // 卡尔曼滤波
        double predictedError = state.kalmanError + config.processNoise;
        double kalmanGain = predictedError / (predictedError + config.measureNoise);
        state.kalmanEstimate = state.kalmanEstimate + kalmanGain * (value - state.kalmanEstimate);
        state.kalmanError = (1 - kalmanGain) * predictedError;
        output = state.kalmanEstimate;
        break;
    }

    default:
        output = value;
        break;
    }

    state.lastOutput = output;
    return output;
}

void FilterUtils::resetState(FilterState& state)
{
    state.buffer.clear();
    state.runningSum = 0;
    state.lastOutput = 0;
    state.kalmanEstimate = 0;
    state.kalmanError = 1.0;
    state.initialized = false;
}

QString FilterUtils::getFilterName(DigitalFilterType type)
{
    switch (type) {
    case DigitalFilterType::MovingAverage:
        return QObject::tr("滑动平均");
    case DigitalFilterType::Median:
        return QObject::tr("中值滤波");
    case DigitalFilterType::ExponentialMA:
        return QObject::tr("指数移动平均");
    case DigitalFilterType::LowPass:
        return QObject::tr("低通滤波");
    case DigitalFilterType::HighPass:
        return QObject::tr("高通滤波");
    case DigitalFilterType::BandPass:
        return QObject::tr("带通滤波");
    case DigitalFilterType::BandStop:
        return QObject::tr("带阻滤波");
    case DigitalFilterType::Kalman:
        return QObject::tr("卡尔曼滤波");
    default:
        return QObject::tr("未知");
    }
}

QVector<DigitalFilterType> FilterUtils::supportedFilters()
{
    return {
        DigitalFilterType::MovingAverage,
        DigitalFilterType::Median,
        DigitalFilterType::ExponentialMA,
        DigitalFilterType::LowPass,
        DigitalFilterType::HighPass,
        DigitalFilterType::BandPass,
        DigitalFilterType::BandStop,
        DigitalFilterType::Kalman
    };
}

} // namespace ComAssistant
