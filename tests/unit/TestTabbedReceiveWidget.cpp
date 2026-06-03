/**
 * @file TestTabbedReceiveWidget.cpp
 * @brief 接收区缓存与显示行为回归测试实现
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#include "TestTabbedReceiveWidget.h"

#include "ui/widgets/TabbedReceiveWidget.h"

#include <QAction>
#include <QFile>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QScrollBar>
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
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget).contains(QStringLiteral("Alpha")), 300);
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget).contains(QStringLiteral("Beta")), 300);

    const QString initialText = mainViewText(widget);
    QVERIFY2(initialText.contains(QStringLiteral("Alpha")),
             "文本模式应显示第一行原始文本");
    QVERIFY2(initialText.contains(QStringLiteral("Beta")),
             "文本模式应显示第二行原始文本");

    widget.setHexDisplayEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget).contains(QStringLiteral("41 6C 70 68 61 0A")), 300);
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget).contains(QStringLiteral("42 65 74 61 0A")), 300);

    const QString hexText = mainViewText(widget);
    QVERIFY2(hexText.contains(QStringLiteral("41 6C 70 68 61 0A")),
             "切到 HEX 显示后，应看到 Alpha 的十六进制内容");
    QVERIFY2(hexText.contains(QStringLiteral("42 65 74 61 0A")),
             "切到 HEX 显示后，应看到 Beta 的十六进制内容");

    widget.setHexDisplayEnabled(false);
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget) == initialText, 300);

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
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget).contains(QStringLiteral("ERROR failed")), 300);

    QLineEdit* filterInput = widget.findChild<QLineEdit*>();
    QVERIFY(filterInput != nullptr);

    filterInput->setText(QStringLiteral("ERROR"));
    QTRY_VERIFY_WITH_TIMEOUT(filterViewText(widget).contains(QStringLiteral("ERROR failed")), 300);

    const QString filteredText = filterViewText(widget);
    QVERIFY2(filteredText.contains(QStringLiteral("ERROR failed")),
             "过滤结果应包含匹配的 ERROR 行");
    QVERIFY2(!filteredText.contains(QStringLiteral("INFO start")),
             "过滤结果不应包含未匹配的 INFO 行");
    QVERIFY2(!filteredText.contains(QStringLiteral("WARN retry")),
             "过滤结果不应包含未匹配的 WARN 行");
}

void TestTabbedReceiveWidget::testFilterViewUpdatesWhenBufferedDataFlushesAfterFilterIsSet()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    /*
     * 先设置过滤词，再接收数据。旧实现只在 QLineEdit::textChanged 时
     * 调用 updateFilterView()，因此此时过滤结果会基于空文档计算。
     * 接收刷新定时器稍后把 pending 文本落到主文档后，过滤结果也应
     * 重新计算，保证用户打开过滤页后继续收数时能看到新匹配行。
     */
    QLineEdit* filterInput = widget.findChild<QLineEdit*>();
    QVERIFY(filterInput != nullptr);
    filterInput->setText(QStringLiteral("ERROR"));

    widget.appendData(QByteArray("INFO start\nERROR arrived later\n"));
    QTRY_VERIFY_WITH_TIMEOUT(filterViewText(widget).contains(QStringLiteral("ERROR arrived later")), 300);

    const QString filteredText = filterViewText(widget);
    QVERIFY2(filteredText.contains(QStringLiteral("ERROR arrived later")),
             "过滤词已存在时，后续批量刷新落屏的数据也应同步进入过滤结果。");
    QVERIFY2(!filteredText.contains(QStringLiteral("INFO start")),
             "过滤结果仍只能包含匹配过滤词的行。");
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
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget).contains(QStringLiteral("first")), 300);
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
    QTRY_VERIFY_WITH_TIMEOUT(mainViewText(widget).contains(QStringLiteral("70 61 75 73 65 64")), 300);
    QVERIFY2(mainViewText(widget).contains(QStringLiteral("70 61 75 73 65 64")),
             "继续显示后应按当前 HEX 显示设置补刷暂停期间收到的数据");

    autoScrollAction->trigger();
    QVERIFY2(!widget.isAutoScrollEnabled(), "自动滚动动作应能关闭自动滚动");
}

