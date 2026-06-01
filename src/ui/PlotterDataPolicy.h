/**
 * @file PlotterDataPolicy.h
 * @brief 绘图数据策略
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_PLOTTERDATAPOLICY_H
#define COMASSISTANT_PLOTTERDATAPOLICY_H

namespace ComAssistant {

/**
 * @brief 数据抽稀比例枚举。
 */
enum class DecimationRatio {
    None = 1,    ///< 无抽稀 (1:1)。
    Half = 2,    ///< 1:2。
    Fifth = 5,   ///< 1:5。
    Tenth = 10   ///< 1:10。
};

/**
 * @brief 绘图数据策略。
 *
 * 该类承接 PlotterWindow 中不依赖 QCustomPlot 的数据策略：抽稀状态机
 * 和实时曲线裁剪计划。窗口类仍负责实际写入/删除图形数据，策略类只
 * 返回“是否跳过”和“该删多少”的决策结果。
 */
class PlotterDataPolicy
{
public:
    /**
     * @brief 曲线裁剪计划。
     */
    struct TrimPlan
    {
        bool shouldTrim = false; ///< 当前数据量是否需要裁剪。
        int targetSize = 0;      ///< 裁剪后目标保留点数。
        int removeCount = 0;     ///< 需要从头部移除的点数。
    };

    /**
     * @brief 设置抽稀比例并重置计数器。
     * @param ratio 新抽稀比例。
     */
    void setDecimationRatio(DecimationRatio ratio);

    /**
     * @brief 获取当前抽稀比例。
     * @return 当前抽稀比例。
     */
    DecimationRatio decimationRatio() const;

    /**
     * @brief 判断下一个数据点是否应跳过。
     * @return true 表示跳过当前点；false 表示保留当前点。
     */
    bool shouldSkipNextPoint();

    /**
     * @brief 创建裁剪计划。
     * @param dataSize 当前曲线点数。
     * @param maxDataPoints 最大允许点数。
     * @return 裁剪计划；不需要裁剪时 shouldTrim 为 false。
     */
    static TrimPlan makeTrimPlan(int dataSize, int maxDataPoints);

private:
    DecimationRatio m_decimationRatio = DecimationRatio::None; ///< 当前抽稀比例。
    int m_decimationCounter = 0; ///< 抽稀计数器，达到比例值时保留该点并归零。
};

} // namespace ComAssistant

#endif // COMASSISTANT_PLOTTERDATAPOLICY_H
