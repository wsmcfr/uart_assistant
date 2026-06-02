# Lua Protocol Registration Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将 `lua.script` 作为可登记、可配置、可诊断的 Lua 脚本协议能力接入协议注册中心，同时保持旧版协议创建和 UI 工作流不变。

**Architecture:** `ProtocolDescriptor` 增加脚本协议、可创建实例和旧版兼容标志；`ProtocolRegistry` 允许不可创建的描述只登记元数据；`ProtocolFactory` 过滤非旧版兼容协议；`ProtocolDiagnosticsBuilder` 为脚本协议导出 Lua 沙箱诊断节点。

**Tech Stack:** C++17、Qt 5.12.9、Qt Test、CMake、MinGW Release 构建目录 `D:\comassistant\build_release`。

---

## 执行原则

| 原则 | 要求 |
|------|------|
| TDD | 每个行为先写失败测试并确认红测，再写实现 |
| 最小闭环 | 只做 descriptor/schema/diagnostics，不实现 Lua `IProtocol` |
| 兼容性 | `ProtocolFactory::supportedTypes()` 仍返回旧版 10 个协议 |
| 文档同步 | 用户可见协议能力变化后更新脚本文档和协议帮助 |
| 记忆同步 | 完成持久改动后更新 `.recallloom/` |

## Task 1: Lua 协议注册红测

**Files:**
- Modify: `tests/unit/TestProtocolRegistry.h`
- Modify: `tests/unit/TestProtocolRegistry.cpp`

**Step 1: Write the failing tests**

在头文件 private slots 增加：

```cpp
void registersLuaScriptProtocolDescriptor();
void factoryLegacyListIgnoresLuaDescriptor();
```

在 cpp 增加：

```cpp
void TestProtocolRegistry::registersLuaScriptProtocolDescriptor()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    QVERIFY(registry.contains(QStringLiteral("lua.script")));

    const ProtocolDescriptor descriptor = registry.descriptor(QStringLiteral("lua.script"));
    QCOMPARE(descriptor.id, QStringLiteral("lua.script"));
    QCOMPARE(descriptor.displayName, QStringLiteral("Lua Script"));
    QCOMPARE(descriptor.category, ProtocolCategory::Custom);
    QVERIFY(descriptor.scriptProtocol);
    QVERIFY(!descriptor.creatable);
    QVERIFY(!descriptor.legacyCompatible);
    QVERIFY(!descriptor.builtin);
    QVERIFY(!descriptor.plotProtocol);
    QVERIFY(!descriptor.frameBuilder);
    QVERIFY(descriptor.configSchema.fields.size() >= 7);
    QCOMPARE(descriptor.defaultConfig.value(QStringLiteral("timeoutMs")).toInt(), 1000);
    QCOMPARE(descriptor.defaultConfig.value(QStringLiteral("memoryLimitKb")).toInt(), 1024);

    QVERIFY(registry.create(QStringLiteral("lua.script")) == nullptr);
}

void TestProtocolRegistry::factoryLegacyListIgnoresLuaDescriptor()
{
    const QList<ProtocolType> types = ProtocolFactory::supportedTypes();
    QCOMPARE(types.size(), 10);
    QVERIFY(!types.isEmpty());
    QCOMPARE(ProtocolFactory::typeId(ProtocolType::Raw), QStringLiteral("raw"));
    QVERIFY(ProtocolFactory::registry().contains(QStringLiteral("lua.script")));
}
```

**Step 2: Run red test**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL，编译错误提示 `ProtocolDescriptor` 没有 `scriptProtocol`、`creatable` 或 `legacyCompatible` 成员。

**Step 3: Commit red tests**

```powershell
git add tests/unit/TestProtocolRegistry.h tests/unit/TestProtocolRegistry.cpp
git commit -m "test: cover lua protocol descriptor registration"
```

## Task 2: ProtocolDescriptor 与注册中心支持不可创建脚本协议

**Files:**
- Modify: `src/core/protocol/ProtocolDescriptor.h`
- Modify: `src/core/protocol/ProtocolRegistry.cpp`
- Modify: `src/core/protocol/ProtocolFactory.cpp`

**Step 1: Add descriptor flags**

在 `ProtocolDescriptor` 增加：

