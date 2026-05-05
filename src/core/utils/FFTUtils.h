/**
 * @file FFTUtils.h
 * @brief FFT 频谱分析工具类（增强版）
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef FFTUTILS_H
#define FFTUTILS_H

#include <QVector>
#include <QString>
#include <complex>
#include <cmath>

namespace ComAssistant {

/**
 * @brief 窗口函数类型枚举
 */
enum class WindowType {
    Rectangular,    ///< 矩形窗（无窗）
    Hanning,        ///< 汉宁窗
    Hamming,        ///< 汉明窗
    Blackman,       ///< 布莱克曼窗
    BlackmanHarris, ///< 布莱克曼-哈里斯窗
    Kaiser,         ///< 凯泽窗
    FlatTop,        ///< 平顶窗
    Gaussian        ///< 高斯窗
};

/**
 * @brief 波形类型枚举
 */
enum class WaveformType {
    Unknown,        ///< 未知
    Sine,           ///< 正弦波
    Square,         ///< 方波
    Triangle,       ///< 三角波
    Sawtooth,       ///< 锯齿波
    Pulse,          ///< 脉冲波
    Noise,          ///< 噪声
    DC,             ///< 直流
    Composite       ///< 复合波
};

/**
 * @brief 滤波器类型枚举
 */
enum class FilterType {
    LowPass,        ///< 低通滤波
    HighPass,       ///< 高通滤波
    BandPass,       ///< 带通滤波
    BandStop        ///< 带阻滤波
};

/**
 * @brief FFT 分析配置
 */
struct FFTConfig {
    int fftSize = 0;                        ///< FFT点数（0=自动，使用数据长度）
    double sampleRate = 1000.0;             ///< 采样率 (Hz)
    WindowType windowType = WindowType::Hanning;  ///< 窗口函数
    double kaiserBeta = 5.0;                ///< 凯泽窗参数
    double gaussianSigma = 0.4;             ///< 高斯窗参数
    bool zeroPadding = true;                ///< 是否零填充到2的幂次
    int averageCount = 1;                   ///< 频谱平均次数
};

/**
 * @brief 谐波信息结构
 */
struct HarmonicInfo {
    int order = 0;              ///< 谐波次数（1=基波）
    double frequency = 0;       ///< 频率 (Hz)
    double magnitude = 0;       ///< 幅度
    double relativeMag = 0;     ///< 相对幅度 (%)
    double phase = 0;           ///< 相位 (度)
};

/**
 * @brief 峰值信息结构
 */
struct PeakInfo {
    int index = 0;              ///< 峰值索引
    double frequency = 0;       ///< 频率 (Hz)
    double magnitude = 0;       ///< 幅度
    double phase = 0;           ///< 相位 (度)
    double powerDB = 0;         ///< 功率 (dB)
};

/**
 * @brief FFT 分析结果结构
 */
struct FFTResult {
    // 基本频谱数据
    QVector<double> frequencies;    ///< 频率数组 (Hz)
    QVector<double> magnitudes;     ///< 幅度谱
    QVector<double> phases;         ///< 相位谱 (度)
    QVector<double> powerSpectrum;  ///< 功率谱
    QVector<double> powerSpectrumDB;///< 功率谱 (dB)

    // 主要特征
    double dominantFrequency = 0;   ///< 主频率 (Hz)
    double dominantMagnitude = 0;   ///< 主频幅度
    double dcComponent = 0;         ///< 直流分量
    double totalPower = 0;          ///< 总功率
    int validPoints = 0;            ///< 有效数据点数
    int fftSize = 0;                ///< 实际FFT点数
    double frequencyResolution = 0; ///< 频率分辨率 (Hz)

