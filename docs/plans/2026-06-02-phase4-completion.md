# Phase 4 Completion Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 完成第四阶段 Lua 协议 UI、稳定协议 ID 接收链路、生产化诊断、文档翻译、最终验证和 `v1.7.0` 发布准备。

**Architecture:** MainWindow 采用稳定协议 ID 与旧版绘图 `ProtocolType` 双轨。配置、诊断、会话保存恢复以 `protocolId` 为事实源；旧绘图菜单继续只负责绘图协议和自动检测。Lua 协议保持受限接收解析，不开放 `serial.receive(timeout)`、外部脚本加载或脚本化构帧。

**Tech Stack:** C++17、Qt 5.12.9 Widgets/Test、LuaSandbox、CMake、MinGW Release 构建目录 `D:\comassistant\build_release`、RecallLoom。

---

## Task 1: 协议配置 UI 红测

**Files:**
- Modify: `tests/unit/TestProtocolConfigEditor.h`
- Modify: `tests/unit/TestProtocolConfigEditor.cpp`

**Step 1: Write failing tests**

在 `TestProtocolConfigEditor.h` 增加：

```cpp
void usesMultilineEditorForLuaScriptSource();
void showsFieldLevelValidationErrors();
void showsHelpfulEmptySchemaText();
```

在 `TestProtocolConfigEditor.cpp` 增加测试：

```cpp
void TestProtocolConfigEditor::usesMultilineEditorForLuaScriptSource()
{
    ProtocolConfigSchema schema;
    schema.fields.append(ProtocolConfigField::string(
        QStringLiteral("scriptSource"),
        QStringLiteral("脚本源码"),
        QString(),
        QStringLiteral("内联 Lua 脚本文本")));

    ProtocolConfigEditor editor;
    editor.setSchema(schema);
    editor.setConfig({{QStringLiteral("scriptSource"),
                       QStringLiteral("function process(data, context)\n  return { valid = false }\nend")}});

    auto* sourceEdit = editor.findChild<QPlainTextEdit*>(
        QStringLiteral("protocolConfig_scriptSource"));
    QVERIFY(sourceEdit != nullptr);
    QVERIFY(sourceEdit->toPlainText().contains(QStringLiteral("function process")));
    QCOMPARE(editor.config().value(QStringLiteral("scriptSource")).toString(),
             sourceEdit->toPlainText());
}
```

字段级错误测试使用非法 BytesHex，断言 `protocolConfigError_frameHeader` 可见且包含 `frameHeader`。空 Schema 测试断言 `protocolConfigEmptyLabel` 文案包含“默认行为”。

**Step 2: Run tests to verify failure**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 新增测试失败，原因是仍使用 `QLineEdit`、没有字段错误 label、空 Schema 文案未更新。

**Step 3: Commit red tests**

```powershell
git add tests/unit/TestProtocolConfigEditor.h tests/unit/TestProtocolConfigEditor.cpp
git commit -m "test: cover protocol config editor polish"
```

## Task 2: 协议配置 UI 实现

**Files:**
- Modify: `src/ui/widgets/ProtocolConfigEditor.h`
- Modify: `src/ui/widgets/ProtocolConfigEditor.cpp`
- Modify: `src/ui/dialogs/ProtocolConfigDialog.cpp`

**Step 1: Implement minimal code**

实现要点：

- 引入 `QPlainTextEdit`。
- `field.key == "scriptSource"` 且类型为 `String` 时使用 `QPlainTextEdit`。
- 新增 `QMap<QString, QLabel*> m_fieldErrorLabels`。
- 每个字段行使用垂直容器承载编辑控件和字段错误 label。
- `validateConfig()` 解析 `result.errors` 的 `key: reason`，显示对应字段错误，并保留底部总错误。
- 空 Schema label 设置 objectName `protocolConfigEmptyLabel`，文案改为“当前协议没有可配置项，可直接使用默认行为。”。
- `ProtocolConfigDialog` 对脚本字段适当增大默认高度，例如 `resize(720, 560)`。

