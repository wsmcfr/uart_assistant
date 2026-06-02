# Lua Script Protocol Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将 `lua.script` 从仅可配置/诊断的元数据协议推进为可创建、可解析接收数据的最小 Lua `IProtocol` 原型。

**Architecture:** 新增 `LuaScriptProtocol` 实现 `IProtocol`，由注册中心按稳定 ID `lua.script` 创建。协议读取 `scriptSource`、`entryFunction` 和沙箱限制，生成受控 Lua wrapper 调用 `process(data, context)`，再把 Lua table 返回值映射为 `FrameResult`；旧版 `ProtocolType` 列表继续跳过 `legacyCompatible=false` 的 Lua 协议。

**Tech Stack:** C++17, Qt 5.12.9 Core/Test, Lua C API through existing `LuaSandbox`, existing `ProtocolRegistry`, `ProtocolConfigSchema`, `ProtocolDiagnosticsBuilder`.

---

### Task 1: 写 LuaScriptProtocol 红测

**Files:**
- Create: `tests/unit/TestLuaScriptProtocol.h`
- Create: `tests/unit/TestLuaScriptProtocol.cpp`
- Modify: `tests/main.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing tests**

新增测试类，覆盖以下行为：

```cpp
void TestLuaScriptProtocol::parsesFrameFromProcessResult()
{
    LuaScriptProtocol protocol;
    QVariantMap config;
    config.insert(QStringLiteral("scriptSource"),
                  QStringLiteral("function process(data, context)\n"
                                 "  return { valid = true, consumedBytes = #data, frame = data, payload = string.sub(data, 2), metadata = { protocol = context.protocolId, length = context.dataLength, ok = true } }\n"
                                 "end\n"));
    protocol.setConfig(config);

    const FrameResult result = protocol.parse(QByteArray::fromHex("A10203"));

    QVERIFY(result.valid);
    QCOMPARE(result.consumedBytes, 3);
    QCOMPARE(result.frame.toHex(' ').toUpper(), QByteArray("A1 02 03"));
    QCOMPARE(result.payload.toHex(' ').toUpper(), QByteArray("02 03"));
    QCOMPARE(result.metadata.value(QStringLiteral("protocol")).toString(), QStringLiteral("lua.script"));
    QCOMPARE(result.metadata.value(QStringLiteral("length")).toInt(), 3);
    QCOMPARE(result.metadata.value(QStringLiteral("ok")).toBool(), true);
}
```

同时添加：

- `returnsIncompleteFrameWithoutConsumingBytes()`：`process()` 返回 `valid=false, consumedBytes=0`。
- `reportsLuaErrorsFromProcess()`：入口函数抛错时返回 `valid=false` 和错误消息。
- `clampsConsumedBytesToInputSize()`：脚本返回超大 `consumedBytes` 时限制到输入长度。
- `buildReturnsPayloadForFirstPrototype()`：`build(payload)` 原样返回 payload。

**Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL，编译错误提示 `LuaScriptProtocol` 头文件或类型不存在。

**Step 3: Commit red tests**

```bash
git add tests/unit/TestLuaScriptProtocol.h tests/unit/TestLuaScriptProtocol.cpp tests/main.cpp tests/CMakeLists.txt
git commit -m "test: cover lua script protocol parser"
```

### Task 2: 实现 LuaScriptProtocol 最小解析器

**Files:**
- Create: `src/core/protocol/LuaScriptProtocol.h`
- Create: `src/core/protocol/LuaScriptProtocol.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create protocol class**

`LuaScriptProtocol` 继承 `IProtocol`，实现：

- `type()` 返回 `ProtocolType::Raw`，因为该协议没有旧版枚举身份。
- `name()` 返回 `Lua Script`。
- `description()` 说明为 Lua 脚本协议解析器。
- `parse(data)` 调用沙箱并映射结果。
- `build(payload, metadata)` 第一版原样返回 payload。
- `validate(frame)` 返回 `parse(frame).valid`。
- `calculateChecksum(data)` 第一版返回空字节数组。

**Step 2: Implement wrapper and result mapping**

实现私有辅助逻辑：

- 从配置读取 `scriptSource`、`entryFunction`、`timeoutMs`、`memoryLimitKb`、`maxOutputLines`、`allowCommunicationApi`。
- 用输入数据的 hex 字符串生成 wrapper。
- wrapper 执行用户脚本并调用入口函数。
- wrapper 用固定哨兵输出字段：
  - `__COMASSISTANT_LUA_PROTOCOL_BEGIN__`
  - `valid=1`
  - `consumedBytes=3`
  - `frameHex=A1 02 03`
  - `payloadHex=02 03`
  - `error=...`
  - `metadataHex:key=valueHex`
  - `metadataNumber:key=123`
  - `metadataBool:key=1`
  - `__COMASSISTANT_LUA_PROTOCOL_END__`
- C++ 只解析哨兵区间。

