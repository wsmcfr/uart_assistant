/**
 * @file TestProtocolRegistry.cpp
 * @brief 协议注册中心单元测试
 */

#include "TestProtocolRegistry.h"

#include "core/protocol/AsciiProtocol.h"
#include "core/protocol/ProtocolRegistry.h"

using namespace ComAssistant;

void TestProtocolRegistry::builtinDescriptorsAreRegistered()
{
    /*
     * 内置协议目录是后续配置 schema、Lua 协议和诊断包共用的事实源。
     * 这里先锁定数量、稳定 ID 和旧版类型映射，避免平台化第一步丢协议。
     */
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const QList<ProtocolDescriptor> descriptors = registry.descriptors();
    QCOMPARE(descriptors.size(), 10);

    QVERIFY(registry.contains(QStringLiteral("raw")));
    QVERIFY(registry.contains(QStringLiteral("ascii")));
    QVERIFY(registry.contains(QStringLiteral("hex")));
    QVERIFY(registry.contains(QStringLiteral("modbus")));
    QVERIFY(registry.contains(QStringLiteral("custom")));
    QVERIFY(registry.contains(QStringLiteral("easyhex")));
    QVERIFY(registry.contains(QStringLiteral("plot.text")));
    QVERIFY(registry.contains(QStringLiteral("plot.stamp")));
    QVERIFY(registry.contains(QStringLiteral("plot.csv")));
    QVERIFY(registry.contains(QStringLiteral("plot.justfloat")));

    const ProtocolDescriptor ascii = registry.descriptor(QStringLiteral("ascii"));
    QCOMPARE(ascii.id, QStringLiteral("ascii"));
    QCOMPARE(ascii.displayName, QStringLiteral("ASCII"));
    QCOMPARE(ascii.legacyType, ProtocolType::Ascii);
    QVERIFY(ascii.builtin);
    QVERIFY(ascii.frameBuilder);
    QVERIFY(!ascii.plotProtocol);
}

void TestProtocolRegistry::rejectsInvalidRegistrations()
{
    /*
     * 注册中心是插件化入口，非法注册必须在入口被拒绝，
     * 避免后续配置、诊断和 UI 看到不完整协议能力。
     */
    ProtocolRegistry registry;
    QString errorMessage;

    ProtocolDescriptor emptyId;
    emptyId.displayName = QStringLiteral("Empty");
    QVERIFY(!registry.registerProtocol(
        emptyId,
        [](QObject*) -> IProtocol* { return nullptr; },
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    ProtocolDescriptor ascii;
    ascii.id = QStringLiteral("ascii");
    ascii.displayName = QStringLiteral("ASCII");
    ascii.legacyType = ProtocolType::Ascii;
    QVERIFY(registry.registerProtocol(
        ascii,
        [](QObject* parent) -> IProtocol* { return new AsciiProtocol(parent); }));
    QVERIFY(!registry.registerProtocol(
        ascii,
        [](QObject* parent) -> IProtocol* { return new AsciiProtocol(parent); },
        &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    ProtocolDescriptor noCreator;
    noCreator.id = QStringLiteral("no.creator");
    noCreator.displayName = QStringLiteral("No Creator");
    noCreator.legacyType = ProtocolType::Custom;
    QVERIFY(!registry.registerProtocol(noCreator, ProtocolCreator(), &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void TestProtocolRegistry::keepsRawCompatibility()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const ProtocolDescriptor raw = registry.descriptor(QStringLiteral("raw"));
    QCOMPARE(raw.id, QStringLiteral("raw"));
    QCOMPARE(raw.legacyType, ProtocolType::Raw);
    QVERIFY(raw.builtin);

    IProtocol* protocol = registry.create(QStringLiteral("raw"));
    QVERIFY(protocol == nullptr);
}
