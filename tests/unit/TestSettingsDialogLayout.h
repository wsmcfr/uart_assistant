/**
 * @file TestSettingsDialogLayout.h
 * @brief 设置窗口布局回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTSETTINGSDIALOGLAYOUT_H
#define TESTSETTINGSDIALOGLAYOUT_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证设置窗口尺寸和表单控件不会压缩到裁剪文字。
 */
class TestSettingsDialogLayout : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 设置窗口显示页的下拉框和数值框必须有稳定高度与宽度。
     */
    void testDisplayControlsHaveEnoughRoom();
};

#endif // TESTSETTINGSDIALOGLAYOUT_H
