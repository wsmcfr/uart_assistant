/**
 * @file TestFFTUtils.cpp
 * @brief FFT 工具边界可靠性测试
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#include "TestFFTUtils.h"
#include "core/utils/FFTUtils.h"

#include <QtGlobal>
#include <cmath>
#include <limits>

using namespace ComAssistant;

namespace {
const double kPi = 3.14159265358979323846;
}

void TestFFTUtils::testAnalyzeWithNonPowerOfTwoSize()
{
    /*
     * 这个测试覆盖用户可在界面配置或旧会话恢复出的非 2 次幂 FFT 点数。
     * FFT 内核是 radix-2 算法，必须在分析层把 1000 规整为 1024，
     * 否则历史实现会产生越界风险或错误频谱。
     */
    QVector<double> samples;
    samples.reserve(1000);
    const double sampleRate = 1000.0;
    const double targetFrequency = 50.0;
    for (int i = 0; i < 1000; ++i) {
        samples.append(std::sin(2.0 * kPi * targetFrequency * i / sampleRate));
    }

    FFTConfig config;
    config.fftSize = 1000;
    config.sampleRate = sampleRate;
    config.windowType = WindowType::Hanning;

    const FFTResult result = FFTUtils::analyzeWithConfig(samples, config);

    QCOMPARE(result.fftSize, 1024);
    QVERIFY2(!result.frequencies.isEmpty(), "非 2 次幂 FFT 点数应自动规整后继续输出频谱");
    QVERIFY2(std::isfinite(result.dominantFrequency), "主频必须是有限数");
    QVERIFY2(std::abs(result.dominantFrequency - targetFrequency) < 2.0,
             "规整 FFT 点数后仍应保持主频检测精度");
}

void TestFFTUtils::testAnalyzeFiltersNonFiniteSamples()
{
    /*
     * 串口解析或用户脚本可能把 NaN/Inf 写入绘图曲线。
     * FFT 应跳过这些非法采样，而不是把 NaN 传染到整条频谱。
     */
    QVector<double> samples;
    samples.reserve(260);
    for (int i = 0; i < 256; ++i) {
        samples.append(std::sin(2.0 * kPi * 25.0 * i / 256.0));
    }
    samples.insert(10, std::numeric_limits<double>::quiet_NaN());
    samples.insert(20, std::numeric_limits<double>::infinity());
    samples.insert(30, -std::numeric_limits<double>::infinity());

    FFTConfig config;
    config.fftSize = 256;
    config.sampleRate = 256.0;

    const FFTResult result = FFTUtils::analyzeWithConfig(samples, config);

    QCOMPARE(result.validPoints, 256);
    QVERIFY2(!result.magnitudes.isEmpty(), "过滤非法值后仍应保留有效频谱");
    for (double magnitude : result.magnitudes) {
        QVERIFY2(std::isfinite(magnitude), "幅度谱不能包含 NaN/Inf");
    }
}

void TestFFTUtils::testAnalyzeClampsInvalidSampleRate()
{
    /*
     * 自动恢复配置或手工输入异常时，采样率可能为 0、负数或无穷。
     * 分析结果必须回退到有效采样率，避免频率分辨率和坐标轴出现 NaN。
     */
    QVector<double> samples;
    for (int i = 0; i < 64; ++i) {
        samples.append(std::sin(2.0 * kPi * 4.0 * i / 64.0));
    }

    FFTConfig config;
    config.fftSize = 64;
    config.sampleRate = std::numeric_limits<double>::infinity();

    const FFTResult result = FFTUtils::analyzeWithConfig(samples, config);

    QCOMPARE(result.fftSize, 64);
    QVERIFY2(std::isfinite(result.frequencyResolution), "频率分辨率必须是有限数");
    QVERIFY2(result.frequencyResolution > 0.0, "非法采样率应回退为正数");
    for (double frequency : result.frequencies) {
        QVERIFY2(std::isfinite(frequency), "频率数组不能包含 NaN/Inf");
    }
}
