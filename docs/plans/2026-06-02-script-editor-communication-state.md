# Script Editor Communication State Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 让脚本编辑器的 `serial.isOpen()` 和 `serial.send` / `serial.sendHex` 使用主窗口真实连接状态与发送结果，并把发送失败返回为稳定 Lua 错误。

**Architecture:** `LuaSandboxOptions` 增加带错误文本的发送回调，worker 把该回调注册给 Lua `serial` API。`ScriptEditorDialog` 保存主窗口注入的连接状态 provider 与发送 handler，并用 UI 线程 wrapper 执行真实状态读取和发送；`MainWindow` 注入 `m_commController->isConnected()` 与 `m_commController->sendData()`，不再依赖异步 `sendData` 信号完成脚本发送。

**Tech Stack:** C++17、Qt 5.12.9、Lua C API、Qt Test、CMake、MinGW Release 构建目录 `D:\comassistant\build_release`。

---

## 执行原则

| 原则 | 要求 |
|------|------|
| TDD | 每个行为先写失败测试，确认红测失败原因正确，再写最小实现 |
| 线程边界 | worker 不直接访问 UI、MainWindow 或通信对象 |
| 错误语义 | Lua 错误必须包含 `serial.send failed` 或 `serial.sendHex failed` 和具体原因 |
| 兼容性 | 保留 `ScriptEditorDialog::sendData(QByteArray)` 作为成功发送后的通知信号 |
| 文档同步 | 修改脚本用户可见行为后同步检查脚本文档、内置协议帮助和 quickstart |
| 记忆同步 | 完成持久改动后更新 `.recallloom/` |

## Task 1: LuaSandbox 发送错误原因红测

**Files:**
- Modify: `tests/unit/TestLuaSandbox.h`
- Modify: `tests/unit/TestLuaSandbox.cpp`

**Step 1: Write the failing test**

在 `TestLuaSandbox.h` 增加：

```cpp
void serialSendFailureCanExposeSpecificReason();
```

在 `TestLuaSandbox.cpp` 增加：

```cpp
/**
 * @brief 验证 serial.send 失败时能暴露调用方提供的具体原因。
 *
 * 4.8 需要把主窗口发送队列拒绝、未连接或写入失败传回 Lua。
 * 该测试先使用新回调注入稳定错误文本，证明 LuaSandbox 不再只能
 * 返回泛化的 serial.send failed。
 */
void TestLuaSandbox::serialSendFailureCanExposeSpecificReason()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;
    options.sendWithErrorCallback = [](const QByteArray&, QString* error) {
        if (error) {
            *error = QStringLiteral("queue rejected for test");
        }
        return false;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.send('data')"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("serial.send failed")));
    QVERIFY(result.errorMessage.contains(QStringLiteral("queue rejected for test")));
}
```

**Step 2: Run test to verify it fails**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL，编译错误提示 `LuaSandboxOptions` 没有 `sendWithErrorCallback` 成员。

**Step 3: Commit red test**

```powershell
git add tests/unit/TestLuaSandbox.h tests/unit/TestLuaSandbox.cpp
git commit -m "test: cover lua sandbox script send error reason"
```

## Task 2: LuaSandbox 支持带错误原因的发送回调

**Files:**
- Modify: `src/core/script/LuaSandbox.h`
- Modify: `src/core/script/LuaSandbox.cpp`

**Step 1: Add callback option**

在 `LuaSandboxOptions` 中加入：

```cpp
using SendWithErrorCallback = std::function<bool(const QByteArray&, QString*)>;

SendWithErrorCallback sendWithErrorCallback; ///< 受控发送回调，可写入失败原因
```

**Step 2: Add shared send helper**

在 `LuaSandbox.cpp` 匿名 namespace 增加 `sendBytesFromSandbox()`，优先调用 `sendWithErrorCallback`，否则回退旧 `sendCallback`。

**Step 3: Update serial.send and sendHex**

`sandboxSerialSend()` 与 `sandboxSerialSendHex()` 发送失败时使用：

```cpp
return raiseSendFailure(L, "serial.send failed", error);
```

错误文本为空时仍回退到原有 `serial.send failed` / `serial.sendHex failed`。

**Step 4: Register serial API when either callback exists**

注册条件改为：

```cpp
if (options.allowCommunicationApi
    && (options.sendCallback || options.sendWithErrorCallback)) {
    registerSerialApi(L);
}
```

