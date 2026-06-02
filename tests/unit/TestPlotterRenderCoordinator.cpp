/**
 * @file TestPlotterRenderCoordinator.cpp
 * @brief 绘图渲染协调器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestPlotterRenderCoordinator.h"

#include "ui/PlotterRenderCoordinator.h"
#include "qcustomplot/qcustomplot.h"

#include <QTimer>

using namespace ComAssistant;

/**
 * @brief 验证高性能档位会正确落到 QCustomPlot 和曲线对象。
 *
 * 主要流程：
 * 1. 构造一条默认曲线和刷新定时器；
 * 2. 调用渲染协调器应用高性能档位；
 * 3. 断言快速折线、曲线抗锯齿关闭、刷新周期和节流参数均符合策略。
 */
void TestPlotterRenderCoordinator::testApplyHighPerformanceProfileToPlot()
{
    QCustomPlot plot;
    QTimer timer;
    QCPGraph* graph = plot.addGraph();
    graph->setAntialiased(true);
    graph->setAdaptiveSampling(false);

    PlotterRenderCoordinator::State state;
    state.openGlRequested = false;

    const PlotterRenderCoordinator::Result result =
        PlotterRenderCoordinator::applyQualityProfile(
            &plot,
            &timer,
            RenderQualityMode::HighPerformance,
            nullptr,
            &state);

    QVERIFY(result.applied);
    QVERIFY(!result.openGlEnabled);
    QVERIFY(!plot.openGl());
    QVERIFY(plot.noAntialiasingOnDrag());
    QVERIFY(plot.plottingHints().testFlag(QCP::phFastPolylines));
    QVERIFY(plot.notAntialiasedElements().testFlag(QCP::aePlottables));
    QVERIFY(!graph->antialiased());
    QVERIFY(graph->adaptiveSampling());
    QCOMPARE(timer.interval(), 25);
    QVERIFY(state.throttleAutoRangeUpdates);
    QCOMPARE(state.valuePanelUpdateEvery, 4);
}

/**
 * @brief 验证高质量档位会恢复曲线抗锯齿与高质量刷新参数。
 *
 * 主要流程：
 * 1. 构造一条被刻意关闭抗锯齿的曲线；
 * 2. 应用高质量档位；
 * 3. 断言曲线抗锯齿、绘图提示、刷新周期和节流参数均被同步。
 */
void TestPlotterRenderCoordinator::testApplyHighQualityProfileToExistingGraphs()
{
    QCustomPlot plot;
    QTimer timer;
    QCPGraph* graph = plot.addGraph();
    graph->setAntialiased(false);
    graph->setAdaptiveSampling(false);

    PlotterRenderCoordinator::State state;
    state.openGlRequested = false;

    const PlotterRenderCoordinator::Result result =
        PlotterRenderCoordinator::applyQualityProfile(
            &plot,
            &timer,
            RenderQualityMode::HighQuality,
            nullptr,
            &state);

    QVERIFY(result.applied);
    QVERIFY(!plot.noAntialiasingOnDrag());
    QVERIFY(!plot.plottingHints().testFlag(QCP::phFastPolylines));
    QVERIFY(plot.antialiasedElements().testFlag(QCP::aePlottables));
    QVERIFY(graph->antialiased());
    QVERIFY(graph->adaptiveSampling());
    QCOMPARE(timer.interval(), 33);
    QVERIFY(!state.throttleAutoRangeUpdates);
    QCOMPARE(state.valuePanelUpdateEvery, 2);
}

/**
 * @brief 验证 OpenGL 已启用时刷新渲染档位不会重复重建后端。
 *
 * 主要流程：
 * 1. 用回调模拟窗口层已经成功启用 OpenGL；
 * 2. 第一次应用档位时允许调用后端切换回调；
 * 3. 第二次在后端已启用的状态下再次应用档位；
 * 4. 断言协调器不会重复调用 OpenGL 切换回调，同时保留 OpenGL 兼容绘图提示。
 */
void TestPlotterRenderCoordinator::testOpenGlProfileKeepsBackendAndCompatibleHints()
{
    QCustomPlot plot;
    QTimer timer;

    PlotterRenderCoordinator::State state;
    state.openGlRequested = true;
    state.openGlActive = false;

    int openGlSwitchCount = 0;
    const PlotterRenderCoordinator::OpenGlSetter setter = [&openGlSwitchCount](bool enabled) {
        ++openGlSwitchCount;
        return enabled;
    };

    const PlotterRenderCoordinator::Result firstResult =
        PlotterRenderCoordinator::applyQualityProfile(
            &plot,
            &timer,
            RenderQualityMode::HighQuality,
            setter,
            &state);

    QVERIFY(firstResult.applied);
    QVERIFY(firstResult.openGlEnabled);
    QCOMPARE(openGlSwitchCount, 1);

    state.openGlActive = firstResult.openGlEnabled;
    const PlotterRenderCoordinator::Result secondResult =
        PlotterRenderCoordinator::applyQualityProfile(
            &plot,
            &timer,
            RenderQualityMode::HighQuality,
            setter,
            &state);

    QVERIFY(secondResult.applied);
    QVERIFY(secondResult.openGlEnabled);
    QCOMPARE(openGlSwitchCount, 1);
    QVERIFY(!plot.plottingHints().testFlag(QCP::phCacheLabels));
    QVERIFY(plot.antialiasedElements().testFlag(QCP::aeAll));
}

/**
 * @brief 验证 XY 参数曲线会跟随渲染质量档位同步抗锯齿状态。
 *
 * 主要流程：
 * 1. 构造 QCustomPlot 和一条 QCPCurve；
 * 2. 分别应用高性能和高质量档位；
 * 3. 断言参数曲线抗锯齿状态与档位策略一致。
 */
void TestPlotterRenderCoordinator::testApplyCurveDefaultsFollowsQualityMode()
{
    QCustomPlot plot;
    QCPCurve* curve = new QCPCurve(plot.xAxis, plot.yAxis);

    /*
     * QCPCurve 与 QCustomPlot 的生产用法是堆分配后交给 plot 管理。
     * 测试也保持同样所有权模型，避免栈对象析构顺序与 QCustomPlot 清理流程冲突。
     */
    PlotterRenderCoordinator::applyCurveDefaults(curve, RenderQualityMode::HighPerformance);
    QVERIFY(!curve->antialiased());

    PlotterRenderCoordinator::applyCurveDefaults(curve, RenderQualityMode::HighQuality);
    QVERIFY(curve->antialiased());
}
