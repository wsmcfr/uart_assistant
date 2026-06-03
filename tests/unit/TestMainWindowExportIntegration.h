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
};

#endif // TESTMAINWINDOWEXPORTINTEGRATION_H