**Step 2: Run focused tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestProtocolConfigEditor` 新旧测试通过。

**Step 3: Commit implementation**

```powershell
git add src/ui/widgets/ProtocolConfigEditor.h src/ui/widgets/ProtocolConfigEditor.cpp src/ui/dialogs/ProtocolConfigDialog.cpp
git commit -m "feat: polish protocol config editor"
```

## Task 3: Lua 协议生产化红测

**Files:**
- Modify: `tests/unit/TestLuaScriptProtocol.h`
- Modify: `tests/unit/TestLuaScriptProtocol.cpp`
- Modify: `tests/unit/TestProtocolDiagnostics.cpp`

**Step 1: Write failing tests**

在 `TestLuaScriptProtocol.h` 增加：

```cpp
void recordsRecentError();
void clearsRecentErrorAfterValidFrame();
void respectsConfiguredOutputLineLimit();
```

测试断言：

- 空脚本或 Lua 错误后 `protocol.recentError()` 非空。
- 成功解析后 `recentError()` 清空。
- `maxOutputLines=1` 时 wrapper 哨兵缺失会产生错误并记录，证明输出边界能被诊断看见。

`TestProtocolDiagnostics.cpp` 增加或调整上下文测试，确保 `ProtocolDiagnosticsContext::recentError` 写入 `luaProtocol.lastError`。

**Step 2: Run tests to verify failure**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 编译失败或测试失败，因为 `LuaScriptProtocol::recentError()` 尚不存在。

**Step 3: Commit red tests**

```powershell
git add tests/unit/TestLuaScriptProtocol.h tests/unit/TestLuaScriptProtocol.cpp tests/unit/TestProtocolDiagnostics.cpp
git commit -m "test: cover lua protocol production diagnostics"
```

## Task 4: Lua 协议生产化实现

**Files:**
- Modify: `src/core/protocol/LuaScriptProtocol.h`
- Modify: `src/core/protocol/LuaScriptProtocol.cpp`
- Modify: `src/core/protocol/ProtocolRegistry.cpp`

**Step 1: Implement minimal code**

实现要点：

- `LuaScriptProtocol` 新增 `QString recentError() const`。
- 新增私有 `setRecentError()` 或直接维护 `m_recentError`。
- `parse()` 在脚本为空、沙箱失败、结果区缺失或返回错误时记录错误。
- `parse()` 成功且 `valid=true` 时清空最近错误。
- 对 `maxOutputLines` 增加最小 wrapper 输出预算保护或在说明中明确用户设置过低会导致结果区缺失。
- 更新 Lua Schema 文案：`scriptSource` 已执行，不再写“仅保存和诊断，不执行”。

**Step 2: Run focused tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: Lua 与诊断测试通过。

**Step 3: Commit implementation**

```powershell
git add src/core/protocol/LuaScriptProtocol.h src/core/protocol/LuaScriptProtocol.cpp src/core/protocol/ProtocolRegistry.cpp
git commit -m "feat: record lua protocol runtime diagnostics"
```

## Task 5: 稳定协议 ID 会话恢复红测

**Files:**
- Modify: `tests/unit/TestMainWindowSessionCoordinator.h`
- Modify: `tests/unit/TestMainWindowSessionCoordinator.cpp`
- Modify: `tests/unit/TestProtocolConfigSchema.cpp`

**Step 1: Write failing tests**

在 `TestMainWindowSessionCoordinator` 增加：

```cpp
void testStableProtocolIdRestoresLuaScript();
void testUnknownStableProtocolIdFallsBackToRaw();
```

断言 `ApplyResult` 包含 `restoredProtocolId`，`lua.script` 不被旧 `protocolType=Raw` 抹掉；未知 ID 回退 `raw`。

在 `TestProtocolConfigSchema.cpp` 增加会话 round-trip 测试，确认 `scriptSource` 多行源码在 `SessionData::toJson()` 和 `fromJson()` 后仍保留。

**Step 2: Run tests to verify failure**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 编译失败或测试失败，因为 `ApplyResult::restoredProtocolId` 尚不存在或 MainWindow 仍只恢复旧枚举。

**Step 3: Commit red tests**

```powershell
git add tests/unit/TestMainWindowSessionCoordinator.h tests/unit/TestMainWindowSessionCoordinator.cpp tests/unit/TestProtocolConfigSchema.cpp
git commit -m "test: cover stable protocol id session restore"
```

## Task 6: 稳定协议 ID 接入 MainWindow

**Files:**
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `src/ui/MainWindowSessionCoordinator.h`
- Modify: `src/ui/MainWindowSessionCoordinator.cpp`

**Step 1: Implement coordinator result**

`ApplyResult` 增加：

```cpp
QString restoredProtocolId = QStringLiteral("raw");
QVariantMap restoredProtocolConfig;
```

新增 `sanitizeProtocolId(const QString& protocolId, int protocolValue)`：

- 有已注册 ID 时返回该 ID。
- 没有 ID 时按旧 `ProtocolType` 推导。
- 未知 ID 回退 `raw`。

**Step 2: Implement MainWindow helpers**

新增私有方法：

```cpp
void switchCurrentProtocolById(const QString& protocolId,
                               const QVariantMap& config = QVariantMap(),
                               bool syncPlotAction = true);