**Step 3: Run tests to verify green for the new class**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: 新增 `TestLuaScriptProtocol` 通过，其他测试不出现回归。

**Step 4: Commit implementation**

```bash
git add src/core/protocol/LuaScriptProtocol.h src/core/protocol/LuaScriptProtocol.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add lua script protocol parser"
```

### Task 3: 接入 ProtocolRegistry 和诊断期望

**Files:**
- Modify: `src/core/protocol/ProtocolRegistry.cpp`
- Modify: `tests/unit/TestProtocolRegistry.h`
- Modify: `tests/unit/TestProtocolRegistry.cpp`
- Modify: `tests/unit/TestProtocolDiagnostics.cpp`

**Step 1: Write/update failing expectations**

更新注册中心测试：

- `registersLuaScriptProtocolDescriptor()` 期望 `descriptor.creatable == true`。
- 期望 `registry.create("lua.script") != nullptr`。
- 期望创建出的协议 `name()` 为 `Lua Script`。

更新诊断测试：

- `luaProtocol.creatable` 期望为 `true`。

**Step 2: Run tests to verify red**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL，Lua descriptor 仍为 `creatable=false` 或创建结果为空。

**Step 3: Register LuaScriptProtocol**

在 `ProtocolRegistry.cpp`：

- include `LuaScriptProtocol.h`。
- `makeLuaScriptDescriptor()` 设置 `description` 为真实最小解析器说明。
- 设置 `creatable=true`，保持 `legacyCompatible=false`。
- 注册 `lua.script` 时提供 `[](QObject* parent) { return new LuaScriptProtocol(parent); }`。

**Step 4: Run tests to verify green**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: `TestProtocolRegistry` 和 `TestProtocolDiagnostics` 通过。

**Step 5: Commit registry integration**

```bash
git add src/core/protocol/ProtocolRegistry.cpp tests/unit/TestProtocolRegistry.h tests/unit/TestProtocolRegistry.cpp tests/unit/TestProtocolDiagnostics.cpp
git commit -m "feat: make lua script protocol creatable"
```

### Task 4: 同步文档

**Files:**
- Modify: `docs/user-guide/scripting.md`
- Modify: `resources/help/protocols.html`
- Check: `resources/help/quickstart.html`

**Step 1: Update scripting guide**

说明：

- `lua.script` 4.10 起可作为最小接收解析器创建。
- 入口函数契约为 `process(data, context)`。
- 返回 table 字段含义。
- 第一版不加载 `scriptPath`，不开放 `serial.receive(timeout)`，`build()` 仍原样返回 payload。

**Step 2: Update protocol help**

替换 4.9 中“不会创建真实协议解析器”的描述，改为：

- 现在可通过注册中心创建最小 Lua 解析器。
- 旧版绘图协议菜单和旧会话枚举链路仍不包含 Lua。
- `serial.receive(timeout)` 仍未开放。

**Step 3: Check quickstart**

检查 `resources/help/quickstart.html` 是否需要更新。若快速连接、普通发送/接收、文件传输、会话保存、快捷键和版本信息主流程未变，则不修改，并在最终说明写明理由。

**Step 4: Commit docs**

```bash
git add docs/user-guide/scripting.md resources/help/protocols.html
git commit -m "docs: describe lua script protocol parser"
```

### Task 5: 同步 RecallLoom 并完整验证

**Files:**
- Modify: `.recallloom/rolling_summary.md`
- Modify: `.recallloom/daily_logs/2026-06-02.md`

**Step 1: Run final verification**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
ctest --test-dir build_release --output-on-failure
cmake --build build_release --config Release --target ComAssistant --parallel
git diff --check
```

Expected:

- 测试目标构建成功。
- `ComAssistant_tests.exe` 输出 `All tests PASSED!`。
- CTest 输出 `100% tests passed`。
- 正式程序构建成功并生成 `D:\comassistant\build_release\ComAssistant.exe`。
- `git diff --check` 无输出。

**Step 2: Update RecallLoom**

使用 RecallLoom helper 更新：

```bash
python C:/Users/caofengrui/.agents/skills/recallloom/scripts/commit_context_file.py D:/comassistant .recallloom/rolling_summary.md --writer-id Codex
python C:/Users/caofengrui/.agents/skills/recallloom/scripts/append_daily_log_entry.py D:/comassistant --writer-id Codex --section work_completed "..."
python C:/Users/caofengrui/.agents/skills/recallloom/scripts/validate_context.py D:/comassistant
```

**Step 3: Commit memory update if project convention requires**

```bash
git add .recallloom/rolling_summary.md .recallloom/daily_logs/2026-06-02.md
git commit -m "docs: update recallloom for lua script protocol"
```

**Step 4: Final status**

报告：

- 主要改动。
- 验证命令和结果。
- `resources/help/quickstart.html` 是否更新及理由。
- `.recallloom/` 更新层级。
