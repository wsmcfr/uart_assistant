/**
 * @file TestProtocolDiagnostics.cpp
 * @brief 协议诊断导出单元测试
 */

#include "TestProtocolDiagnostics.h"

#include "core/protocol/ProtocolDiagnostics.h"
#include "core/protocol/ProtocolFactory.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace ComAssistant;

namespace {

/**
 * @brief 构建一个带非法字段的 EasyHEX 配置。
 * @return frameHeader 非法的配置表，用于验证诊断仍能导出错误。
 */
QVariantMap makeInvalidEasyHexConfig()
{
    QVariantMap config = ProtocolFactory::descriptor(ProtocolType::EasyHex).defaultConfig;
    config.insert(QStringLiteral("frameHeader"), QStringLiteral("AA Z1"));
    return config;
}

} // namespace

void TestProtocolDiagnostics::exportsDescriptorAndCapabilities()
{
    /*
     * 诊断 JSON 的第一层价值是锁定“当前协议是谁、具备哪些能力”。
     * 这些字段来自注册中心描述，后续插件/Lua 协议也会复用同一事实源。
     */
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::Ascii);
    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        descriptor.defaultConfig,
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    QCOMPARE(json.value(QStringLiteral("diagnosticVersion")).toInt(), 1);
    QCOMPARE(json.value(QStringLiteral("generatedAt")).toString(),
             QStringLiteral("2026-06-02T12:30:00+08:00"));

    const QJsonObject protocol = json.value(QStringLiteral("protocol")).toObject();
    QCOMPARE(protocol.value(QStringLiteral("id")).toString(), QStringLiteral("ascii"));
    QCOMPARE(protocol.value(QStringLiteral("displayName")).toString(), QStringLiteral("ASCII"));
    QCOMPARE(protocol.value(QStringLiteral("category")).toString(), QStringLiteral("Basic"));

    const QJsonObject capabilities = json.value(QStringLiteral("capabilities")).toObject();
    QVERIFY(capabilities.value(QStringLiteral("builtin")).toBool());
    QVERIFY(capabilities.value(QStringLiteral("frameBuilder")).toBool());
    QVERIFY(!capabilities.value(QStringLiteral("plotProtocol")).toBool());
}

void TestProtocolDiagnostics::exportsSchemaAndConfigs()
{
    /*
     * EasyHEX 覆盖 BytesHex 规范化和较多 Schema 字段，是诊断导出的
     * 高价值样本。currentConfig 保留原输入，normalizedConfig 给排障方
     * 看 Schema 接受后的实际配置。
     */
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::EasyHex);
    QVariantMap currentConfig = descriptor.defaultConfig;
    currentConfig.insert(QStringLiteral("frameHeader"), QStringLiteral("aa-55"));

    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        currentConfig,
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    const QJsonObject configuration = json.value(QStringLiteral("configuration")).toObject();
    QCOMPARE(configuration.value(QStringLiteral("configVersion")).toInt(), descriptor.configVersion);
    QCOMPARE(configuration.value(QStringLiteral("schemaVersion")).toInt(), descriptor.configSchema.version);
    QVERIFY(configuration.value(QStringLiteral("fieldCount")).toInt() >= 5);

    const QJsonArray schemaFields = configuration.value(QStringLiteral("schemaFields")).toArray();
    QVERIFY(!schemaFields.isEmpty());
    QCOMPARE(schemaFields.first().toObject().contains(QStringLiteral("key")), true);
    QCOMPARE(schemaFields.first().toObject().contains(QStringLiteral("type")), true);

    const QJsonObject current = configuration.value(QStringLiteral("currentConfig")).toObject();
    const QJsonObject normalized = configuration.value(QStringLiteral("normalizedConfig")).toObject();
    QCOMPARE(current.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("aa-55"));
    QCOMPARE(normalized.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("AA 55"));

    const QJsonObject validation = json.value(QStringLiteral("validation")).toObject();
    QVERIFY(validation.value(QStringLiteral("valid")).toBool());
}

void TestProtocolDiagnostics::exportsValidationErrorsForInvalidConfig()
{
    /*
     * 诊断入口不能因为当前配置非法就拒绝打开。相反，它应该保留
     * currentConfig 和 errors，帮助用户把坏配置本身导出来。
     */
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::EasyHex);
    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        makeInvalidEasyHexConfig(),
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    const QJsonObject validation = json.value(QStringLiteral("validation")).toObject();
    QVERIFY(!validation.value(QStringLiteral("valid")).toBool());
    QVERIFY(!validation.value(QStringLiteral("errors")).toArray().isEmpty());

    const QJsonObject configuration = json.value(QStringLiteral("configuration")).toObject();
    QCOMPARE(configuration.value(QStringLiteral("normalizedConfig")).toObject().isEmpty(), true);
}

void TestProtocolDiagnostics::exportsRawProtocolWithoutFields()
{
    /*
     * Raw 表示无协议实例，但它仍是注册中心中的协议描述。
     * 诊断导出必须能处理空 Schema，避免用户当前处于 Raw 时对话框崩溃。
     */
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::Raw);
    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        descriptor.defaultConfig,
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    QCOMPARE(json.value(QStringLiteral("protocol")).toObject().value(QStringLiteral("id")).toString(),
             QStringLiteral("raw"));
    QCOMPARE(json.value(QStringLiteral("configuration")).toObject().value(QStringLiteral("fieldCount")).toInt(),
             0);
    QVERIFY(json.value(QStringLiteral("validation")).toObject().value(QStringLiteral("valid")).toBool());
}
