/**
 * @file ProtocolRegistry.cpp
 * @brief 协议注册中心实现
 */

#include "ProtocolRegistry.h"

#include "AsciiProtocol.h"
#include "CsvProtocol.h"
#include "CustomProtocol.h"
#include "EasyHexProtocol.h"
#include "HexProtocol.h"
#include "JustFloatProtocol.h"
#include "LuaScriptProtocol.h"
#include "ModbusProtocol.h"
#include "StampProtocol.h"
#include "TextProtocol.h"

namespace ComAssistant {

namespace {

/**
 * @brief 构造协议描述
 * @param id 稳定协议 ID
 * @param displayName 用户可见协议名称
 * @param description 协议说明
 * @param category 协议分类
 * @param legacyType 旧版协议枚举映射
 * @param plotProtocol 是否为绘图协议
 * @param frameBuilder 是否支持构帧
 * @param configSchema 协议配置 Schema
 * @return 填充好的协议描述
 *
 * 内置协议注册时有大量重复字段，通过该辅助函数集中初始化，
 * 后续增加默认配置或能力标志时不需要在每个注册点重复样板代码。
 */
ProtocolDescriptor makeDescriptor(const QString& id,
                                  const QString& displayName,
                                  const QString& description,
                                  ProtocolCategory category,
                                  ProtocolType legacyType,
                                  bool plotProtocol,
                                  bool frameBuilder,
                                  const ProtocolConfigSchema& configSchema)
{
    ProtocolDescriptor descriptor;
    descriptor.id = id;
    descriptor.displayName = displayName;
    descriptor.description = description;
    descriptor.category = category;
    descriptor.legacyType = legacyType;
    descriptor.builtin = true;
    descriptor.plotProtocol = plotProtocol;
    descriptor.frameBuilder = frameBuilder;
    descriptor.scriptProtocol = false;
    descriptor.creatable = legacyType != ProtocolType::Raw;
    descriptor.legacyCompatible = true;
    descriptor.configVersion = configSchema.version;
    descriptor.configSchema = configSchema;
    descriptor.defaultConfig = configSchema.defaults();
    return descriptor;
}

/**
 * @brief 创建空配置 Schema
 * @return 不包含字段的 Schema，适用于 Raw 和暂未暴露配置的绘图协议
 */
ProtocolConfigSchema makeEmptySchema()
{
    ProtocolConfigSchema schema;
    schema.version = 1;
    return schema;
}

/**
 * @brief 创建 ASCII 协议配置 Schema
 * @return ASCII 协议默认配置和字段定义
 */
ProtocolConfigSchema makeAsciiSchema()
{
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("lineEnding"),
        QStringLiteral("行结束符"),
        QStringLiteral("CRLF"),
        QStringList{QStringLiteral("None"), QStringLiteral("CR"), QStringLiteral("LF"), QStringLiteral("CRLF")},
        QStringLiteral("发送时追加的行结束符")));
    schema.fields.append(ProtocolConfigField::boolean(
        QStringLiteral("appendLineEnding"),
        QStringLiteral("追加行结束符"),
        true,
        QStringLiteral("发送文本时是否自动追加行结束符")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("timeoutMs"),
        QStringLiteral("分帧超时"),
        100,
        1,
        60000,
        QStringLiteral("无行结束符时的分帧超时时间")));
    schema.fields.append(ProtocolConfigField::string(
        QStringLiteral("encoding"),
        QStringLiteral("编码"),
        QStringLiteral("UTF-8"),
        QStringLiteral("文本编码")));
    return schema;
}

/**
 * @brief 创建 HEX 协议配置 Schema
 * @return HEX 协议默认配置和字段定义
 */
