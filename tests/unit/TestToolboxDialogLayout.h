/**
 * @file TestToolboxDialogLayout.h
 * @brief 工具箱窗口布局回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTTOOLBOXDIALOGLAYOUT_H
#define TESTTOOLBOXDIALOGLAYOUT_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证工具箱窗口在紧凑尺寸下不会裁剪或遮挡编码转换控件。
 */
class TestToolboxDialogLayout : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 编码转换页必须保留滚动能力和稳定控件尺寸，避免底部控件互相遮挡。
     */
    void testEncodingTabKeepsControlsReadable();
};

#endif // TESTTOOLBOXDIALOGLAYOUT_H
