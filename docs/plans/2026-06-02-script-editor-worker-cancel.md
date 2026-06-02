# Script Editor Worker Cancellation Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 把脚本编辑器的 LuaSandbox 执行迁移到后台线程，并让“停止”按钮通过沙箱 hook 触发受控取消。

**Architecture:** `LuaSandboxOptions` 新增外部中断回调，Lua hook 优先检查取消再检查超时。`ScriptEditorDialog` 管理一个专用 `QThread` 与 `ScriptExecutionWorker`，worker 只执行脚本并通过 queued signal 回传输出、发送请求和执行结果，所有 UI 更新仍留在主线程。

**Tech Stack:** C++17、Qt 5.12.9、Lua C API、Qt Test、CMake、MinGW Release 构建目录 `D:\comassistant\build_release`。

---

## 执行原则

| 原则 | 要求 |
|------|------|
| TDD | 每个行为先写失败测试，确认红测失败原因正确，再写最小实现 |
| 线程边界 | worker 不直接访问 UI 控件，所有 UI 输出经对话框槽函数处理 |
| 取消语义 | 停止按钮只请求取消，不强杀线程 |
| 构建目录 | 本地验证统一使用 `build_release` |
| 文档同步 | 修改脚本用户可见行为后同步检查 `docs/user-guide/scripting.md`、`resources/help/protocols.html`、`resources/help/quickstart.html` |
| 记忆同步 | 完成持久改动后更新 `.recallloom/` |

## Task 1: LuaSandbox 外部取消红测

**Files:**
- Modify: `tests/unit/TestLuaSandbox.h`
- Modify: `tests/unit/TestLuaSandbox.cpp`

**Step 1: Write the failing test**

在 `TestLuaSandbox.h` 的 private slots 增加：

```cpp
void interruptsInfiniteLoopWhenCallbackRequestsStop();
```

在 `TestLuaSandbox.cpp` 增加：

```cpp
/**
 * @brief 验证外部取消回调能中断 Lua 死循环。
 *
 * 该用例使用无超时配置，确保失败原因来自 interruptCallback，
 * 而不是既有 timeout hook。回调在被 hook 查询数次后返回 true，
 * 模拟 UI 线程点击停止按钮后的取消标记。
 */
void TestLuaSandbox::interruptsInfiniteLoopWhenCallbackRequestsStop()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 0;
    options.memoryLimitKb = 256;

    int hookChecks = 0;
    options.interruptCallback = [&hookChecks]() {
        ++hookChecks;
        return hookChecks >= 2;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("while true do end"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.interrupted);
    QVERIFY(!result.timedOut);
    QVERIFY(hookChecks >= 2);
    QVERIFY(result.errorMessage.contains(QStringLiteral("interrupted"),
                                         Qt::CaseInsensitive));
}
```

**Step 2: Run test to verify it fails**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL，编译错误提示 `LuaSandboxOptions` 没有 `interruptCallback` 成员。

**Step 3: Commit red test**

```powershell
git add tests/unit/TestLuaSandbox.h tests/unit/TestLuaSandbox.cpp
git commit -m "test: cover lua sandbox external interruption"
```

## Task 2: LuaSandbox hook 支持外部取消

**Files:**
- Modify: `src/core/script/LuaSandbox.h`
- Modify: `src/core/script/LuaSandbox.cpp`

**Step 1: Add interrupt callback option**

在 `LuaSandboxOptions` 中加入：

```cpp
using InterruptCallback = std::function<bool()>;

InterruptCallback interruptCallback; ///< 外部取消回调，返回 true 时 hook 中断脚本
```

**Step 2: Update hook logic**

把 `sandboxHook()` 改为先检查取消、再检查超时：

```cpp
if (context->options.interruptCallback
    && context->options.interruptCallback()) {
    context->result->interrupted = true;
    luaL_error(L, "Lua sandbox interrupted");
}
```

保留原有 timeout 逻辑。

**Step 3: Normalize interrupted error**

在 `LuaSandbox::execute()` 收尾处补充：