ProtocolConfigSchema makeHexSchema()
{
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::bytesHex(
        QStringLiteral("frameHead"),
        QStringLiteral("帧头"),
        QString(),
        QStringLiteral("帧头字节")));
    schema.fields.append(ProtocolConfigField::bytesHex(
        QStringLiteral("frameTail"),
        QStringLiteral("帧尾"),
        QString(),
        QStringLiteral("帧尾字节")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("lengthFieldOffset"),
        QStringLiteral("长度字段偏移"),
        -1,
        -1,
        65535,
        QStringLiteral("长度字段在帧中的偏移，-1 表示不使用")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("lengthFieldSize"),
        QStringLiteral("长度字段大小"),
        1,
        1,
        4,
        QStringLiteral("长度字段字节数")));
    schema.fields.append(ProtocolConfigField::boolean(
        QStringLiteral("lengthBigEndian"),
        QStringLiteral("长度大端序"),
        true,
        QStringLiteral("长度字段是否使用大端序")));
    schema.fields.append(ProtocolConfigField::boolean(
        QStringLiteral("useChecksum"),
        QStringLiteral("使用校验"),
        false,
        QStringLiteral("是否启用帧校验")));
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("checksumType"),
        QStringLiteral("校验算法"),
        QStringLiteral("Sum8"),
        QStringList{QStringLiteral("Sum8"), QStringLiteral("Sum16"), QStringLiteral("XOR"), QStringLiteral("CRC8"), QStringLiteral("CRC16"), QStringLiteral("CRC32")},
        QStringLiteral("校验和算法")));
    return schema;
}

/**
 * @brief 创建 Modbus 协议配置 Schema
 * @return Modbus 协议默认配置和字段定义
 */
ProtocolConfigSchema makeModbusSchema()
{
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("mode"),
        QStringLiteral("模式"),
        QStringLiteral("RTU"),
        QStringList{QStringLiteral("RTU"), QStringLiteral("ASCII")},
        QStringLiteral("Modbus 帧格式")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("slaveAddress"),
        QStringLiteral("从站地址"),
        1,
        1,
        247,
        QStringLiteral("默认从站地址")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("responseTimeoutMs"),
        QStringLiteral("响应超时"),
        1000,
        1,
        60000,
        QStringLiteral("响应超时时间")));
    return schema;
}

/**
 * @brief 创建 Custom 协议配置 Schema
 * @return Custom 协议默认配置和字段定义
 */
ProtocolConfigSchema makeCustomSchema()
{
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("delimiter"),
        QStringLiteral("分帧方式"),
        QStringLiteral("None"),
        QStringList{QStringLiteral("None"), QStringLiteral("FixedLength"), QStringLiteral("Timeout"), QStringLiteral("StartEnd"), QStringLiteral("LengthField")},
        QStringLiteral("通用帧解析方式")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("maxFrameSize"),
        QStringLiteral("最大帧长度"),
        65536,
        1,
        1048576,
        QStringLiteral("允许的最大帧长度")));
    schema.fields.append(ProtocolConfigField::boolean(
        QStringLiteral("useChecksum"),
        QStringLiteral("使用校验"),
        false,
        QStringLiteral("是否启用校验")));
    return schema;
}

/**
 * @brief 创建 EasyHEX 协议配置 Schema
 * @return EasyHEX 协议默认配置和字段定义
 */
ProtocolConfigSchema makeEasyHexSchema()
{
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::bytesHex(
        QStringLiteral("frameHeader"),
        QStringLiteral("帧头"),
        QStringLiteral("AA 55"),
        QStringLiteral("帧头字节")));
    schema.fields.append(ProtocolConfigField::bytesHex(
        QStringLiteral("frameTail"),
        QStringLiteral("帧尾"),
        QString(),
        QStringLiteral("帧尾字节")));
    schema.fields.append(ProtocolConfigField::boolean(
        QStringLiteral("useChecksum"),
        QStringLiteral("使用校验"),
        true,
        QStringLiteral("是否启用校验")));
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("checksumType"),
        QStringLiteral("校验算法"),
        QStringLiteral("SUM8"),
        QStringList{QStringLiteral("SUM8"), QStringLiteral("XOR8"), QStringLiteral("CRC8")},
        QStringLiteral("校验算法")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("lengthFieldOffset"),
        QStringLiteral("长度字段偏移"),
        2,
        -1,
        65535,
        QStringLiteral("长度字段在帧中的偏移，-1 表示不使用")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("lengthFieldSize"),
        QStringLiteral("长度字段大小"),
        1,
        1,
        2,
        QStringLiteral("长度字段字节数")));
    return schema;
}

/**
 * @brief 创建 Lua 脚本协议配置 Schema
 * @return Lua 协议第一版默认配置和字段定义
 *
 * 4.9 只把 Lua 协议登记到 descriptor/schema/diagnostics 事实源，不执行脚本、
 * 不加载外部文件，也不开放接收 API。这里的字段用于后续协议解析器接入前
 * 先稳定配置键名、默认值和诊断输出。
 */
