/**
 * @file TestTabbedReceiveWidget.cpp
 * @brief 接收区缓存与显示行为回归测试实现
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#include "TestTabbedReceiveWidget.h"

#include "ui/widgets/TabbedReceiveWidget.h"

#include <QAction>
#include <QLineEdit>
#include <QMenu>
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

/**
 * @brief 从菜单中按 objectName 查找动作。
 * @param menu 待检查的右键菜单。
 * @param objectName 菜单动作稳定对象名。
 * @return 找到的动作指针；未找到时返回空指针。
 */
QAction* findActionByObjectName(const QMenu& menu, const char* objectName)
{
    const QList<QAction*> actions = menu.actions();
    for (QAction* action : actions) {
        if (action && action->objectName() == QLatin1String(objectName)) {
            return action;
        }
    }
    return nullptr;
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

void TestTabbedReceiveWidget::testReceiveContextMenuContainsOperationalActions()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QScopedPointer<QMenu> menu(widget.createReceiveContextMenu());
    QVERIFY2(!menu.isNull(), "接收区应提供自定义右键菜单");

    QVERIFY2(findActionByObjectName(*menu, "receiveContextCopyAction") != nullptr,
             "右键菜单应保留复制动作");
    QVERIFY2(findActionByObjectName(*menu, "receiveContextSelectAllAction") != nullptr,
             "右键菜单应保留全选动作");
    QVERIFY2(findActionByObjectName(*menu, "receiveContextClearAction") != nullptr,
             "右键菜单应提供清屏动作");
    QVERIFY2(findActionByObjectName(*menu, "receiveContextPauseAction") != nullptr,
             "右键菜单应提供暂停/继续显示动作");
    QVERIFY2(findActionByObjectName(*menu, "receiveContextAutoScrollAction") != nullptr,
             "右键菜单应提供自动滚动开关");
    QVERIFY2(findActionByObjectName(*menu, "receiveContextHexDisplayAction") != nullptr,
             "右键菜单应提供 HEX 显示开关");
}

void TestTabbedReceiveWidget::testReceiveContextMenuActionsOperateOnDisplayState()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    widget.appendData(QByteArray("first\n"));
    QTest::qWait(30);
    QVERIFY2(mainViewText(widget).contains(QStringLiteral("first")),
             "测试前应先有可清除的接收内容");

    QScopedPointer<QMenu> menu(widget.createReceiveContextMenu());
    QAction* clearAction = findActionByObjectName(*menu, "receiveContextClearAction");
    QAction* pauseAction = findActionByObjectName(*menu, "receiveContextPauseAction");
    QAction* autoScrollAction = findActionByObjectName(*menu, "receiveContextAutoScrollAction");
    QAction* hexDisplayAction = findActionByObjectName(*menu, "receiveContextHexDisplayAction");

    QVERIFY(clearAction != nullptr);
    QVERIFY(pauseAction != nullptr);
    QVERIFY(autoScrollAction != nullptr);
    QVERIFY(hexDisplayAction != nullptr);

    clearAction->trigger();
    QCOMPARE(mainViewText(widget), QString());

    pauseAction->trigger();
    QVERIFY2(widget.isDisplayPaused(), "触发暂停动作后应进入显示暂停状态");

    widget.appendData(QByteArray("paused\n"));
    QTest::qWait(30);
    QVERIFY2(!mainViewText(widget).contains(QStringLiteral("paused")),
             "暂停显示时新数据不应立即刷到文本区");

    hexDisplayAction->trigger();
    QVERIFY2(widget.isHexDisplayEnabled(), "暂停期间也应能切换后续显示格式");
    QVERIFY2(!mainViewText(widget).contains(QStringLiteral("70 61 75 73 65 64")),
             "暂停期间切换 HEX 显示不应提前刷出暂停期间收到的数据");

    QScopedPointer<QMenu> pausedMenu(widget.createReceiveContextMenu());
    QAction* resumeAction = findActionByObjectName(*pausedMenu, "receiveContextPauseAction");
    QVERIFY(resumeAction != nullptr);
    QVERIFY2(resumeAction->text().contains(QStringLiteral("继续")),
             "暂停后菜单文字应切换为继续显示");

    resumeAction->trigger();
    QVERIFY2(!widget.isDisplayPaused(), "触发继续动作后应退出显示暂停状态");
    QTest::qWait(30);
    QVERIFY2(mainViewText(widget).contains(QStringLiteral("70 61 75 73 65 64")),
             "继续显示后应按当前 HEX 显示设置补刷暂停期间收到的数据");

    autoScrollAction->trigger();
    QVERIFY2(!widget.isAutoScrollEnabled(), "自动滚动动作应能关闭自动滚动");
}
