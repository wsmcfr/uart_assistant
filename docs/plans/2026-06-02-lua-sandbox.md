# Lua Sandbox Foundation Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a safe, testable Lua sandbox core that executes scripts with restricted libraries, timeout control, memory budget, output capture, and clear structured results.

**Architecture:** Add a new `LuaSandbox` core class that creates a fresh restricted Lua state per execution. Keep historical `LuaEngine` and `ScriptEditorDialog` behavior unchanged in this phase, while documenting that future UI and protocol scripting should call the sandbox.

**Tech Stack:** C++17, Qt 5.12.9 Core/Test, Lua 5.4 C API, CMake, existing `ChecksumUtils`, `ConversionUtils`, and project Qt test harness.

---

### Task 1: Add Red Tests For LuaSandbox Core Contract

**Files:**
- Create: `tests/unit/TestLuaSandbox.h`
- Create: `tests/unit/TestLuaSandbox.cpp`
- Modify: `tests/main.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test header**

Create `tests/unit/TestLuaSandbox.h`:

```cpp
/**
 * @file TestLuaSandbox.h
 * @brief Lua 安全沙箱单元测试头文件
 */

#ifndef TESTLUASANDBOX_H
#define TESTLUASANDBOX_H

#include <QObject>
#include <QTest>

/**
 * @brief Lua 安全沙箱测试类
 *
 * 该测试类验证 LuaSandbox 是否只暴露安全能力，并能在超时、内存超限、
 * 运行时错误和大量输出时返回结构化结果，避免后续脚本协议绕过安全边界。
 */
class TestLuaSandbox : public QObject
{
    Q_OBJECT

private slots:
    void allowsSafeMathStringAndTableUsage();
    void blocksUnsafeLibrariesAndLoaders();
    void capturesPrintOutput();
    void reportsRuntimeErrors();
    void timesOutInfiniteLoop();
    void limitsMemoryUsage();
    void isolatesExecutions();
    void truncatesExcessiveOutput();
};

#endif // TESTLUASANDBOX_H
```

**Step 2: Write the failing test body**

Create `tests/unit/TestLuaSandbox.cpp`:

```cpp
/**
 * @file TestLuaSandbox.cpp
 * @brief Lua 安全沙箱单元测试
 */

#include "TestLuaSandbox.h"

#include "core/script/LuaSandbox.h"

using namespace ComAssistant;

void TestLuaSandbox::allowsSafeMathStringAndTableUsage()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "local items = {'a', 'b', string.upper('c')}\n"
            "print(math.floor(2.8), table.concat(items, '-'))"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.size(), 1);
    QCOMPARE(result.outputLines.first(), QStringLiteral("2\ta-b-C"));
}

void TestLuaSandbox::blocksUnsafeLibrariesAndLoaders()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "print(os == nil)\n"
            "print(io == nil)\n"
            "print(package == nil)\n"
            "print(debug == nil)\n"
            "print(require == nil)\n"
            "print(dofile == nil)\n"
            "print(loadfile == nil)\n"
            "print(load == nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.size(), 8);
    for (const QString& line : result.outputLines) {
        QCOMPARE(line, QStringLiteral("true"));
    }
}

void TestLuaSandbox::capturesPrintOutput()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print('hello', 42, true, nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines,
             QStringList({QStringLiteral("hello\t42\ttrue\tnil")}));
    QVERIFY(result.elapsedMs >= 0);
}

void TestLuaSandbox::reportsRuntimeErrors()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("error('boom')"),
        options);

    QVERIFY(!result.success);
    QVERIFY(!result.timedOut);
    QVERIFY(!result.memoryExceeded);
    QVERIFY(result.errorMessage.contains(QStringLiteral("boom")));
}

void TestLuaSandbox::timesOutInfiniteLoop()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 20;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("while true do end"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.timedOut);
    QVERIFY(result.errorMessage.contains(QStringLiteral("timeout"),
                                        Qt::CaseInsensitive));
}

void TestLuaSandbox::limitsMemoryUsage()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 32;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "local t = {}\n"
            "for i = 1, 200000 do t[i] = string.rep('x', 64) end"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.memoryExceeded);
    QVERIFY(result.errorMessage.contains(QStringLiteral("memory"),
                                        Qt::CaseInsensitive));
}