ProtocolConfigSchema makeLuaScriptSchema()
{
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::string(
        QStringLiteral("scriptSource"),
        QStringLiteral("脚本源码"),
        QString(),
        QStringLiteral("内联 Lua 脚本文本；当前阶段仅保存和诊断，不执行")));
    schema.fields.append(ProtocolConfigField::string(
        QStringLiteral("scriptPath"),
        QStringLiteral("脚本路径"),
        QString(),
        QStringLiteral("外部 Lua 脚本路径占位；当前阶段不加载文件")));
    schema.fields.append(ProtocolConfigField::string(
        QStringLiteral("entryFunction"),
        QStringLiteral("入口函数"),
        QStringLiteral("process"),
        QStringLiteral("后续协议解析器调用的入口函数名称")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("timeoutMs"),
        QStringLiteral("执行超时"),
        1000,
        1,
        600000,
        QStringLiteral("Lua 沙箱单次执行超时时间")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("memoryLimitKb"),
        QStringLiteral("内存限制"),
        1024,
        64,
        65536,
        QStringLiteral("Lua state 内存预算，单位 KB")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("maxOutputLines"),
        QStringLiteral("最大输出行数"),
        200,
        0,
        10000,
        QStringLiteral("脚本 print 输出最多保留的行数")));
    schema.fields.append(ProtocolConfigField::boolean(
        QStringLiteral("allowCommunicationApi"),
        QStringLiteral("允许通信 API"),
        false,
        QStringLiteral("是否允许发送类通信 API；当前阶段仍不开放 serial.receive")));
    return schema;
}

/**
 * @brief 创建 Lua 脚本协议描述
 * @return 可创建最小解析器、但不参与旧版枚举链路的 Lua 协议描述
 */
ProtocolDescriptor makeLuaScriptDescriptor()
{
    ProtocolDescriptor descriptor;
    descriptor.id = QStringLiteral("lua.script");
    descriptor.displayName = QStringLiteral("Lua Script");
    descriptor.description = QStringLiteral("Lua 脚本协议解析器（process(data, context) 最小原型）");
    descriptor.category = ProtocolCategory::Custom;
    descriptor.legacyType = ProtocolType::Raw;
    descriptor.builtin = false;
    descriptor.plotProtocol = false;
    descriptor.frameBuilder = false;
    descriptor.scriptProtocol = true;
    descriptor.creatable = true;
    descriptor.legacyCompatible = false;
    descriptor.configSchema = makeLuaScriptSchema();
    descriptor.configVersion = descriptor.configSchema.version;
    descriptor.defaultConfig = descriptor.configSchema.defaults();
    return descriptor;
}

} // namespace

bool ProtocolRegistry::registerProtocol(const ProtocolDescriptor& descriptor,
                                        ProtocolCreator creator,
                                        QString* errorMessage)
{
    /*
     * Task 2 的最小实现先支持合法内置协议注册。
     * 更严格的空 ID、重复 ID、空创建器错误信息会在后续红测试中继续收紧。
     */
    if (descriptor.id.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("协议 ID 不能为空");
        }
        return false;
    }

    if (m_descriptors.contains(descriptor.id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("协议 ID 已注册: %1").arg(descriptor.id);
        }
        return false;
    }

    if (descriptor.creatable && !creator) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("协议创建器不能为空: %1").arg(descriptor.id);
        }
        return false;
    }

    m_orderedIds.append(descriptor.id);
    m_descriptors.insert(descriptor.id, descriptor);
    m_creators.insert(descriptor.id, creator);
    return true;
}

