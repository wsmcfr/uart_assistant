# Script Editor Lua Sandbox Integration Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make the script editor run real Lua through `LuaSandbox` and expose controlled `serial.send` / `serial.sendHex` APIs that continue through the existing send queue.

**Architecture:** Extend `LuaSandboxOptions` with optional communication callbacks and register a `serial` Lua table only when explicitly enabled. Replace `ScriptEditorDialog::executeSimpleScript()` with a sandbox-backed execution path that renders sandbox output/errors and converts sandbox send callbacks into the existing `sendData(QByteArray)` signal.

**Tech Stack:** C++17, Qt 5.12.9 Widgets/Test/Core, Lua C API, existing `LuaSandbox`, `ConversionUtils`, `ScriptEditorDialog`, `MainWindow::onSendData`, and Qt test harness.

---

### Task 1: Add Red Tests For LuaSandbox Communication API

**Files:**
- Modify: `src/core/script/LuaSandbox.h`
- Modify: `tests/unit/TestLuaSandbox.h`
- Modify: `tests/unit/TestLuaSandbox.cpp`

**Step 1: Add communication API test slots**

Modify `tests/unit/TestLuaSandbox.h`:

```cpp
void registersSerialSendWhenCommunicationAllowed();
void registersSerialSendHexWhenCommunicationAllowed();
void serialApiStaysDisabledWithoutCallback();
void serialSendFailureReturnsLuaError();
void serialIsOpenUsesCallback();
```

**Step 2: Write failing tests**

Append to `tests/unit/TestLuaSandbox.cpp`:

```cpp
void TestLuaSandbox::registersSerialSendWhenCommunicationAllowed()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;

    QList<QByteArray> sentPayloads;
    options.sendCallback = [&sentPayloads](const QByteArray& data) {
        sentPayloads.append(data);
        return true;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.send('AT\\r\\n')"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(sentPayloads.size(), 1);
    QCOMPARE(sentPayloads.first(), QByteArray("AT\r\n"));
}
```

```cpp
void TestLuaSandbox::registersSerialSendHexWhenCommunicationAllowed()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;

    QByteArray sent;
    options.sendCallback = [&sent](const QByteArray& data) {
        sent = data;
        return true;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.sendHex('AA 55 01')"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(sent.toHex(' ').toUpper(), QByteArray("AA 55 01"));
}
```

```cpp
void TestLuaSandbox::serialApiStaysDisabledWithoutCallback()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print(serial == nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.first(), QStringLiteral("true"));
}
```

```cpp
void TestLuaSandbox::serialSendFailureReturnsLuaError()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;
    options.sendCallback = [](const QByteArray&) {
        return false;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.send('data')"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("serial.send failed")));
}
```

```cpp
void TestLuaSandbox::serialIsOpenUsesCallback()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;
    options.sendCallback = [](const QByteArray&) {
        return true;
    };
    options.isOpenCallback = []() {
        return false;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print(serial.isOpen())"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.first(), QStringLiteral("false"));
}
```

**Step 3: Add the desired API to the header only if needed for compilation**

To make the tests compile and fail at runtime for missing behavior, add to `LuaSandboxOptions`:

```cpp
#include <QByteArray>
#include <functional>

using SendCallback = std::function<bool(const QByteArray&)>;
using IsOpenCallback = std::function<bool()>;
SendCallback sendCallback;
IsOpenCallback isOpenCallback;
```

**Step 4: Run tests to verify RED**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestLuaSandbox` fails for serial API missing or not invoking callbacks.

**Step 5: Commit red tests**

```powershell
git add src/core/script/LuaSandbox.h tests/unit/TestLuaSandbox.h tests/unit/TestLuaSandbox.cpp
git commit -m "test: cover lua sandbox serial api"
```

### Task 2: Implement LuaSandbox Serial API

**Files:**
- Modify: `src/core/script/LuaSandbox.cpp`
- Modify: `src/core/script/LuaSandbox.h`

**Step 1: Implement serial table registration**

In `LuaSandbox.cpp`:

- Add `sandboxSerialSend(lua_State*)`.
- Add `sandboxSerialSendHex(lua_State*)`.
- Add `sandboxSerialIsOpen(lua_State*)`.
- Add `registerSerialApi(lua_State*)`.
- Call `registerSerialApi(L)` after `registerSafeFunctions(L)` only if:
  - `options.allowCommunicationApi == true`
  - `options.sendCallback` is set

Behavior:

```cpp
serial.send(data)
```

- Read Lua string with `luaL_checklstring`.
- Convert to `QByteArray`.
- Call `context->options.sendCallback(bytes)`.
- If false, `luaL_error(L, "serial.send failed")`.

```cpp
serial.sendHex(hex)
```

- Read Lua string with `luaL_checkstring`.
- Convert through `ConversionUtils::hexStringToBytes`.
- Call send callback.
- If false, `luaL_error(L, "serial.sendHex failed")`.

```cpp
serial.isOpen()
```

- If `isOpenCallback` is set, return its value.
- Otherwise return `true`, meaning the API is enabled by the caller.

**Step 2: Run tests to verify GREEN**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestLuaSandbox` passes and all existing tests pass.

