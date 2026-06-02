# 4.3 协议配置 UI 设计

## 背景

4.1 已建立协议注册中心，4.2 已建立协议配置 Schema、默认配置、配置校验、协议实例应用和会话持久化。当前缺口是：这些配置能力仍主要停留在核心层，用户无法在界面中查看、编辑和校验协议配置。

主窗口目前的“绘图协议”菜单偏向绘图数据解析选择，而 4.2 的 Schema 已覆盖 ASCII、HEX、Modbus、Custom、EasyHEX 等更通用协议。因此 4.3 不重做全局协议体系，而是先新增一个 Schema 驱动的配置编辑入口，让现有协议配置变成可见、可调、可随会话保存恢复。

## 目标与非目标

| 类型 | 内容 |
|---|---|
| 目标 | 新增可复用 `ProtocolConfigEditor`，按 `ProtocolConfigSchema` 自动生成配置控件 |
| 目标 | 新增 `ProtocolConfigDialog`，提供查看、编辑、校验、恢复默认值和确认应用 |
| 目标 | 在主窗口菜单提供“协议配置...”入口，打开当前协议的配置对话框 |
| 目标 | 配置确认后通过 Schema 校验并应用到 `m_currentProtocol`，会话保存继续复用 4.2 的 `protocolConfig` |
| 目标 | 对空 Schema 或 Raw 协议给出清晰提示，不制造空白对话框 |
| 非目标 | 不重构“绘图协议”菜单为全局协议选择器 |
| 非目标 | 不实现插件协议配置 UI、Lua 协议执行或外部 DLL 插件加载 |
| 非目标 | 不做复杂嵌套字段、数组字段或条件显示逻辑 |
| 非目标 | 不改变现有串口/TCP/UDP/HID 连接流程 |

## 推荐方案

采用“可复用编辑器 + 对话框 + 主窗口入口”的三层方案。

| 层 | 职责 |
|---|---|
| `ProtocolConfigEditor` | 只负责根据 Schema 创建控件、加载配置、读取配置、显示错误和恢复默认值 |
| `ProtocolConfigDialog` | 负责包装编辑器、显示协议名称/说明、提供确认/取消/恢复默认值按钮 |
| `MainWindow` | 负责提供菜单入口、把当前协议 descriptor/config 传入对话框、确认后应用配置 |

这样 UI 生成逻辑不塞进 `MainWindow`，未来 Lua/插件协议也能复用同一个编辑器。

## 控件映射

| Schema 类型 | UI 控件 | 行为 |
|---|---|---|
| `Bool` | `QCheckBox` | 勾选表示 `true` |
| `Integer` | `QSpinBox` | 使用 `minValue` / `maxValue`，缺失时用保守范围 |
| `Double` | `QDoubleSpinBox` | 使用 `minValue` / `maxValue`，保留合理小数精度 |
| `String` | `QLineEdit` | 普通文本输入 |
| `BytesHex` | `QLineEdit` | 用户可输入 `AA 55`、`aa-55` 等，确认时由 Schema 规范化 |
| `Enum` | `QComboBox` | 下拉选项来自 `enumValues` |

每个字段使用 `displayName` 作为标签，`description` 作为 tooltip。字段控件的 `objectName` 使用 `protocolConfig_<key>`，便于单元测试定位。

## 数据流

| 步骤 | 行为 |
|---|---|
| 打开对话框 | `MainWindow` 取当前协议 `ProtocolFactory::descriptor(m_currentProtocolType)` |
| 初始配置 | 优先使用 `m_currentProtocol->config()`；若无实例或配置为空，则使用 descriptor `defaultConfig` |
| 编辑配置 | `ProtocolConfigEditor` 生成控件并加载配置 |
| 点击确定 | 编辑器读取控件值，调用 `schema.validate()` |
| 校验通过 | 使用 `normalizedConfig` 更新当前协议实例，并在主窗口保留为当前配置 |
| 校验失败 | 对话框显示错误，不关闭 |
| 保存会话 | 继续走现有 `SessionData.protocolConfig` 持久化 |
| 加载会话 | 继续走现有 `MainWindow::applySessionDataToUi()` 应用配置 |

