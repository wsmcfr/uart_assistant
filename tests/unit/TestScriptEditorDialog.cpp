/**
 * @file TestScriptEditorDialog.cpp
 * @brief 脚本编辑器沙箱执行单元测试
 */

#include "TestScriptEditorDialog.h"

#include "ui/dialogs/ScriptEditorDialog.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QTextEdit>

using namespace ComAssistant;

namespace {

/**
 * @brief 查找脚本输出区域。
 * @param dialog 被测脚本编辑器对话框。
 * @return 输出 QTextEdit 指针，未找到时返回 nullptr。
 *
 * 测试通过对象名定位控件，避免对布局层级和控件创建顺序产生脆弱依赖。
 */
QTextEdit* outputArea(ScriptEditorDialog& dialog)
{
    return dialog.findChild<QTextEdit*>(QStringLiteral("scriptOutputArea"));
}

/**
 * @brief 查找运行按钮。
 * @param dialog 被测脚本编辑器对话框。
 * @return 运行 QPushButton 指针，未找到时返回 nullptr。
 */
QPushButton* runButton(ScriptEditorDialog& dialog)
{
    return dialog.findChild<QPushButton*>(QStringLiteral("runScriptBtn"));
}

} // namespace

/**
 * @brief 验证运行脚本时使用 LuaSandbox 的 print 输出。
 *
 * 旧的正则模拟执行器不会解析 Lua 的 print()，因此该用例先作为红测
 * 证明脚本编辑器尚未接入真实 LuaSandbox。
 */
void TestScriptEditorDialog::runScriptUsesLuaSandboxPrintOutput()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("print('hello from lua')"));

    QVERIFY(runButton(dialog));
    runButton(dialog)->click();

    QVERIFY(outputArea(dialog));
    QVERIFY(outputArea(dialog)->toPlainText().contains(QStringLiteral("hello from lua")));
}

/**
 * @brief 验证 serial.send 会通过脚本编辑器发送信号输出原始字节。
 *
 * 该用例覆盖 UI 层与既有发送链路之间的合约：脚本执行器不直接操作串口，
 * 只通过 sendData(QByteArray) 交还 MainWindow 的发送队列。
 */
void TestScriptEditorDialog::runScriptEmitsSendDataFromSerialSend()
{
    ScriptEditorDialog dialog;
    QSignalSpy sendSpy(&dialog, SIGNAL(sendData(QByteArray)));
    dialog.setScript(QStringLiteral("serial.send('AT\\r\\n')"));

    QVERIFY(runButton(dialog));
    runButton(dialog)->click();

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), QByteArray("AT\r\n"));
    QVERIFY(outputArea(dialog));
    QVERIFY(outputArea(dialog)->toPlainText().contains(QStringLiteral("[发送]")));
}

/**
 * @brief 验证 Lua 运行时错误会显示在输出区域。
 *
 * 用户写脚本时需要看到来自 LuaSandbox 的错误消息；如果仍是旧模拟执行器，
 * error('bad script') 会被忽略，该用例会失败。
 */
void TestScriptEditorDialog::runScriptShowsLuaErrors()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("error('bad script')"));

    QVERIFY(runButton(dialog));
    runButton(dialog)->click();

    QVERIFY(outputArea(dialog));
    const QString output = outputArea(dialog)->toPlainText();
    QVERIFY(output.contains(QStringLiteral("bad script")));
    QVERIFY(output.contains(QStringLiteral("错误"))
            || output.contains(QStringLiteral("error"), Qt::CaseInsensitive));
}
