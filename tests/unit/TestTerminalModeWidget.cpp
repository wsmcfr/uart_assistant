/**
 * @file TestTerminalModeWidget.cpp
 * @brief 终端模式交互行为回归测试
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#include "TestTerminalModeWidget.h"

#include "ui/modes/TerminalModeWidget.h"

#include <QCoreApplication>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

using namespace ComAssistant;

namespace {

/**
 * @brief 将终端当前屏幕导出到临时文件并读回文本内容。
 * @param widget 需要检查显示内容的终端模式控件。
 * @param tempDir 测试专用临时目录，保证导出文件不会污染用户工作区。
 * @return 导出的屏幕文本；导出失败时返回空字符串。
 */
QString exportTerminalScreen(TerminalModeWidget& widget, const QTemporaryDir& tempDir)
{
    const QString filePath = tempDir.filePath(QStringLiteral("terminal_screen.txt"));
    if (!widget.exportToFile(filePath)) {
        return QString();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream stream(&file);
    return stream.readAll();
}

} // namespace

void TestTerminalModeWidget::testAppendSentDataDoesNotEchoToTerminal()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    TerminalModeWidget widget;
    widget.clear();

    /*
     * 主窗口会在底层通信对象写入成功后调用 appendSentData。
     * 终端模式这里不应该显示本地发送内容，否则没有设备回包时也会
     * 看到“自己发送的命令”，和真实终端交互模型不一致。
     */
    widget.appendSentData("help\n");
    QString screenText = exportTerminalScreen(widget, tempDir);
    QVERIFY2(!screenText.contains(QStringLiteral("help")),
             "appendSentData must not echo local terminal commands to screen");

    /*
     * 接收路径仍然必须正常显示，避免测试只证明“完全不显示数据”。
     */
    widget.appendReceivedData("OK\n");
    screenText = exportTerminalScreen(widget, tempDir);
    QVERIFY2(screenText.contains(QStringLiteral("OK")),
             "appendReceivedData must continue to render device responses");
}

void TestTerminalModeWidget::testEnterKeepsTypedCommandVisible()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    TerminalModeWidget widget;
    widget.clear();
    widget.setConnected(true);

    VT100Display* display = widget.findChild<VT100Display*>();
    QVERIFY(display != nullptr);

    /*
     * 模拟用户直接在终端黑色面板内输入命令。这里走真实键盘事件，
     * 可以同时覆盖 VT100Display 的本地输入缓冲和 Enter 提交流程。
     */
    QTest::keyClicks(display, QStringLiteral("AT+RST"));
    QString screenText = exportTerminalScreen(widget, tempDir);
    QVERIFY2(screenText.contains(QStringLiteral("AT+RST")),
             "typed command must be visible before pressing Enter");

    QTest::keyClick(display, Qt::Key_Return);
    screenText = exportTerminalScreen(widget, tempDir);
    QVERIFY2(screenText.contains(QStringLiteral("AT+RST")),
             "pressing Enter must keep the typed command visible");

    /*
     * 底层发送成功后主窗口会调用 appendSentData。该回调仍然不能额外追加
     * 第二份命令，否则会从“消失”变成“重复显示”。
     */
    widget.appendSentData("AT+RST\n");
    screenText = exportTerminalScreen(widget, tempDir);
    QCOMPARE(screenText.count(QStringLiteral("AT+RST")), 1);
}

void TestTerminalModeWidget::testTabCompletionListsMatchingCommands()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    /*
     * 终端命令列表来自 QSettings。测试前写入专用命令，确保补全顺序稳定：
     * c 能匹配 cd/chmod/chown，ch 只匹配 chmod/chown。
     */
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("ComAssistantTest"));
    QCoreApplication::setApplicationName(QStringLiteral("TerminalCompletion"));

    QSettings settings;
    settings.beginGroup(QStringLiteral("TerminalMode"));
    settings.beginWriteArray(QStringLiteral("Commands"));
    settings.setArrayIndex(0);
    settings.setValue(QStringLiteral("command"), QStringLiteral("cd"));
    settings.setArrayIndex(1);
    settings.setValue(QStringLiteral("command"), QStringLiteral("chmod"));
    settings.setArrayIndex(2);
    settings.setValue(QStringLiteral("command"), QStringLiteral("chown"));
    settings.endArray();
    settings.endGroup();
    settings.sync();

    TerminalModeWidget widget;
    widget.clear();
    widget.setConnected(true);

    VT100Display* display = widget.findChild<VT100Display*>();
    QVERIFY(display != nullptr);

    QTest::keyClicks(display, QStringLiteral("c"));
    QTest::keyClick(display, Qt::Key_Tab);
    QString screenText = exportTerminalScreen(widget, tempDir);
    QVERIFY2(screenText.contains(QStringLiteral("cd")),
             "c+Tab should list cd");
    QVERIFY2(screenText.contains(QStringLiteral("chmod")),
             "c+Tab should list chmod");
    QVERIFY2(screenText.contains(QStringLiteral("chown")),
             "c+Tab should list chown");
    QVERIFY2(screenText.contains(QStringLiteral("c")),
             "input prefix should remain visible after listing candidates");

    QTest::keyClicks(display, QStringLiteral("h"));
    QTest::keyClick(display, Qt::Key_Tab);
    screenText = exportTerminalScreen(widget, tempDir);
    QVERIFY2(screenText.contains(QStringLiteral("chmod")),
             "ch+Tab should list chmod");
    QVERIFY2(screenText.contains(QStringLiteral("chown")),
             "ch+Tab should list chown");
    QCOMPARE(screenText.count(QStringLiteral("cd")), 1);
}
