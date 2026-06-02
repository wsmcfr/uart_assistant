/**
 * @file TestMainWindowPlotDataRouter.h
 * @brief 主窗口绘图数据路由器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMAINWINDOWPLOTDATAROUTER_H
#define TESTMAINWINDOWPLOTDATAROUTER_H

#include <QObject>
#include <QTest>

/**
 * @brief 主窗口绘图数据路由器回归测试
 *
 * 该测试验证绘图协议缓冲、按行解析和路由决策从 MainWindow 拆出后，
 * 仍能保持 Raw 自动检测、分块数据和异常缓冲清理等行为稳定。
 */
class TestMainWindowPlotDataRouter : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Raw 模式应要求调用方执行自动协议检测，并且不继续解析绘图数据。
     */
    void testRawProtocolRequestsAutoDetectionOnly();

    /**
     * @brief 分块输入应缓存未完成行，并在收到换行后生成绘图路由结果。
     */
    void testChunkedPlotLinesAreBufferedUntilNewline();

    /**
     * @brief 带 CRLF 的绘图行应去掉行尾回车后再交给协议解析。
     */
    void testCarriageReturnIsRemovedBeforeParsing();

    /**
     * @brief 残留缓冲超过上限时应清空，避免坏数据长期占用内存。
     */
    void testOversizedPartialBufferIsCleared();
};

#endif // TESTMAINWINDOWPLOTDATAROUTER_H
