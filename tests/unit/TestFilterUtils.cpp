/**
 * @file TestFilterUtils.cpp
 * @brief 数字滤波工具边界可靠性测试
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#include "TestFilterUtils.h"
#include "core/utils/FilterUtils.h"

#include <cmath>

using namespace ComAssistant;

void TestFilterUtils::testFilterPointClampsInvalidWindowSize()
{
    /*
     * 实时滤波配置可能来自旧会话或脚本，窗口大小为 0 时不能进入
     * 空缓冲除法。滑动平均应把非法窗口规整为 1，并保持有限输出。
     */
    FilterConfig config;
    config.type = DigitalFilterType::MovingAverage;
    config.windowSize = 0;

    FilterState state;
    const double first = FilterUtils::filterPoint(10.0, config, state);
    const double second = FilterUtils::filterPoint(20.0, config, state);

    QVERIFY2(std::isfinite(first), "首次滤波输出必须是有限数");
    QVERIFY2(std::isfinite(second), "非法窗口大小不能导致 NaN/Inf");
    QCOMPARE(second, 20.0);
}

void TestFilterUtils::testFilterPointMedianWithInvalidWindowSize()
{
    /*
     * 中值滤波同样依赖窗口缓冲。非法窗口大小应自动回退到 1，
     * 这样实时绘图滤波不会因为恢复到坏配置而访问空数组。
     */
    FilterConfig config;
    config.type = DigitalFilterType::Median;
    config.windowSize = -5;

    FilterState state;
    const double first = FilterUtils::filterPoint(3.0, config, state);
    const double second = FilterUtils::filterPoint(7.0, config, state);

    QVERIFY2(std::isfinite(first), "首次中值滤波输出必须是有限数");
    QVERIFY2(std::isfinite(second), "非法窗口大小不能导致中值滤波访问空缓冲");
    QCOMPARE(second, 7.0);
}