```cpp
if (result.interrupted
    && !result.errorMessage.contains(QStringLiteral("interrupted"), Qt::CaseInsensitive)) {
    result.errorMessage = QStringLiteral("Lua sandbox interrupted");
}
```

**Step 4: Run test to verify it passes**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestLuaSandbox::interruptsInfiniteLoopWhenCallbackRequestsStop` PASS，既有 LuaSandbox 测试仍 PASS。

**Step 5: Commit implementation**

```powershell
git add src/core/script/LuaSandbox.h src/core/script/LuaSandbox.cpp
git commit -m "feat: support lua sandbox external interruption"
```

## Task 3: ScriptEditorDialog 后台执行红测

**Files:**
- Modify: `tests/unit/TestScriptEditorDialog.h`
- Modify: `tests/unit/TestScriptEditorDialog.cpp`

**Step 1: Add button helper**

在 `TestScriptEditorDialog.cpp` 匿名 namespace 中增加停止按钮查找函数：

```cpp
/**
 * @brief 查找停止按钮。
 * @param dialog 被测脚本编辑器对话框。
 * @return 停止 QPushButton 指针，未找到时返回 nullptr。
 */
QPushButton* stopButton(ScriptEditorDialog& dialog)
{
    return dialog.findChild<QPushButton*>(QStringLiteral("stopScriptBtn"));
}
```

**Step 2: Write failing async tests**

在头文件 private slots 增加：

```cpp
void runScriptReturnsControlBeforeLongScriptFinishes();
void stopScriptCancelsRunningSandbox();
void runScriptEmitsSendDataFromWorkerThread();
void runScriptRestoresButtonsAfterError();
```

在 cpp 增加测试：

```cpp
/**
 * @brief 验证长脚本运行后 UI 立即返回事件循环。
 */
void TestScriptEditorDialog::runScriptReturnsControlBeforeLongScriptFinishes()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("while true do end"));

    QVERIFY(runButton(dialog));
    QVERIFY(stopButton(dialog));
    runButton(dialog)->click();

    QVERIFY(!runButton(dialog)->isEnabled());
    QVERIFY(stopButton(dialog)->isEnabled());
    QVERIFY(outputArea(dialog)->toPlainText().contains(QStringLiteral("开始执行脚本")));

    stopButton(dialog)->click();
    QTRY_VERIFY_WITH_TIMEOUT(runButton(dialog)->isEnabled(), 1000);
}

/**
 * @brief 验证停止按钮能请求取消正在运行的沙箱脚本。
 */
void TestScriptEditorDialog::stopScriptCancelsRunningSandbox()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("while true do end"));

    runButton(dialog)->click();
    QVERIFY(stopButton(dialog)->isEnabled());
    stopButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("脚本已取消")), 1000);
    QVERIFY(runButton(dialog)->isEnabled());
    QVERIFY(!stopButton(dialog)->isEnabled());
}

/**
 * @brief 验证后台线程中的 serial.send 仍回到对话框发送信号。
 */
void TestScriptEditorDialog::runScriptEmitsSendDataFromWorkerThread()
{
    ScriptEditorDialog dialog;
    QSignalSpy sendSpy(&dialog, SIGNAL(sendData(QByteArray)));
    dialog.setScript(QStringLiteral("serial.send('AT\\r\\n')"));

    runButton(dialog)->click();

    QTRY_COMPARE_WITH_TIMEOUT(sendSpy.count(), 1, 1000);
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), QByteArray("AT\r\n"));
    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("[发送]")), 1000);
}

/**
 * @brief 验证脚本错误后运行按钮和停止按钮恢复到空闲状态。
 */
