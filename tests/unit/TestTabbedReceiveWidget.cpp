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
