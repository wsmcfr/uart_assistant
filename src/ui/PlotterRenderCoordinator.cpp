/**
 * @file PlotterRenderCoordinator.cpp
 * @brief 绘图渲染协调器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "PlotterRenderCoordinator.h"

#include <QtGlobal>

namespace ComAssistant {

/**
 * @brief 将渲染质量档位应用到绘图控件。
 *
 * 主要流程：
 * 1. 由 RenderQualityMode 生成可测试的 RenderQualityProfile；
 * 2. 把抗锯齿、快速折线、拖拽抗锯齿、刷新周期写入 QCustomPlot/QTimer；
 * 3. 根据窗口层传入的 OpenGL 请求回调切换绘图后端；
 * 4. 遍历已有曲线并同步曲线级抗锯齿和自适应采样状态。
 *
 * @param plot 目标 QCustomPlot；为空时函数直接返回未应用结果。
 * @param updateTimer 刷新定时器；可为空，空值表示只应用绘图控件状态。
 * @param mode 渲染质量档位。
 * @param openGlSetter OpenGL 切换回调；由窗口层处理真实硬件探测。
 * @param state 输入当前 OpenGL 请求，并输出高频路径需要的节流参数。
 * @param xyCurve XY 视图曲线；可为空，非空时同步抗锯齿状态。
 * @return 应用结果，包含是否应用成功、OpenGL 实际状态和使用的策略。
 */
PlotterRenderCoordinator::Result PlotterRenderCoordinator::applyQualityProfile(
    QCustomPlot* plot,
    QTimer* updateTimer,
    RenderQualityMode mode,
    const OpenGlSetter& openGlSetter,
    State* state,
    QCPCurve* xyCurve)
{
    Result result;
    result.profile = makeRenderQualityProfile(mode);

    if (!plot) {
        return result;
    }

    /*
     * 这些状态会被 PlotterWindow 的高频 updatePlot 路径直接读取。
     * 在这里统一回写，可以避免窗口类和策略类各维护一套默认值。
     */
    if (state) {
        state->throttleAutoRangeUpdates = result.profile.throttleAutoRangeUpdates;
        state->valuePanelUpdateEvery = qMax(1, result.profile.valuePanelUpdateEvery);
    }

    QCP::AntialiasedElements notAntialiased =
        QCP::aeGrid | QCP::aeAxes | QCP::aeLegend | QCP::aeLegendItems;
    QCP::AntialiasedElements antialiased = QCP::aeItems;
    if (result.profile.antialiasPlottables) {
        antialiased = antialiased | QCP::aePlottables;
    } else {
        notAntialiased = notAntialiased | QCP::aePlottables;
    }

    QCP::PlottingHints hints = QCP::phCacheLabels;
    if (result.profile.useFastPolylines) {
        hints = hints | QCP::phFastPolylines;
    }

    plot->setNotAntialiasedElements(notAntialiased);
    plot->setAntialiasedElements(antialiased);
    plot->setNoAntialiasingOnDrag(result.profile.noAntialiasingOnDrag);
    plot->setPlottingHints(hints);

    if (updateTimer) {
        updateTimer->setInterval(result.profile.updateIntervalMs);
    }

    const bool openGlRequested = state ? state->openGlRequested : false;
    if (openGlRequested && openGlSetter) {
        result.openGlEnabled = openGlSetter(true);
    } else {
        plot->setOpenGl(false);
        result.openGlEnabled = false;
    }

    for (int i = 0; i < plot->graphCount(); ++i) {
        applyGraphDefaults(plot->graph(i), mode);
    }

    if (xyCurve) {
        applyCurveDefaults(xyCurve, mode);
    }

    result.applied = true;
    return result;
}

/**
 * @brief 为普通 QCPGraph 应用窗口统一曲线默认样式。
 *
 * 主要流程：
 * 1. 根据当前渲染档位决定曲线抗锯齿状态；
 * 2. 开启 QCustomPlot 自适应采样，让高密度数据绘制时自动降低屏幕采样压力。
 *
 * @param graph 目标曲线；为空时安全返回。
 * @param mode 当前渲染质量档位。
 */
void PlotterRenderCoordinator::applyGraphDefaults(QCPGraph* graph, RenderQualityMode mode)
{
    if (!graph) {
        return;
    }

    const RenderQualityProfile profile = makeRenderQualityProfile(mode);
    graph->setAntialiased(profile.antialiasPlottables);

    /*
     * QCustomPlot 的自适应采样能在远距离缩放时减少绘制点数。
     * 两个质量档位都保留该能力，因为它不改变数据本身，只影响绘制路径。
     */
    graph->setAdaptiveSampling(true);
}

/**
 * @brief 为 QCPCurve 应用窗口统一曲线默认样式。
 *
 * 主要流程：
 * 1. 根据当前渲染档位生成配置；
 * 2. 同步参数曲线的抗锯齿状态。
 *
 * @param curve 目标参数曲线；为空时安全返回。
 * @param mode 当前渲染质量档位。
 */
void PlotterRenderCoordinator::applyCurveDefaults(QCPCurve* curve, RenderQualityMode mode)
{
    if (!curve) {
        return;
    }

    const RenderQualityProfile profile = makeRenderQualityProfile(mode);
    curve->setAntialiased(profile.antialiasPlottables);
}

} // namespace ComAssistant
