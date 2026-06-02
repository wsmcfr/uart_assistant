/**
 * @file TestProtocolConfigSchema.cpp
 * @brief 协议配置 Schema 单元测试
 */

#include "TestProtocolConfigSchema.h"

#include "core/protocol/ProtocolConfigSchema.h"

using namespace ComAssistant;

void TestProtocolConfigSchema::fillsMissingDefaults()
{
    /*
     * 缺失字段应自动补默认值。这样旧会话或插件未提供完整配置时，
     * 后续协议实例仍能拿到可用、稳定的规范化配置。
     */
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("timeoutMs"),
        QStringLiteral("超时时间"),
        100,
        1,
        10000,
        QStringLiteral("分帧超时时间")));
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("lineEnding"),
        QStringLiteral("行结束符"),
        QStringLiteral("CRLF"),
        QStringList{QStringLiteral("None"), QStringLiteral("CR"), QStringLiteral("LF"), QStringLiteral("CRLF")},
        QStringLiteral("发送时追加的行结束符")));

    const ProtocolConfigValidationResult result = schema.validate(QVariantMap());

    QVERIFY(result.valid);
    QCOMPARE(result.normalizedConfig.value(QStringLiteral("timeoutMs")).toInt(), 100);
    QCOMPARE(result.normalizedConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("CRLF"));
}

void TestProtocolConfigSchema::rejectsInvalidValues()
{
    /*
     * Schema 是协议配置入口的防线。超出范围和非法枚举必须被明确拒绝，
     * 否则后续 UI、Lua 或插件都可能把坏配置传进协议实现。
     */
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("slaveAddress"),
        QStringLiteral("从站地址"),
        1,
        1,
        247,
        QStringLiteral("Modbus 从站地址")));
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("mode"),
        QStringLiteral("模式"),
        QStringLiteral("RTU"),
        QStringList{QStringLiteral("RTU"), QStringLiteral("ASCII")},
        QStringLiteral("Modbus 帧格式")));

    QVariantMap config;
    config.insert(QStringLiteral("slaveAddress"), 300);
    config.insert(QStringLiteral("mode"), QStringLiteral("TCP"));

    const ProtocolConfigValidationResult result = schema.validate(config);

    QVERIFY(!result.valid);
    QVERIFY(result.errors.size() >= 2);
    QVERIFY(result.errors.join(QStringLiteral("\n")).contains(QStringLiteral("slaveAddress")));
    QVERIFY(result.errors.join(QStringLiteral("\n")).contains(QStringLiteral("mode")));
}

void TestProtocolConfigSchema::normalizesHexBytes()
{
    /*
     * 字节字段统一保存为大写空格分隔十六进制文本，避免会话 JSON、
     * 未来配置 UI 和脚本接口之间出现多种表示方式。
     */
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::bytesHex(
        QStringLiteral("frameHeader"),
        QStringLiteral("帧头"),
        QStringLiteral("AA 55"),
        QStringLiteral("帧头字节")));

    QVariantMap config;
    config.insert(QStringLiteral("frameHeader"), QStringLiteral("aa-55"));

    const ProtocolConfigValidationResult result = schema.validate(config);

    QVERIFY(result.valid);
    QCOMPARE(result.normalizedConfig.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("AA 55"));
}
