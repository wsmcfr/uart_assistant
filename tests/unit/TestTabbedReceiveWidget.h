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
};

#endif // TESTTABBEDRECEIVEWIDGET_H
