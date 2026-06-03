/**
 * @file TestDataWindow.h
 * @brief 数据分窗单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTDATAWINDOW_H
#define TESTDATAWINDOW_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证数据分窗在长时间接收和导出场景下的内存边界行为。
 */
class TestDataWindow : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 无换行长流超过字符上限后，应裁剪最早内容并保留最新内容。
     */
    void testLongUnbrokenTextDropsOldestContent();

    /**
     * @brief 导出文件应只包含裁剪后的可见分窗内容。
     */
    void testExportUsesTrimmedVisibleContent();
};

#endif // TESTDATAWINDOW_H
