/**
 * @file TestMainWindowExportIntegration.h
 * @brief 主窗口增强导出入口集成测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTMAINWINDOWEXPORTINTEGRATION_H
#define TESTMAINWINDOWEXPORTINTEGRATION_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证主窗口“导出数据”入口真正接入增强导出对话框和结构化收发记录。
 */
class TestMainWindowExportIntegration : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 收到 RX/TX 历史后，主窗口导出入口应打开增强导出对话框并填充预览记录。
     *
     * 该测试覆盖用户可见入口，避免工具栏/菜单仍停留在旧的简单文本保存流程。
     */
    void testExportActionOpensEnhancedDialogWithRxAndTxRecords();

    /**
     * @brief 串口底层分片收到同一文本行时，增强导出历史应按完整行合并为一条 RX 记录。
     *
     * 实际串口 readyRead 可能把一行日志拆成 32/32/4 等多段；导出功能面向用户
     * 的“记录”应和接收区显示的一行一致，不能把同一条设备日志导出成多条。
     */
    void testExportHistoryMergesReceiveChunksIntoCompleteTextLine();

    /**
     * @brief 断开连接后增强导出历史应自动裁剪到轻量上限，避免大抓包后内存长期停留峰值。
     */
    void testDisconnectTrimsExportHistoryToSmallRecentSlice();

    /**
     * @brief 关闭增强导出对话框后应销毁对话框，释放其中复制的历史记录和预览文本。
     */
    void testExportDialogReleasesCopiedRecordsAfterClose();
};

#endif // TESTMAINWINDOWEXPORTINTEGRATION_H
