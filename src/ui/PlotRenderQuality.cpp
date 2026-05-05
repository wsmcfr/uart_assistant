/**
 * @file PlotRenderQuality.cpp
 * @brief 波形渲染质量策略实现
 */

#include "PlotRenderQuality.h"

namespace ComAssistant {

RenderQualityProfile makeRenderQualityProfile(RenderQualityMode mode)
{
    RenderQualityProfile profile;

    if (mode == RenderQualityMode::HighPerformance) {
        profile.updateIntervalMs = 25;
        profile.noAntialiasingOnDrag = true;
        profile.useFastPolylines = true;
        profile.antialiasPlottables = false;
        profile.throttleAutoRangeUpdates = true;
        profile.valuePanelUpdateEvery = 4;
    } else {
        profile.updateIntervalMs = 33;
        profile.noAntialiasingOnDrag = false;
        profile.useFastPolylines = false;
        profile.antialiasPlottables = true;
        profile.throttleAutoRangeUpdates = false;
        profile.valuePanelUpdateEvery = 2;
    }

    return profile;
}

PlotBackendProfile makeDefaultPlotBackendProfile()
{
    PlotBackendProfile profile;

    /*
     * 根因说明：
     * 1. QCustomPlot 一旦启用 OpenGL，会把绘图缓冲切换为 FBO；
     * 2. 该控件默认还存在 overlay 缓冲层，因此不是只有一份缓冲；
     * 3. setOpenGl(true) 若不显式指定 multisampling，会走默认 16x 采样，
     *    在高 DPI/较大窗口下会放大显存与进程映射内存占用。
     *
     * 对串口实时波形来说，默认软件绘制已经能保持当前画质与刷新体验，
     * 因此这里把 OpenGL 改为“按需手动开启”更合理；同时保留 4x MSAA，
     * 让用户手动开启硬件加速时仍具备平滑边缘效果，而不会再被 16x 放大。
     */
    profile.openGlEnabledByDefault = false;
    profile.openGlMultisamples = 4;

    return profile;
}

} // namespace ComAssistant
