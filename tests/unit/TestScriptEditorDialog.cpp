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

/**
 * @brief 查找停止按钮。
 * @param dialog 被测脚本编辑器对话框。
 * @return 停止 QPushButton 指针，未找到时返回 nullptr。
 */
QPushButton* stopButton(ScriptEditorDialog& dialog)
{
    return dialog.findChild<QPushButton*>(QStringLiteral("stopScriptBtn"));
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
    QTRY_VERIFY_WITH_TIMEOUT(
        outputArea(dialog)->toPlainText().contains(QStringLiteral("hello from lua")),
        1000);
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

    QTRY_COMPARE_WITH_TIMEOUT(sendSpy.count(), 1, 1000);
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), QByteArray("AT\r\n"));
    QVERIFY(outputArea(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("[发送]")),
                             1000);
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
    QTRY_VERIFY_WITH_TIMEOUT(
        outputArea(dialog)->toPlainText().contains(QStringLiteral("bad script")),
        1000);
    const QString output = outputArea(dialog)->toPlainText();
    QVERIFY(output.contains(QStringLiteral("错误"))
            || output.contains(QStringLiteral("error"), Qt::CaseInsensitive));
}

/**
 * @brief 验证长脚本运行后 UI 立即返回事件循环。
 *
 * 后台执行的关键合约是 onRunScript() 不能等 Lua timeout 才返回；
 * 因此点击运行后应立即看到运行按钮禁用、停止按钮启用，并可继续点击停止。
 */
void TestScriptEditorDialog::runScriptReturnsControlBeforeLongScriptFinishes()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("while true do end"));

    QVERIFY(runButton(dialog));
    QVERIFY(stopButton(dialog));
    QVERIFY(outputArea(dialog));
    runButton(dialog)->click();

    QVERIFY(!runButton(dialog)->isEnabled());
    QVERIFY(stopButton(dialog)->isEnabled());
    QVERIFY(outputArea(dialog)->toPlainText().contains(QStringLiteral("开始执行脚本")));

    stopButton(dialog)->click();
    QTRY_VERIFY_WITH_TIMEOUT(runButton(dialog)->isEnabled(), 1000);
}

/**
 * @brief 验证停止按钮能请求取消正在运行的沙箱脚本。
 *
 * 该用例覆盖 UI 停止按钮到 LuaSandbox interruptCallback 的完整链路，
 * 最终输出必须是“脚本已取消”，而不是旧版本的“等待超时保护”提示。
 */
void TestScriptEditorDialog::stopScriptCancelsRunningSandbox()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("while true do end"));

    QVERIFY(runButton(dialog));
    QVERIFY(stopButton(dialog));
    QVERIFY(outputArea(dialog));
    runButton(dialog)->click();
    QVERIFY(stopButton(dialog)->isEnabled());
    stopButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("脚本已取消")), 1000);
    QVERIFY(runButton(dialog)->isEnabled());
    QVERIFY(!stopButton(dialog)->isEnabled());
}

/**
 * @brief 验证后台线程中的 serial.send 仍回到对话框发送信号。
 *
 * worker 线程不能直接触碰主窗口通信对象；这里要求发送请求仍由
 * ScriptEditorDialog 在 UI 线程转发为 sendData(QByteArray)。
 */
void TestScriptEditorDialog::runScriptEmitsSendDataFromWorkerThread()
{
    ScriptEditorDialog dialog;
    QSignalSpy sendSpy(&dialog, SIGNAL(sendData(QByteArray)));
    dialog.setScript(QStringLiteral("serial.send('AT\\r\\n')"));

    QVERIFY(runButton(dialog));
    QVERIFY(outputArea(dialog));
    runButton(dialog)->click();

    QTRY_COMPARE_WITH_TIMEOUT(sendSpy.count(), 1, 1000);
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), QByteArray("AT\r\n"));
    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("[发送]")), 1000);
}

/**
 * @brief 验证脚本错误后运行按钮和停止按钮恢复到空闲状态。
 *
 * 后台执行完成、失败或被取消都必须统一收敛回 Idle 状态，避免用户无法再次运行脚本。
 */
void TestScriptEditorDialog::runScriptRestoresButtonsAfterError()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("error('bad script')"));

    QVERIFY(runButton(dialog));
    QVERIFY(stopButton(dialog));
    QVERIFY(outputArea(dialog));
    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("bad script")), 1000);
    QVERIFY(runButton(dialog)->isEnabled());
    QVERIFY(!stopButton(dialog)->isEnabled());
}