void TestScriptEditorDialog::runScriptRestoresButtonsAfterError()
{
    ScriptEditorDialog dialog;
    dialog.setScript(QStringLiteral("error('bad script')"));

    runButton(dialog)->click();

    QTRY_VERIFY_WITH_TIMEOUT(outputArea(dialog)->toPlainText().contains(QStringLiteral("bad script")), 1000);
    QVERIFY(runButton(dialog)->isEnabled());
    QVERIFY(!stopButton(dialog)->isEnabled());
}
```

**Step 3: Run test to verify it fails**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL。同步执行版本会在 `while true do end` 阻塞到沙箱超时，或停止按钮输出旧提示，不会出现“脚本已取消”。

**Step 4: Commit red tests**

```powershell
git add tests/unit/TestScriptEditorDialog.h tests/unit/TestScriptEditorDialog.cpp
git commit -m "test: cover script editor worker cancellation"
```

## Task 4: 新增 ScriptExecutionWorker

**Files:**
- Create: `src/ui/dialogs/ScriptExecutionWorker.h`
- Create: `src/ui/dialogs/ScriptExecutionWorker.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create worker header**

关键结构：

```cpp
class ScriptExecutionWorker : public QObject
{
    Q_OBJECT

public:
    using InterruptCallback = std::function<bool()>;

    explicit ScriptExecutionWorker(QString script,
                                   InterruptCallback interruptCallback,
                                   QObject* parent = nullptr);

public slots:
    void run();

signals:
    void sendRequested(const QByteArray& data);
    void finished(const ComAssistant::LuaSandboxResult& result);

private:
    QString m_script;
    InterruptCallback m_interruptCallback;
};
```

文件内必须包含中文注释，说明 worker 不访问 UI，只通过信号回传。

**Step 2: Create worker implementation**

核心流程：

```cpp
void ScriptExecutionWorker::run()
{
    LuaSandboxOptions options;
    options.timeoutMs = 3000;
    options.memoryLimitKb = 2048;
    options.maxOutputLines = 500;
    options.allowCommunicationApi = true;
    options.interruptCallback = m_interruptCallback;
    options.sendCallback = [this](const QByteArray& bytes) {
        if (bytes.isEmpty()) {
            return false;
        }
        emit sendRequested(bytes);
        return true;
    };
    options.isOpenCallback = []() {
        return true;
    };

    LuaSandbox sandbox;
    emit finished(sandbox.execute(m_script, options));
}
```

**Step 3: Update CMake lists**

在主工程 `UI_SOURCES` 加：

```cmake
src/ui/dialogs/ScriptExecutionWorker.cpp
```

在主工程 `UI_HEADERS` 加：

```cmake
src/ui/dialogs/ScriptExecutionWorker.h
```

在测试目标 sources/header 列表加对应 worker 文件。

**Step 4: Build to verify worker compiles**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: 编译通过，Task 3 的行为测试仍可能失败。

**Step 5: Commit worker**

```powershell
git add CMakeLists.txt tests/CMakeLists.txt src/ui/dialogs/ScriptExecutionWorker.h src/ui/dialogs/ScriptExecutionWorker.cpp
git commit -m "feat: add script execution worker"
```

## Task 5: ScriptEditorDialog 接入 worker 与取消状态

**Files:**
- Modify: `src/ui/dialogs/ScriptEditorDialog.h`
- Modify: `src/ui/dialogs/ScriptEditorDialog.cpp`

**Step 1: Update dialog members and slots**

新增成员：

```cpp
enum class ScriptRunState
{
    Idle,
    Running,
    Cancelling
};

QThread* m_scriptThread = nullptr;
ScriptExecutionWorker* m_scriptWorker = nullptr;
std::shared_ptr<std::atomic_bool> m_cancelRequested;
ScriptRunState m_runState = ScriptRunState::Idle;
```

新增私有函数：

```cpp
void startWorkerExecution(const QString& script);
void requestWorkerCancellation();
void handleWorkerSendRequested(const QByteArray& data);
void handleWorkerFinished(const LuaSandboxResult& result);
void cleanupWorkerThread();
void setRunState(ScriptRunState state);
```

`ScriptEditorDialog.h` 需要补 `QThread`、`atomic`、`memory` 前置或 include，并前置声明 `ScriptExecutionWorker`。

**Step 2: Register metatype**

在构造函数或 cpp 匿名初始化逻辑中注册：

```cpp
qRegisterMetaType<ComAssistant::LuaSandboxResult>("ComAssistant::LuaSandboxResult");
```

