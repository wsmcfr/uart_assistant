/**
 * @file TestThemeCoverage.cpp
 * @brief 主题样式覆盖测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestThemeCoverage.h"

#include <QDir>
#include <QFile>
#include <QStringList>

#ifndef COMASSISTANT_SOURCE_DIR
#error "COMASSISTANT_SOURCE_DIR is not defined"
#endif

namespace {

/**
 * @brief 获取仓库根目录的规范路径。
 * @return 仓库根目录绝对路径。
 */
QString projectRootPath()
{
    return QDir::cleanPath(QStringLiteral(COMASSISTANT_SOURCE_DIR));
}

/**
 * @brief 读取指定主题的 QSS 文本。
 * @param themeName 主题名称，例如 light 或 dark。
 * @return 主题文件内容；读取失败时返回空字符串。
 */
QString readThemeQss(const QString& themeName)
{
    const QString filePath = projectRootPath() + QStringLiteral("/resources/themes/")
        + themeName + QStringLiteral(".qss");
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

/**
 * @brief 截取主题末尾的专业覆盖层文本。
 * @param qss 完整主题文件内容。
 * @return 覆盖层文本；如果未找到标记则返回空字符串。
 */
QString professionalOverlayText(const QString& qss)
{
    const int markerIndex = qss.indexOf(QStringLiteral("专业桌面工具覆盖层 v4.0"));
    if (markerIndex < 0) {
        return QString();
    }
    return qss.mid(markerIndex);
}

/**
 * @brief 断言主题包含一组关键 QSS 片段。
 * @param qss 主题文件内容。
 * @param snippets 必须出现的选择器或属性片段。
 */
void verifyThemeContainsSnippets(const QString& qss, const QStringList& snippets)
{
    for (const QString& snippet : snippets) {
        QVERIFY2(qss.contains(snippet),
                 qPrintable(QStringLiteral("Theme QSS must contain snippet: %1").arg(snippet)));
    }
}

/**
 * @brief 所有主题都必须具备的 UI 覆盖选择器。
 * @return 关键选择器和属性片段列表。
 */
QStringList requiredThemeCoverageSnippets()
{
    return {
        QStringLiteral("QDialog {"),
        QStringLiteral("QMessageBox {"),
        QStringLiteral("QMessageBox QLabel"),
        QStringLiteral("QDialogButtonBox QPushButton"),
        QStringLiteral("QDialogButtonBox QPushButton:default"),
        QStringLiteral("QPushButton[danger=\"true\"]"),
        QStringLiteral("QPushButton:focus"),
        QStringLiteral("QToolButton:focus"),
        QStringLiteral("QLineEdit:focus"),
        QStringLiteral("QComboBox:focus"),
        QStringLiteral("QMenu {"),
        QStringLiteral("QMenu::icon"),
        QStringLiteral("QGroupBox {"),
        QStringLiteral("QGroupBox::title"),
        QStringLiteral("QTableWidget, QTableView"),
        QStringLiteral("QHeaderView::section"),
        QStringLiteral("QFrame[panel=\"true\"]"),
        QStringLiteral("QWidget#dialogFooter"),
        QStringLiteral("QLabel[role=\"muted\"]"),
        QStringLiteral("QLabel[role=\"title\"]"),
        QStringLiteral("QMainWindow#plotterWindow"),
        QStringLiteral("QWidget#plotValuePanel"),
        QStringLiteral("QPlainTextEdit#codeEditor"),
        QStringLiteral("QTextEdit#scriptOutputArea")
    };
}

} // namespace

void TestThemeCoverage::testLightThemeCoversDialogAndPopupSurfaces()
{
    const QString qss = readThemeQss(QStringLiteral("light"));
    QVERIFY2(!qss.isEmpty(), "Light theme QSS must be readable.");

    verifyThemeContainsSnippets(qss, requiredThemeCoverageSnippets());
}

void TestThemeCoverage::testDarkThemeCoversDialogAndPopupSurfaces()
{
    const QString qss = readThemeQss(QStringLiteral("dark"));
    QVERIFY2(!qss.isEmpty(), "Dark theme QSS must be readable.");

    verifyThemeContainsSnippets(qss, requiredThemeCoverageSnippets());
}

void TestThemeCoverage::testDarkThemeKeepsOriginalAppleDarkPalette()
{
    const QString qss = readThemeQss(QStringLiteral("dark"));
    QVERIFY2(!qss.isEmpty(), "Dark theme QSS must be readable.");

    const QString overlay = professionalOverlayText(qss);
    QVERIFY2(!overlay.isEmpty(), "Dark theme must keep the professional overlay block.");

    /*
     * 用户明确要求暗背景回到原来的颜色。这里锁住原 Apple 深色背景，
     * 同时禁止上一版蓝灰覆盖层再次混入暗色主题。
     */
    verifyThemeContainsSnippets(overlay, {
        QStringLiteral("#1c1c1e"),
        QStringLiteral("#2c2c2e"),
        QStringLiteral("#3a3a3c"),
        QStringLiteral("#48484a"),
        QStringLiteral("#0A84FF")
    });
    QVERIFY2(!overlay.contains(QStringLiteral("#0f172a")), "Dark overlay must not use slate navy dialog background.");
    QVERIFY2(!overlay.contains(QStringLiteral("#111827")), "Dark overlay must not use slate navy panel background.");
    QVERIFY2(!overlay.contains(QStringLiteral("#0b1220")), "Dark overlay must not use blue-black input background.");
    QVERIFY2(!overlay.contains(QStringLiteral("#020617")), "Dark overlay must not use near-black editor background.");
}

void TestThemeCoverage::testThemesReserveInputControlHeight()
{
    const QStringList themes = {QStringLiteral("light"), QStringLiteral("dark")};
    for (const QString& theme : themes) {
        const QString qss = readThemeQss(theme);
        QVERIFY2(!qss.isEmpty(), qPrintable(theme + QStringLiteral(" theme QSS must be readable.")));

        const QString overlay = professionalOverlayText(qss);
        QVERIFY2(!overlay.isEmpty(), qPrintable(theme + QStringLiteral(" theme must keep the professional overlay block.")));

        /*
         * QSpinBox/QComboBox 有后缀或下拉箭头时，如果只加 padding 而不加稳定
         * 高度，Qt 5.12 在部分缩放下会裁掉文字。主题必须显式保留高度和宽度。
         */
        verifyThemeContainsSnippets(overlay, {
            QStringLiteral("min-height: 34px;"),
            QStringLiteral("min-width: 180px;"),
            QStringLiteral("QFontComboBox")
        });
    }
}