void TestLuaSandbox::isolatesExecutions()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult first = sandbox.execute(
        QStringLiteral("leakedValue = 123\nprint(leakedValue)"),
        options);
    const LuaSandboxResult second = sandbox.execute(
        QStringLiteral("print(leakedValue == nil)"),
        options);

    QVERIFY2(first.success, qPrintable(first.errorMessage));
    QVERIFY2(second.success, qPrintable(second.errorMessage));
    QCOMPARE(first.outputLines.first(), QStringLiteral("123"));
    QCOMPARE(second.outputLines.first(), QStringLiteral("true"));
}

void TestLuaSandbox::truncatesExcessiveOutput()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.maxOutputLines = 3;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("for i = 1, 10 do print(i) end"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.size(), 4);
    QCOMPARE(result.outputLines.at(0), QStringLiteral("1"));
    QCOMPARE(result.outputLines.at(2), QStringLiteral("3"));
    QVERIFY(result.outputLines.last().contains(QStringLiteral("truncated"),
                                               Qt::CaseInsensitive));
}
```

**Step 3: Register the tests**

Modify `tests/main.cpp`:

- Add `#include "unit/TestLuaSandbox.h"` near the other unit includes.
- Add a block after script/protocol related tests:

```cpp
    {
        qDebug() << "\n[TEST] LuaSandbox";
        TestLuaSandbox test;
        result |= QTest::qExec(&test, argc, argv);
    }
```

Modify `tests/CMakeLists.txt`:

- Add `unit/TestLuaSandbox.cpp` to `TEST_SOURCES`.
- Add `${CMAKE_CURRENT_SOURCE_DIR}/../src/core/script/LuaSandbox.h` to the test executable file list.

**Step 4: Run test build to verify RED**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `src/core/script/LuaSandbox.h` does not exist.

**Step 5: Commit red tests**

Do not commit if the only failure is not the missing `LuaSandbox` header. Once verified:

```powershell
git add tests/main.cpp tests/CMakeLists.txt tests/unit/TestLuaSandbox.h tests/unit/TestLuaSandbox.cpp
git commit -m "test: cover lua sandbox contract"
```

### Task 2: Implement LuaSandbox Restricted Execution

**Files:**
- Create: `src/core/script/LuaSandbox.h`
- Create: `src/core/script/LuaSandbox.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create `LuaSandbox.h`**

Create `src/core/script/LuaSandbox.h`:

```cpp
/**
 * @file LuaSandbox.h
 * @brief Lua 安全沙箱执行器
 */

#ifndef COMASSISTANT_LUASANDBOX_H
#define COMASSISTANT_LUASANDBOX_H

#include <QString>
#include <QStringList>

namespace ComAssistant {

/**
 * @brief Lua 沙箱执行选项。
 *
 * 该结构集中描述脚本资源边界和能力开关。UI、协议脚本和自动化测试
 * 都应通过这里传入限制，避免在执行器内部散落魔法数字。
 */
struct LuaSandboxOptions
{
    int timeoutMs = 1000;              ///< 最大执行时长，<=0 表示不限制时间。
    int memoryLimitKb = 1024;          ///< Lua state 内存预算，<=0 表示不限制内存。
    int maxOutputLines = 200;          ///< print 输出最多保留的行数。
    bool allowCommunicationApi = false;///< 是否启用后续通信 API，4.5 默认关闭。
};

/**
 * @brief Lua 沙箱执行结果。
 *
 * 结果用显式布尔值区分普通 Lua 错误、超时、中断和内存超限，
 * 方便后续 UI 展示、协议诊断导出和自动化测试做稳定判断。
 */
struct LuaSandboxResult
{
    bool success = false;              ///< 脚本是否完整执行成功。
    bool timedOut = false;             ///< 是否被超时 hook 中断。
    bool interrupted = false;          ///< 是否被外部取消中断。
    bool memoryExceeded = false;       ///< 是否触发 Lua 内存预算。
    QString errorMessage;              ///< 失败原因或 Lua 错误文本。
    QStringList outputLines;           ///< print 收集到的输出行。
    qint64 elapsedMs = 0;              ///< 本次执行耗时。
};

/**
 * @brief Lua 安全沙箱执行器。
 *
 * 每次 execute() 都创建全新的 Lua state，只打开白名单标准库并注册受控 API。
 * 这样一次脚本的全局变量、hook 或库表修改不会污染下一次执行。
 */
class LuaSandbox
{
public:
    /**
     * @brief 执行一段 Lua 脚本文本。
     * @param script Lua 脚本文本，按 UTF-8 传入 Lua 运行时。
     * @param options 本次执行使用的资源限制和能力开关。
     * @return 结构化执行结果，包含输出、耗时和失败原因。
     */
    LuaSandboxResult execute(const QString& script,
                             const LuaSandboxOptions& options = LuaSandboxOptions());
};

} // namespace ComAssistant

