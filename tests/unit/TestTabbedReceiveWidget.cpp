/**
 * @file TestTabbedReceiveWidget.cpp
 * @brief 接收区缓存与显示行为回归测试实现
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#include "TestTabbedReceiveWidget.h"

#include "ui/widgets/TabbedReceiveWidget.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>

using namespace ComAssistant;

namespace {

/**
 * @brief 获取接收区主文本编辑器内容。
 * @param widget 待检查的接收区控件。
 * @return 当前文本标签页中的纯文本内容。
 */
QString mainViewText(TabbedReceiveWidget& widget)
{
    QPlainTextEdit* editor = widget.findChild<QPlainTextEdit*>();
    return editor ? editor->toPlainText() : QString();
}

/**
 * @brief 获取过滤结果文本框内容。
 * @param widget 待检查的接收区控件。
 * @return 过滤标签页中的结果文本。
 */
QString filterViewText(TabbedReceiveWidget& widget)
{
    QTextEdit* editor = widget.findChild<QTextEdit*>();
    return editor ? editor->toPlainText() : QString();
}

} // namespace

void TestTabbedReceiveWidget::testHexToggleRebuildsMainViewFromBufferedData()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const QByteArray payload("Alpha\nBeta\n");
    widget.appendData(payload);
    QTest::qWait(30);

    const QString initialText = mainViewText(widget);
    QVERIFY2(initialText.contains(QStringLiteral("Alpha")),
             "文本模式应显示第一行原始文本");
    QVERIFY2(initialText.contains(QStringLiteral("Beta")),
             "文本模式应显示第二行原始文本");

    widget.setHexDisplayEnabled(true);
    QTest::qWait(30);

    const QString hexText = mainViewText(widget);
    QVERIFY2(hexText.contains(QStringLiteral("41 6C 70 68 61 0A")),
             "切到 HEX 显示后，应看到 Alpha 的十六进制内容");
    QVERIFY2(hexText.contains(QStringLiteral("42 65 74 61 0A")),
             "切到 HEX 显示后，应看到 Beta 的十六进制内容");

    widget.setHexDisplayEnabled(false);
    QTest::qWait(30);

    const QString rebuiltText = mainViewText(widget);
    QCOMPARE(rebuiltText, initialText);
}

void TestTabbedReceiveWidget::testFilterViewFindsMatchingLinesFromCurrentDocument()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    widget.appendData(QByteArray("INFO start\nERROR failed\nWARN retry\n"));
    QTest::qWait(30);

    QLineEdit* filterInput = widget.findChild<QLineEdit*>();
    QVERIFY(filterInput != nullptr);

    filterInput->setText(QStringLiteral("ERROR"));
    QTest::qWait(30);

    const QString filteredText = filterViewText(widget);
    QVERIFY2(filteredText.contains(QStringLiteral("ERROR failed")),
             "过滤结果应包含匹配的 ERROR 行");
    QVERIFY2(!filteredText.contains(QStringLiteral("INFO start")),
             "过滤结果不应包含未匹配的 INFO 行");
    QVERIFY2(!filteredText.contains(QStringLiteral("WARN retry")),
             "过滤结果不应包含未匹配的 WARN 行");
}