void ProtocolRegistry::registerBuiltinProtocols()
{
    /*
     * 内置协议按旧版 supportedTypes() 顺序注册，保证能力列表和旧 UI 顺序稳定。
     * 这里不能因为注册表已经有外部协议就直接返回：后续插件或脚本协议可能
     * 先登记自己的能力，然后再补齐内置协议。重复的内置 ID 会被 registerProtocol()
     * 拒绝并跳过，缺失的内置协议则继续注册。
     *
     * Raw 代表无协议，因此登记描述但不提供创建器。
     */
    registerProtocol(
        makeDescriptor(QStringLiteral("raw"),
                       QStringLiteral("Raw"),
                       QStringLiteral("原始数据（无协议）"),
                       ProtocolCategory::Basic,
                       ProtocolType::Raw,
                       false,
                       false,
                       makeEmptySchema()),
        ProtocolCreator());

    registerProtocol(
        makeDescriptor(QStringLiteral("ascii"),
                       QStringLiteral("ASCII"),
                       QStringLiteral("ASCII文本协议"),
                       ProtocolCategory::Basic,
                       ProtocolType::Ascii,
                       false,
                       true,
                       makeAsciiSchema()),
        [](QObject* parent) -> IProtocol* { return new AsciiProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("hex"),
                       QStringLiteral("HEX"),
                       QStringLiteral("十六进制协议"),
                       ProtocolCategory::Basic,
                       ProtocolType::Hex,
                       false,
                       true,
                       makeHexSchema()),
        [](QObject* parent) -> IProtocol* { return new HexProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("modbus"),
                       QStringLiteral("Modbus"),
                       QStringLiteral("Modbus RTU/ASCII工业通信协议"),
                       ProtocolCategory::Industrial,
                       ProtocolType::Modbus,
                       false,
                       true,
                       makeModbusSchema()),
        [](QObject* parent) -> IProtocol* { return new ModbusProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("custom"),
                       QStringLiteral("Custom"),
                       QStringLiteral("自定义协议"),
                       ProtocolCategory::Custom,
                       ProtocolType::Custom,
                       false,
                       true,
                       makeCustomSchema()),
        [](QObject* parent) -> IProtocol* { return new CustomProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("easyhex"),
                       QStringLiteral("EasyHEX"),
                       QStringLiteral("简易十六进制协议"),
                       ProtocolCategory::Custom,
                       ProtocolType::EasyHex,
                       false,
                       true,
                       makeEasyHexSchema()),
        [](QObject* parent) -> IProtocol* { return new EasyHexProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.text"),
                       QStringLiteral("TEXT绘图"),
                       QStringLiteral("TEXT绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::TextPlot,
                       true,
                       true,
                       makeEmptySchema()),
        [](QObject* parent) -> IProtocol* { return new TextProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.stamp"),
                       QStringLiteral("STAMP绘图"),
                       QStringLiteral("STAMP绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::StampPlot,
                       true,
                       true,
                       makeEmptySchema()),
        [](QObject* parent) -> IProtocol* { return new StampProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.csv"),
                       QStringLiteral("CSV绘图"),
                       QStringLiteral("CSV绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::CsvPlot,
                       true,
                       true,
                       makeEmptySchema()),
        [](QObject* parent) -> IProtocol* { return new CsvProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.justfloat"),
                       QStringLiteral("JustFloat"),
                       QStringLiteral("JustFloat二进制绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::JustFloat,
                       true,
                       true,
                       makeEmptySchema()),
        [](QObject* parent) -> IProtocol* { return new JustFloatProtocol(parent); });

    /*
     * Lua 脚本协议在 4.10 进入可创建的最小解析器阶段。它仍不参与
     * 旧版 ProtocolType 工作流，也不开放 serial.receive(timeout)，
     * 因此旧绘图协议菜单和旧会话枚举恢复链路不会被 Lua 协议污染。
     */
    registerProtocol(
        makeLuaScriptDescriptor(),
        [](QObject* parent) -> IProtocol* { return new LuaScriptProtocol(parent); });
}

bool ProtocolRegistry::contains(const QString& id) const
{
    return m_descriptors.contains(id);
}

ProtocolDescriptor ProtocolRegistry::descriptor(const QString& id) const
{
    return m_descriptors.value(id);
}

QList<ProtocolDescriptor> ProtocolRegistry::descriptors() const
{
    QList<ProtocolDescriptor> result;
    result.reserve(m_orderedIds.size());

    for (const QString& id : m_orderedIds) {
        result.append(m_descriptors.value(id));
    }

    return result;
}

QList<ProtocolDescriptor> ProtocolRegistry::descriptorsByCategory(ProtocolCategory category) const
{
    QList<ProtocolDescriptor> result;

    /*
     * 分类筛选保持原始注册顺序，避免 UI 分组或诊断包导出时因为筛选
     * 改变协议列表顺序，导致用户看到的协议排列不稳定。
     */
    for (const QString& id : m_orderedIds) {
        const ProtocolDescriptor protocolDescriptor = m_descriptors.value(id);
        if (protocolDescriptor.category == category) {
            result.append(protocolDescriptor);
        }
    }

    return result;
}

IProtocol* ProtocolRegistry::create(const QString& id, QObject* parent) const
{
    const ProtocolCreator creator = m_creators.value(id);
    if (!creator) {
        return nullptr;
    }

    return creator(parent);
}

} // namespace ComAssistant