`LuaSandbox.h` 末尾添加：

```cpp
Q_DECLARE_METATYPE(ComAssistant::LuaSandboxResult)
```

**Step 3: Replace synchronous onRunScript**

`onRunScript()` 改为：

```cpp
if (m_runState != ScriptRunState::Idle) {
    return;
}

appendOutput(tr("[系统] 开始执行脚本..."), QColor(100, 149, 237));
startWorkerExecution(m_codeEditor->toPlainText());
```

**Step 4: Implement cancellation**

`onStopScript()` 改为：

```cpp
if (m_runState != ScriptRunState::Running) {
    return;
}

requestWorkerCancellation();
appendOutput(tr("[系统] 正在请求停止脚本..."), QColor(255, 165, 0));
setRunState(ScriptRunState::Cancelling);
```

**Step 5: Handle worker result**

`handleWorkerFinished()` 负责：
- 追加 `result.outputLines`
- `interrupted` 时输出 `[系统] 脚本已取消`
- `success` 时输出 `[系统] 脚本执行完成`
- 失败且非取消时输出 `[错误] <message>`
- 恢复 Idle 状态并清理线程

**Step 6: Remove old synchronous helper**

删除或停止使用 `executeSandboxScript()`，避免 UI 线程继续同步执行 Lua。

**Step 7: Run tests to verify**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestScriptEditorDialog` 新旧用例全部 PASS，`TestLuaSandbox` 全部 PASS。

**Step 8: Commit dialog integration**

```powershell
git add src/core/script/LuaSandbox.h src/ui/dialogs/ScriptEditorDialog.h src/ui/dialogs/ScriptEditorDialog.cpp
git commit -m "feat: run script editor sandbox in worker"
```

## Task 6: 文档同步

**Files:**
- Modify: `docs/user-guide/scripting.md`
- Review: `resources/help/protocols.html`
- Review: `resources/help/quickstart.html`

**Step 1: Update scripting guide**

把 4.6 的“后台执行、强取消后续评估”改为 4.7 实际语义：

```markdown
> 脚本编辑器会在后台线程执行 LuaSandbox，点击“停止”会请求取消并由 Lua hook 在安全检查点中断脚本。取消不是强制杀死线程；如果脚本正在执行长时间 C 回调，需要等回调返回后才会响应。
```

保留 `serial.receive(timeout)` 暂不可用说明。

**Step 2: Review built-in help**

检查：

```powershell
rg -n "脚本|Lua|停止|后台|serial.receive|LATEST_RELEASE" resources/help docs/user-guide/scripting.md
```

Expected:
- `resources/help/protocols.html` 如已有脚本沙箱说明，按需补一句“脚本编辑器后台执行并支持请求停止”。
- `resources/help/quickstart.html` 若只覆盖快速连接、普通发送、接收、文件传输、会话保存和版本信息，则无需更新。

**Step 3: Commit docs**

```powershell
git add docs/user-guide/scripting.md resources/help/protocols.html resources/help/quickstart.html
git commit -m "docs: document script worker cancellation"
```

如 `quickstart.html` 未变化，不要强行提交；最终说明写明未更新理由。

## Task 7: 完整验证与 RecallLoom

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
- `ComAssistant_tests` 通过
- `ctest` 通过
- 主程序构建成功
- `git diff --check` 无空白错误
- RecallLoom 校验通过

**Step 2: Update RecallLoom**

用 RecallLoom revision-aware helper 更新：
- `rolling_summary.md`：记录 4.7 已完成、当前行为、下一步
- `daily_logs/2026-06-02.md`：追加 4.7 实现、测试、文档、验证命令

**Step 3: Final status**

Run:

```powershell
git status --short --branch
git log --oneline -8
```

Expected: 工作区只包含本次预期变更；若全部已提交则干净。

**Step 4: Final commit if needed**

如果 RecallLoom 更新未提交：

```powershell
git add .recallloom/rolling_summary.md .recallloom/daily_logs/2026-06-02.md
git commit -m "docs: update 4.7 project memory"
```
