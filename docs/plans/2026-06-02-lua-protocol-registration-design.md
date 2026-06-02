# 4.9 Lua 协议注册最小闭环设计

## 背景

4.1 到 4.4 已完成协议注册中心、配置 Schema、配置 UI 和诊断导出。4.5 到 4.8 已完成受限 Lua 沙箱、脚本编辑器真实执行、后台取消、真实连接状态注入和发送错误语义。

当前 Lua 能力仍停留在“脚本编辑器工具”层。下一步如果直接实现 Lua 协议解析器，会立刻碰到接收缓冲、超时、线程边界和协议生命周期问题。4.9 先做更小但完整的闭环：让 Lua 协议作为一种可登记、可配置、可诊断的协议能力进入统一事实源，但暂不创建 `IProtocol` 实例，也不开放 `serial.receive(timeout)`。

## 范围

| 类型 | 内容 |
|------|------|
| 目标 | 在 `ProtocolRegistry` 中登记稳定 ID 为 `lua.script` 的 Lua 脚本协议能力 |
| 目标 | 通过 `ProtocolDescriptor` 暴露脚本协议、可创建实例、旧版枚举兼容等能力标志 |
| 目标 | 通过 `ProtocolConfigSchema` 暴露 Lua 协议第一版配置字段和默认值 |
| 目标 | 让 `ProtocolDiagnosticsBuilder` 为 Lua 协议导出沙箱选项、接收 API 状态和最近错误 |
| 目标 | 保持 `ProtocolFactory::supportedTypes()` 和旧版 `ProtocolType` 工作流不包含 Lua 协议 |
| 目标 | 同步脚本文档和内置协议帮助 |
| 非目标 | 不实现 Lua 协议 `IProtocol` 解析器 |
| 非目标 | 不开放 `serial.receive(timeout)` |
| 非目标 | 不加载外部 Lua 文件、不做插件热加载、不做 ABI 或版本握手 |
| 非目标 | 不改变脚本编辑器现有运行、发送、取消流程 |

## 方案比较

| 方案 | 做法 | 优点 | 风险 | 结论 |
|------|------|------|------|------|
| A | 直接实现 Lua `IProtocol` 解析器 | 看起来一步到位 | 接收缓冲、阻塞语义、取消和权限提示均未设计，容易把协议路径做歪 | 不采用 |
| B | 只登记 Lua 协议元数据、Schema 和诊断扩展 | 闭环小，可测试，可复用现有事实源 | 暂不能在 UI 中真正选择 Lua 协议解析数据 | 推荐 |
| C | 只在文档中说明后续 Lua 协议 | 改动最小 | 注册中心、Schema、诊断仍没有事实源，后续仍要补 | 不采用 |

推荐方案 B。4.9 把 Lua 协议能力先纳入目录和诊断，后续 4.10 再评估是否实现解析器、脚本来源管理和接收 API。

## 架构

| 组件 | 职责 |
|------|------|
| `ProtocolDescriptor` | 新增 `scriptProtocol`、`creatable`、`legacyCompatible` 标志，区分脚本协议、可创建实例和旧版枚举可见性 |
| `ProtocolRegistry` | 登记 `lua.script` 描述；允许 `creatable=false` 的描述只登记元数据而不提供创建器 |
| `ProtocolFactory` | 旧版枚举列表和 typeId 反查跳过 `legacyCompatible=false` 的协议，避免 Lua 协议污染旧 UI |
| `ProtocolConfigSchema` | 继续使用现有扁平字段，描述 Lua 协议沙箱资源和脚本来源字段 |
| `ProtocolDiagnosticsBuilder` | 对 `scriptProtocol=true` 的协议额外导出 `luaProtocol` 节点 |

## Lua 协议描述

| 字段 | 值 |
|------|----|
| `id` | `lua.script` |
| `displayName` | `Lua Script` |
| `description` | `Lua 脚本协议（当前仅登记配置和诊断元数据）` |
| `category` | `Custom` |
| `builtin` | `false` |
| `plotProtocol` | `false` |
| `frameBuilder` | `false` |
| `scriptProtocol` | `true` |
| `creatable` | `false` |
| `legacyCompatible` | `false` |

