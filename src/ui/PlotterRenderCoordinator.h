/**
 * @file PlotterRenderCoordinator.h
 * @brief 绘图渲染协调器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_PLOTTERRENDERCOORDINATOR_H
#define COMASSISTANT_PLOTTERRENDERCOORDINATOR_H

#include "PlotRenderQuality.h"
#include "qcustomplot/qcustomplot.h"

#include <QTimer>
#include <functional>

namespace ComAssistant {

/**
 * @brief 绘图渲染协调器。
 *
 * 该类负责把“渲染质量档位”落到 QCustomPlot、刷新定时器和已有曲线上。
 * PlotterWindow 只保留当前模式与 UI 状态同步，避免窗口类直接承载大量渲染细节。
 */
class PlotterRenderCoordinator
{
public:
    /**
     * @brief OpenGL 后端切换回调。
     *
     * 参数表示是否请求启用 OpenGL，返回值表示实际是否启用成功。
     * 该回调由窗口层提供，因为 OpenGL 探测、日志与内存回收仍依赖窗口上下文。
     */
    using OpenGlSetter = std::function<bool(bool)>;

    /**
     * @brief 渲染协调后需要回写给窗口的状态。
     */
    struct State
    {
        bool openGlRequested = false;         ///< 用户或配置层当前是否请求 OpenGL。
        bool openGlActive = false;            ///< QCustomPlot 当前实际是否已经处于 OpenGL 后端。
        bool throttleAutoRangeUpdates = false;///< 是否节流 Y 轴自动范围刷新。
        int valuePanelUpdateEvery = 1;        ///< 数值面板每 N 帧刷新一次。
    };

    /**
     * @brief 渲染档位应用结果。
     */
    struct Result
    {
        bool applied = false;                 ///< 是否成功应用到有效绘图控件。
        bool openGlEnabled = false;           ///< OpenGL 实际是否处于启用状态。
        RenderQualityProfile profile;         ///< 本次档位对应的渲染策略。
    };

    /**
     * @brief 应用指定渲染质量档位。
     * @param plot 目标绘图控件。
     * @param updateTimer 绘图刷新定时器，可为空。
     * @param mode 渲染质量模式。
     * @param openGlSetter OpenGL 切换回调；为空时仅按软件绘制处理。
     * @param state 输入/输出状态，函数会读取 openGlRequested 并回写节流参数。
     * @param xyCurve XY 模式曲线，可为空。
     * @return 渲染档位应用结果。
     */
    static Result applyQualityProfile(QCustomPlot* plot,
                                      QTimer* updateTimer,
                                      RenderQualityMode mode,
                                      const OpenGlSetter& openGlSetter,
                                      State* state,
                                      QCPCurve* xyCurve = nullptr);

    /**
     * @brief 为新建曲线应用统一的实时绘图样式。
     * @param graph 新建或待更新的曲线。
     * @param mode 当前渲染质量模式。
     */
    static void applyGraphDefaults(QCPGraph* graph, RenderQualityMode mode);

    /**
     * @brief 为 XY 视图曲线应用统一渲染样式。
     * @param curve XY 视图使用的参数曲线。
     * @param mode 当前渲染质量模式。
     */
    static void applyCurveDefaults(QCPCurve* curve, RenderQualityMode mode);
};

} // namespace ComAssistant

#endif // COMASSISTANT_PLOTTERRENDERCOORDINATOR_H
