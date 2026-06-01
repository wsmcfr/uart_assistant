/**
 * @file PlotterDataPolicy.cpp
 * @brief 绘图数据策略实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "PlotterDataPolicy.h"

namespace ComAssistant {

void PlotterDataPolicy::setDecimationRatio(DecimationRatio ratio)
{
    /*
     * 切换抽稀比例时必须清零计数器，否则用户从 1:5 切到 1:2 后，
     * 新策略会继承旧策略的中间计数，导致第一批数据点保留节奏异常。
     */
    m_decimationRatio = ratio;
    m_decimationCounter = 0;
}

DecimationRatio PlotterDataPolicy::decimationRatio() const
{
    return m_decimationRatio;
}

bool PlotterDataPolicy::shouldSkipNextPoint()
{
    if (m_decimationRatio == DecimationRatio::None) {
        return false;
    }

    /*
     * 抽稀含义保持原 PlotterWindow 口径：1:N 保留第 N 个点，跳过
     * 前 N-1 个点。返回 true 表示调用方应跳过当前输入样本。
     */
    ++m_decimationCounter;
    if (m_decimationCounter >= static_cast<int>(m_decimationRatio)) {
        m_decimationCounter = 0;
        return false;
    }
    return true;
}

PlotterDataPolicy::TrimPlan PlotterDataPolicy::makeTrimPlan(int dataSize, int maxDataPoints)
{
    TrimPlan plan;
    plan.targetSize = maxDataPoints;

    if (dataSize <= maxDataPoints || maxDataPoints <= 0) {
        return plan;
    }

    /*
     * 延续原有“裁剪到上限 90%”策略：一次多删一点，避免每次新增少量
     * 数据都触发昂贵的 QCustomPlot 容器删除。
     */
    plan.shouldTrim = true;
    plan.targetSize = maxDataPoints * 9 / 10;
    plan.removeCount = dataSize - plan.targetSize;
    if (plan.removeCount < 0) {
        plan.removeCount = 0;
        plan.shouldTrim = false;
    }
    return plan;
}

} // namespace ComAssistant