    // 波形分析
    WaveformType waveformType = WaveformType::Unknown;  ///< 波形类型
    QString waveformName;           ///< 波形名称描述
    double thd = 0;                 ///< 总谐波失真 (%)
    double snr = 0;                 ///< 信噪比 (dB)
    double sinad = 0;               ///< 信纳比 (dB)
    double enob = 0;                ///< 有效位数
    double fundamentalFreq = 0;     ///< 基波频率 (Hz)
    double fundamentalPeriod = 0;   ///< 基波周期 (s)

    // 时域参数
    double peakToPeak = 0;          ///< 峰峰值
    double rmsValue = 0;            ///< 有效值 (RMS)
    double avgValue = 0;            ///< 平均值
    double maxValue = 0;            ///< 最大值
    double minValue = 0;            ///< 最小值
    double dutyCycle = 0;           ///< 占空比 (%)（方波/脉冲）
    double crestFactor = 0;         ///< 峰值因子

    // 谐波分析
    QVector<HarmonicInfo> harmonics;  ///< 谐波列表
    int harmonicCount = 0;            ///< 有效谐波数量

    // 峰值检测
    QVector<PeakInfo> peaks;          ///< 检测到的峰值列表
};

/**
 * @brief FFT 工具类
 *
 * 提供快速傅里叶变换功能，用于频谱分析
 */
class FFTUtils
{
public:
    // ==================== 核心分析函数 ====================

    /**
     * @brief 执行 FFT 分析（简单版本）
     * @param data 输入数据
     * @param sampleRate 采样率 (Hz)
     * @return FFT 分析结果
     */
    static FFTResult analyze(const QVector<double>& data, double sampleRate = 0);

    /**
     * @brief 执行 FFT 分析（配置版本）
     * @param data 输入数据
     * @param config FFT配置
     * @return FFT 分析结果
     */
    static FFTResult analyzeWithConfig(const QVector<double>& data, const FFTConfig& config);

    /**
     * @brief 计算 FFT
     * @param input 输入数据（复数形式）
     * @return FFT 结果（复数形式）
     */
    static QVector<std::complex<double>> fft(const QVector<std::complex<double>>& input);

    /**
     * @brief 计算逆 FFT
     * @param input FFT 结果
     * @return 原始数据（复数形式）
     */
    static QVector<std::complex<double>> ifft(const QVector<std::complex<double>>& input);

    // ==================== 窗口函数 ====================

    /**
     * @brief 应用窗口函数
     * @param data 输入数据
     * @param type 窗口类型
     * @param param 窗口参数（凯泽窗的beta或高斯窗的sigma）
     * @return 加窗后的数据
     */
    static QVector<double> applyWindow(const QVector<double>& data, WindowType type, double param = 0);

    static QVector<double> applyRectangularWindow(const QVector<double>& data);
    static QVector<double> applyHanningWindow(const QVector<double>& data);
    static QVector<double> applyHammingWindow(const QVector<double>& data);
    static QVector<double> applyBlackmanWindow(const QVector<double>& data);
    static QVector<double> applyBlackmanHarrisWindow(const QVector<double>& data);
    static QVector<double> applyKaiserWindow(const QVector<double>& data, double beta = 5.0);
    static QVector<double> applyFlatTopWindow(const QVector<double>& data);
    static QVector<double> applyGaussianWindow(const QVector<double>& data, double sigma = 0.4);

    /**
     * @brief 获取窗口函数名称
     */
    static QString getWindowName(WindowType type);

    /**
     * @brief 获取窗口函数的相干增益（用于幅度校正）
     */
    static double getWindowCoherentGain(WindowType type);

    /**
     * @brief 获取Kaiser窗口的相干增益（动态计算，取决于beta参数）
     */
    static double getWindowCoherentGainKaiser(double beta);

    /**
     * @brief 获取Gaussian窗口的相干增益（动态计算，取决于sigma参数）
     */
    static double getWindowCoherentGainGaussian(double sigma);

    // ==================== 波形分析 ====================

    /**
     * @brief 识别波形类型
     */
    static WaveformType identifyWaveform(FFTResult& result);

