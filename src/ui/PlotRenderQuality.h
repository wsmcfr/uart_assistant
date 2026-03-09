/**
 * @file PlotRenderQuality.h
 * @brief 波形渲染质量策略定义
 */

#ifndef PLOTRENDERQUALITY_H
#define PLOTRENDERQUALITY_H

namespace ComAssistant {

/**
 * @brief 波形渲染质量模式
 */
enum class RenderQualityMode {
    HighQuality = 0,      ///< 高质量优先（画面细腻）
    HighPerformance = 1   ///< 高性能优先（流畅优先）
};

/**
 * @brief 渲染质量配置档案（可测试）
 */
struct RenderQualityProfile {
    int updateIntervalMs = 33;               ///< 定时刷新间隔
    bool noAntialiasingOnDrag = false;       ///< 拖拽时禁用抗锯齿
    bool useFastPolylines = false;           ///< 启用快速折线渲染
    bool antialiasPlottables = true;         ///< 曲线是否抗锯齿
    bool throttleAutoRangeUpdates = false;   ///< 是否节流Y轴自动范围更新
};

/**
 * @brief 根据模式生成渲染参数
 * @param mode 渲染模式
 * @return 渲染配置
 */
RenderQualityProfile makeRenderQualityProfile(RenderQualityMode mode);

} // namespace ComAssistant

#endif // PLOTRENDERQUALITY_H
