/**
 * @file TestExportDialog.h
 * @brief 增强导出对话框单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTEXPORTDIALOG_H
#define TESTEXPORTDIALOG_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证增强导出对话框的过滤统计和预览行为。
 */
class TestExportDialog : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 启用内容过滤后，过滤后数量应显示真实可导出记录数。
     */
    void testFilteredStatisticsReflectCurrentOptions();
};

#endif // TESTEXPORTDIALOG_H
