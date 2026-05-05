/**
 * @file FFTUtils.cpp
 * @brief FFT 频谱分析工具类实现（增强版）
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "FFTUtils.h"
#include <algorithm>
#include <numeric>
#include <limits>

namespace ComAssistant {

const double PI = 3.14159265358979323846;

// ==================== 工具函数 ====================

int FFTUtils::nextPowerOfTwo(int n)
{
    if (n <= 1) {
        return 1;
    }

    int power = 1;
    while (power < n && power <= (std::numeric_limits<int>::max() / 2)) {
        power *= 2;
    }
    return power;
}

double FFTUtils::besselI0(double x)
{
    // 修正贝塞尔函数 I0 的级数展开近似
    // I0(x) = Σ (x²/4)^k / (k!)²
    double sum = 1.0;
    double term = 1.0;
    double x2 = x * x / 4.0;

    for (int k = 1; k < 50; ++k) {
        term *= x2 / (k * k);
        sum += term;
        // 相对误差 < 1e-15 时提前退出
        if (term < 1e-15 * sum) break;
    }

    return sum;
}

void FFTUtils::bitReversalPermutation(QVector<std::complex<double>>& data)
{
    int n = data.size();
    int bits = static_cast<int>(std::log2(n));

    for (int i = 0; i < n; ++i) {
        int j = 0;
        for (int k = 0; k < bits; ++k) {
            if (i & (1 << k)) {
                j |= (1 << (bits - 1 - k));
            }
        }
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }
}

// ==================== 核心 FFT 函数 ====================

QVector<std::complex<double>> FFTUtils::fft(const QVector<std::complex<double>>& input)
{
    int n = input.size();
    int paddedSize = nextPowerOfTwo(n);
    QVector<std::complex<double>> data(paddedSize, std::complex<double>(0, 0));

    for (int i = 0; i < n; ++i) {
        data[i] = input[i];
    }

    bitReversalPermutation(data);

    // Cooley-Tukey FFT
    for (int len = 2; len <= paddedSize; len *= 2) {
        double angle = -2.0 * PI / len;
        std::complex<double> wn(std::cos(angle), std::sin(angle));

        for (int i = 0; i < paddedSize; i += len) {
            std::complex<double> w(1, 0);
            for (int j = 0; j < len / 2; ++j) {
                std::complex<double> u = data[i + j];
                std::complex<double> t = w * data[i + j + len / 2];
                data[i + j] = u + t;
                data[i + j + len / 2] = u - t;
                w *= wn;
            }
        }
    }

    return data;
}

QVector<std::complex<double>> FFTUtils::ifft(const QVector<std::complex<double>>& input)
{
    int n = input.size();
    QVector<std::complex<double>> conjugate(n);

    for (int i = 0; i < n; ++i) {
        conjugate[i] = std::conj(input[i]);
    }

    QVector<std::complex<double>> result = fft(conjugate);

    // 归一化使用原始输入大小 n，而非 fft 可能补零后的 result.size()
    for (int i = 0; i < result.size(); ++i) {
        result[i] = std::conj(result[i]) / static_cast<double>(n);
    }

    return result;
}

// ==================== 窗口函数实现 ====================

QVector<double> FFTUtils::applyWindow(const QVector<double>& data, WindowType type, double param)
{
    switch (type) {
    case WindowType::Rectangular:
        return applyRectangularWindow(data);
    case WindowType::Hanning:
        return applyHanningWindow(data);
    case WindowType::Hamming:
        return applyHammingWindow(data);
    case WindowType::Blackman:
        return applyBlackmanWindow(data);
    case WindowType::BlackmanHarris:
        return applyBlackmanHarrisWindow(data);
    case WindowType::Kaiser:
        return applyKaiserWindow(data, param > 0 ? param : 5.0);
    case WindowType::FlatTop:
        return applyFlatTopWindow(data);
    case WindowType::Gaussian:
        return applyGaussianWindow(data, param > 0 ? param : 0.4);
    default:
        return applyHanningWindow(data);
    }
}

QVector<double> FFTUtils::applyRectangularWindow(const QVector<double>& data)
{
    return data;
}

QVector<double> FFTUtils::applyHanningWindow(const QVector<double>& data)
{
    int n = data.size();
    QVector<double> result(n);
    for (int i = 0; i < n; ++i) {
        double window = 0.5 * (1.0 - std::cos(2.0 * PI * i / (n - 1)));
        result[i] = data[i] * window;
    }
    return result;
}

QVector<double> FFTUtils::applyHammingWindow(const QVector<double>& data)
{
    int n = data.size();
    QVector<double> result(n);
    for (int i = 0; i < n; ++i) {
        double window = 0.54 - 0.46 * std::cos(2.0 * PI * i / (n - 1));
        result[i] = data[i] * window;
    }
    return result;
}

QVector<double> FFTUtils::applyBlackmanWindow(const QVector<double>& data)
{
    int n = data.size();
    QVector<double> result(n);
    for (int i = 0; i < n; ++i) {
        double window = 0.42 - 0.5 * std::cos(2.0 * PI * i / (n - 1))
                       + 0.08 * std::cos(4.0 * PI * i / (n - 1));
        result[i] = data[i] * window;
    }
    return result;
}

QVector<double> FFTUtils::applyBlackmanHarrisWindow(const QVector<double>& data)
{
    int n = data.size();
    QVector<double> result(n);
    const double a0 = 0.35875;
    const double a1 = 0.48829;
    const double a2 = 0.14128;
    const double a3 = 0.01168;

    for (int i = 0; i < n; ++i) {
        double window = a0 - a1 * std::cos(2.0 * PI * i / (n - 1))
                          + a2 * std::cos(4.0 * PI * i / (n - 1))
                          - a3 * std::cos(6.0 * PI * i / (n - 1));
        result[i] = data[i] * window;
    }
    return result;
}

QVector<double> FFTUtils::applyKaiserWindow(const QVector<double>& data, double beta)
{
    int n = data.size();
    QVector<double> result(n);
    double denom = besselI0(beta);

    for (int i = 0; i < n; ++i) {
        double ratio = 2.0 * i / (n - 1) - 1.0;
        double arg = beta * std::sqrt(1.0 - ratio * ratio);
        double window = besselI0(arg) / denom;
        result[i] = data[i] * window;
    }
    return result;
}

QVector<double> FFTUtils::applyFlatTopWindow(const QVector<double>& data)
{
    int n = data.size();
    QVector<double> result(n);
    const double a0 = 0.21557895;
    const double a1 = 0.41663158;
    const double a2 = 0.277263158;
    const double a3 = 0.083578947;
    const double a4 = 0.006947368;

    for (int i = 0; i < n; ++i) {
        double window = a0 - a1 * std::cos(2.0 * PI * i / (n - 1))
                          + a2 * std::cos(4.0 * PI * i / (n - 1))
                          - a3 * std::cos(6.0 * PI * i / (n - 1))
                          + a4 * std::cos(8.0 * PI * i / (n - 1));
        result[i] = data[i] * window;
    }
    return result;
}

QVector<double> FFTUtils::applyGaussianWindow(const QVector<double>& data, double sigma)
{
    int n = data.size();
    QVector<double> result(n);

    for (int i = 0; i < n; ++i) {
        double ratio = (i - (n - 1) / 2.0) / (sigma * (n - 1) / 2.0);
        double window = std::exp(-0.5 * ratio * ratio);
        result[i] = data[i] * window;
    }
    return result;
}

QString FFTUtils::getWindowName(WindowType type)
{
    switch (type) {
    case WindowType::Rectangular:    return QStringLiteral("矩形窗");
    case WindowType::Hanning:        return QStringLiteral("汉宁窗");
    case WindowType::Hamming:        return QStringLiteral("汉明窗");
    case WindowType::Blackman:       return QStringLiteral("布莱克曼窗");
    case WindowType::BlackmanHarris: return QStringLiteral("布莱克曼-哈里斯窗");
    case WindowType::Kaiser:         return QStringLiteral("凯泽窗");
    case WindowType::FlatTop:        return QStringLiteral("平顶窗");
    case WindowType::Gaussian:       return QStringLiteral("高斯窗");
    default:                         return QStringLiteral("未知");
    }
}

double FFTUtils::getWindowCoherentGain(WindowType type)
{
    switch (type) {
    case WindowType::Rectangular:    return 1.0;
    case WindowType::Hanning:        return 0.5;
    case WindowType::Hamming:        return 0.54;
    case WindowType::Blackman:       return 0.42;
    case WindowType::BlackmanHarris: return 0.35875;
    case WindowType::Kaiser:         return 0.5;  // 默认beta=5的近似值，实际调用应使用带参数版本
    case WindowType::FlatTop:        return 0.21557895;
    case WindowType::Gaussian:       return 0.5;  // 默认sigma=0.4的近似值，实际调用应使用带参数版本
    default:                         return 1.0;
    }
}

double FFTUtils::getWindowCoherentGainKaiser(double beta)
{
    // 相干增益 = 窗口函数均值，通过数值积分近似
    const int N = 1024;
    double sum = 0;
    double denom = besselI0(beta);
    for (int i = 0; i < N; ++i) {
        double ratio = 2.0 * i / (N - 1) - 1.0;
        double arg = beta * std::sqrt(1.0 - ratio * ratio);
        sum += besselI0(arg) / denom;
    }
    return sum / N;
}

double FFTUtils::getWindowCoherentGainGaussian(double sigma)
{
    // 相干增益 = 窗口函数均值，通过数值积分近似
    const int N = 1024;
    double sum = 0;
    for (int i = 0; i < N; ++i) {
        double ratio = (i - (N - 1) / 2.0) / (sigma * (N - 1) / 2.0);
        sum += std::exp(-0.5 * ratio * ratio);
    }
    return sum / N;
}

// ==================== 时域参数计算 ====================

void FFTUtils::calculateTimeDomainParams(const QVector<double>& data, FFTResult& result)
{
    if (data.isEmpty()) return;

    int n = data.size();

    result.maxValue = *std::max_element(data.begin(), data.end());
    result.minValue = *std::min_element(data.begin(), data.end());
    result.peakToPeak = result.maxValue - result.minValue;

    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    result.avgValue = sum / n;

    double sumSquares = 0;
    for (double v : data) {
        sumSquares += v * v;
    }
    result.rmsValue = std::sqrt(sumSquares / n);

    // 峰值因子（基于交流分量，去除直流偏移）
    double acPeak = std::max(std::abs(result.maxValue - result.avgValue),
                             std::abs(result.minValue - result.avgValue));
    double acRms = std::sqrt(std::max(0.0, result.rmsValue * result.rmsValue - result.avgValue * result.avgValue));
    if (acRms > 1e-10) {
        result.crestFactor = acPeak / acRms;
    }

    // 占空比
    double threshold = (result.maxValue + result.minValue) / 2;
    int highCount = 0;
    for (double v : data) {
        if (v > threshold) highCount++;
    }
    result.dutyCycle = 100.0 * highCount / n;
}

// ==================== 波形分析 ====================

double FFTUtils::calculateTHD(const FFTResult& result)
{
    if (result.harmonics.isEmpty() || result.harmonics.size() < 2) {
        return 0;
    }

    double fundamentalMag = result.harmonics[0].magnitude;
    if (fundamentalMag < 1e-10) return 0;

    double harmonicPowerSum = 0;
    for (int i = 1; i < result.harmonics.size(); ++i) {
        double mag = result.harmonics[i].magnitude;
        harmonicPowerSum += mag * mag;
    }

    return std::sqrt(harmonicPowerSum) / fundamentalMag * 100.0;
}

double FFTUtils::calculateSNR(const FFTResult& result)
{
    // SNR = 信号功率 / 纯噪声功率
    // 纯噪声 = 总功率 - 直流功率 - 基波功率 - 谐波功率
    if (result.magnitudes.isEmpty()) return 0;

    double signalPower = result.dominantMagnitude * result.dominantMagnitude;
    if (signalPower < 1e-20) return 0;

    // 计算所有谐波（含基波）的功率之和
    double harmonicPowerSum = 0;
    for (const auto& h : result.harmonics) {
        harmonicPowerSum += h.magnitude * h.magnitude;
    }

    // 噪声功率 = 总功率 - 直流功率 - 谐波功率（含基波）
    double dcPower = result.dcComponent * result.dcComponent;
    double noisePower = result.totalPower - dcPower - harmonicPowerSum;
    if (noisePower <= 0) noisePower = 1e-20;

    return 10.0 * std::log10(signalPower / noisePower);
}

double FFTUtils::calculateSINAD(const FFTResult& result)
{
    // SINAD = 信号功率 / (噪声功率 + 失真功率)
    // 噪声+失真 = 总功率 - 直流功率 - 基波功率
    if (result.magnitudes.isEmpty()) return 0;

    double signalPower = result.dominantMagnitude * result.dominantMagnitude;
    if (signalPower < 1e-20) return 0;

    double dcPower = result.dcComponent * result.dcComponent;
    double noiseAndDistortionPower = result.totalPower - dcPower - signalPower;
    if (noiseAndDistortionPower <= 0) noiseAndDistortionPower = 1e-20;

    return 10.0 * std::log10(signalPower / noiseAndDistortionPower);
}

double FFTUtils::calculateENOB(double sinad)
{
    // ENOB = (SINAD - 1.76) / 6.02
    return (sinad - 1.76) / 6.02;
}

QVector<HarmonicInfo> FFTUtils::extractHarmonics(const FFTResult& result, int maxHarmonics)
{
    QVector<HarmonicInfo> harmonics;

    if (result.frequencies.isEmpty() || result.dominantFrequency <= 0) {
        return harmonics;
    }

    double fundamentalFreq = result.dominantFrequency;
    double freqResolution = result.frequencyResolution > 0 ? result.frequencyResolution :
        (result.frequencies.size() > 1 ? result.frequencies[1] - result.frequencies[0] : 1);

    for (int h = 1; h <= maxHarmonics; ++h) {
        double targetFreq = fundamentalFreq * h;

        int idx = -1;
        double minDiff = freqResolution * 5;  // 搜索窗口 ±5 bins

        for (int i = 0; i < result.frequencies.size(); ++i) {
            double diff = std::abs(result.frequencies[i] - targetFreq);
            if (diff < minDiff) {
                minDiff = diff;
                idx = i;
            }
        }

        if (idx >= 0 && idx < result.magnitudes.size()) {
            HarmonicInfo info;
            info.order = h;
            info.frequency = result.frequencies[idx];
            info.magnitude = result.magnitudes[idx];
            info.relativeMag = (result.dominantMagnitude > 0) ?
                (info.magnitude / result.dominantMagnitude * 100.0) : 0;
            info.phase = result.phases[idx];

            // 谐波阈值：至少为主频幅度的1%，避免随机噪声被误判为谐波
            if (info.relativeMag > 1.0 || h == 1) {
                harmonics.append(info);
            }
        }
    }

    return harmonics;
}

WaveformType FFTUtils::identifyWaveform(FFTResult& result)
{
    if (result.magnitudes.isEmpty()) {
        result.waveformType = WaveformType::Unknown;
        result.waveformName = QStringLiteral("未知");
        return WaveformType::Unknown;
    }

    result.harmonics = extractHarmonics(result, 20);
    result.harmonicCount = result.harmonics.size();

    result.thd = calculateTHD(result);
    result.snr = calculateSNR(result);
    result.sinad = calculateSINAD(result);
    result.enob = calculateENOB(result.sinad);

    result.fundamentalFreq = result.dominantFrequency;
    if (result.fundamentalFreq > 0) {
        result.fundamentalPeriod = 1.0 / result.fundamentalFreq;
    }

    WaveformType type = WaveformType::Unknown;

    // 1. 检查是否为直流：主频分量远小于直流分量
    if (result.dominantMagnitude < result.dcComponent * 0.1 && result.dcComponent > 0.01) {
        type = WaveformType::DC;
    }
    // 2. 检查噪声：SNR 低（< 3dB）或主频幅度不突出（相对于总功率）
    //    优先于正弦波检测，避免随机数据被误判
    else if (result.snr < 3.0 && result.harmonics.size() < 3) {
        type = WaveformType::Noise;
    }
    // 3. 检查是否为正弦波：THD < 5% 表示几乎没有谐波
    else if (result.thd < 5.0 && result.dominantMagnitude > 0.001) {
        type = WaveformType::Sine;
    }
    // 4. 有足够的谐波分量，判断具体波形
    else if (result.harmonics.size() >= 3) {
        bool hasOnlyOddHarmonics = true;

        for (int i = 1; i < qMin(5, result.harmonics.size()); ++i) {
            int order = result.harmonics[i].order;
            if (order % 2 == 0 && result.harmonics[i].relativeMag > 5) {
                hasOnlyOddHarmonics = false;
            }
        }

        // 理论THD参考：方波≈48.3%、三角波≈12.1%、锯齿波≈80.3%
        if (hasOnlyOddHarmonics && result.thd > 35 && result.thd < 65) {
            type = WaveformType::Square;
        }
        else if (hasOnlyOddHarmonics && result.thd > 5 && result.thd < 25) {
            type = WaveformType::Triangle;
        }
        // 脉冲波：占空比远离50%（<20%或>80%）且谐波幅度衰减缓慢
        else if ((result.dutyCycle < 20.0 || result.dutyCycle > 80.0) && result.thd > 40) {
            type = WaveformType::Pulse;
        }
        else if (!hasOnlyOddHarmonics && result.thd > 50) {
            type = WaveformType::Sawtooth;
        }
        else {
            type = WaveformType::Composite;
        }
    }
    else {
        type = WaveformType::Composite;
    }

    result.waveformType = type;
    result.waveformName = getWaveformName(type);

    return type;
}

QString FFTUtils::getWaveformName(WaveformType type)
{
    switch (type) {
    case WaveformType::Sine:      return QStringLiteral("正弦波");
    case WaveformType::Square:    return QStringLiteral("方波");
    case WaveformType::Triangle:  return QStringLiteral("三角波");
    case WaveformType::Sawtooth:  return QStringLiteral("锯齿波");
    case WaveformType::Pulse:     return QStringLiteral("脉冲波");
    case WaveformType::Noise:     return QStringLiteral("噪声");
    case WaveformType::DC:        return QStringLiteral("直流");
    case WaveformType::Composite: return QStringLiteral("复合波");
    default:                      return QStringLiteral("未知");
    }
}

// ==================== 峰值检测 ====================

QVector<PeakInfo> FFTUtils::detectPeaks(const FFTResult& result, int maxPeaks,
                                         double minMagnitude, double minDistance)
{
    QVector<PeakInfo> peaks;

    if (result.magnitudes.size() < 3) return peaks;

    double maxMag = *std::max_element(result.magnitudes.begin(), result.magnitudes.end());
    double threshold = maxMag * minMagnitude;

    double freqRes = result.frequencyResolution > 0 ? result.frequencyResolution :
        (result.frequencies.size() > 1 ? result.frequencies[1] - result.frequencies[0] : 1);
    int minDistanceIdx = minDistance > 0 ? static_cast<int>(minDistance / freqRes) : 1;

    // 查找所有局部最大值
    QVector<QPair<int, double>> candidates;
    for (int i = 1; i < result.magnitudes.size() - 1; ++i) {
        if (result.magnitudes[i] > result.magnitudes[i-1] &&
            result.magnitudes[i] > result.magnitudes[i+1] &&
            result.magnitudes[i] > threshold) {
            candidates.append({i, result.magnitudes[i]});
        }
    }

    // 按幅度排序
    std::sort(candidates.begin(), candidates.end(),
              [](const QPair<int, double>& a, const QPair<int, double>& b) {
                  return a.second > b.second;
              });

    // 选择峰值（避免太近的峰值）
    QVector<int> selectedIndices;
    for (const auto& cand : candidates) {
        if (peaks.size() >= maxPeaks) break;

        bool tooClose = false;
        for (int idx : selectedIndices) {
            if (std::abs(cand.first - idx) < minDistanceIdx) {
                tooClose = true;
                break;
            }
        }

        if (!tooClose) {
            PeakInfo peak;
            peak.index = cand.first;
            peak.frequency = result.frequencies[cand.first];
            peak.magnitude = result.magnitudes[cand.first];
            peak.phase = result.phases[cand.first];
            peak.powerDB = result.powerSpectrumDB.size() > cand.first ?
                result.powerSpectrumDB[cand.first] :
                (cand.second > 1e-20 ? 20.0 * std::log10(cand.second) : -100);

            peaks.append(peak);
            selectedIndices.append(cand.first);
        }
    }

    return peaks;
}

// ==================== 频谱处理 ====================

FFTResult FFTUtils::averageSpectrum(const QVector<FFTResult>& spectrums)
{
    FFTResult result;

    if (spectrums.isEmpty()) return result;
    if (spectrums.size() == 1) return spectrums[0];

    int n = spectrums[0].magnitudes.size();
    result.frequencies = spectrums[0].frequencies;
    result.magnitudes.resize(n);
    result.phases.resize(n);
    result.powerSpectrum.resize(n);
    result.powerSpectrumDB.resize(n);

    // 记录主频幅度最大的频谱索引（用于取相位）
    int bestIdx = 0;
    double bestDomMag = 0;

    for (int s = 0; s < spectrums.size(); ++s) {
        if (spectrums[s].dominantMagnitude > bestDomMag) {
            bestDomMag = spectrums[s].dominantMagnitude;
            bestIdx = s;
        }
        for (int i = 0; i < qMin(n, spectrums[s].magnitudes.size()); ++i) {
            result.magnitudes[i] += spectrums[s].magnitudes[i];
            result.powerSpectrum[i] += spectrums[s].powerSpectrum[i];
        }
    }

    double count = spectrums.size();
    double maxMag = 0;
    int maxIdx = 0;

    for (int i = 0; i < n; ++i) {
        result.magnitudes[i] /= count;
        result.powerSpectrum[i] /= count;
        result.powerSpectrumDB[i] = result.powerSpectrum[i] > 1e-20 ?
            10.0 * std::log10(result.powerSpectrum[i]) : -100;

        if (i > 0 && result.magnitudes[i] > maxMag) {
            maxMag = result.magnitudes[i];
            maxIdx = i;
        }
        result.totalPower += result.powerSpectrum[i];
    }

    result.dominantFrequency = result.frequencies[maxIdx];
    result.dominantMagnitude = maxMag;
    result.dcComponent = result.magnitudes[0];
    result.fftSize = spectrums[0].fftSize;
    result.frequencyResolution = spectrums[0].frequencyResolution;
    result.validPoints = spectrums[0].validPoints;

    // 相位取主频幅度最大的那帧频谱的相位（平均相位无物理意义）
    if (bestIdx >= 0 && bestIdx < spectrums.size() &&
        spectrums[bestIdx].phases.size() >= n) {
        for (int i = 0; i < n; ++i) {
            result.phases[i] = spectrums[bestIdx].phases[i];
        }
    }

    return result;
}

void FFTUtils::updateMaxHold(const FFTResult& current, QVector<double>& maxHold)
{
    if (maxHold.isEmpty()) {
        maxHold = current.magnitudes;
        return;
    }

    for (int i = 0; i < qMin(maxHold.size(), current.magnitudes.size()); ++i) {
        maxHold[i] = qMax(maxHold[i], current.magnitudes[i]);
    }
}

// ==================== 数字滤波 ====================

QVector<double> FFTUtils::applyFIRFilter(const QVector<double>& data, FilterType filterType,
                                          double cutoffFreq, double sampleRate, int order)
{
    if (data.isEmpty() || sampleRate <= 0) return data;

    // 归一化截止频率
    double normalizedCutoff = cutoffFreq / (sampleRate / 2.0);
    if (normalizedCutoff <= 0 || normalizedCutoff >= 1) return data;

    // 生成 FIR 滤波器系数（sinc函数 + 汉宁窗）
    QVector<double> coeffs(order);
    int halfOrder = order / 2;

    for (int i = 0; i < order; ++i) {
        int n = i - halfOrder;
        double sinc;
        if (n == 0) {
            sinc = normalizedCutoff;
        } else {
            sinc = std::sin(PI * normalizedCutoff * n) / (PI * n);
        }

        // 汉宁窗
        double window = 0.5 * (1.0 - std::cos(2.0 * PI * i / (order - 1)));
        coeffs[i] = sinc * window;
    }

    // 高通滤波器：频谱反转（低通系数取反，中心抽头加1）
    // 注意：高通滤波器不能用系数和归一化（那会强制DC增益为1，但高通的DC增益应为0）
    if (filterType == FilterType::HighPass) {
        for (int i = 0; i < order; ++i) {
            coeffs[i] = -coeffs[i];
        }
        coeffs[halfOrder] += 1.0;
    } else {
        // 低通滤波器：归一化系数使DC增益为1
        double sum = std::accumulate(coeffs.begin(), coeffs.end(), 0.0);
        if (std::abs(sum) > 1e-10) {
            for (double& c : coeffs) {
                c /= sum;
            }
        }
    }

    // 应用滤波器（卷积）
    QVector<double> result(data.size());
    for (int i = 0; i < data.size(); ++i) {
        double sum = 0;
        for (int j = 0; j < order; ++j) {
            int idx = i - j + halfOrder;
            if (idx >= 0 && idx < data.size()) {
                sum += data[idx] * coeffs[j];
            }
        }
        result[i] = sum;
    }

    return result;
}

QVector<double> FFTUtils::applyBandpassFilter(const QVector<double>& data,
                                               double lowFreq, double highFreq,
                                               double sampleRate, int order)
{
    // 带通 = 低通(high) - 低通(low)
    QVector<double> lowPassed = applyFIRFilter(data, FilterType::LowPass, highFreq, sampleRate, order);
    QVector<double> highPassed = applyFIRFilter(lowPassed, FilterType::HighPass, lowFreq, sampleRate, order);
    return highPassed;
}

QVector<double> FFTUtils::applyBandStopFilter(const QVector<double>& data,
                                               double lowFreq, double highFreq,
                                               double sampleRate, int order)
{
    // 带阻 = 低通(low) + 高通(high)
    QVector<double> lowPassed = applyFIRFilter(data, FilterType::LowPass, lowFreq, sampleRate, order);
    QVector<double> highPassed = applyFIRFilter(data, FilterType::HighPass, highFreq, sampleRate, order);
    QVector<double> result(data.size());
    for (int i = 0; i < data.size(); ++i) {
        result[i] = lowPassed[i] + highPassed[i];
    }
    return result;
}

QVector<double> FFTUtils::applyMovingAverage(const QVector<double>& data, int windowSize)
{
    if (data.isEmpty() || windowSize <= 1) return data;

    QVector<double> result(data.size());
    int halfWindow = windowSize / 2;

    for (int i = 0; i < data.size(); ++i) {
        double sum = 0;
        int count = 0;
        for (int j = -halfWindow; j <= halfWindow; ++j) {
            int idx = i + j;
            if (idx >= 0 && idx < data.size()) {
                sum += data[idx];
                count++;
            }
        }
        result[i] = sum / count;
    }

    return result;
}

QVector<double> FFTUtils::applyMedianFilter(const QVector<double>& data, int windowSize)
{
    if (data.isEmpty() || windowSize <= 1) return data;

    QVector<double> result(data.size());
    int halfWindow = windowSize / 2;

    for (int i = 0; i < data.size(); ++i) {
        QVector<double> window;
        for (int j = -halfWindow; j <= halfWindow; ++j) {
            int idx = i + j;
            if (idx >= 0 && idx < data.size()) {
                window.append(data[idx]);
            }
        }
        std::sort(window.begin(), window.end());
        result[i] = window[window.size() / 2];
    }

    return result;
}

// ==================== 数学运算 ====================

QVector<double> FFTUtils::differentiate(const QVector<double>& data, double dt)
{
    if (data.size() < 2) return QVector<double>();

    QVector<double> result(data.size());

    // 中心差分（内部点）
    for (int i = 1; i < data.size() - 1; ++i) {
        result[i] = (data[i + 1] - data[i - 1]) / (2.0 * dt);
    }

    // 边界点使用前向/后向差分
    result[0] = (data[1] - data[0]) / dt;
    result[data.size() - 1] = (data[data.size() - 1] - data[data.size() - 2]) / dt;

    return result;
}

QVector<double> FFTUtils::integrate(const QVector<double>& data, double dt)
{
    if (data.isEmpty()) return QVector<double>();

    QVector<double> result(data.size());
    result[0] = 0;

    // 梯形积分
    for (int i = 1; i < data.size(); ++i) {
        result[i] = result[i - 1] + (data[i] + data[i - 1]) * dt / 2.0;
    }

    return result;
}

// ==================== 核心分析函数 ====================

FFTResult FFTUtils::analyze(const QVector<double>& data, double sampleRate)
{
    FFTConfig config;
    config.sampleRate = sampleRate > 0 ? sampleRate : data.size();
    config.windowType = WindowType::Hanning;
    config.fftSize = 0;  // 自动
    return analyzeWithConfig(data, config);
}

FFTResult FFTUtils::analyzeWithConfig(const QVector<double>& data, const FFTConfig& config)
{
    FFTResult result;

    if (data.isEmpty()) {
        return result;
    }

    /*
     * 串口解析、Lua 脚本或会话恢复可能把 NaN/Inf 写入曲线。
     * FFT 内核只接受有限实数；这里保留原始顺序并跳过非法采样，避免
     * 一个异常点把均值、幅度谱和坐标轴全部污染成 NaN。
     */
    QVector<double> finiteData;
    finiteData.reserve(data.size());
    for (double value : data) {
        if (std::isfinite(value)) {
            finiteData.append(value);
        }
    }

    if (finiteData.size() < 4) {
        result.validPoints = finiteData.size();
        calculateTimeDomainParams(finiteData, result);
        return result;
    }

    int n = finiteData.size();
    result.validPoints = n;

    /*
     * 采样率来自用户输入或恢复配置，必须同时检查正数和有限性。
     * 无效值回退为有效采样点数，确保频率分辨率和频率数组始终可用。
     */
    double sampleRate = (std::isfinite(config.sampleRate) && config.sampleRate > 0.0)
        ? config.sampleRate
        : static_cast<double>(n);

    // 计算时域参数（基于原始数据）
    calculateTimeDomainParams(finiteData, result);

    // 去除直流分量（减去均值），避免DC泄漏污染低频谱线
    double dcOffset = std::accumulate(finiteData.begin(), finiteData.end(), 0.0) / n;
    QVector<double> dcRemovedData(n);
    for (int i = 0; i < n; ++i) {
        dcRemovedData[i] = finiteData[i] - dcOffset;
    }

    /*
     * 当前 fft() 实现是 radix-2 Cooley-Tukey，非 2 次幂输入会被内部补零。
     * 分析层先把配置规整成明确的 2 次幂，可避免分段平均时不同路径对
     * fftSize 的解释不一致，也避免用户配置 1000 点时触发越界风险。
     */
    int requestedFftSize = config.fftSize;
    if (requestedFftSize <= 0) {
        requestedFftSize = config.zeroPadding ? nextPowerOfTwo(n) : n;
    }
    int fftSize = nextPowerOfTwo(qMax(requestedFftSize, 4));
    if (fftSize < n && config.fftSize > 0) {
        fftSize = nextPowerOfTwo(n);
    }

    // 窗口相干增益补偿（Kaiser/Gaussian使用参数化动态计算）
    double windowGain;
    if (config.windowType == WindowType::Kaiser) {
        windowGain = getWindowCoherentGainKaiser(config.kaiserBeta);
    } else if (config.windowType == WindowType::Gaussian) {
        windowGain = getWindowCoherentGainGaussian(config.gaussianSigma);
    } else {
        windowGain = getWindowCoherentGain(config.windowType);
    }
    if (!std::isfinite(windowGain) || windowGain <= 1e-12) {
        windowGain = 1.0;
    }

    // 频谱平均：将数据分成 averageCount 段，分别做FFT后平均
    int avgCount = qMax(1, config.averageCount);
    int segLen = n / avgCount;  // 每段长度
    if (segLen < 4) {
        avgCount = 1;
        segLen = n;
    }
    avgCount = qMax(1, qMin(avgCount, n / qMax(4, segLen)));

    // 累加缓冲区
    int halfSize = 0;
    QVector<double> accMagnitudes;
    QVector<double> accPowerSpectrum;
    QVector<double> accPowerSpectrumDB;
    QVector<std::complex<double>> accFFT;  // 用于相位提取（取最大幅度帧）
    int bestFFTIdx = 0;
    double bestDomMag = 0;

    for (int avg = 0; avg < avgCount; ++avg) {
        int segStart = avg * segLen;
        if (segStart + segLen > dcRemovedData.size()) {
            break;
        }

        QVector<double> segment(segLen);
        for (int i = 0; i < segLen; ++i) {
            segment[i] = dcRemovedData[segStart + i];
        }

        // 应用窗口函数
        QVector<double> windowedData;
        if (config.windowType == WindowType::Kaiser) {
            windowedData = applyKaiserWindow(segment, config.kaiserBeta);
        } else if (config.windowType == WindowType::Gaussian) {
            windowedData = applyGaussianWindow(segment, config.gaussianSigma);
        } else {
            windowedData = applyWindow(segment, config.windowType);
        }

        // 转换为复数并零填充
        int segFftSize = config.fftSize > 0 ? fftSize :
            (config.zeroPadding ? nextPowerOfTwo(segLen) : nextPowerOfTwo(segLen));
        segFftSize = nextPowerOfTwo(qMax(segFftSize, segLen));
        QVector<std::complex<double>> complexData(segFftSize, std::complex<double>(0, 0));
        for (int i = 0; i < segLen; ++i) {
            complexData[i] = std::complex<double>(windowedData[i], 0);
        }

        // 执行 FFT
        QVector<std::complex<double>> fftData = fft(complexData);

        int curHalfSize = fftData.size() / 2;

        // 首次初始化累加缓冲区
        if (accMagnitudes.isEmpty()) {
            halfSize = curHalfSize;
            accMagnitudes = QVector<double>(halfSize, 0.0);
            accPowerSpectrum = QVector<double>(halfSize, 0.0);
            accPowerSpectrumDB = QVector<double>(halfSize, 0.0);
            accFFT = fftData;
        }

        // 累加幅度和功率谱
        double segDomMag = 0;
        int curFFTSize = fftData.size();
        Q_UNUSED(curFFTSize);
        for (int i = 0; i < qMin(halfSize, curHalfSize); ++i) {
            double magnitude = std::abs(fftData[i]) * 2.0 / segLen / windowGain;
            if (i == 0) magnitude /= 2.0;

            accMagnitudes[i] += magnitude;
            accPowerSpectrum[i] += magnitude * magnitude;

            if (i > 0 && magnitude > segDomMag) {
                segDomMag = magnitude;
            }
        }

        // 记录主频幅度最大的FFT结果（用于相位提取）
        if (segDomMag > bestDomMag) {
            bestDomMag = segDomMag;
            bestFFTIdx = avg;
            accFFT = fftData;
        }
    }

    if (accFFT.isEmpty() || halfSize <= 0) {
        return result;
    }

    // 计算平均值
    result.fftSize = accFFT.size();
    result.frequencyResolution = sampleRate / accFFT.size();

    result.frequencies.resize(halfSize);
    result.magnitudes.resize(halfSize);
    result.phases.resize(halfSize);
    result.powerSpectrum.resize(halfSize);
    result.powerSpectrumDB.resize(halfSize);

    double maxMagnitude = 0;
    int maxIndex = 0;

    // 对称窗口的群延迟补偿（使用段长度）
    double groupDelay = (segLen - 1) / 2.0;

    for (int i = 0; i < halfSize; ++i) {
        result.frequencies[i] = i * sampleRate / accFFT.size();

        result.magnitudes[i] = accMagnitudes[i] / avgCount;
        result.powerSpectrum[i] = accPowerSpectrum[i] / avgCount;
        result.powerSpectrumDB[i] = result.powerSpectrum[i] > 1e-20 ?
            10.0 * std::log10(result.powerSpectrum[i]) : -100;

        // 相位取主频幅度最大的那帧的相位，并补偿窗口延迟
        double phaseCompensation = 2.0 * PI * i * groupDelay / accFFT.size();
        double rawPhase = std::atan2(accFFT[i].imag(), accFFT[i].real());
        result.phases[i] = (rawPhase + phaseCompensation) * 180.0 / PI;

        if (i > 0 && result.magnitudes[i] > maxMagnitude) {
            maxMagnitude = result.magnitudes[i];
            maxIndex = i;
        }

        result.totalPower += result.powerSpectrum[i];
    }

    result.dcComponent = dcOffset;  // 使用原始数据的均值作为直流分量
    result.dominantFrequency = result.frequencies[maxIndex];
    result.dominantMagnitude = maxMagnitude;

    // 识别波形类型并计算谐波
    identifyWaveform(result);

    // 检测峰值
    result.peaks = detectPeaks(result, 10, 0.01, result.frequencyResolution * 2);

    return result;
}

} // namespace ComAssistant
