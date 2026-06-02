/**
 * @file ProtocolDiagnostics.cpp
 * @brief 协议能力诊断 JSON 构建器实现
 */

#include "ProtocolDiagnostics.h"

#include "version.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariant>

namespace ComAssistant {
namespace {

/**
 * @brief 把协议分类转换为稳定字符串。
 * @param category 协议描述中的分类枚举。
 * @return 用于诊断 JSON 的分类名称。
 *
 * JSON 使用字符串而不是整数，方便用户直接阅读和 Issue 中检索。
 */
QString protocolCategoryToString(ProtocolCategory category)
{
    switch (category) {
        case ProtocolCategory::Basic:
            return QStringLiteral("Basic");
        case ProtocolCategory::Industrial:
            return QStringLiteral("Industrial");
        case ProtocolCategory::Custom:
            return QStringLiteral("Custom");
        case ProtocolCategory::Plot:
            return QStringLiteral("Plot");
    }

    return QStringLiteral("Unknown");
}

/**
 * @brief 把旧版协议类型转换为稳定字符串。
 * @param type 旧版 ProtocolType 枚举。
 * @return 用于诊断 JSON 的旧协议类型名称。
 *
 * 旧枚举仍用于现有 UI 和会话兼容，因此诊断包保留它，方便定位迁移问题。
 */
QString protocolTypeToString(ProtocolType type)
{
    switch (type) {
        case ProtocolType::Raw:
            return QStringLiteral("Raw");
        case ProtocolType::Ascii:
            return QStringLiteral("Ascii");
        case ProtocolType::Hex:
            return QStringLiteral("Hex");
        case ProtocolType::Modbus:
            return QStringLiteral("Modbus");
        case ProtocolType::Custom:
            return QStringLiteral("Custom");
        case ProtocolType::TextPlot:
            return QStringLiteral("TextPlot");
        case ProtocolType::StampPlot:
            return QStringLiteral("StampPlot");
        case ProtocolType::CsvPlot:
            return QStringLiteral("CsvPlot");
        case ProtocolType::EasyHex:
            return QStringLiteral("EasyHex");
        case ProtocolType::JustFloat:
            return QStringLiteral("JustFloat");
    }

    return QStringLiteral("Unknown");
}

/**
 * @brief 把 Schema 字段类型转换为稳定字符串。
 * @param type 协议配置字段类型。
 * @return 用于诊断 JSON 的字段类型名称。
 */
QString configFieldTypeToString(ProtocolConfigFieldType type)
{
    switch (type) {
        case ProtocolConfigFieldType::Bool:
            return QStringLiteral("Bool");
        case ProtocolConfigFieldType::Integer:
            return QStringLiteral("Integer");
        case ProtocolConfigFieldType::Double:
            return QStringLiteral("Double");
        case ProtocolConfigFieldType::String:
            return QStringLiteral("String");
        case ProtocolConfigFieldType::BytesHex:
            return QStringLiteral("BytesHex");
        case ProtocolConfigFieldType::Enum:
            return QStringLiteral("Enum");
    }

    return QStringLiteral("Unknown");
}

/**
 * @brief 把字符串列表转换为 JSON 数组。
 * @param values 输入字符串列表。
 * @return 保持原顺序的 JSON 数组。
 */
QJsonArray stringListToJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

/**
 * @brief 把 QVariant 转换为 JSON 值。
 * @param value Qt 变体值。
 * @return 对应 JSON 值；空值返回空 JSON 值。
 *
 * 字段默认值、上下限可能是 int/double/bool/string，也可能为空。
 * 统一走 QVariant 转 JSON，避免手写类型分支遗漏后续字段类型。
 */
QJsonValue variantToJsonValue(const QVariant& value)
{
    if (!value.isValid()) {
        return QJsonValue();
    }
    return QJsonValue::fromVariant(value);
}

/**
 * @brief 导出单个 Schema 字段。
 * @param field 协议配置字段定义。
 * @return 包含字段 key、显示名、类型、默认值、范围、枚举和说明的 JSON。
 */
QJsonObject schemaFieldToJson(const ProtocolConfigField& field)
{
    QJsonObject object;
    object.insert(QStringLiteral("key"), field.key);
    object.insert(QStringLiteral("displayName"), field.displayName);
    object.insert(QStringLiteral("description"), field.description);
    object.insert(QStringLiteral("type"), configFieldTypeToString(field.type));
    object.insert(QStringLiteral("defaultValue"), variantToJsonValue(field.defaultValue));
    object.insert(QStringLiteral("minValue"), variantToJsonValue(field.minValue));
    object.insert(QStringLiteral("maxValue"), variantToJsonValue(field.maxValue));
    object.insert(QStringLiteral("enumValues"), stringListToJsonArray(field.enumValues));
    object.insert(QStringLiteral("required"), field.required);
    return object;
}

/**
 * @brief 导出 Schema 字段数组。
 * @param schema 协议配置 Schema。
 * @return 按 Schema 原始字段顺序排列的 JSON 数组。
 */
QJsonArray schemaFieldsToJson(const ProtocolConfigSchema& schema)
{
    QJsonArray fields;
    for (const ProtocolConfigField& field : schema.fields) {
        fields.append(schemaFieldToJson(field));
    }
    return fields;
}

/**
 * @brief 构建应用版本信息节点。
 * @return 包含应用名、版本号、完整版本和构建日期的 JSON。
 */
QJsonObject buildApplicationObject()
{
    QJsonObject application;
    application.insert(QStringLiteral("appName"), QStringLiteral(APP_NAME));
    application.insert(QStringLiteral("appVersion"), QStringLiteral(APP_VERSION));
    application.insert(QStringLiteral("appVersionString"), QStringLiteral(APP_VERSION_STRING));
    application.insert(QStringLiteral("buildDate"), QStringLiteral(APP_BUILD_DATE));
    return application;
}

/**
 * @brief 构建协议描述节点。
 * @param descriptor 当前协议描述。
 * @return 包含稳定 ID、名称、说明、分类和旧类型的 JSON。
 */
QJsonObject buildProtocolObject(const ProtocolDescriptor& descriptor)
{
    QJsonObject protocol;
    protocol.insert(QStringLiteral("id"), descriptor.id);
    protocol.insert(QStringLiteral("displayName"), descriptor.displayName);
    protocol.insert(QStringLiteral("description"), descriptor.description);
    protocol.insert(QStringLiteral("category"), protocolCategoryToString(descriptor.category));
    protocol.insert(QStringLiteral("legacyType"), protocolTypeToString(descriptor.legacyType));
    protocol.insert(QStringLiteral("legacyTypeValue"), static_cast<int>(descriptor.legacyType));
    return protocol;
}

/**
 * @brief 构建协议能力节点。
 * @param descriptor 当前协议描述。
 * @return 包含内置、绘图、构帧能力标志的 JSON。
 */
QJsonObject buildCapabilitiesObject(const ProtocolDescriptor& descriptor)
{
    QJsonObject capabilities;
    capabilities.insert(QStringLiteral("builtin"), descriptor.builtin);
    capabilities.insert(QStringLiteral("plotProtocol"), descriptor.plotProtocol);
    capabilities.insert(QStringLiteral("frameBuilder"), descriptor.frameBuilder);
    capabilities.insert(QStringLiteral("scriptProtocol"), descriptor.scriptProtocol);
    capabilities.insert(QStringLiteral("creatable"), descriptor.creatable);
    capabilities.insert(QStringLiteral("legacyCompatible"), descriptor.legacyCompatible);
    return capabilities;
}

/**
 * @brief 从规范化配置或默认配置中读取字段值。
 * @param descriptor 当前协议描述。
 * @param validation 当前配置的 Schema 校验结果。
 * @param key 字段 key。
 * @return 校验通过时优先返回规范化值，否则返回 descriptor 默认配置值。
 *
 * Lua 诊断需要展示沙箱预算。配置非法时 normalizedConfig 在主配置节点会被置空，
 * 但诊断仍应展示可回退的默认沙箱限制，便于用户理解当前安全边界。
 */
QVariant normalizedOrDefaultValue(const ProtocolDescriptor& descriptor,
                                  const ProtocolConfigValidationResult& validation,
                                  const QString& key)
{
    if (validation.valid && validation.normalizedConfig.contains(key)) {
        return validation.normalizedConfig.value(key);
    }

    return descriptor.defaultConfig.value(key);
}

/**
 * @brief 构建 Lua 沙箱诊断节点。
 * @param descriptor 当前 Lua 协议描述。
 * @param validation 当前配置校验结果。
 * @return 包含资源限制、通信 API 开关和库边界的 JSON。
 *
 * 这些字段描述当前项目中的 LuaSandbox 边界。Lua 协议解析器会执行
 * 内联 scriptSource，但仍不开放外部脚本加载和 serial.receive(timeout)。
 */
QJsonObject buildLuaSandboxObject(const ProtocolDescriptor& descriptor,
                                  const ProtocolConfigValidationResult& validation)
{
    QJsonObject sandbox;
    sandbox.insert(QStringLiteral("timeoutMs"),
                   normalizedOrDefaultValue(descriptor, validation, QStringLiteral("timeoutMs")).toInt());
    sandbox.insert(QStringLiteral("memoryLimitKb"),
                   normalizedOrDefaultValue(descriptor, validation, QStringLiteral("memoryLimitKb")).toInt());
    sandbox.insert(QStringLiteral("maxOutputLines"),
                   normalizedOrDefaultValue(descriptor, validation, QStringLiteral("maxOutputLines")).toInt());
    sandbox.insert(QStringLiteral("communicationApi"),
                   normalizedOrDefaultValue(descriptor, validation, QStringLiteral("allowCommunicationApi")).toBool());
    sandbox.insert(QStringLiteral("safeLibraries"),
                   stringListToJsonArray(QStringList{
                       QStringLiteral("_G"),
                       QStringLiteral("string"),
                       QStringLiteral("table"),
                       QStringLiteral("math"),
                       QStringLiteral("utf8")
                   }));
    sandbox.insert(QStringLiteral("blockedLibraries"),
                   stringListToJsonArray(QStringList{
                       QStringLiteral("io"),
                       QStringLiteral("os"),
                       QStringLiteral("package"),
                       QStringLiteral("debug")
                   }));
    sandbox.insert(QStringLiteral("blockedGlobals"),
                   stringListToJsonArray(QStringList{
                       QStringLiteral("require"),
                       QStringLiteral("dofile"),
                       QStringLiteral("loadfile"),
                       QStringLiteral("load")
                   }));
    return sandbox;
}

/**
 * @brief 构建 Lua 协议专用诊断节点。
 * @param descriptor 当前协议描述。
 * @param validation 当前配置校验结果。
 * @param context 运行期补充诊断上下文。
 * @return Lua 协议诊断 JSON。
 */
QJsonObject buildLuaProtocolObject(const ProtocolDescriptor& descriptor,
                                   const ProtocolConfigValidationResult& validation,
                                   const ProtocolDiagnosticsContext& context)
{
    QJsonObject luaProtocol;
    luaProtocol.insert(QStringLiteral("enabled"), descriptor.scriptProtocol);
    luaProtocol.insert(QStringLiteral("creatable"), descriptor.creatable);
    luaProtocol.insert(QStringLiteral("receiveApiAvailable"), false);
    luaProtocol.insert(QStringLiteral("lastError"), context.recentError);
    luaProtocol.insert(QStringLiteral("sandbox"), buildLuaSandboxObject(descriptor, validation));
    return luaProtocol;
}

/**
 * @brief 构建配置快照节点。
 * @param descriptor 当前协议描述。
 * @param currentConfig 当前协议配置。
 * @param validation Schema 校验结果。
 * @return 包含版本、Schema 字段、默认配置、当前配置和规范化配置的 JSON。
 */
QJsonObject buildConfigurationObject(const ProtocolDescriptor& descriptor,
                                     const QVariantMap& currentConfig,
                                     const ProtocolConfigValidationResult& validation)
{
    QJsonObject configuration;
    configuration.insert(QStringLiteral("configVersion"), descriptor.configVersion);
    configuration.insert(QStringLiteral("schemaVersion"), descriptor.configSchema.version);
    configuration.insert(QStringLiteral("fieldCount"), descriptor.configSchema.fields.size());
    configuration.insert(QStringLiteral("schemaFields"), schemaFieldsToJson(descriptor.configSchema));
    configuration.insert(QStringLiteral("defaultConfig"), QJsonObject::fromVariantMap(descriptor.defaultConfig));
    configuration.insert(QStringLiteral("currentConfig"), QJsonObject::fromVariantMap(currentConfig));

    /*
     * 配置非法时 normalizedConfig 可能包含部分默认合并结果。诊断第一版选择
     * 导出空对象，避免用户误以为这些值已经能安全应用到协议实例。
     */
    configuration.insert(QStringLiteral("normalizedConfig"),
                         validation.valid
                             ? QJsonObject::fromVariantMap(validation.normalizedConfig)
                             : QJsonObject());
    return configuration;
}

/**
 * @brief 构建配置校验节点。
 * @param validation Schema 校验结果。
 * @return 包含 valid、errors、warnings 的 JSON。
 */
QJsonObject buildValidationObject(const ProtocolConfigValidationResult& validation)
{
    QJsonObject object;
    object.insert(QStringLiteral("valid"), validation.valid);
    object.insert(QStringLiteral("errors"), stringListToJsonArray(validation.errors));
    object.insert(QStringLiteral("warnings"), stringListToJsonArray(validation.warnings));
    return object;
}

} // namespace

QJsonObject ProtocolDiagnosticsBuilder::build(const ProtocolDescriptor& descriptor,
                                              const QVariantMap& currentConfig,
                                              const QString& generatedAt,
                                              const ProtocolDiagnosticsContext& context)
{
    /*
     * 诊断构建从 Schema 校验开始。这样 configuration 和 validation
     * 两个节点使用同一次校验结果，避免 UI 展示和 JSON 内容不一致。
     */
    const ProtocolConfigValidationResult validation =
        descriptor.configSchema.validate(currentConfig);
    const QString timestamp = generatedAt.isEmpty()
        ? QDateTime::currentDateTime().toString(Qt::ISODate)
        : generatedAt;

    QJsonObject root;
    root.insert(QStringLiteral("diagnosticVersion"), 1);
    root.insert(QStringLiteral("generatedAt"), timestamp);
    root.insert(QStringLiteral("application"), buildApplicationObject());
    root.insert(QStringLiteral("protocol"), buildProtocolObject(descriptor));
    root.insert(QStringLiteral("capabilities"), buildCapabilitiesObject(descriptor));
    root.insert(QStringLiteral("configuration"),
                buildConfigurationObject(descriptor, currentConfig, validation));
    root.insert(QStringLiteral("validation"), buildValidationObject(validation));
    if (descriptor.scriptProtocol) {
        root.insert(QStringLiteral("luaProtocol"),
                    buildLuaProtocolObject(descriptor, validation, context));
    }
    return root;
}

} // namespace ComAssistant
