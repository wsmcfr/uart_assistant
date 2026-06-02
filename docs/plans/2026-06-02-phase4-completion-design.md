# 第四阶段完整收口设计

## 背景

第四阶段已经完成协议注册中心、协议配置 Schema、协议诊断、Lua 沙箱、脚本编辑器、Lua 协议注册和最小 Lua 接收解析器。当前缺口集中在三个地方：协议配置 UI 对 Lua 脚本不够友好，`lua.script` 还没有进入用户可选的接收协议链路，Lua 协议运行错误和诊断信息还没有生产化闭环。

## 推荐方案

| 方案 | 内容 | 取舍 | 结论 |
|---|---|---|---|
| A | 把 `lua.script` 塞进旧版 `ProtocolType` 枚举 | 实现直观，但会污染旧绘图菜单和旧会话枚举语义 | 不采用 |
| B | MainWindow 同时维护稳定协议 ID 和旧版绘图协议枚举 | 兼容现有绘图菜单，又能支持 Lua 和后续插件协议 | 采用 |
| C | 全面重构所有协议链路为注册中心驱动 | 长期更彻底，但本阶段风险和改动面过大 | 暂不采用 |

采用方案 B。旧版 `ProtocolType` 继续服务绘图协议菜单和自动检测；稳定 `protocolId` 作为当前接收协议事实源，用于配置、诊断、会话保存和恢复。`lua.script` 通过注册中心创建协议实例，不进入旧绘图菜单。

## 设计范围

| 子阶段 | 设计 |
|---|---|
| 4.11 协议配置 UI | `ProtocolConfigEditor` 对 `scriptSource` 使用多行脚本编辑器；每个字段增加字段级错误 label；空 Schema 文案改为“当前协议没有可配置项，可直接使用默认行为”。 |
| 4.12 稳定协议 ID 接收链路 | MainWindow 新增当前协议 ID、当前协议描述和当前配置状态；配置、诊断、保存、恢复都以 `protocolId` 为准；旧绘图菜单仅改变绘图协议。 |
| Lua 生产化 | `LuaScriptProtocol` 记录最近错误；脚本源码通过会话配置保存恢复；MainWindow 将 Lua 最近错误传入诊断上下文；解析错误在状态栏显示。 |
| 文档与翻译 | 更新 Lua 脚本用户指南、协议帮助、快速开始中的协议配置/版本信息、英文翻译。 |
| 发布准备 | 第四阶段作为功能版本发布，版本号升级到 `1.7.0`，同步 `CMakeLists.txt`、`src/version.h`、`CHANGELOG.md`、`README.md` 与内置帮助版本块。 |
| 最终验证 | 运行目标单测、全量测试、CTest、Release 构建、`git diff --check`、RecallLoom 校验。 |

## MainWindow 协议状态

| 状态 | 用途 |
|---|---|
| `m_currentProtocolId` | 当前接收协议稳定 ID，例如 `raw`、`plot.text`、`lua.script`。 |
| `m_currentProtocolType` | 旧版绘图协议状态，继续给绘图菜单、自动检测和旧协议名显示使用。 |
| `m_currentProtocol` | 当前接收协议实例；Raw 为空，Lua 或其他可创建协议为真实实例。 |
| `m_currentProtocolConfig` | Raw 或当前实例不存在时仍保存配置，确保配置对话框和会话保存不丢失。 |

切换旧绘图菜单时，MainWindow 调用稳定 ID 切换辅助函数，把 `plot.text`、`plot.stamp`、`plot.csv`、`plot.justfloat` 或 `raw` 设为当前接收协议。恢复 Lua 会话时，直接按 `lua.script` 创建实例并应用配置，同时旧绘图菜单保持“无”。

## 接收数据流

| 步骤 | 行为 |
|---|---|
| 1 | 原始数据继续先进入当前显示模式、通信工作台、统计、数据分窗和数据表格。 |
| 2 | 如果当前协议是 Raw，继续喂给绘图自动检测器。 |
| 3 | 如果当前协议是绘图协议，继续进入 `MainWindowPlotDataRouter`。 |
| 4 | 如果当前协议是非绘图协议，例如 `lua.script`，调用 `IProtocol::parse()` 获取 `FrameResult`。 |
| 5 | 有错误时记录最近错误并状态栏提示；有效帧可写入数据表格描述和协议诊断，暂不改变现有 FrameModeWidget 的本地帧头/帧尾 UI。 |

本阶段不开放 `serial.receive(timeout)`，不做外部脚本路径加载，不脚本化发送构帧，不重构 FrameModeWidget 的帧头/帧尾解析 UI。

## 错误与诊断

| 错误来源 | 处理 |
|---|---|
| 配置 Schema 错误 | 配置对话框底部保留总错误，同时字段下方显示字段级错误。 |
| Lua 脚本为空 | `LuaScriptProtocol` 返回错误并记录最近错误。 |
| Lua 运行错误 | 返回 `FrameResult::errorMessage`，记录最近错误，发出 `parseError`。 |
| Lua 未完整帧 | `valid=false` 且无错误时不提示，避免半包刷屏。 |
| 诊断导出 | `luaProtocol.lastError` 使用当前实例最近错误或 MainWindow 记录的最近协议错误。 |

## 测试策略

| 测试文件 | 覆盖点 |
|---|---|
| `tests/unit/TestProtocolConfigEditor.*` | Lua `scriptSource` 多行编辑、字段级错误 label、空 Schema 文案。 |
| `tests/unit/TestLuaScriptProtocol.*` | 最近错误记录、成功解析后清空错误、脚本输出行数边界。 |
| `tests/unit/TestMainWindowSessionCoordinator.*` | 非旧版稳定协议 ID 恢复结果。 |
| `tests/unit/TestProtocolDiagnostics.*` | Lua 最近错误通过上下文进入诊断 JSON。 |
| `tests/unit/TestProtocolConfigSchema.*` | 会话保存/恢复保留 `lua.script` 源码配置。 |

## 文档与发布

文档更新覆盖用户可见的协议配置流程、Lua 协议接收解析、诊断信息和版本说明。发布准备使用 `1.7.0`，因为本次把 Lua 协议从“可创建原型”推进到“用户可选接收链路”，属于新增功能版本。

## 风险控制

| 风险 | 控制 |
|---|---|
| Lua 协议污染绘图菜单 | 旧绘图菜单仍只列绘图协议，Lua 通过稳定协议 ID 保存和恢复。 |
| 会话恢复回退 Raw | `SessionData` 和 `MainWindowSessionCoordinator` 同时保留稳定 ID，未知 ID 才回退 Raw。 |
| Lua 错误刷屏 | 只有非空错误提示状态栏；半包无错误不提示。 |
| UI 改动影响旧协议配置 | 多行控件只针对 `scriptSource`，其他 String/BytesHex 保持原控件。 |