```cpp
bool scriptProtocol = false;
bool creatable = true;
bool legacyCompatible = true;
```

每个字段写中文注释，说明脚本协议、是否可创建实例、是否参与旧版枚举映射。

**Step 2: Update registration validation**

`registerProtocol()` 从“非 Raw 必须有 creator”改为“`creatable=true` 必须有 creator”。

**Step 3: Register lua.script descriptor**

在 `registerBuiltinProtocols()` 末尾登记 Lua 协议描述：

```cpp
ProtocolDescriptor luaDescriptor;
luaDescriptor.id = QStringLiteral("lua.script");
luaDescriptor.displayName = QStringLiteral("Lua Script");
luaDescriptor.description = QStringLiteral("Lua 脚本协议（当前仅登记配置和诊断元数据）");
luaDescriptor.category = ProtocolCategory::Custom;
luaDescriptor.legacyType = ProtocolType::Raw;
luaDescriptor.builtin = false;
luaDescriptor.scriptProtocol = true;
luaDescriptor.creatable = false;
luaDescriptor.legacyCompatible = false;
luaDescriptor.configSchema = makeLuaScriptSchema();
luaDescriptor.configVersion = luaDescriptor.configSchema.version;
luaDescriptor.defaultConfig = luaDescriptor.configSchema.defaults();
registerProtocol(luaDescriptor, ProtocolCreator());
```

**Step 4: Add Lua schema helper**

在 `ProtocolRegistry.cpp` 匿名 namespace 中新增 `makeLuaScriptSchema()`，字段包括 `scriptSource`、`scriptPath`、`entryFunction`、`timeoutMs`、`memoryLimitKb`、`maxOutputLines`、`allowCommunicationApi`。

**Step 5: Filter legacy factory list**

`ProtocolFactory::typeId()` 和 `ProtocolFactory::supportedTypes()` 跳过 `legacyCompatible=false` 的描述。

**Step 6: Update existing expected counts**

把注册中心描述总数从 10 调整到 11；外部协议 + 内置协议总数从 11 调整到 12。

**Step 7: Run tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 注册中心相关测试通过。

**Step 8: Commit implementation**

```powershell
git add src/core/protocol/ProtocolDescriptor.h src/core/protocol/ProtocolRegistry.cpp src/core/protocol/ProtocolFactory.cpp tests/unit/TestProtocolRegistry.cpp
git commit -m "feat: register lua script protocol metadata"
```

## Task 3: Lua 协议诊断红测

**Files:**
- Modify: `tests/unit/TestProtocolDiagnostics.h`
- Modify: `tests/unit/TestProtocolDiagnostics.cpp`

**Step 1: Add failing test**

头文件新增：

```cpp
void exportsLuaProtocolDiagnostics();
```

cpp 新增：

```cpp
void TestProtocolDiagnostics::exportsLuaProtocolDiagnostics()
{
    const ProtocolDescriptor descriptor =
        ProtocolFactory::registry().descriptor(QStringLiteral("lua.script"));
    QVariantMap config = descriptor.defaultConfig;
    config.insert(QStringLiteral("timeoutMs"), 1500);
    config.insert(QStringLiteral("allowCommunicationApi"), true);

    ProtocolDiagnosticsContext context;
    context.recentError = QStringLiteral("last lua error");

    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        config,
        QStringLiteral("2026-06-02T16:30:00+08:00"),
        context);

    const QJsonObject lua = json.value(QStringLiteral("luaProtocol")).toObject();
    QVERIFY(lua.value(QStringLiteral("enabled")).toBool());
    QVERIFY(!lua.value(QStringLiteral("creatable")).toBool());
    QVERIFY(!lua.value(QStringLiteral("receiveApiAvailable")).toBool());
    QCOMPARE(lua.value(QStringLiteral("lastError")).toString(), QStringLiteral("last lua error"));

    const QJsonObject sandbox = lua.value(QStringLiteral("sandbox")).toObject();
    QCOMPARE(sandbox.value(QStringLiteral("timeoutMs")).toInt(), 1500);
    QCOMPARE(sandbox.value(QStringLiteral("memoryLimitKb")).toInt(), 1024);
    QCOMPARE(sandbox.value(QStringLiteral("maxOutputLines")).toInt(), 200);
    QVERIFY(sandbox.value(QStringLiteral("communicationApi")).toBool());
    QVERIFY(!sandbox.value(QStringLiteral("blockedLibraries")).toArray().isEmpty());
}
```

