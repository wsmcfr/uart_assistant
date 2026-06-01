# 4.1 协议注册中心基座设计

## 背景

ComAssistant 当前协议层以 `IProtocol` 和 `ProtocolFactory` 为核心，内置协议通过 `ProtocolType` 枚举和 `switch` 分支创建。这个结构对现有功能足够稳定，但后续第四阶段要支持配置 schema、Lua 协议扩展、外部插件和诊断包时，需要一个统一的协议能力目录，能够描述“有哪些协议、它们能做什么、如何创建、默认配置是什么”。

本阶段目标是先搭建协议插件化基座，不急于引入 DLL 动态加载或 Lua 协议执行。第一版只把现有内置协议登记为可描述、可查询、可创建的能力，保证旧 API 行为不变，并为后续扩展留出稳定入口。

## 目标与非目标

| 类型 | 内容 |
|---|---|
| 目标 | 新增协议描述结构，记录协议 ID、名称、分类、能力标志、默认配置和旧版 `ProtocolType` 映射 |
| 目标 | 新增协议注册中心，支持注册、查询、列出、分类筛选、按 ID 创建协议 |
| 目标 | 将现有内置协议统一注册到注册中心，并保持 `ProtocolFactory` 旧接口兼容 |
| 目标 | 用单元测试覆盖注册完整性、重复注册拒绝、按 ID 创建、Raw 兼容和绘图协议元数据 |
| 非目标 | 本阶段不做 Qt `QPluginLoader` 动态插件加载 |
| 非目标 | 本阶段不执行 Lua 自定义协议脚本 |
| 非目标 | 本阶段不改变主窗口、绘图窗口或协议选择 UI 的操作路径 |

## 架构设计

| 模块 | 职责 |
|---|---|
| `ProtocolDescriptor` | 描述一个协议能力，包括稳定 ID、显示名称、说明、分类、是否内置、是否绘图、是否支持构帧、默认配置和旧版类型映射 |
| `ProtocolRegistry` | 维护协议描述与创建器，负责注册协议、拒绝非法注册、按 ID 创建实例、列出全部协议和按分类筛选 |
| `ProtocolFactory` | 继续作为旧代码入口，内部逐步委托给 `ProtocolRegistry`，保证 `create(ProtocolType)`、`supportedTypes()`、`typeName()` 不破坏 |
| 内置协议注册逻辑 | 在注册中心第一次访问时懒加载内置协议，避免要求应用启动时显式初始化 |

### 协议描述

`ProtocolDescriptor` 建议包含以下字段：

| 字段 | 含义 |
|---|---|
| `id` | 稳定协议 ID，例如 `raw`、`ascii`、`hex`、`modbus`、`plot.text` |
| `displayName` | 面向用户显示的名称，例如 `ASCII`、`TEXT绘图` |
| `description` | 协议用途说明，用于后续帮助、诊断包或配置 UI |
| `category` | 协议分类，例如基础协议、工业协议、自定义协议、绘图协议 |
| `legacyType` | 可选的旧版 `ProtocolType` 映射，用于兼容旧 API |
| `builtin` | 是否为内置协议 |
| `plotProtocol` | 是否支持绘图数据解析 |
| `frameBuilder` | 是否支持通过 payload 构建发送帧 |
| `defaultConfig` | 默认配置，后续配置 schema 可以基于它扩展 |

### 注册中心行为

`ProtocolRegistry` 的第一版行为保持简单：

| 行为 | 设计 |
|---|---|
| 初始化 | 首次使用时注册全部内置协议 |
| 注册 | ID 不能为空，创建器不能为空；重复 ID 拒绝覆盖，返回失败 |
| 查询 | 支持按 ID 查询描述，未知 ID 返回空结果 |
| 创建 | 支持按 ID 创建协议实例，未知 ID 或 Raw 返回 `nullptr` |
| 列表 | 返回稳定顺序的协议描述，顺序与当前 `supportedTypes()` 基本一致 |
| 分类 | 支持按分类筛选，为后续 UI 和诊断包预留入口 |