**Step 5: Run tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestLuaSandbox` 全部通过。

**Step 6: Commit implementation**

```powershell
git add src/core/script/LuaSandbox.h src/core/script/LuaSandbox.cpp
git commit -m "feat: expose lua sandbox send failure reasons"
```

## Task 3: ScriptEditorDialog 真实连接状态红测

**Files:**
- Modify: `tests/unit/TestScriptEditorDialog.h`
- Modify: `tests/unit/TestScriptEditorDialog.cpp`

**Step 1: Add helper**

在测试 cpp 匿名 namespace 中增加：

```cpp
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
```

**Step 2: Update existing send tests**

在既有 `serial.send` 成功测试里调用 `enableAcceptedScriptSend(dialog)`。

**Step 3: Add failing tests**

在头文件 private slots 增加：

```cpp
void serialIsOpenReflectsInjectedConnectionState();
void serialSendRejectsWhenConnectionClosed();
void serialSendReportsHandlerFailureReason();
```

在 cpp 增加三个测试：

```cpp
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

    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("false")), 1000);
}

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

    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("当前连接未打开")), 1000);
    QCOMPARE(sendSpy.count(), 0);
    QVERIFY(!handlerCalled);
}

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

    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("queue rejected for script test")), 1000);
    QCOMPARE(sendSpy.count(), 0);
}
```

**Step 4: Run test to verify it fails**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL，编译错误提示 `ScriptEditorDialog` 没有连接状态/发送 handler setter 或 `ScriptSendResult`。

**Step 5: Commit red tests**

```powershell
git add tests/unit/TestScriptEditorDialog.h tests/unit/TestScriptEditorDialog.cpp
git commit -m "test: cover script editor communication state injection"
```

## Task 4: ScriptExecutionWorker 接收连接状态与发送结果回调

**Files:**
- Modify: `src/ui/dialogs/ScriptExecutionWorker.h`
- Modify: `src/ui/dialogs/ScriptExecutionWorker.cpp`

**Step 1: Add ScriptSendResult and callbacks**

在 worker 头文件中增加：

```cpp
struct ScriptSendResult
{
    bool accepted = false;
    QString error;
};

using SendCallback = std::function<ScriptSendResult(const QByteArray&)>;
using ConnectionStateCallback = std::function<bool()>;
```

构造函数新增 `SendCallback` 和 `ConnectionStateCallback` 参数。

**Step 2: Use callbacks in run()**

`options.sendWithErrorCallback` 调用 `m_sendCallback`，把 `ScriptSendResult::error` 写给 LuaSandbox。`options.isOpenCallback` 调用 `m_connectionStateCallback`，没有回调时返回 `false`。

**Step 3: Remove old sendRequested path**

worker 不再发 `sendRequested` 信号；发送结果通过同步回调返回给 Lua。

**Step 4: Build**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: 编译仍可能因对话框未适配而失败。

**Step 5: Commit worker change**

```powershell
git add src/ui/dialogs/ScriptExecutionWorker.h src/ui/dialogs/ScriptExecutionWorker.cpp
git commit -m "feat: let script worker query communication callbacks"
```

## Task 5: ScriptEditorDialog 注入 UI 线程通信 wrapper

**Files:**
- Modify: `src/ui/dialogs/ScriptEditorDialog.h`
- Modify: `src/ui/dialogs/ScriptEditorDialog.cpp`

**Step 1: Add public setters**

新增：

```cpp
using ConnectionStateProvider = std::function<bool()>;
using SendDataHandler = std::function<ScriptSendResult(const QByteArray&)>;

void setConnectionStateProvider(ConnectionStateProvider provider);
void setSendDataHandler(SendDataHandler handler);
```

**Step 2: Add UI-thread helpers**

新增私有函数：

```cpp
bool isScriptConnectionOpenOnUiThread();
ScriptSendResult sendScriptDataOnUiThread(const QByteArray& data);
bool isScriptConnectionOpenDirect() const;
ScriptSendResult performScriptSend(const QByteArray& data);
```

`sendScriptDataOnUiThread()` 用 `QMetaObject::invokeMethod` 和 `Qt::BlockingQueuedConnection` 切回 UI 线程。

**Step 3: Wire worker**

`startWorkerExecution()` 创建 `QPointer<ScriptEditorDialog>`，把连接状态和发送 wrapper 传给 `ScriptExecutionWorker` 构造函数。

**Step 4: Render send results**

`performScriptSend()`：
- 未连接时返回 `当前连接未打开`。
- 空数据返回 `脚本发送数据不能为空`。
- handler 为空时返回 `脚本发送通道未连接到主窗口`。
- 成功时追加 `[发送] ...` 并发出兼容 `sendData(data)` 信号。
- 失败时追加 `[发送失败] ...`，不发 `sendData`。

**Step 5: Run tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestScriptEditorDialog` 和 `TestLuaSandbox` 全部通过。

