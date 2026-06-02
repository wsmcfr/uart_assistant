/**
 * @file TestProtocolRegistry.cpp
 * @brief 协议注册中心单元测试
 */

#include "TestProtocolRegistry.h"

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
