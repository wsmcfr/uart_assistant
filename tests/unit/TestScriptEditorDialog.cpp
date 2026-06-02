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

/**
 * @brief 为脚本编辑器注入成功发送能力。
 * @param dialog 被测对话框。
 *
 * 4.8 后脚本发送必须显式注入连接状态和发送 handler。测试中使用
 * 最小 handler，避免依赖真实 MainWindow 或通信对象。
 */
void enableAcceptedScriptSend(ScriptEditorDialog& dialog)
{
    dialog.setConnectionStateProvider([]() {
        return true;
    });
    dialog.setSendDataHandler([](const QByteArray&) {
        ScriptSendResult result;
        result.accepted = true;
        return result;
    });
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
    enableAcceptedScriptSend(dialog);
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
    enableAcceptedScriptSend(dialog);
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

/**
 * @brief 验证 serial.isOpen() 使用注入的真实连接状态。
 *
 * 4.7 的 worker 固定返回 true；4.8 要求脚本查询主窗口当前连接状态，
 * 因此测试注入 false 后输出也必须为 false。
 */
void TestScriptEditorDialog::serialIsOpenReflectsInjectedConnectionState()
{
    ScriptEditorDialog dialog;
    dialog.setConnectionStateProvider([]() {
        return false;
    });
    dialog.setSendDataHandler([](const QByteArray&) {
        ScriptSendResult result;
        result.accepted = true;
        return result;
    });
    dialog.setScript(QStringLiteral("print(serial.isOpen())"));

    QVERIFY(runButton(dialog));
    QVERIFY(outputArea(dialog));
    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("false")),
                             1000);
}

/**
 * @brief 验证未连接时脚本发送被拒绝且不发出发送信号。
 *
 * 该用例保护发送侧真实状态语义：未连接时 Lua 应拿到稳定错误，
 * 而不是先发出 sendData 信号再让主窗口异步失败。
 */
void TestScriptEditorDialog::serialSendRejectsWhenConnectionClosed()
{
    ScriptEditorDialog dialog;
    bool handlerCalled = false;
    dialog.setConnectionStateProvider([]() {
        return false;
    });
    dialog.setSendDataHandler([&handlerCalled](const QByteArray&) {
        handlerCalled = true;
        ScriptSendResult result;
        result.accepted = true;
        return result;
    });
    QSignalSpy sendSpy(&dialog, SIGNAL(sendData(QByteArray)));
    dialog.setScript(QStringLiteral("serial.send('AT')"));

    QVERIFY(runButton(dialog));
    QVERIFY(outputArea(dialog));
    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(
        outputArea(dialog)->toPlainText().contains(QStringLiteral("当前连接未打开")),
        1000);
    QCOMPARE(sendSpy.count(), 0);
    QVERIFY(!handlerCalled);
}

/**
 * @brief 验证发送 handler 拒绝时具体失败原因能显示给用户。
 *
 * 主窗口发送队列拒绝、底层写入失败或连接中途断开时，handler 会提供
 * 具体错误文本；脚本编辑器必须把它传入 Lua 错误并显示在输出区域。
 */
void TestScriptEditorDialog::serialSendReportsHandlerFailureReason()
{
    ScriptEditorDialog dialog;
    dialog.setConnectionStateProvider([]() {
        return true;
    });
    dialog.setSendDataHandler([](const QByteArray&) {
        ScriptSendResult result;
        result.accepted = false;
        result.error = QStringLiteral("queue rejected for script test");
        return result;
    });
    QSignalSpy sendSpy(&dialog, SIGNAL(sendData(QByteArray)));
    dialog.setScript(QStringLiteral("serial.send('AT')"));

    QVERIFY(runButton(dialog));
    QVERIFY(outputArea(dialog));
    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(
        outputArea(dialog)->toPlainText().contains(QStringLiteral("queue rejected for script test")),
        1000);
    QCOMPARE(sendSpy.count(), 0);
}