#endif // COMASSISTANT_LUASANDBOX_H
```

**Step 2: Create `LuaSandbox.cpp`**

Create `src/core/script/LuaSandbox.cpp` with these implementation requirements:

- Include `LuaSandbox.h`, `ChecksumUtils.h`, and `ConversionUtils.h`.
- Include Lua headers with `extern "C"`.
- Create internal `SandboxContext` containing:
  - `LuaSandboxOptions options`
  - `LuaSandboxResult* result`
  - `QElapsedTimer timer`
  - `qint64 currentBytes`
  - `qint64 maxBytes`
  - `bool memoryExceeded`
  - `bool outputTruncated`
- Use `lua_newstate(sandboxAlloc, &context)`.
- Store `SandboxContext*` in Lua registry with a private key.
- Open only safe libraries:
  - `_G` via `luaL_requiref(L, "_G", luaopen_base, 1)` and then remove unsafe globals.
  - `string`, `table`, `math`, and `utf8`.
- Remove unsafe globals after base load:
  - `collectgarbage`, `dofile`, `loadfile`, `load`, `require`, `rawequal`, `rawget`, `rawset`, `setmetatable`.
  - Explicitly set `os`, `io`, `package`, and `debug` to nil.
- Register custom `print`, `hexToBytes`, `bytesToHex`, `crc16`, and `crc32`.
- Set `lua_sethook(L, sandboxHook, LUA_MASKCOUNT, 1000)`.
- Execute with `luaL_loadbuffer` then `lua_pcall`.
- On `LUA_OK`, set `success=true`.
- On error, copy `lua_tostring(L, -1)` into `errorMessage`.
- If allocator marked memory exceeded, force `memoryExceeded=true` and ensure `errorMessage` mentions memory.
- Always close Lua state and set `elapsedMs`.

Key helper behavior:

```cpp
static int sandboxPrint(lua_State* L)
{
    // 逐个参数转换为 Lua 字符串语义，参数之间用 tab 连接。
    // 超过 maxOutputLines 后只追加一条 truncated 提示，避免无限输出撑爆 UI。
}
```

```cpp
static void sandboxHook(lua_State* L, lua_Debug*)
{
    // 从 registry 取 SandboxContext。
    // 如果 timeoutMs > 0 且 timer.elapsed() > timeoutMs，设置 timedOut 并 luaL_error。
}
```

Allocator must use `std::malloc`, `std::realloc`, and `std::free`, tracking `nsize - osize`.

**Step 3: Add production and test CMake entries**

Modify `CMakeLists.txt`:

- Add `src/core/script/LuaSandbox.cpp` to `CORE_SOURCES`.
- Add `src/core/script/LuaSandbox.h` to `CORE_HEADERS`.

Modify `tests/CMakeLists.txt`:

- Add `${CMAKE_CURRENT_SOURCE_DIR}/../src/core/script/LuaSandbox.cpp` to `target_sources(ComAssistant_tests PRIVATE ...)`.
- Link Lua into tests:

```cmake
target_link_libraries(ComAssistant_tests PRIVATE
    ...
    ${LUA_LIBRARIES}
)
```

**Step 4: Run tests to verify GREEN**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS for `TestLuaSandbox`; if other tests fail, fix before continuing.

**Step 5: Commit implementation**

```powershell
git add CMakeLists.txt tests/CMakeLists.txt src/core/script/LuaSandbox.h src/core/script/LuaSandbox.cpp
git commit -m "feat: add lua sandbox execution core"
```

### Task 3: Add Pure Utility API Tests And Implementation Details

**Files:**
- Modify: `tests/unit/TestLuaSandbox.cpp`
- Modify: `src/core/script/LuaSandbox.cpp`

**Step 1: Add failing tests for utility APIs**

Add test slots to `TestLuaSandbox.h`:

```cpp
void exposesSafeChecksumAndHexUtilities();
void keepsCommunicationApiDisabledByDefault();
```

Add implementations:

```cpp
void TestLuaSandbox::exposesSafeChecksumAndHexUtilities()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "local bytes = hexToBytes('AA 55 01')\n"
            "print(bytesToHex(bytes))\n"
            "print(crc16(hexToBytes('01 03 00 00 00 02')))\n"
            "print(crc32('abc'))"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.at(0), QStringLiteral("AA 55 01"));
    QVERIFY(result.outputLines.at(1).toInt() > 0);
    QVERIFY(result.outputLines.at(2).toUInt() > 0U);
}
```

```cpp
void TestLuaSandbox::keepsCommunicationApiDisabledByDefault()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print(serial == nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.first(), QStringLiteral("true"));
}
```

**Step 2: Run tests to verify RED if utilities are missing**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL if any utility API is not registered or serial is accidentally exposed.

**Step 3: Implement or adjust APIs**

In `LuaSandbox.cpp`:

- Register `hexToBytes`, `bytesToHex`, `crc16`, and `crc32` as globals.
- Keep `serial` nil unless `allowCommunicationApi` is true.
- If `allowCommunicationApi` is true in 4.5, register an empty table with no send/receive functions and document it as reserved, or leave it unregistered. Prefer leaving it unregistered until a real callback design exists.

**Step 4: Run tests to verify GREEN**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 5: Commit utility coverage**

```powershell
git add tests/unit/TestLuaSandbox.h tests/unit/TestLuaSandbox.cpp src/core/script/LuaSandbox.cpp
git commit -m "feat: expose safe lua sandbox utilities"
```

### Task 4: Update User Documentation And Help

**Files:**
- Modify: `docs/user-guide/scripting.md`
- Modify: `resources/help/protocols.html`
- Review only: `resources/help/quickstart.html`

**Step 1: Update `docs/user-guide/scripting.md`**

Replace the existing safety section with:

```markdown
## 安全说明