**Step 2: Run red test**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL，编译错误提示缺少 `ProtocolDiagnosticsContext` 或 `build()` 新重载。

**Step 3: Commit red test**

```powershell
git add tests/unit/TestProtocolDiagnostics.h tests/unit/TestProtocolDiagnostics.cpp
git commit -m "test: cover lua protocol diagnostics export"
```

## Task 4: ProtocolDiagnosticsBuilder 导出 Lua 节点

**Files:**
- Modify: `src/core/protocol/ProtocolDiagnostics.h`
- Modify: `src/core/protocol/ProtocolDiagnostics.cpp`
- Modify: `src/ui/dialogs/ProtocolDiagnosticsDialog.cpp`
- Modify if needed: `resources/translations/en_US.ts`

**Step 1: Add context struct**

在 `ProtocolDiagnostics.h` 增加：

```cpp
struct ProtocolDiagnosticsContext
{
    QString recentError;
};
```

并把 `build()` 签名扩展为：

```cpp
static QJsonObject build(const ProtocolDescriptor& descriptor,
                         const QVariantMap& currentConfig,
                         const QString& generatedAt = QString(),
                         const ProtocolDiagnosticsContext& context = ProtocolDiagnosticsContext());
```

**Step 2: Add Lua helper functions**

在 cpp 中新增：
- `valueFromNormalizedOrDefault()`
- `buildLuaSandboxObject()`
- `buildLuaProtocolObject()`

当 `descriptor.scriptProtocol` 为 true 时，根对象插入 `luaProtocol`。

**Step 3: Export new capability flags**

`capabilities` 节点新增 `scriptProtocol`、`creatable`、`legacyCompatible`。

**Step 4: Update dialog summary**

能力摘要补充脚本协议和可创建实例状态，例如：

```cpp
tr("能力：内置=%1，绘图=%2，构帧=%3，脚本=%4，可创建=%5")
```

**Step 5: Run tests**

Run:

```powershell
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 诊断相关测试通过。

**Step 6: Commit implementation**

```powershell
git add src/core/protocol/ProtocolDiagnostics.h src/core/protocol/ProtocolDiagnostics.cpp src/ui/dialogs/ProtocolDiagnosticsDialog.cpp resources/translations/en_US.ts
git commit -m "feat: export lua protocol diagnostics"
```

## Task 5: 文档同步

**Files:**
- Modify: `docs/user-guide/scripting.md`
- Modify: `resources/help/protocols.html`
- Review: `resources/help/quickstart.html`

**Step 1: Update scripting guide**

补充：
- `lua.script` 已进入协议能力目录。
- 当前仅提供配置 Schema 和诊断元数据。
- 尚不能作为接收解析器使用，`serial.receive(timeout)` 仍未开放。

**Step 2: Update protocol help**

更新协议能力平台化段落，说明 Lua 协议现在已经登记为脚本协议元数据，并能在诊断 JSON 中导出沙箱限制和最近错误。

**Step 3: Review quickstart**

如果快速连接、普通发送/接收、文件传输、快捷键和版本信息不变，不更新 quickstart，并在最终说明写明理由。

**Step 4: Commit docs**

```powershell
git add docs/user-guide/scripting.md resources/help/protocols.html
git commit -m "docs: describe lua protocol registration"
```

## Task 6: 完整验证与 RecallLoom

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
- 测试目标构建成功
- `ComAssistant_tests` 输出 `All tests PASSED!`
- `ctest` 通过
- 主程序构建成功并生成 `D:\comassistant\build_release\ComAssistant.exe`
- `git diff --check` 无空白错误
- RecallLoom 校验通过

**Step 2: Update RecallLoom**

用 revision-aware helper 更新：
- `rolling_summary.md`：记录 4.9 已完成、Lua 协议当前边界和下一步。
- `daily_logs/2026-06-02.md`：追加 4.9 实现、测试、文档和验证命令。

**Step 3: Final status**

Run:

```powershell
git status --short --branch
git log --oneline -12
```

Expected: 工作区只包含预期变更；若全部已提交则干净。
