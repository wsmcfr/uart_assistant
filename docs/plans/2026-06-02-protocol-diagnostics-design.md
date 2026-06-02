# 4.4 协议能力诊断导出设计

## 背景

4.1 已建立协议注册中心，4.2 已建立协议配置 Schema、默认配置、Factory 校验应用和会话持久化，4.3 已提供 Schema 驱动的协议配置 UI。当前协议能力已经有统一事实源，但当用户遇到协议配置异常、会话恢复异常，或后续 Lua/插件协议接入问题时，还缺少一个可以直接导出当前协议状态的诊断入口。

4.4 的目标是把“当前协议事实源”做成可读、可复制、可保存的诊断快照。第一版兼顾普通用户阅读和开发排障：界面展示摘要，完整 JSON 用于 Issue、日志和后续插件/Lua 调试。

## 目标与非目标

| 类型 | 内容 |
|---|---|
| 目标 | 新增 `工具 -> 协议诊断...` 菜单入口 |
| 目标 | 新增只读 `ProtocolDiagnosticsDialog`，展示协议概览、能力、配置摘要和校验结果 |
| 目标 | 新增核心诊断构建能力，按当前协议 descriptor、当前配置和 Schema 校验结果生成 JSON |
| 目标 | 支持一键复制完整诊断 JSON |
| 目标 | 支持保存诊断 JSON 到 `.json` 文件 |
| 目标 | Raw 或空 Schema 协议也能正常导出，不崩溃、不显示空白界面 |
| 非目标 | 不在诊断对话框内编辑协议配置 |
| 非目标 | 不加载插件、不执行 Lua、不做动态协议注册 |
| 非目标 | 不分析实时通信日志、串口收发缓冲区或文件传输状态 |
| 非目标 | 不改变现有连接、发送、接收和会话保存主流程 |

## 推荐方案

采用“核心 Builder + 只读对话框 + 主窗口入口”的三层方案。

| 层 | 职责 |
|---|---|
| Core | 负责从 `ProtocolDescriptor`、当前配置和版本信息生成稳定诊断 JSON |
| UI Dialog | 负责展示用户可读摘要、校验状态和完整 JSON 文本 |
| MainWindow | 负责收集当前协议状态并打开诊断对话框 |

这样可以让诊断数据生成逻辑不依赖界面控件。后续 Lua、插件协议或自动化测试也能复用同一套 JSON 构建逻辑。

## 诊断 JSON 结构

顶层结构使用 `diagnosticVersion` 固定版本号，后续字段变化时可以兼容旧诊断包。

```json
{
  "diagnosticVersion": 1,
  "generatedAt": "2026-06-02T12:30:00+08:00",
  "application": {},
  "protocol": {},
  "capabilities": {},
  "configuration": {},
  "validation": {}
}
```

| 模块 | 字段 | 说明 |
|---|---|---|
| `application` | `appName`、`appVersion`、`buildDate` | 定位用户使用的程序版本和构建日期 |
| `protocol` | `id`、`displayName`、`description`、`category`、`legacyType` | 对应注册中心中的协议描述 |
| `capabilities` | `builtin`、`plotProtocol`、`frameBuilder` | 当前协议能力标志 |
| `configuration` | `configVersion`、`schemaVersion`、`fieldCount`、`schemaFields`、`defaultConfig`、`currentConfig`、`normalizedConfig` | 协议配置事实源与当前配置 |
| `validation` | `valid`、`errors`、`warnings` | 当前配置的 Schema 校验结果 |

## Schema 字段导出

`schemaFields` 按 `ProtocolConfigSchema::fields` 原始顺序输出。

| 字段 | 说明 |
|---|---|
| `key` | 稳定配置键名 |
| `displayName` | 用户可见字段名 |
| `description` | 字段说明 |
| `type` | 字段类型字符串，例如 `Bool`、`Integer`、`BytesHex` |
| `defaultValue` | 字段默认值 |
| `minValue` | 数值最小值，没有则为空 |
| `maxValue` | 数值最大值，没有则为空 |
| `enumValues` | 枚举可选项 |
| `required` | 是否必填 |

## UI 设计

第一版对话框保持只读，避免和 4.3 的协议配置入口职责混淆。

