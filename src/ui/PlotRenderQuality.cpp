/**
 * @file PlotRenderQuality.cpp
 * @brief 波形渲染质量策略实现
 */

#include "PlotRenderQuality.h"

#include <QtGlobal>

namespace ComAssistant {

namespace {

int maxChannelDistance(const QColor& left, const QColor& right)
{
    /*
     * 用 RGB 三通道最大差值判断“是否明显不同”，比简单灰度差更稳：
     * 蓝色/红色曲线在灰度上可能接近背景，但任一颜色通道都会有明显偏移。
     */
    return qMax(qAbs(left.red() - right.red()),
                qMax(qAbs(left.green() - right.green()),
                     qAbs(left.blue() - right.blue())));
}

QColor findReferenceBackgroundColor(const QImage& image, const QColor& fallbackColor)
{
    /*
     * 绘图截图的四个角通常都是背景区域；用角点作为参考色，可以识别
     * “整张图被清成纯白/纯黑/纯灰”的白屏类问题，即使系统主题背景色不同也能工作。
     */
    const QPoint corners[] = {
        QPoint(0, 0),
        QPoint(qMax(0, image.width() - 1), 0),
        QPoint(0, qMax(0, image.height() - 1)),
        QPoint(qMax(0, image.width() - 1), qMax(0, image.height() - 1))
    };

    int red = 0;
    int green = 0;
    int blue = 0;
    int count = 0;
    for (const QPoint& corner : corners) {
        const QColor color = image.pixelColor(corner);
        if (color.alpha() <= 24) {
            continue;
        }
        red += color.red();
        green += color.green();
        blue += color.blue();
        ++count;
    }

    if (count == 0) {
        return fallbackColor;
    }
    return QColor(red / count, green / count, blue / count);
}

} // namespace

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

PlotFrameInkAnalysis analyzePlotFrameInk(const QImage& image,
                                         const QColor& backgroundColor,
                                         double minimumInkRatio)
{
    PlotFrameInkAnalysis analysis;

    /*
     * 空图、零尺寸图和透明图都不能证明 OpenGL 真正白屏。
     * 调用方会把 valid=false 当作“跳过诊断”，避免在窗口尚未显示时误关 OpenGL。
     */
    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        analysis.valid = false;
        analysis.likelyBlank = true;
        return analysis;
    }

    const QImage frame = image.convertToFormat(QImage::Format_ARGB32);
    const QColor referenceColor = findReferenceBackgroundColor(frame, backgroundColor);
    const double requiredInkRatio = qBound(0.0, minimumInkRatio, 1.0);

    /*
     * 18 个色阶的阈值用于过滤抗锯齿边缘和浅色背景渐变。
     * 坐标轴、文字、网格和曲线都会明显超过该阈值；纯白屏则不会产生有效像素。
     */
    constexpr int visibleDeltaThreshold = 18;
    constexpr int opaqueAlphaThreshold = 24;

    for (int y = 0; y < frame.height(); ++y) {
        const QRgb* scanLine = reinterpret_cast<const QRgb*>(frame.constScanLine(y));
        for (int x = 0; x < frame.width(); ++x) {
            const QColor pixelColor = QColor::fromRgba(scanLine[x]);
            if (pixelColor.alpha() <= opaqueAlphaThreshold) {
                continue;
            }

            ++analysis.sampledPixels;
            if (maxChannelDistance(pixelColor, referenceColor) > visibleDeltaThreshold) {
                ++analysis.inkPixels;
            }
        }
    }

    analysis.valid = analysis.sampledPixels > 0;
    analysis.inkRatio = analysis.valid
        ? static_cast<double>(analysis.inkPixels) / static_cast<double>(analysis.sampledPixels)
        : 0.0;
    analysis.likelyBlank = !analysis.valid || analysis.inkRatio < requiredInkRatio;
    return analysis;
}

} // namespace ComAssistant
