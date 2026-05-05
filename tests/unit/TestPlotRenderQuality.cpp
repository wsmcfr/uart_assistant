/**
 * @file TestPlotRenderQuality.cpp
 * @brief 绘图渲染质量策略单元测试
 * @author ComAssistant Team
 * @date 2026-03-09
 */

#include "TestPlotRenderQuality.h"
#include "ui/PlotRenderQuality.h"

using namespace ComAssistant;

void TestPlotRenderQuality::initTestCase()
{
    qDebug() << "Starting PlotRenderQuality tests...";
}

void TestPlotRenderQuality::cleanupTestCase()
{
    qDebug() << "PlotRenderQuality tests completed.";
}

void TestPlotRenderQuality::testDefaultBackendProfile()
{
    const PlotBackendProfile profile = makeDefaultPlotBackendProfile();

    /*
     * 绘图窗口默认不直接启用 OpenGL，避免新窗口刚打开就分配大体积 FBO。
     * 同时保留 4x 多重采样，保证用户手动开启硬件加速时仍具备平滑边缘质量。
     */
    QCOMPARE(profile.openGlEnabledByDefault, false);
    QCOMPARE(profile.openGlMultisamples, 4);
}

void TestPlotRenderQuality::testHighQualityProfile()
{
    const RenderQualityProfile profile = makeRenderQualityProfile(RenderQualityMode::HighQuality);

    QCOMPARE(profile.updateIntervalMs, 33);
    QCOMPARE(profile.noAntialiasingOnDrag, false);
    QCOMPARE(profile.useFastPolylines, false);
    QCOMPARE(profile.antialiasPlottables, true);
    QCOMPARE(profile.throttleAutoRangeUpdates, false);
    QCOMPARE(profile.valuePanelUpdateEvery, 2);
}

void TestPlotRenderQuality::testHighPerformanceProfile()
{
    const RenderQualityProfile profile = makeRenderQualityProfile(RenderQualityMode::HighPerformance);

    QCOMPARE(profile.updateIntervalMs, 25);
    QCOMPARE(profile.noAntialiasingOnDrag, true);
    QCOMPARE(profile.useFastPolylines, true);
    QCOMPARE(profile.antialiasPlottables, false);
    QCOMPARE(profile.throttleAutoRangeUpdates, true);
    QCOMPARE(profile.valuePanelUpdateEvery, 4);
}

void TestPlotRenderQuality::testModeDifference()
{
    const RenderQualityProfile quality = makeRenderQualityProfile(RenderQualityMode::HighQuality);
    const RenderQualityProfile performance = makeRenderQualityProfile(RenderQualityMode::HighPerformance);

    QVERIFY(quality.updateIntervalMs > performance.updateIntervalMs);
    QVERIFY(quality.antialiasPlottables != performance.antialiasPlottables);
    QVERIFY(quality.useFastPolylines != performance.useFastPolylines);
    QVERIFY(quality.throttleAutoRangeUpdates != performance.throttleAutoRangeUpdates);
}

void TestPlotRenderQuality::testBackendProfileIndependentFromQualityMode()
{
    const PlotBackendProfile backend = makeDefaultPlotBackendProfile();
    const RenderQualityProfile quality = makeRenderQualityProfile(RenderQualityMode::HighQuality);
    const RenderQualityProfile performance = makeRenderQualityProfile(RenderQualityMode::HighPerformance);

    /*
     * 这条断言用于防止后续维护时把“渲染质量档位”和“默认后端选择”重新耦合。
     * 内存优化只应影响默认后端，不应偷偷篡改已有质量档位的刷新节流参数。
     */
    QCOMPARE(backend.openGlEnabledByDefault, false);
    QCOMPARE(backend.openGlMultisamples, 4);
    QCOMPARE(quality.updateIntervalMs, 33);
    QCOMPARE(performance.updateIntervalMs, 25);
}
