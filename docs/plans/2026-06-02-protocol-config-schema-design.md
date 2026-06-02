# 4.2 协议配置 Schema 与默认配置治理设计

## 背景

4.1 已把内置协议纳入 `ProtocolRegistry`，每个协议都有稳定 ID、分类、能力标志和 `defaultConfig` 预留字段。但当前协议配置仍然分散：

| 现状 | 问题 |
|---|---|
| `IProtocol` 只有通用 `QVariantMap config()` / `setConfig()` | 没有统一字段类型、默认值、范围校验或错误提示 |
| `ProtocolDescriptor::defaultConfig` 只是预留字段 | 后续 UI、Lua、插件和诊断包不知道配置项含义 |
| 会话文件只保存 `protocolType` | 用户协议配置无法随 `.cas` 文件恢复 |
| `CustomProtocol` / `EasyHexProtocol` 有结构体配置 | 和统一配置入口之间缺少转换与校验层 |

4.2 目标是做“核心配置治理”，先建立可测试、可序列化、可迁移的协议配置 Schema。第一版不做配置 UI、不做 Lua 执行、不做外部插件加载，只把后续扩展需要的配置事实源和会话兼容打牢。

## 目标与非目标

| 类型 | 内容 |
|---|---|
| 目标 | 新增协议配置字段 Schema，描述字段 key、显示名、类型、默认值、范围、枚举选项、是否必填和说明 |
| 目标 | 新增配置校验与默认值合并服务，输出规范化配置和明确错误列表 |
| 目标 | 让内置协议描述携带默认配置与 Schema，至少覆盖 ASCII、HEX、Modbus、Custom、EasyHEX 和绘图协议的基础默认值 |
| 目标 | 会话文件新增 `protocolId` 与 `protocolConfig`，并保留 `protocolType` 旧字段兼容 |
| 目标 | 新增迁移逻辑：旧会话缺少 `protocolId` 时从 `protocolType` 映射；缺少 `protocolConfig` 时使用默认配置 |
| 非目标 | 本阶段不做可视化协议配置面板 |
| 非目标 | 本阶段不实现 Lua 协议或 DLL 插件加载 |
| 非目标 | 本阶段不重写各协议的全部私有结构体配置解析，只建立统一外层模型和关键协议适配入口 |

## 方案

### 核心数据结构

新增 `ProtocolConfigSchema.h/.cpp`，提供三个核心结构：

| 结构 | 职责 |
|---|---|
| `ProtocolConfigField` | 描述单个配置字段，包括 key、类型、默认值、范围、枚举选项和说明 |
| `ProtocolConfigSchema` | 描述一个协议的配置版本、字段列表和是否允许未知字段 |
| `ProtocolConfigValidationResult` | 返回规范化后的配置、错误列表和警告列表 |

字段类型第一版保持克制：

| 类型 | 说明 |
|---|---|
| `Bool` | 布尔值 |
| `Integer` | 整数，可配置最小值/最大值 |
| `Double` | 浮点数，可配置最小值/最大值 |
| `String` | 字符串 |
| `BytesHex` | 十六进制字符串，规范化为大写空格分隔文本，例如 `AA 55` |
| `Enum` | 字符串枚举，例如 `CRLF`、`RTU`、`SUM8` |

不直接在 Schema 中存 `QByteArray` 或复杂嵌套对象，避免会话 JSON、帮助文档和未来 Lua/插件边界处理困难。需要二进制字段时使用 `BytesHex`。

### Schema 与 Descriptor 的关系

`ProtocolDescriptor` 新增：

| 字段 | 含义 |
|---|---|
| `int configVersion` | 当前协议配置版本，第一版为 `1` |
| `ProtocolConfigSchema configSchema` | 配置字段定义 |

保留现有 `defaultConfig` 字段，但它不再只是预留字段，而是由 `configSchema.defaults()` 生成或与 schema 保持一致。这样旧调用方仍能只看 `defaultConfig`，新代码则优先使用 `configSchema`。

### 配置校验流程

新增 `ProtocolConfigService` 或在 `ProtocolConfigSchema` 中提供静态校验函数。流程如下：

| 步骤 | 行为 |
|---|---|
| 读取 Schema | 通过 `ProtocolFactory::descriptor(type)` 或 `ProtocolRegistry::descriptor(id)` 获取 |
| 合并默认值 | 对缺失字段写入字段默认值 |
| 类型校验 | 按字段类型检查 `QVariant` 类型或可转换性 |
| 范围校验 | 对整数/浮点字段检查最小值和最大值 |
| 枚举校验 | 对枚举字段检查值是否在允许集合内 |
| 字节规范化 | 对 `BytesHex` 去除非十六进制分隔字符，转为大写空格分隔 |
| 未知字段处理 | 第一版默认保留未知字段为 warning，避免未来插件或旧文件被硬拒绝 |

如果存在错误，返回 `valid=false` 和错误列表；调用方可以选择拒绝应用配置。缺失字段不算错误，因为会补默认值。

### 内置协议默认配置

第一版默认配置建议：