    /**
     * @brief 获取波形类型名称
     */
    static QString getWaveformName(WaveformType type);

    /**
     * @brief 计算 THD（总谐波失真）
     */
    static double calculateTHD(const FFTResult& result);

    /**
     * @brief 计算 SNR（信噪比）
     */
    static double calculateSNR(const FFTResult& result);

    /**
     * @brief 计算 SINAD（信纳比）
     */
    static double calculateSINAD(const FFTResult& result);

    /**
     * @brief 计算 ENOB（有效位数）
     */
    static double calculateENOB(double sinad);

    /**
     * @brief 提取谐波分量
     */
    static QVector<HarmonicInfo> extractHarmonics(const FFTResult& result, int maxHarmonics = 20);

    /**
     * @brief 计算时域参数
     */
    static void calculateTimeDomainParams(const QVector<double>& data, FFTResult& result);

    // ==================== 峰值检测 ====================

    /**
     * @brief 检测频谱峰值
     * @param result FFT结果
     * @param maxPeaks 最大峰值数量
     * @param minMagnitude 最小幅度阈值（相对于最大峰值的比例）
     * @param minDistance 峰值间最小频率间隔 (Hz)
     * @return 峰值列表
     */
    static QVector<PeakInfo> detectPeaks(const FFTResult& result, int maxPeaks = 10,
                                          double minMagnitude = 0.01, double minDistance = 0);

    // ==================== 频谱处理 ====================

    /**
     * @brief 频谱平均
     * @param spectrums 多个频谱结果
     * @return 平均后的频谱
     */
    static FFTResult averageSpectrum(const QVector<FFTResult>& spectrums);

    /**
     * @brief 更新峰值保持
     * @param current 当前频谱
     * @param maxHold 峰值保持数据
     */
    static void updateMaxHold(const FFTResult& current, QVector<double>& maxHold);

    // ==================== 数字滤波 ====================

    /**
     * @brief 应用 FIR 滤波器
     * @param data 输入数据
     * @param filterType 滤波器类型
     * @param cutoffFreq 截止频率 (Hz)
     * @param sampleRate 采样率 (Hz)
     * @param order 滤波器阶数
     * @return 滤波后数据
     */
    static QVector<double> applyFIRFilter(const QVector<double>& data, FilterType filterType,
                                           double cutoffFreq, double sampleRate, int order = 31);

    /**
     * @brief 应用带通 FIR 滤波器
     */
    static QVector<double> applyBandpassFilter(const QVector<double>& data,
                                                double lowFreq, double highFreq,
                                                double sampleRate, int order = 31);

    /**
     * @brief 应用带阻 FIR 滤波器
     */
    static QVector<double> applyBandStopFilter(const QVector<double>& data,
                                                double lowFreq, double highFreq,
                                                double sampleRate, int order = 31);

    /**
     * @brief 简单移动平均滤波
     */
    static QVector<double> applyMovingAverage(const QVector<double>& data, int windowSize);

    /**
     * @brief 中值滤波
     */
    static QVector<double> applyMedianFilter(const QVector<double>& data, int windowSize);

    // ==================== 数学运算 ====================

    /**
     * @brief 计算数据的微分
     */
    static QVector<double> differentiate(const QVector<double>& data, double dt = 1.0);

    /**
     * @brief 计算数据的积分
     */
    static QVector<double> integrate(const QVector<double>& data, double dt = 1.0);

    // ==================== 工具函数 ====================

    /**
     * @brief 将数据长度扩展到最近的2的幂次
     */
    static int nextPowerOfTwo(int n);

    /**
     * @brief 计算贝塞尔函数 I0（用于凯泽窗）
     */
    static double besselI0(double x);

private:
    static void bitReversalPermutation(QVector<std::complex<double>>& data);
};

} // namespace ComAssistant

#endif // FFTUTILS_H