| 区域 | 展示内容 |
|---|---|
| 协议概览 | 协议名称、稳定 ID、分类、描述 |
| 能力 | 内置协议、绘图协议、构帧能力 |
| 配置摘要 | 配置版本、Schema 版本、字段数量、当前配置是否校验通过 |
| 校验结果 | 通过时显示成功；存在错误或警告时逐行列出 |
| JSON 诊断 | 只读文本框展示格式化 JSON |
| 操作按钮 | `复制 JSON`、`保存 JSON...`、`关闭` |

入口放在汉堡菜单的“工具”菜单中：

| 入口 | 原因 |
|---|---|
| `工具 -> 协议诊断...` | 与协议配置同级，但职责是查看和导出，不承担修改配置 |

## 数据流

| 步骤 | 行为 |
|---|---|
| 1 | 用户点击 `工具 -> 协议诊断...` |
| 2 | `MainWindow` 获取当前 `ProtocolType` 与 `ProtocolFactory::descriptor(m_currentProtocolType)` |
| 3 | 若当前协议实例存在，读取 `m_currentProtocol->config()`；否则使用 descriptor `defaultConfig` |
| 4 | Core Builder 调用 `descriptor.configSchema.validate(currentConfig)` |
| 5 | Builder 生成 `QJsonObject` 与格式化 JSON 字符串 |
| 6 | `ProtocolDiagnosticsDialog` 展示摘要、校验状态和 JSON |
| 7 | 用户可复制 JSON 或保存为 `.json` 文件 |

## 错误处理

| 场景 | 处理 |
|---|---|
| Raw 协议 | 正常展示，Schema 字段数量为 0，配置为空或默认配置 |
| 空 Schema 协议 | 正常展示，字段数量为 0，校验结果为通过 |
| 当前配置非法 | 对话框仍打开，摘要显示校验失败，JSON 保留 `errors` |
| 保存失败 | 弹出错误提示，不关闭对话框 |
| 剪贴板不可用 | 保留 JSON 文本框，用户仍可手动复制 |
| 未知协议描述 | 输出 `unknown` 风格的兜底字段，避免崩溃 |

## 测试计划

| 测试 | 覆盖点 |
|---|---|
| Builder 导出基础信息 | 协议 ID、名称、分类、能力和版本信息存在 |
| Builder 导出 Schema 字段 | 字段 key、类型、默认值、范围、枚举和 required 正确 |
| Builder 导出配置 | `defaultConfig`、`currentConfig`、`normalizedConfig` 正确 |
| Builder 导出非法配置校验 | 非法配置时 `validation.valid=false` 且保留错误 |
| Raw/空 Schema 导出 | 没有字段也能导出并通过校验 |
| Dialog 基础行为 | JSON 文本只读，复制和保存入口存在 |
| 构建验证 | `ComAssistant_tests`、`ctest`、主程序 Release 构建 |
| 静态检查 | `git diff --check` |

## 文档影响

| 文件 | 处理 |
|---|---|
| `resources/help/protocols.html` | 增加协议诊断入口、诊断 JSON 用途和导出说明 |
| `resources/help/quickstart.html` | 第一版不改变快速连接、发送、接收、会话保存快捷键和主流程，预计不需要更新；最终实现后再次确认 |
| `.recallloom/` | 完成设计、计划或实现后同步更新当前状态和里程碑记录 |

## 风险与边界

| 风险 | 缓解 |
|---|---|
| 诊断 UI 与配置 UI 职责混淆 | 诊断对话框只读，不提供编辑入口 |
| JSON 字段未来变化影响排障脚本 | 使用 `diagnosticVersion` 标记版本 |
| `QVariant` 到 JSON 的类型转换不稳定 | 统一通过 Qt JSON 转换，测试覆盖数值、字符串、布尔、数组和空值 |
| 对话框过度复杂 | 第一版只展示摘要和完整 JSON，不做实时刷新、不做日志分析 |
| 空协议显示尴尬 | Raw 和空 Schema 明确显示“无配置字段，但诊断仍可导出” |

## 后续路线

| 小阶段 | 内容 |
|---|---|
| 4.4+ | 字段级错误定位、诊断包附带最近会话摘要 |
| 4.5 | Lua 安全沙箱设计，让 Lua 协议通过同一 descriptor/schema 暴露能力 |
| 4.6 | 外部插件注册 descriptor、schema、默认配置和诊断字段 |
