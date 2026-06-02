/**
 * @file TestThemeCoverage.h
 * @brief 主题样式覆盖测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTTHEMECOVERAGE_H
#define TESTTHEMECOVERAGE_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证内置 QSS 主题是否覆盖主界面和所有常见弹窗控件。
 *
 * UI 升级主要通过全局 QSS 完成。该测试直接读取主题文件，检查 QDialog、
 * QMessageBox、QMenu、QDialogButtonBox、表格和重点窗口选择器，避免新增或
 * 重构主题时只美化主窗口却漏掉弹出窗口。
 */
class TestThemeCoverage : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 浅色主题必须覆盖主窗口、弹窗、消息框、菜单和数据控件。
     */
    void testLightThemeCoversDialogAndPopupSurfaces();

    /**
     * @brief 暗色主题必须覆盖主窗口、弹窗、消息框、菜单和数据控件。
     */
    void testDarkThemeCoversDialogAndPopupSurfaces();

    /**
     * @brief 暗色主题覆盖层必须保留项目原有 Apple 深色背景。
     */
    void testDarkThemeKeepsOriginalAppleDarkPalette();

    /**
     * @brief 主题必须为输入、下拉和数值控件预留足够显示高度。
     */
    void testThemesReserveInputControlHeight();
};

#endif // TESTTHEMECOVERAGE_H
