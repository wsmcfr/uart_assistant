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
    } else {
        profile.updateIntervalMs = 33;
        profile.noAntialiasingOnDrag = false;
        profile.useFastPolylines = false;
        profile.antialiasPlottables = true;
        profile.throttleAutoRangeUpdates = false;
    }

    return profile;
}

} // namespace ComAssistant
