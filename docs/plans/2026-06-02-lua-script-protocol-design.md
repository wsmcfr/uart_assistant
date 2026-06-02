# Lua 脚本协议解析器最小原型设计

## 背景

第四阶段 4.9 已将 `lua.script` 登记为协议能力目录、配置 Schema 和诊断 JSON 的稳定事实源，但仍为 `creatable=false`，无法创建真实 `IProtocol` 实例。4.10 的目标是在不改变旧版 `ProtocolType` 工作流、不接入绘图协议菜单、不开放 `serial.receive(timeout)` 的前提下，让 `lua.script` 具备最小接收解析能力。

## 方案取舍

| 方案 | 内容 | 取舍 |
|---|---|---|
| 推荐方案 A | 新增 `LuaScriptProtocol`，`lua.script` 可通过注册中心创建；脚本定义 `process(data, context)` 并返回 table | 最小但真实可测，旧 UI 和旧会话链路不受影响 |
| 方案 B | 只增强 `LuaSandbox` 的结构化调用能力，不创建协议实例 | 底层更通用，但 `lua.script` 仍不能解析接收数据 |
| 方案 C | 同时开放 `serial.receive(timeout)` 并接 UI 协议选择 | 范围过大，会提前牵动接收缓冲、阻塞超时、线程和权限提示 |

采用方案 A。用户已确认继续按该边界实施。

## 目标边界

| 类型 | 内容 |
|---|---|
| 目标 | `ProtocolRegistry::create("lua.script")` 返回真实 `LuaScriptProtocol` 实例 |
| 目标 | Lua 脚本入口为 `process(data, context)`，默认入口名来自 Schema 的 `entryFunction` |
| 目标 | `process()` 返回 table，并映射到 `FrameResult` |
| 目标 | `LuaSandbox` 的超时、内存、输出行数和通信 API 开关继续由协议配置控制 |
| 非目标 | 不开放 `serial.receive(timeout)` |
| 非目标 | 不加载 `scriptPath` 外部文件，只执行 `scriptSource` 内联脚本 |
| 非目标 | 不把 Lua 协议加入 `ProtocolFactory::supportedTypes()`、旧版绘图协议菜单或旧会话枚举恢复链路 |
| 非目标 | 不实现 Lua 脚本化构帧；`build(payload)` 第一版原样返回 payload |

## 入口契约

Lua 脚本必须定义入口函数：

```lua
function process(data, context)
    return {
        valid = true,
        consumedBytes = #data,
        frame = data,
        payload = data,
        metadata = {
            source = context.protocolId
        }
    }
end
```

| 字段 | Lua 类型 | C++ 映射 | 说明 |
|---|---|---|---|
| `valid` | boolean | `FrameResult::valid` | 是否解析出有效帧 |
| `consumedBytes` | number | `FrameResult::consumedBytes` | 本次消耗的输入字节数，越界时由 C++ 限制到 `0..data.size()` |
| `frame` | string | `FrameResult::frame` | 完整帧原始字节 |
| `payload` | string | `FrameResult::payload` | 有效载荷原始字节 |
| `metadata` | table | `FrameResult::metadata` | 第一版只收集 string、number、boolean 标量 |
| `error` | string | `FrameResult::errorMessage` | 脚本主动返回的错误说明 |

`context` 是只读约定对象，第一版包含：

| 字段 | 含义 |
|---|---|
| `protocolId` | 固定为 `lua.script` |
| `entryFunction` | 当前调用的入口函数名 |
| `dataLength` | 本次传入数据长度 |

## 数据流

1. 调用方创建 `lua.script` 协议实例并设置配置。
2. `LuaScriptProtocol::parse(data)` 读取 `scriptSource`、`entryFunction` 和沙箱限制。
3. C++ 生成一段受控 Lua wrapper：
   - 先执行用户脚本；
   - 用 `hexToBytes()` 构造本次输入 `data`；
   - 构造 `context` table；
   - 查找并调用入口函数；
   - 将返回 table 编码为带哨兵标记的输出行。
4. `LuaSandbox::execute()` 执行 wrapper 并捕获输出、超时、取消、内存错误和 Lua 错误。
5. C++ 从输出行中提取哨兵区间，将字段解码为 `FrameResult`。

## 错误处理

| 场景 | 行为 |
|---|---|
| `scriptSource` 为空 | 返回 `valid=false`，`errorMessage` 提示脚本源码为空，`consumedBytes=0` |
| 入口函数不存在 | 沙箱 Lua 错误回传到 `FrameResult::errorMessage` |
| 入口函数返回非 table | 沙箱 Lua 错误回传到 `FrameResult::errorMessage` |
| 脚本超时或内存超限 | 保留 `LuaSandboxResult` 的稳定错误文本 |
| `consumedBytes` 越界 | C++ 限制到 `0..data.size()`，避免脚本错误消耗未知缓冲 |
| 用户脚本 print 干扰 | C++ 只解析哨兵区间内的协议结果输出 |

## 组件改动

| 组件 | 改动 |
|---|---|
| `LuaScriptProtocol` | 新增协议实现，负责配置读取、wrapper 生成、沙箱执行和结果映射 |
| `ProtocolRegistry` | 将 `lua.script` 改为 `creatable=true` 并注册创建器，继续保持 `legacyCompatible=false` |
| `ProtocolDiagnostics` | 更新 Lua 诊断中的可创建状态事实 |
| 测试 | 新增 `TestLuaScriptProtocol`；更新注册中心和诊断测试中 `creatable` 期望 |
| 文档 | 更新脚本文档和协议帮助；检查 `quickstart.html` 是否受影响 |

## 风险控制

| 风险 | 控制 |
|---|---|
| Lua 协议污染旧 UI | `legacyCompatible=false` 保持不变，旧版列表测试继续保护 |
| wrapper 输出格式被用户输出干扰 | 使用固定 begin/end 哨兵，只解析哨兵区间 |
| 二进制数据经过文本损坏 | 输入、frame、payload 和 string metadata 使用十六进制编码传输 |
| 脚本误消耗过多字节 | `consumedBytes` 在 C++ 侧夹紧到输入长度范围 |
| 误承诺接收 API | 文档继续明确 `serial.receive(timeout)` 未开放 |

## 验证

| 验证项 | 命令 |
|---|---|
| 构建测试目标 | `cmake --build build_release --config Release --target ComAssistant_tests --parallel` |
| 运行全部单元测试 | `build_release\tests\ComAssistant_tests.exe -o -,txt` |
| 运行 CTest | `ctest --test-dir build_release --output-on-failure` |
| 构建正式程序 | `cmake --build build_release --config Release --target ComAssistant --parallel` |
| 检查空白错误 | `git diff --check` |
| 校验 RecallLoom | `python C:/Users/caofengrui/.agents/skills/recallloom/scripts/validate_context.py D:/comassistant` |
