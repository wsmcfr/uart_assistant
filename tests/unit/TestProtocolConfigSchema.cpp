/**
 * @file TestProtocolConfigSchema.cpp
 * @brief 协议配置 Schema 单元测试
 */

#include "TestProtocolConfigSchema.h"

#include "core/protocol/AsciiProtocol.h"
#include "core/protocol/EasyHexProtocol.h"
#include "core/protocol/IProtocol.h"
#include "core/protocol/ProtocolConfigSchema.h"
#include "core/protocol/ProtocolFactory.h"
#include "core/session/SessionData.h"

#include <QJsonObject>
#include <memory>

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

void TestProtocolConfigSchema::factoryAppliesValidatedAsciiConfig()
{
    /*
     * 工厂入口要负责把外部传入的配置先交给 Schema 校验和补全，
     * 再应用到真实协议实例。这里通过 ASCII 协议的行结束符和保存配置
     * 同时验证结构体状态与 QVariantMap 状态保持一致。
     */
    QVariantMap config;
    config.insert(QStringLiteral("lineEnding"), QStringLiteral("LF"));
    config.insert(QStringLiteral("appendLineEnding"), true);
    config.insert(QStringLiteral("timeoutMs"), 250);
    config.insert(QStringLiteral("encoding"), QStringLiteral("UTF-8"));

    std::unique_ptr<IProtocol> protocol(ProtocolFactory::create(ProtocolType::Ascii, config));
    QVERIFY(protocol != nullptr);

    auto* ascii = dynamic_cast<AsciiProtocol*>(protocol.get());
    QVERIFY(ascii != nullptr);
    QCOMPARE(ascii->lineEnding(), LineEnding::LF);
    QCOMPARE(protocol->config().value(QStringLiteral("timeoutMs")).toInt(), 250);
}

void TestProtocolConfigSchema::factoryAppliesValidatedEasyHexConfig()
{
    /*
     * EasyHEX 的十六进制字段需要被 Schema 规范化为大写空格分隔文本，
     * 然后再转换为协议内部使用的 QByteArray 和枚举配置。
     */
    QVariantMap config;
    config.insert(QStringLiteral("frameHeader"), QStringLiteral("55 aa"));
    config.insert(QStringLiteral("frameTail"), QStringLiteral(""));
    config.insert(QStringLiteral("useChecksum"), true);
    config.insert(QStringLiteral("checksumType"), QStringLiteral("XOR8"));
    config.insert(QStringLiteral("lengthFieldOffset"), 2);
    config.insert(QStringLiteral("lengthFieldSize"), 1);

    std::unique_ptr<IProtocol> protocol(ProtocolFactory::create(ProtocolType::EasyHex, config));
    QVERIFY(protocol != nullptr);

    auto* easyhex = dynamic_cast<EasyHexProtocol*>(protocol.get());
    QVERIFY(easyhex != nullptr);
    QCOMPARE(EasyHexProtocol::toHexString(easyhex->easyHexConfig().frameHeader), QStringLiteral("55 AA"));
    QCOMPARE(easyhex->easyHexConfig().checksumType, EasyHexConfig::XOR8);
}

void TestProtocolConfigSchema::sessionPersistsProtocolIdAndConfig()
{
    /*
     * 新会话文件应保存稳定协议 ID、配置版本和协议配置。
     * 旧的 protocolType 继续保留，用于兼容已有恢复链路和旧文件。
     */
    SessionData session;
    session.protocolType = static_cast<int>(ProtocolType::Ascii);
    session.protocolId = QStringLiteral("ascii");
    session.protocolConfigVersion = 1;
    session.protocolConfig.insert(QStringLiteral("lineEnding"), QStringLiteral("LF"));
    session.protocolConfig.insert(QStringLiteral("timeoutMs"), 250);

    const QJsonObject json = session.toJson();
    const SessionData restored = SessionData::fromJson(json);

    QCOMPARE(restored.protocolId, QStringLiteral("ascii"));
    QCOMPARE(restored.protocolConfigVersion, 1);
    QCOMPARE(restored.protocolConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("LF"));
    QCOMPARE(restored.protocolConfig.value(QStringLiteral("timeoutMs")).toInt(), 250);
}

void TestProtocolConfigSchema::sessionPersistsLuaScriptProtocolConfig()
{
    /*
     * Lua 协议依赖多行 scriptSource。保存会话和恢复会话必须保留源码文本，
     * 否则用户配置好的脚本协议在下次打开后无法继续解析接收数据。
     */
    const ProtocolDescriptor descriptor =
        ProtocolFactory::registry().descriptor(QStringLiteral("lua.script"));
    const QString script = QStringLiteral("function process(data, context)\n"
                                          "  return { valid = false, consumedBytes = 0 }\n"
                                          "end");

    SessionData session;
    session.protocolType = static_cast<int>(ProtocolType::Raw);
    session.protocolId = QStringLiteral("lua.script");
    session.protocolConfigVersion = descriptor.configVersion;
    session.protocolConfig = descriptor.defaultConfig;
    session.protocolConfig.insert(QStringLiteral("scriptSource"), script);

    const QJsonObject json = session.toJson();
    const SessionData restored = SessionData::fromJson(json);

    QCOMPARE(restored.protocolId, QStringLiteral("lua.script"));
    QCOMPARE(restored.protocolConfigVersion, descriptor.configVersion);
    QCOMPARE(restored.protocolConfig.value(QStringLiteral("scriptSource")).toString(), script);
}

void TestProtocolConfigSchema::sessionMigratesLegacyProtocolTypeToProtocolId()
{
    /*
     * 旧会话只含 protocolType，没有稳定协议 ID 和配置表。
     * 读取时应根据旧枚举迁移到内置协议描述，并补齐默认配置。
     */
    QJsonObject legacy;
    legacy.insert(QStringLiteral("protocolType"), static_cast<int>(ProtocolType::Ascii));

    const SessionData restored = SessionData::fromJson(legacy);

    QCOMPARE(restored.protocolId, QStringLiteral("ascii"));
    QCOMPARE(restored.protocolConfigVersion, 1);
    QCOMPARE(restored.protocolConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("CRLF"));
}