`creatable=false` 表示当前阶段不能通过 `ProtocolRegistry::create("lua.script")` 得到协议实例。`legacyCompatible=false` 表示它不会出现在 `ProtocolFactory::supportedTypes()`、旧版绘图协议菜单或旧会话枚举恢复链路中。

## Lua 配置 Schema

| key | 类型 | 默认值 | 说明 |
|-----|------|--------|------|
| `scriptSource` | String | 空字符串 | 内联 Lua 脚本文本；4.9 只保存和诊断，不执行 |
| `scriptPath` | String | 空字符串 | 外部脚本路径占位；4.9 不加载文件 |
| `entryFunction` | String | `process` | 后续协议解析入口函数名称 |
| `timeoutMs` | Integer | `1000` | Lua 沙箱执行超时 |
| `memoryLimitKb` | Integer | `1024` | Lua state 内存预算 |
| `maxOutputLines` | Integer | `200` | `print` 输出保留行数 |
| `allowCommunicationApi` | Bool | `false` | 后续是否允许发送类通信 API；4.9 不开放接收 |

## 诊断扩展

Lua 协议诊断在既有 JSON 上增加 `luaProtocol` 节点：

```json
{
  "luaProtocol": {
    "enabled": true,
    "creatable": false,
    "receiveApiAvailable": false,
    "lastError": "",
    "sandbox": {
      "timeoutMs": 1000,
      "memoryLimitKb": 1024,
      "maxOutputLines": 200,
      "communicationApi": false,
      "safeLibraries": ["_G", "string", "table", "math", "utf8"],
      "blockedLibraries": ["io", "os", "package", "debug"],
      "blockedGlobals": ["require", "dofile", "loadfile", "load"]
    }
  }
}
```

`lastError` 来自诊断上下文，默认空。4.9 只提供 Builder 接口和测试覆盖；后续真实 Lua 协议实例或脚本管理器可把最近运行错误注入该字段。

## 错误处理

| 场景 | 处理 |
|------|------|
| Lua 协议无创建器 | 注册中心允许登记，但 `create("lua.script")` 返回 `nullptr` |
| 外部协议无创建器且 `creatable=true` | 仍按旧规则拒绝注册 |
| Lua 配置非法 | 诊断导出 `validation.valid=false` 和错误列表，`configuration.normalizedConfig` 为空对象 |
| Lua 诊断上下文无最近错误 | `lastError` 输出空字符串 |
| 旧版协议列表查询 | 跳过 `legacyCompatible=false`，保持旧 UI 和旧测试数量稳定 |

## 测试计划

| 测试 | 覆盖点 |
|------|------|
| `TestProtocolRegistry::registersLuaScriptProtocolDescriptor` | Lua 协议稳定 ID、能力标志、Schema、默认配置和不可创建行为 |
| `TestProtocolRegistry::factoryLegacyListIgnoresLuaDescriptor` | Lua 协议不污染旧版 `ProtocolType` 列表 |
| `TestProtocolDiagnostics::exportsLuaProtocolDiagnostics` | Lua 诊断导出沙箱选项、接收 API 状态和最近错误 |
| 既有注册中心测试 | 内置协议数量、Raw 兼容、外部注册和默认配置自洽 |
| 既有诊断测试 | 普通协议和 Raw 诊断保持兼容 |

## 文档影响

| 文件 | 动作 |
|------|------|
| `docs/user-guide/scripting.md` | 补充 Lua 协议 4.9 状态：已进入协议目录和诊断，但尚不能作为接收解析器 |
| `resources/help/protocols.html` | 更新协议能力平台化说明，明确 `lua.script` 和 `serial.receive(timeout)` 状态 |
| `resources/help/quickstart.html` | 复核是否影响快速连接、普通发送、接收、文件传输、会话保存和版本信息；若不影响则不更新并在最终说明写明理由 |

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 用户误以为 Lua 协议已经可选可执行 | 文档和描述明确“当前仅登记配置和诊断元数据” |
| 新描述污染旧版协议菜单 | 用 `legacyCompatible=false` 并更新 `ProtocolFactory` 过滤 |
| 诊断字段未来变化 | 继续使用 `diagnosticVersion=1`，Lua 节点保持保守字段 |
| Schema 暴露了未执行的脚本字段 | 字段说明中注明 4.9 只保存和诊断，不加载、不执行 |
