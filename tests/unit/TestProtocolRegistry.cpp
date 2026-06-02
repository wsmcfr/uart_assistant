/**
 * @file TestProtocolRegistry.cpp
 * @brief 协议注册中心单元测试
 */

#include "TestProtocolRegistry.h"

#include "core/protocol/AsciiProtocol.h"
#include "core/protocol/ProtocolConfigSchema.h"
#include "core/protocol/ProtocolFactory.h"
#include "core/protocol/ProtocolRegistry.h"

#include <memory>

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
    QCOMPARE(descriptors.size(), 11);

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

void TestProtocolRegistry::createsProtocolById()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    /*
     * 稳定 ID 是平台化后的创建入口，不能依赖旧枚举 switch。
     * 这里用基础协议和绘图协议各取样，覆盖普通构帧与绘图能力。
     */
    std::unique_ptr<IProtocol> ascii(registry.create(QStringLiteral("ascii")));
    QVERIFY(ascii != nullptr);
    QCOMPARE(ascii->type(), ProtocolType::Ascii);

    std::unique_ptr<IProtocol> hex(registry.create(QStringLiteral("hex")));
    QVERIFY(hex != nullptr);
    QCOMPARE(hex->type(), ProtocolType::Hex);

    std::unique_ptr<IProtocol> text(registry.create(QStringLiteral("plot.text")));
    QVERIFY(text != nullptr);
    QVERIFY(text->isPlotProtocol());
}

void TestProtocolRegistry::marksPlotProtocols()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    QVERIFY(registry.descriptor(QStringLiteral("plot.text")).plotProtocol);
    QVERIFY(registry.descriptor(QStringLiteral("plot.stamp")).plotProtocol);
    QVERIFY(registry.descriptor(QStringLiteral("plot.csv")).plotProtocol);
    QVERIFY(registry.descriptor(QStringLiteral("plot.justfloat")).plotProtocol);
    QVERIFY(!registry.descriptor(QStringLiteral("ascii")).plotProtocol);
}

void TestProtocolRegistry::keepsFactoryCompatibility()
{
    const QList<ProtocolType> types = ProtocolFactory::supportedTypes();
    QCOMPARE(types.size(), 10);
    QVERIFY(types.contains(ProtocolType::Raw));
    QVERIFY(types.contains(ProtocolType::JustFloat));

    QCOMPARE(ProtocolFactory::typeId(ProtocolType::Ascii), QStringLiteral("ascii"));
    QCOMPARE(ProtocolFactory::typeId(ProtocolType::TextPlot), QStringLiteral("plot.text"));
    QCOMPARE(ProtocolFactory::descriptor(ProtocolType::Ascii).displayName, QStringLiteral("ASCII"));

    QCOMPARE(ProtocolFactory::typeName(ProtocolType::Ascii), QStringLiteral("ASCII"));
    QCOMPARE(ProtocolFactory::typeName(ProtocolType::TextPlot), QStringLiteral("TEXT绘图"));

    std::unique_ptr<IProtocol> ascii(ProtocolFactory::create(ProtocolType::Ascii));
    QVERIFY(ascii != nullptr);
    QCOMPARE(ascii->type(), ProtocolType::Ascii);

    std::unique_ptr<IProtocol> raw(ProtocolFactory::create(ProtocolType::Raw));
    QVERIFY(raw == nullptr);
}

void TestProtocolRegistry::filtersDescriptorsByCategory()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const QList<ProtocolDescriptor> plotProtocols =
        registry.descriptorsByCategory(ProtocolCategory::Plot);
    QCOMPARE(plotProtocols.size(), 4);

    for (const ProtocolDescriptor& descriptor : plotProtocols) {
        QVERIFY(descriptor.plotProtocol);
        QCOMPARE(descriptor.category, ProtocolCategory::Plot);
    }
}

void TestProtocolRegistry::builtinRegistrationKeepsExistingProtocols()
{
    /*
     * 后续插件、脚本协议会先向同一个注册中心登记自有能力。
     * registerBuiltinProtocols() 必须只跳过已存在的内置 ID，而不能因为注册表非空
     * 就直接返回，否则“先插件后内置”的初始化顺序会丢失全部内置协议。
     */
    ProtocolRegistry registry;

    ProtocolDescriptor externalDescriptor;
    externalDescriptor.id = QStringLiteral("external.echo");
    externalDescriptor.displayName = QStringLiteral("External Echo");
    externalDescriptor.description = QStringLiteral("测试用外部协议");
    externalDescriptor.category = ProtocolCategory::Custom;
    externalDescriptor.legacyType = ProtocolType::Custom;
    externalDescriptor.builtin = false;
    externalDescriptor.frameBuilder = true;

    QVERIFY(registry.registerProtocol(
        externalDescriptor,
        [](QObject* parent) -> IProtocol* { return new AsciiProtocol(parent); }));

    registry.registerBuiltinProtocols();

    QVERIFY(registry.contains(QStringLiteral("external.echo")));
    QVERIFY(registry.contains(QStringLiteral("raw")));
    QVERIFY(registry.contains(QStringLiteral("ascii")));
    QCOMPARE(registry.descriptors().size(), 12);
}