void TestTabbedReceiveWidget::testSmartScrollPauseKeepsScrollPositionWhenDataArrives()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 240);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    /*
     * 先生成足够多的可见行，让主文本区出现垂直滚动条。随后把滚动条
     * 拉到顶部，模拟用户正在查看历史输出。此时控件应进入智能暂停滚动。
     */
    for (int line = 0; line < 160; ++line) {
        widget.appendData(QStringLiteral("history-%1\n").arg(line, 3, 10, QChar('0')).toUtf8());
    }
    QPlainTextEdit* editor = widget.findChild<QPlainTextEdit*>();
    QVERIFY(editor != nullptr);
    QScrollBar* scrollBar = editor->verticalScrollBar();
    QVERIFY(scrollBar != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(scrollBar->maximum() > 0, 500);

    scrollBar->setValue(0);
    QTRY_VERIFY_WITH_TIMEOUT(scrollBar->value() == 0, 300);

    /*
     * 新数据到达时，旧实现会在 flushPendingReceiveViews() 中调用
     * moveCursor(QTextCursor::End)，导致视图立即跳到底部。智能暂停状态下
     * 应只追加文档内容，不改变用户当前阅读的滚动条位置。
     */
    widget.appendData(QByteArray("new-data-after-user-scroll\n"));
    QTest::qWait(80);

    QVERIFY2(mainViewText(widget).contains(QStringLiteral("new-data-after-user-scroll")),
             "新数据仍应被追加到文本区。");
    QVERIFY2(scrollBar->value() <= 2,
             qPrintable(QStringLiteral("智能暂停滚动后不应跳到底部，当前滚动值为 %1，最大值为 %2")
                            .arg(scrollBar->value())
                            .arg(scrollBar->maximum())));
}

void TestTabbedReceiveWidget::testPausedReceiveBuffersStayBounded()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    /*
     * 用较小的行数设置把主文本区字符上限收紧到 64KB。暂停显示时连续喂入
     * 明显超过该值的数据，继续显示后只应补刷最近一段，而不是把暂停期间
     * 所有数据一次性撑进 QTextDocument。
     */
    widget.setMaxLines(100);
    widget.setDisplayPaused(true);

    const QByteArray chunk(16 * 1024, 'A');
    for (int index = 0; index < 16; ++index) {
        widget.appendData(chunk);
    }

    widget.setDisplayPaused(false);
    QTest::qWait(50);

    const QString text = mainViewText(widget);
    QVERIFY2(text.size() <= 80 * 1024,
             "暂停期间积压的文本补刷后仍应接近主文本区上限。");
    QVERIFY2(text.size() > 0,
             "继续显示后仍应保留最近收到的数据，而不是直接丢空。");
}

void TestTabbedReceiveWidget::testMainViewDropsOldestTextWhenLimitReached()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    /*
     * 用最小行数把接收区字符上限收紧到 64KB。连续追加明显超过上限
     * 且不换行的数据，可以覆盖“输出显示不断增长但没有换行”的高风险场景。
     */
    widget.setMaxLines(100);
    widget.appendData(QByteArray("EARLIEST_MARK"));
    widget.appendData(QByteArray(70 * 1024, 'A'));
    widget.appendData(QByteArray("LATEST_MARK"));
    QTest::qWait(80);

    const QString text = mainViewText(widget);
    QVERIFY2(text.size() <= 66 * 1024,
             "主文本区超过上限后应裁掉最早内容，不能继续无限增长。");
    QVERIFY2(text.contains(QStringLiteral("LATEST_MARK")),
             "裁剪时必须保留最新收到的数据。");
    QVERIFY2(!text.contains(QStringLiteral("EARLIEST_MARK")),
             "超过上限后最早的一段历史应被删除，而不是只限制后续追加。");
}

void TestTabbedReceiveWidget::testClearReleasesReceiveBuffers()
{
    TabbedReceiveWidget widget;
    widget.resize(900, 600);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    /*
     * 先打开 HEX 表格并喂入大块数据，让原始缓存、待刷新缓存和 HEX 环形
     * 模型都有机会增长到较大容量。随后清空应把这些容器 squeeze/释放，
     * 以便窗口关闭或手动清屏后工作集更容易回落。
     */
    widget.setHexDisplayEnabled(true);
    widget.appendData(QByteArray(512 * 1024, 'B'));
    QTest::qWait(120);
    QVERIFY2(!mainViewText(widget).isEmpty(),
             "测试前应先生成可清空的接收内容。");

    widget.clear();
    QTest::qWait(30);

    QCOMPARE(mainViewText(widget), QString());

    /*
     * ReceiveHexModel 是私有类，测试无法直接访问容量；这里通过源码级
     * 回归约束确保 clear() 不再把 QByteArray 重新 resize 到最大缓存。
     */
    QFile sourceFile(QStringLiteral(COMASSISTANT_SOURCE_DIR)
                     + QStringLiteral("/src/ui/widgets/TabbedReceiveWidget.cpp"));
    QVERIFY2(sourceFile.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("无法读取 TabbedReceiveWidget.cpp: %1").arg(sourceFile.errorString())));

    const QString source = QString::fromUtf8(sourceFile.readAll());
    QVERIFY2(source.contains(QStringLiteral("m_buffer.clear();")),
             "HEX 环形缓存 clear() 应释放 QByteArray 容量。");
    QVERIFY2(source.contains(QStringLiteral("m_rawData.squeeze();")),
             "原始接收缓存 clear() 后应 squeeze，避免容量停留在峰值。");
    QVERIFY2(source.contains(QStringLiteral("m_pendingHexData.squeeze();")),
             "待刷新 HEX 缓存 clear() 后应 squeeze，避免暂停/批量刷新峰值残留。");
}