**Step 3: Commit implementation**

```powershell
git add src/core/script/LuaSandbox.h src/core/script/LuaSandbox.cpp
git commit -m "feat: add controlled lua serial send api"
```

### Task 3: Add Red Tests For ScriptEditorDialog Sandbox Execution

**Files:**
- Create or modify: `tests/unit/TestScriptEditorDialog.h`
- Create or modify: `tests/unit/TestScriptEditorDialog.cpp`
- Modify: `tests/main.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create test header**

Create `tests/unit/TestScriptEditorDialog.h`:

```cpp
/**
 * @file TestScriptEditorDialog.h
 * @brief 脚本编辑器沙箱执行单元测试头文件
 */

#ifndef TESTSCRIPTEDITORDIALOG_H
#define TESTSCRIPTEDITORDIALOG_H

#include <QObject>
#include <QTest>

/**
 * @brief 脚本编辑器沙箱执行测试类
 *
 * 该测试类验证脚本编辑器是否真正调用 LuaSandbox 执行脚本，并把输出、
 * 错误和 serial.send 发送请求映射回现有 UI 信号。
 */
class TestScriptEditorDialog : public QObject
{
    Q_OBJECT

private slots:
    void runScriptUsesLuaSandboxPrintOutput();
    void runScriptEmitsSendDataFromSerialSend();
    void runScriptShowsLuaErrors();
};

#endif // TESTSCRIPTEDITORDIALOG_H
```

**Step 2: Create test body**

Create `tests/unit/TestScriptEditorDialog.cpp`:

```cpp
/**
 * @file TestScriptEditorDialog.cpp
 * @brief 脚本编辑器沙箱执行单元测试
 */

#include "TestScriptEditorDialog.h"

#include "ui/dialogs/ScriptEditorDialog.h"

#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTextEdit>

using namespace ComAssistant;

namespace {

QTextEdit* outputArea(ScriptEditorDialog& dialog)
{
    return dialog.findChild<QTextEdit*>(QStringLiteral("scriptOutputArea"));
}

QPushButton* runButton(ScriptEditorDialog& dialog)
{
    return dialog.findChild<QPushButton*>(QStringLiteral("runScriptBtn"));
}

} // namespace

void TestScriptEditorDialog::runScriptUsesLuaSandboxPrintOutput()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("print('hello from lua')"));

    QVERIFY(runButton(dialog));
    runButton(dialog)->click();

    QVERIFY(outputArea(dialog));
    QVERIFY(outputArea(dialog)->toPlainText().contains(QStringLiteral("hello from lua")));
}

void TestScriptEditorDialog::runScriptEmitsSendDataFromSerialSend()
{
    ScriptEditorDialog dialog;
    QSignalSpy sendSpy(&dialog, SIGNAL(sendData(QByteArray)));
    dialog.setScript(QStringLiteral("serial.send('AT\\r\\n')"));

    runButton(dialog)->click();

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), QByteArray("AT\r\n"));
    QVERIFY(outputArea(dialog)->toPlainText().contains(QStringLiteral("[发送]")));
}

void TestScriptEditorDialog::runScriptShowsLuaErrors()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("error('bad script')"));

    runButton(dialog)->click();

    const QString output = outputArea(dialog)->toPlainText();
    QVERIFY(output.contains(QStringLiteral("bad script")));
    QVERIFY(output.contains(QStringLiteral("错误"))
            || output.contains(QStringLiteral("error"), Qt::CaseInsensitive));
}
```

**Step 3: Register tests**

Modify `tests/main.cpp`:

- Add `#include "unit/TestScriptEditorDialog.h"`.
- Add a test block near other UI dialog tests:

```cpp
{
    qDebug() << "\n[TEST] ScriptEditorDialog";
    TestScriptEditorDialog test;
    status |= QTest::qExec(&test, filteredArgs);
}
```

Modify `tests/CMakeLists.txt`:

- Add `unit/TestScriptEditorDialog.cpp` to `TEST_SOURCES`.
- Add `${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ScriptEditorDialog.h` to the executable file list if missing.
- Add `${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ScriptEditorDialog.cpp` and `${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/syntax/LuaSyntaxHighlighter.cpp` to `target_sources`.
- Add header `${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/syntax/LuaSyntaxHighlighter.h` if needed.

