/**
 * @file TestTabbedReceiveWidget.h
 * @brief 接收区缓存与显示行为回归测试头文件
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#ifndef TESTTABBEDRECEIVEWIDGET_H
#define TESTTABBEDRECEIVEWIDGET_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证接收区在缓存优化后仍保持正确显示与过滤能力。
 *
 * 这些测试聚焦两个高风险点：
 * 1. 文本/HEX 显示切换后，已接收数据仍能被正确重建；
 * 2. 过滤标签页仍能从当前接收内容中筛出匹配文本。
 */
class TestTabbedReceiveWidget : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 切换 HEX 显示后，主文本区应能基于缓存正确重建内容。
     *
     * 测试流程：
     * 1. 追加一段带换行的原始数据；
     * 2. 记录文本模式下的显示结果；
     * 3. 切到 HEX 显示，确认显示内容变成十六进制；
     * 4. 再切回文本模式，确认内容与初始文本一致。
     */
    void testHexToggleRebuildsMainViewFromBufferedData();

    /**
     * @brief 过滤视图应能从当前文本内容中筛选出匹配行。
     *
     * 这里验证过滤功能依旧可用，避免缓存结构调整后只保留了显示、
     * 却丢失了搜索/过滤能力。
     */
    void testFilterViewFindsMatchingLinesFromCurrentDocument();

    /**
     * @brief 过滤词已设置时，后续批量刷新的接收数据也应更新过滤结果。
     *
     * 接收区现在会先把高频数据放入 pending 队列，再由定时器统一落屏。
     * 该测试覆盖“过滤词先变化、文档稍后变化”的顺序，避免过滤结果
     * 停留在旧文档状态。
     */
    void testFilterViewUpdatesWhenBufferedDataFlushesAfterFilterIsSet();

    /**
     * @brief 接收区右键菜单应包含可操作的显示控制项。
     *
     * 菜单需要保留常用文本操作，同时补充清屏、暂停/继续显示、
     * 自动滚动和 HEX 显示等接收区业务动作。
     */
    void testReceiveContextMenuContainsOperationalActions();

    /**
     * @brief 接收区右键菜单动作应直接改变显示状态。
     *
     * 测试流程覆盖清屏、暂停/继续显示、自动滚动和 HEX 显示，
     * 避免菜单只显示动作却没有绑定真实逻辑。
     */
    void testReceiveContextMenuActionsOperateOnDisplayState();

    /**
     * @brief 智能暂停滚动后，继续接收数据不应把阅读位置强行拉回底部。
     *
     * 用户向上查看历史时，接收区会显示“已暂停滚动”。该状态下仍会继续
     * 刷新新数据，但必须保留当前滚动位置，避免用户正在看的历史被新数据打断。
     */
    void testSmartScrollPauseKeepsScrollPositionWhenDataArrives();

    /**
     * @brief 暂停显示期间的待刷新缓存应保持有界，避免长时间暂停造成内存持续增长。
     */
    void testPausedReceiveBuffersStayBounded();

    /**
     * @brief 接收区达到上限后应删除最早内容并保留最新内容。
     */
    void testMainViewDropsOldestTextWhenLimitReached();

    /**
     * @brief 清空接收区时应释放可见缓存容量，避免清屏后内存仍保持峰值。
     */
    void testClearReleasesReceiveBuffers();
};

#endif // TESTTABBEDRECEIVEWIDGET_H