**Step 6: Commit dialog change**

```powershell
git add src/ui/dialogs/ScriptEditorDialog.h src/ui/dialogs/ScriptEditorDialog.cpp
git commit -m "feat: inject script editor communication state"
```

## Task 6: MainWindow 注入真实通信状态与发送结果

**Files:**
- Modify: `src/ui/MainWindow.cpp`

**Step 1: Update onScriptEditor**

创建对话框时不再连接 `ScriptEditorDialog::sendData` 到 `MainWindow::onSendData()`，改为：

```cpp
m_scriptEditorDialog->setConnectionStateProvider([this]() {
    return m_commController && m_commController->isConnected();
});
m_scriptEditorDialog->setSendDataHandler([this](const QByteArray& data) {
    ScriptSendResult result;
    if (!m_commController || !m_commController->isConnected()) {
        result.error = tr("当前连接未打开");
        statusBar()->showMessage(tr("请先打开当前连接后再发送。"), 3000);
        return result;
    }

    result.accepted = m_commController->sendData(data);
    result.error = result.accepted ? QString() : m_commController->lastError();
    if (!result.accepted && result.error.isEmpty()) {
        result.error = tr("发送失败");
    }
    return result;
});
```

**Step 2: Build and run tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 全部测试通过。

**Step 3: Commit MainWindow integration**

```powershell
git add src/ui/MainWindow.cpp
git commit -m "feat: bind script editor to real communication state"
```

## Task 7: 文档同步

**Files:**
- Modify: `docs/user-guide/scripting.md`
- Modify: `resources/help/protocols.html`
- Review: `resources/help/quickstart.html`
- Modify if needed: `resources/translations/en_US.ts`

**Step 1: Update scripting guide**

说明：
- `serial.isOpen()` 返回当前主窗口真实连接状态。
- `serial.send` 和 `serial.sendHex` 失败时会抛出 Lua 错误。
- 失败原因包括未连接、空数据、队列拒绝或底层写入失败。

**Step 2: Update built-in protocols help**

在 Lua 沙箱/脚本段落中补充同样语义。

**Step 3: Review quickstart**

检查 `resources/help/quickstart.html` 是否涉及脚本通信 API。若不涉及，最终说明写明不更新原因。

**Step 4: Update translation if new UI strings are introduced**

如新增 `tr(...)` 文案，更新 `resources/translations/en_US.ts` 中对应源字符串。

**Step 5: Commit docs**

```powershell
git add docs/user-guide/scripting.md resources/help/protocols.html resources/translations/en_US.ts
git commit -m "docs: document script communication errors"
```

## Task 8: 完整验证与 RecallLoom

**Files:**
- Modify: `.recallloom/rolling_summary.md`
- Modify: `.recallloom/daily_logs/2026-06-02.md`

**Step 1: Run full verification**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
ctest --test-dir build_release --output-on-failure
cmake --build build_release --config Release --target ComAssistant --parallel
git diff --check
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\validate_context.py .
```

Expected:
- 构建成功
- `ComAssistant_tests` 输出 `All tests PASSED!`
- `ctest` 通过
- 主程序构建成功并更新 `D:\comassistant\build_release\ComAssistant.exe`
- `git diff --check` 无输出
- RecallLoom 校验通过

**Step 2: Update RecallLoom**

用 revision-aware helper 更新：
- `rolling_summary.md`：记录 4.8 已完成、当前行为、下一步。
- `daily_logs/2026-06-02.md`：追加 4.8 实现、测试、文档、验证命令。

**Step 3: Final status**

Run:

```powershell
git status --short --branch
git log --oneline -10
```

Expected: 工作区只包含本次预期变更；若全部已提交则干净。
