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

void TestPlotRenderQuality::testHighQualityProfile()
{
    const RenderQualityProfile profile = makeRenderQualityProfile(RenderQualityMode::HighQuality);

    QCOMPARE(profile.updateIntervalMs, 33);
    QCOMPARE(profile.noAntialiasingOnDrag, false);
    QCOMPARE(profile.useFastPolylines, false);
    QCOMPARE(profile.antialiasPlottables, true);
    QCOMPARE(profile.throttleAutoRangeUpdates, false);
}

void TestPlotRenderQuality::testHighPerformanceProfile()
{
    const RenderQualityProfile profile = makeRenderQualityProfile(RenderQualityMode::HighPerformance);

    QCOMPARE(profile.updateIntervalMs, 25);
    QCOMPARE(profile.noAntialiasingOnDrag, true);
    QCOMPARE(profile.useFastPolylines, true);
    QCOMPARE(profile.antialiasPlottables, false);
    QCOMPARE(profile.throttleAutoRangeUpdates, true);
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