ProtocolDescriptor currentProtocolDescriptor() const;
QVariantMap currentProtocolConfig() const;
QString currentProtocolDisplayName() const;
ProtocolDiagnosticsContext currentProtocolDiagnosticsContext() const;
```

新增成员：

```cpp
QString m_currentProtocolId = QStringLiteral("raw");
QVariantMap m_currentProtocolConfig;
QString m_recentProtocolError;
```

**Step 3: Wire save/restore/config/diagnostics**

- `onSaveSession()` 保存 `m_currentProtocolId`、descriptor configVersion、当前配置。
- `applySessionDataToUi()` 调用 `switchCurrentProtocolById(applyResult.restoredProtocolId, session.protocolConfig)`。
- `onProtocolConfig()` 使用当前稳定 ID descriptor。
- `onProtocolDiagnostics()` 传入 `currentProtocolDiagnosticsContext()`。
- 绘图菜单触发时按 `ProtocolFactory::typeId(type)` 切换。
- `onPlotProtocolAutoDetected()` 同步稳定 ID。
- `onDataReceived()` 对非绘图可创建协议调用 `parse()`，记录错误并状态栏提示。

**Step 4: Run tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 会话、Lua、配置、诊断测试通过。

**Step 5: Commit implementation**

```powershell
git add src/ui/MainWindow.h src/ui/MainWindow.cpp src/ui/MainWindowSessionCoordinator.h src/ui/MainWindowSessionCoordinator.cpp
git commit -m "feat: restore receive protocols by stable id"
```

## Task 7: 文档和翻译收口

**Files:**
- Modify: `docs/user-guide/scripting.md`
- Modify: `docs/user-guide/quickstart.md`
- Modify: `resources/help/protocols.html`
- Modify: `resources/help/quickstart.html`
- Modify: `resources/translations/en_US.ts`
- Modify: `resources/translations/zh_CN.ts` if needed

**Step 1: Update docs**

更新内容：

- Lua 协议已可通过稳定 ID 保存/恢复。
- `scriptSource` 通过多行编辑器编辑。
- 字段错误会显示到具体字段下方。
- 诊断 JSON 包含 Lua 最近错误和沙箱边界。
- `serial.receive(timeout)`、外部 `scriptPath` 和脚本化构帧仍未开放。

**Step 2: Update translations**

新增或调整英文翻译：

- “当前协议没有可配置项，可直接使用默认行为。”
- “协议配置已应用”
- 新增协议选择/错误提示相关文案。

**Step 3: Commit docs**

```powershell
git add docs/user-guide/scripting.md docs/user-guide/quickstart.md resources/help/protocols.html resources/help/quickstart.html resources/translations/en_US.ts resources/translations/zh_CN.ts
git commit -m "docs: describe phase4 protocol workflow"
```

## Task 8: 版本发布准备

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/version.h`
- Modify: `CHANGELOG.md`
- Modify: `README.md`
- Modify: `resources/help/quickstart.html`

**Step 1: Update version**

升级到 `1.7.0`：

- `CMakeLists.txt` 中 `VERSION 1.7.0`。
- `src/version.h` 中 `APP_VERSION`、major/minor/patch/build、`APP_VERSION_STRING`、`APP_BUILD_DATE`。
- `CHANGELOG.md` 新增 `## [1.7.0] - 2026-06-02`。
- `README.md` 最新版本块。
- `quickstart.html` 帮助版本块。

**Step 2: Commit release metadata**

```powershell
git add CMakeLists.txt src/version.h CHANGELOG.md README.md resources/help/quickstart.html
git commit -m "chore: prepare v1.7.0 release metadata"
```

## Task 9: RecallLoom 更新与最终验证

**Files:**
- Modify: `.recallloom/rolling_summary.md`
- Modify: `.recallloom/daily_logs/2026-06-02.md`
- Review: `.recallloom/context_brief.md`

**Step 1: Run full verification**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
ctest --test-dir build_release --output-on-failure
cmake --build build_release --config Release --target ComAssistant --parallel
git diff --check
```

Expected: 全部通过。

**Step 2: Update RecallLoom**

使用 RecallLoom helper 优先更新当前状态、当天日志和必要的长期规则。

**Step 3: Validate RecallLoom**

Run RecallLoom validate helper and expect no findings.

**Step 4: Final status**

最终说明包含：

- 已完成的第四阶段项。
- 版本号。
- 测试和构建结果。
- `quickstart.html` 已更新。
- `.recallloom/` 已更新的记忆层。