- 新的 Lua 沙箱核心默认禁用 `io`、`os`、`package`、`debug`、`require`、`dofile`、`loadfile` 和 `load`，避免脚本直接读写文件、执行系统命令或加载外部模块。
- 沙箱执行支持超时中断、Lua 内存预算和输出行数限制；脚本超出限制时会返回明确错误。
- 4.5 阶段先落地沙箱核心基座，脚本编辑器的完整 Lua 运行时迁移仍属于后续升级；当前编辑器入口保持既有行为。
- 仍然建议只运行自己编写或可信来源的脚本；沙箱用于降低 Lua 层风险，不等同于操作系统级隔离。
```

Also update the opening paragraph to avoid saying the only current engine opens all standard libraries.

**Step 2: Update `resources/help/protocols.html`**

In the protocol platform section added by 4.1-4.4, add a short bilingual note:

```html
<p><strong>中文：</strong>Lua 扩展会先经过受限沙箱执行，后续脚本协议仍需通过协议描述、配置 Schema 和诊断导出暴露能力。</p>
<p><strong>English:</strong> Lua extensions will run through the restricted sandbox first. Future script protocols still need descriptors, config schemas, and diagnostics before they become protocol capabilities.</p>
```

**Step 3: Review `resources/help/quickstart.html`**

Check whether quickstart mentions Lua execution details. If it does not, leave it unchanged and record the reason in final notes:

`quickstart.html` 不需要更新，因为 4.5 不改变快速连接、发送、接收、会话保存或脚本编辑器主流程。

**Step 4: Run documentation checks**

Run:

```powershell
git diff --check
```

Expected: exit 0.

**Step 5: Commit docs**

```powershell
git add docs/user-guide/scripting.md resources/help/protocols.html
git commit -m "docs: describe lua sandbox safety"
```

### Task 5: Final Verification And RecallLoom Update

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
- Application build exit 0.
- `git diff --check` exit 0.

**Step 2: Update RecallLoom memory**

Use the RecallLoom project rules:

- Update `rolling_summary.md` with 4.5 completion, current judgments, risks, and next step.
- Append `daily_logs/2026-06-02.md` with a milestone entry covering implementation, tests, docs, and commit chain.
- Validate with:

```powershell
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\validate_context.py .
```

Expected: Errors 0, Warnings 0.

**Step 3: Commit memory if tracked or keep local if ignored**

This repository keeps `.recallloom/` local and ignored. Do not force-add it unless the user explicitly asks.

**Step 4: Final status**

Run:

```powershell
git status --short --branch
git log --oneline -6
```

Report:

- Latest commit hashes for 4.5.
- Verification commands and outcomes.
- Whether `quickstart.html` changed and why.
- Which RecallLoom layers were updated.