void TestProtocolRegistry::builtinDescriptorsExposeConfigSchema()
{
    /*
     * 4.2 要求协议目录不仅说明能力，还要成为配置事实源。
     * ASCII 和 EasyHEX 是第一批必须暴露字段级 Schema 的协议。
     */
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const ProtocolDescriptor ascii = registry.descriptor(QStringLiteral("ascii"));
    QCOMPARE(ascii.configVersion, 1);
    QVERIFY(ascii.configSchema.fields.size() >= 4);
    QVERIFY(ascii.defaultConfig.contains(QStringLiteral("lineEnding")));
    QCOMPARE(ascii.defaultConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("CRLF"));

    const ProtocolDescriptor easyhex = registry.descriptor(QStringLiteral("easyhex"));
    QVERIFY(easyhex.configSchema.fields.size() >= 5);
    QCOMPARE(easyhex.defaultConfig.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("AA 55"));
}

void TestProtocolRegistry::defaultConfigMatchesSchema()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    /*
     * 每个内置协议的默认配置都必须能被自己的 Schema 接受。
     * 这是后续会话迁移和带配置创建协议实例的最基本自洽性检查。
     */
    for (const ProtocolDescriptor& descriptor : registry.descriptors()) {
        const ProtocolConfigValidationResult result =
            descriptor.configSchema.validate(descriptor.defaultConfig);
        QVERIFY2(result.valid,
                 qPrintable(QStringLiteral("默认配置校验失败: %1 %2")
                            .arg(descriptor.id, result.errors.join(QStringLiteral("; ")))));
        QCOMPARE(result.normalizedConfig, descriptor.defaultConfig);
    }
}

void TestProtocolRegistry::registersLuaScriptProtocolDescriptor()
{
    /*
     * 4.10 将 Lua 协议推进为可创建的最小解析器。它必须继续清楚标记
     * “脚本协议、不参与旧版枚举兼容链路”，同时允许注册中心创建实例。
     */
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    QVERIFY(registry.contains(QStringLiteral("lua.script")));

    const ProtocolDescriptor descriptor = registry.descriptor(QStringLiteral("lua.script"));
    QCOMPARE(descriptor.id, QStringLiteral("lua.script"));
    QCOMPARE(descriptor.displayName, QStringLiteral("Lua Script"));
    QCOMPARE(descriptor.category, ProtocolCategory::Custom);
    QVERIFY(descriptor.scriptProtocol);
    QVERIFY(descriptor.creatable);
    QVERIFY(!descriptor.legacyCompatible);
    QVERIFY(!descriptor.builtin);
    QVERIFY(!descriptor.plotProtocol);
    QVERIFY(!descriptor.frameBuilder);
    QVERIFY(descriptor.configSchema.fields.size() >= 7);
    QCOMPARE(descriptor.defaultConfig.value(QStringLiteral("timeoutMs")).toInt(), 1000);
    QCOMPARE(descriptor.defaultConfig.value(QStringLiteral("memoryLimitKb")).toInt(), 1024);
    QCOMPARE(descriptor.defaultConfig.value(QStringLiteral("maxOutputLines")).toInt(), 200);
    QCOMPARE(descriptor.defaultConfig.value(QStringLiteral("entryFunction")).toString(),
             QStringLiteral("process"));

    std::unique_ptr<IProtocol> protocol(registry.create(QStringLiteral("lua.script")));
    QVERIFY(protocol != nullptr);
    QCOMPARE(protocol->name(), QStringLiteral("Lua Script"));
    QCOMPARE(protocol->type(), ProtocolType::Raw);
}

void TestProtocolRegistry::factoryLegacyListIgnoresLuaDescriptor()
{
    /*
     * Lua 脚本协议已经进入共享注册中心，但它当前没有旧版 ProtocolType
     * 映射意义。supportedTypes() 仍必须只返回旧工作流中的 10 个协议，
     * 避免影响绘图协议菜单、旧会话恢复和已有 UI 逻辑。
     */
    const QList<ProtocolType> types = ProtocolFactory::supportedTypes();
    QCOMPARE(types.size(), 10);
    QVERIFY(types.contains(ProtocolType::Raw));
    QVERIFY(types.contains(ProtocolType::JustFloat));
    QCOMPARE(ProtocolFactory::typeId(ProtocolType::Raw), QStringLiteral("raw"));
    QVERIFY(ProtocolFactory::registry().contains(QStringLiteral("lua.script")));
}