### Raw 兼容

Raw 当前代表“无协议”，旧行为中 `ProtocolFactory::create(ProtocolType::Raw)` 返回 `nullptr`。注册中心仍应登记 Raw 的描述，让能力列表完整，但按 ID 创建 Raw 时继续返回 `nullptr`，避免破坏旧调用方对 Raw 的判断逻辑。

## 数据流

| 场景 | 流程 |
|---|---|
| 旧代码按枚举创建协议 | 调用 `ProtocolFactory::create(type)` → 映射到协议 ID → 委托 `ProtocolRegistry::create(id, parent)` |
| 新代码查询协议能力 | 调用 `ProtocolRegistry::descriptors()` → 获取协议元数据 → 用于配置、诊断或后续 UI |
| 新代码按 ID 创建协议 | 调用 `ProtocolRegistry::create(id, parent)` → 使用注册的创建器生成 `IProtocol` 实例 |
| 后续插件扩展 | 插件或 Lua 适配层构造 `ProtocolDescriptor` 和创建器 → 注册到同一个 `ProtocolRegistry` |

## 错误处理

| 情况 | 处理方式 |
|---|---|
| 空协议 ID | 注册失败，并返回明确错误信息 |
| 空创建器 | 非 Raw 协议注册失败，并返回明确错误信息 |
| 重复 ID | 注册失败，不覆盖已有协议 |
| 未知 ID | 查询返回空描述，创建返回 `nullptr` |
| Raw 创建 | 返回 `nullptr`，这是兼容行为，不视为错误 |

本阶段不引入异常，继续使用 Qt/C++17 风格的布尔返回、空指针和错误字符串表达失败。

## 测试计划

| 测试 | 覆盖点 |
|---|---|
| `TestProtocolRegistry::builtinDescriptorsAreRegistered` | 内置协议数量、ID、显示名、分类和旧版类型映射完整 |
| `TestProtocolRegistry::rejectsInvalidRegistrations` | 空 ID、空创建器、重复 ID 都会被拒绝 |
| `TestProtocolRegistry::createsProtocolById` | ASCII、HEX、Modbus、绘图协议可以按 ID 创建 |
| `TestProtocolRegistry::keepsRawCompatibility` | Raw 出现在描述列表中，但创建结果仍为 `nullptr` |
| `TestProtocolRegistry::marksPlotProtocols` | TEXT、STAMP、CSV、JustFloat 标记为绘图协议 |
| `TestProtocolRegistry::keepsFactoryCompatibility` | `ProtocolFactory::supportedTypes()`、`typeName()`、`create(ProtocolType)` 行为不变 |

测试遵循 TDD：先写失败测试，确认失败原因是注册中心缺失，再实现最小代码让测试通过。

## 文档影响

| 文件 | 处理 |
|---|---|
| `resources/help/protocols.html` | 更新协议能力说明，表达协议层已平台化，为后续扩展预留 |
| `docs/user-guide/scripting.md` | 如果实现阶段没有改 Lua API，则不需要更新；若提到未来 Lua 协议扩展，只作为路线说明，不写成已支持 |
| `resources/help/quickstart.html` | 本阶段不改变快速上手入口、按钮名称或操作流程，预计不需要更新 |

## 后续路线

| 小阶段 | 依赖关系 |
|---|---|
| 4.2 配置 Schema | 将协议默认配置、用户配置和迁移规则绑定到协议 ID |
| 4.3 Lua 安全沙箱 | Lua 协议扩展通过注册中心登记能力，但执行环境由沙箱控制 |
| 4.4 外部插件 | Qt 插件只负责向注册中心登记协议描述和创建器 |
| 4.5 诊断包 | 导出协议注册表、配置 schema、日志和环境信息 |