Raw 协议和空 Schema 协议不会强行生成表单。对话框显示“当前协议没有可配置项”，确认按钮可关闭，恢复默认值按钮禁用。

## 主窗口入口

第一版入口放在汉堡菜单的“工具”菜单中：

| 入口 | 原因 |
|---|---|
| `工具 -> 协议配置...` | 与设置、工具箱、Modbus 分析等高级配置入口同级，避免误解为只配置绘图协议 |

现有“绘图 -> 绘图协议”菜单暂不修改。原因是该菜单当前只影响绘图数据解析协议，直接塞入 ASCII/EasyHEX 配置会让用户误以为它控制所有串口发送/接收协议。

## 错误处理

| 场景 | 处理 |
|---|---|
| 当前协议是 Raw | 显示无可配置项 |
| 当前协议无 Schema 字段 | 显示无可配置项 |
| 字段输入类型不合法 | `schema.validate()` 返回错误，对话框显示错误摘要 |
| BytesHex 输入奇数长度或非法字符 | 阻止关闭，并提示对应 key |
| 用户点击恢复默认 | 编辑器加载 `schema.defaults()` |
| 协议实例不存在 | 对话框仍可显示默认配置；确认后若仍无实例，只更新主窗口待保存配置不强行创建 Raw |

第一版不做逐字段红框样式，避免引入复杂样式和翻译负担。先在对话框底部显示错误列表，后续可升级为字段级提示。

## 测试计划

| 测试 | 覆盖点 |
|---|---|
| `TestProtocolConfigEditor::buildsWidgetsFromSchema` | Bool、Integer、String、BytesHex、Enum 控件生成和 objectName |
| `TestProtocolConfigEditor::loadsAndReadsConfig` | 加载配置后读取出的 `QVariantMap` 与输入一致 |
| `TestProtocolConfigEditor::restoresDefaults` | 恢复默认值后读取结果等于 schema 默认配置 |
| `TestProtocolConfigEditor::reportsValidationErrors` | 非法 BytesHex 或枚举值会返回错误并不产生有效配置 |
| `TestProtocolConfigDialog::acceptsNormalizedConfig` | 对话框确认后返回规范化配置，例如 `aa-55` -> `AA 55` |
| `TestMainWindow...` 或轻量源码测试 | 菜单包含“协议配置...”入口，避免语言切换时丢失入口 |

UI 测试优先直接实例化 `ProtocolConfigEditor` 和 `ProtocolConfigDialog`，避免启动完整主窗口造成测试脆弱。

## 文档影响

| 文件 | 处理 |
|---|---|
| `resources/help/protocols.html` | 增加协议配置入口、Schema 校验和恢复默认说明 |
| `resources/help/quickstart.html` | 如果只新增高级入口，不改变快速连接/发送/接收流程，可不更新；最终说明中写明理由 |

## 风险与边界

| 风险 | 缓解 |
|---|---|
| 自动生成 UI 过早复杂化 | 第一版只支持 4.2 已定义的 6 种字段类型 |
| 主窗口协议概念混淆 | 不改“绘图协议”菜单，只新增“工具 -> 协议配置...”入口 |
| 对话框应用后没有协议实例 | Raw/空 Schema 只展示提示；非 Raw 正常已有实例或可在协议切换时创建 |
| 测试 UI 容易脆弱 | 核心测试放在编辑器和对话框，不依赖完整主窗口布局 |
| 翻译词条增加 | 中文源文本先进入代码，翻译完整性测试如要求英文再补 `.ts` |

## 后续路线

| 小阶段 | 内容 |
|---|---|
| 4.4 协议能力诊断导出 | 导出当前协议 ID、Schema、默认配置、当前配置和校验结果 |
| 4.5 Lua 安全沙箱 | Lua 协议通过同一 Schema 描述可配置项 |
| 4.6 外部插件 | 插件注册 descriptor、schema、默认配置和创建器 |
