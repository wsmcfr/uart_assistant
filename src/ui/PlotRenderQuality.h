/**
 * @file PlotRenderQuality.h
 * @brief 波形渲染质量策略定义
 */

#ifndef PLOTRENDERQUALITY_H
#define PLOTRENDERQUALITY_H

#include <QColor>
#include <QImage>

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
    int valuePanelUpdateEvery = 2;            ///< 数值面板每 N 次绘图刷新一次
};

/**
 * @brief 绘图后端默认配置
 *
 * 这里单独抽出“绘图后端策略”，是为了让内存优化可以通过纯配置测试覆盖，
 * 避免逻辑散落在 PlotterWindow 构造流程里难以回归。
 */
struct PlotBackendProfile {
    bool openGlEnabledByDefault = false;   ///< 新绘图窗口默认是否开启 OpenGL
    int openGlMultisamples = 4;            ///< 手动开启 OpenGL 时使用的多重采样级别
};

/**
 * @brief 绘图帧有效像素分析结果
 *
 * 该结构用于把 OpenGL 白屏检测做成可测试的纯逻辑：
 * valid 表示输入图像是否可分析，inkRatio 表示非背景像素比例，
 * likelyBlank 表示该帧是否很可能没有真正绘出坐标轴、网格或曲线。
 */
struct PlotFrameInkAnalysis {
    bool valid = false;          ///< 输入图像尺寸和像素内容是否足够用于判断
    bool likelyBlank = true;     ///< 是否接近纯背景帧
    int sampledPixels = 0;       ///< 实际采样的有效像素数量
    int inkPixels = 0;           ///< 采样中被判定为有效绘制内容的像素数量
    double inkRatio = 0.0;       ///< 有效绘制内容占采样像素的比例
};

/**
 * @brief 根据模式生成渲染参数
 * @param mode 渲染模式
 * @return 渲染配置
 */
RenderQualityProfile makeRenderQualityProfile(RenderQualityMode mode);

/**
 * @brief 获取绘图后端默认策略
 * @return 绘图后端配置
 */
PlotBackendProfile makeDefaultPlotBackendProfile();

/**
 * @brief 分析绘图截图中是否存在足够的非背景像素
 * @param image 待分析的绘图帧截图
 * @param backgroundColor 预期背景色，用于过滤纯背景或接近背景的像素
 * @param minimumInkRatio 判定为“有内容”所需的最小非背景像素比例
 * @return 绘图帧有效像素分析结果
 */
PlotFrameInkAnalysis analyzePlotFrameInk(const QImage& image,
                                         const QColor& backgroundColor,
                                         double minimumInkRatio);

} // namespace ComAssistant

#endif // PLOTRENDERQUALITY_H