| 协议 ID | 默认配置 |
|---|---|
| `raw` | 空配置 |
| `ascii` | `lineEnding=CRLF`、`appendLineEnding=true`、`timeoutMs=100`、`encoding=UTF-8` |
| `hex` | `frameHead=""`、`frameTail=""`、`lengthFieldOffset=-1`、`lengthFieldSize=1`、`lengthBigEndian=true`、`useChecksum=false`、`checksumType=Sum8` |
| `modbus` | `mode=RTU`、`slaveAddress=1`、`responseTimeoutMs=1000` |
| `custom` | `delimiter=None`、`maxFrameSize=65536`、`useChecksum=false` |
| `easyhex` | `frameHeader=AA 55`、`frameTail=""`、`useChecksum=true`、`checksumType=SUM8`、`lengthFieldOffset=2`、`lengthFieldSize=1` |
| 绘图协议 | 第一版配置为空或只保留 `windowId=""`，避免过早承诺 UI 行为 |

### 应用到协议实例

4.2 第一版只要求 `ProtocolFactory::create(type)` 创建实例后能应用规范化配置，新增一个安全入口：

```cpp
static std::unique_ptr<IProtocol> create(ProtocolType type, const QVariantMap& config);
static IProtocol* create(ProtocolType type, const QVariantMap& config, QObject* parent);
```

内部流程：

1. 通过 `typeId(type)` 找到协议 ID。
2. 用 descriptor 的 schema 校验并补默认值。
3. 创建协议实例。
4. 调用 `protocol->setConfig(normalizedConfig)`。

为了避免大范围改协议行为，本阶段允许多数协议先只存储 `IProtocol::m_config`。但对 `EasyHexProtocol`、`AsciiProtocol`、`ModbusProtocol` 这种已有明确结构体配置的协议，计划逐步覆盖 `setConfig(const QVariantMap&)`，把统一配置转换到内部字段，让测试能证明配置真正生效。

### 会话文件兼容

`SessionData` 新增：

| 字段 | 说明 |
|---|---|
| `QString protocolId` | 稳定协议 ID，例如 `plot.text` |
| `QVariantMap protocolConfig` | 规范化协议配置 |
| `int protocolConfigVersion` | 配置版本，第一版为 `1` |

JSON 格式新增：

```json
{
  "protocolType": 1,
  "protocolId": "ascii",
  "protocolConfigVersion": 1,
  "protocolConfig": {
    "lineEnding": "CRLF",
    "appendLineEnding": true,
    "timeoutMs": 100,
    "encoding": "UTF-8"
  }
}
```

兼容策略：

| 场景 | 处理 |
|---|---|
| 新会话 | 同时写 `protocolType`、`protocolId`、`protocolConfigVersion`、`protocolConfig` |
| 旧会话无 `protocolId` | 通过 `ProtocolFactory::typeId(protocolType)` 补齐 |
| 旧会话无 `protocolConfig` | 使用对应协议 schema 默认配置 |
| `protocolId` 未知但 `protocolType` 有效 | 警告并回退到 `protocolType` 映射 |
| 配置校验失败 | 回退默认配置，保留 warning，避免加载会话失败 |

### 文档影响

| 文件 | 处理 |
|---|---|
| `resources/help/protocols.html` | 补充“协议配置会随会话保存，并按 schema 校验默认值”的说明 |
| `resources/help/quickstart.html` | 如果用户流程不变，不更新；若会话保存/加载说明中提到协议配置恢复，则同步更新 |
| `docs/user-guide/scripting.md` | 本阶段不更新，除非明确暴露给脚本 |

## 测试计划

| 测试 | 覆盖点 |
|---|---|
| `TestProtocolConfigSchema::defaultConfigMatchesSchema` | 每个内置协议的默认配置都能通过自己的 schema |
| `TestProtocolConfigSchema::rejectsInvalidValues` | 整数范围、枚举值、十六进制字段校验失败会给出错误 |
| `TestProtocolConfigSchema::normalizesHexBytes` | `aa55`、`AA 55`、`aa-55` 规范化为 `AA 55` |
| `TestProtocolConfigSchema::fillsMissingDefaults` | 缺失字段自动补默认值 |
| `TestProtocolRegistry::builtinDescriptorsExposeConfigSchema` | 注册中心能暴露配置版本、schema 和 defaultConfig |
| `TestProtocolFactory::createsProtocolWithValidatedConfig` | 带配置创建协议并应用规范化配置 |
| `TestSessionData::persistsProtocolIdAndConfig` | 会话 JSON 保存和读取新字段 |
| `TestSessionData::migratesLegacyProtocolTypeToProtocolId` | 旧会话只有 `protocolType` 时可迁移到协议 ID 和默认配置 |

## 风险与边界

| 风险 | 缓解 |
|---|---|
| 一次性适配所有协议内部结构体会扩大范围 | 第一版先建立 schema 和少数关键协议适配，其他协议至少保留规范化 `QVariantMap` |
| 会话加载时严格报错会影响旧用户文件 | 配置错误回退默认值并记录 warning，不让会话加载失败 |
| 未来插件需要更复杂嵌套配置 | 第一版保留未知字段 warning，不做复杂嵌套；后续插件阶段再扩展 schema 类型 |
| `EasyHexProtocol::type()` 当前返回 `ProtocolType::Custom` | 4.2 不顺手重构该历史问题，只用稳定协议 ID 解决配置识别 |

## 后续路线

| 小阶段 | 内容 |
|---|---|
| 4.3 协议配置 UI | 基于 schema 自动生成基础配置编辑面板 |
| 4.4 Lua 安全沙箱 | Lua 协议通过 schema 描述可配置项 |
| 4.5 外部插件 | 插件注册协议描述、schema、默认配置和创建器 |
| 4.6 诊断包 | 导出协议注册表、schema、当前配置和校验结果 |