**Step 4: Run tests to verify RED**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestScriptEditorDialog` fails because editor still uses `executeSimpleScript()` and does not run true Lua `print` / `error` behavior.

**Step 5: Commit red tests**

```powershell
git add tests/main.cpp tests/CMakeLists.txt tests/unit/TestScriptEditorDialog.h tests/unit/TestScriptEditorDialog.cpp
git commit -m "test: cover script editor sandbox execution"
```

### Task 4: Connect ScriptEditorDialog To LuaSandbox

**Files:**
- Modify: `src/ui/dialogs/ScriptEditorDialog.h`
- Modify: `src/ui/dialogs/ScriptEditorDialog.cpp`

**Step 1: Replace execution method**

In `ScriptEditorDialog.h`:

- Replace `executeSimpleScript(const QString& script)` with `executeSandboxScript(const QString& script)`.

In `ScriptEditorDialog.cpp`:

- Include `core/script/LuaSandbox.h`.
- Remove `QRegularExpression` include if no longer needed.
- Replace default script with real LuaSandbox API:

```lua
print("脚本加载完成")

if serial.isOpen() then
    serial.send("AT\r\n")
    serial.sendHex("AA 55 01 02 03")
end

local bytes = hexToBytes("01 03 00 00 00 02")
print("CRC16:", crc16(bytes))
```

**Step 2: Implement `executeSandboxScript()`**

Implementation requirements:

- Create `LuaSandbox sandbox;`
- Create options:
  - `timeoutMs = 3000`
  - `memoryLimitKb = 2048`
  - `maxOutputLines = 500`
  - `allowCommunicationApi = true`
  - `sendCallback` emits `sendData(bytes)` and returns `true` when bytes are non-empty.
  - `isOpenCallback` returns `true` in 4.6.
- For each payload sent, append output:
  - Printable text: `[发送] <simplified text>`
  - Binary/hex from `sendHex`: first version can use `[发送] <N bytes>`
- Execute the script.
- Append each `result.outputLines` in green or default color.
- If `!result.success`, append `[错误] <message>` in red.
- If `result.success`, leave completion message to `onRunScript()`.

**Step 3: Adjust run/stop behavior**

In `onRunScript()`:

- Call `executeSandboxScript(script)`.
- Keep run/stop button state restoration.

In `onStopScript()`:

- Keep existing visual state, but append a clearer message:
  - `[系统] 当前版本会在脚本超时保护触发后停止，后台取消将在后续版本提供`

**Step 4: Run tests to verify GREEN**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestScriptEditorDialog` passes and all tests pass.

**Step 5: Commit implementation**

```powershell
git add src/ui/dialogs/ScriptEditorDialog.h src/ui/dialogs/ScriptEditorDialog.cpp
git commit -m "feat: run script editor through lua sandbox"
```

### Task 5: Update Documentation And Help

**Files:**
- Modify: `docs/user-guide/scripting.md`
- Modify: `resources/help/protocols.html` if appropriate
- Review: `resources/help/quickstart.html`

**Step 1: Update scripting guide**

In `docs/user-guide/scripting.md`:

- State that script editor now executes through `LuaSandbox`.
- Update common API table:
  - `print(...)`
  - `serial.send(data)`
  - `serial.sendHex(hex)`
  - `serial.isOpen()`
  - `hexToBytes(hex)`
  - `bytesToHex(data)`
  - `crc16(data)`
  - `crc32(data)`
- Remove or mark `sleep(ms)` and `serial.receive(timeout)` as not available in 4.6.
- Update examples to avoid `serial.receive`.

**Step 2: Update help if needed**

If `resources/help/protocols.html` still only describes future Lua extensions, update the note:

```html
<p><strong>中文：</strong>脚本编辑器已先接入受限 Lua 沙箱，支持通过 <code>serial.send</code> 和 <code>serial.sendHex</code> 发起发送；后续脚本协议仍需通过协议描述、配置 Schema 和诊断导出暴露能力。</p>
```

Add English equivalent.

**Step 3: Review quickstart**

Search `resources/help/quickstart.html` for script/Lua references. If absent, do not update and record:

`quickstart.html` 不需要更新，因为 4.6 不改变快速连接、普通发送、接收、文件传输或会话保存主流程。

**Step 4: Run checks**

Run:

```powershell
git diff --check
```

Expected: exit 0.

**Step 5: Commit docs**

```powershell
git add docs/user-guide/scripting.md resources/help/protocols.html
git commit -m "docs: document sandboxed script editor send api"
```

### Task 6: Final Verification And RecallLoom Update

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
```

Expected:

- Test build exit 0.
- Qt test output contains `All tests PASSED!`.
- CTest output contains `100% tests passed`.
- Application build exit 0 and deploys `D:\comassistant\build_release\ComAssistant.exe`.
- `git diff --check` exit 0.

**Step 2: Update RecallLoom**

- Update `rolling_summary.md` with 4.6 completion, current judgments, risks, and next step.
- Append `daily_logs/2026-06-02.md` milestone entry for 4.6.
- Use RecallLoom revision-aware helpers.
- Validate:

```powershell
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\validate_context.py .
```

Expected: Errors 0, Warnings 0.

**Step 3: Final status**

Run:

```powershell
git status --short --branch
git log --oneline -8
```

Report:

- 4.6 commit hashes.
- Verification outcomes.
- `quickstart.html` update decision and reason.
- RecallLoom layers updated.
