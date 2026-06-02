/**
 * @file TestMainWindowProtocolState.cpp
 * @brief 主窗口协议状态协调器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMainWindowProtocolState.h"

#include "protocol/ProtocolFactory.h"
#include "ui/MainWindowProtocolState.h"

using namespace ComAssistant;

namespace {

/**
 * @brief 创建可立即解析有效帧的 Lua 脚本配置。
 * @return Lua 协议配置表，包含多行 scriptSource。
 */
QVariantMap makeValidLuaConfig()
{
    const ProtocolDescriptor descriptor =
        ProtocolFactory::registry().descriptor(QStringLiteral("lua.script"));
    QVariantMap config = descriptor.defaultConfig;
    config.insert(QStringLiteral("scriptSource"),
                  QStringLiteral("function process(data, context)\n"
                                 "  return {\n"
                                 "    valid = true,\n"
                                 "    frame = data,\n"
                                 "    payload = data,\n"
                                 "    consumedBytes = #data\n"
                                 "  }\n"
                                 "end"));
    return config;
}

/**
 * @brief 创建会触发 Lua 运行错误的协议配置。
 * @return Lua 协议配置表。
 */
QVariantMap makeFailingLuaConfig()
{
    const ProtocolDescriptor descriptor =
        ProtocolFactory::registry().descriptor(QStringLiteral("lua.script"));
    QVariantMap config = descriptor.defaultConfig;
    config.insert(QStringLiteral("scriptSource"),
                  QStringLiteral("function process(data, context)\n"
                                 "  error('boom from test')\n"
                                 "end"));
    return config;
}

} // namespace

void TestMainWindowProtocolState::restoresLuaProtocolByStableId()
{
    MainWindowProtocolState state;
    state.switchById(QStringLiteral("lua.script"), makeValidLuaConfig());

    QCOMPARE(state.protocolId(), QStringLiteral("lua.script"));
    QCOMPARE(state.protocolType(), ProtocolType::Raw);
    QVERIFY(state.protocol() != nullptr);
    QVERIFY(!state.descriptor().legacyCompatible);
    QVERIFY(state.descriptor().scriptProtocol);
    QCOMPARE(state.config().value(QStringLiteral("scriptSource")).toString(),
             makeValidLuaConfig().value(QStringLiteral("scriptSource")).toString());
}

void TestMainWindowProtocolState::unknownProtocolIdFallsBackToRaw()
{
    MainWindowProtocolState state;
    state.switchById(QStringLiteral("future.protocol"), QVariantMap());

    QCOMPARE(state.protocolId(), QStringLiteral("raw"));
    QCOMPARE(state.protocolType(), ProtocolType::Raw);
    QVERIFY(state.protocol() == nullptr);
    QVERIFY(state.config().isEmpty());
}

void TestMainWindowProtocolState::recordsLuaParseErrorForDiagnostics()
{
    MainWindowProtocolState state;
    state.switchById(QStringLiteral("lua.script"), makeFailingLuaConfig());

    const FrameResult result = state.parseNonPlotData(QByteArray("abc"));

    QVERIFY(!result.valid);
    QVERIFY(result.errorMessage.contains(QStringLiteral("boom from test")));
    QVERIFY(state.recentProtocolError().contains(QStringLiteral("boom from test")));
    QCOMPARE(state.diagnosticsContext().recentError, state.recentProtocolError());

    state.switchById(QStringLiteral("lua.script"), makeValidLuaConfig());
    const FrameResult validResult = state.parseNonPlotData(QByteArray("ok"));
    QVERIFY(validResult.valid);
    QVERIFY(state.recentProtocolError().isEmpty());
    QVERIFY(state.diagnosticsContext().recentError.isEmpty());
}

void TestMainWindowProtocolState::switchesPlotProtocolByLegacyType()
{
    MainWindowProtocolState state;
    state.switchByLegacyType(ProtocolType::TextPlot);

    QCOMPARE(state.protocolId(), QStringLiteral("plot.text"));
    QCOMPARE(state.protocolType(), ProtocolType::TextPlot);
    QVERIFY(state.protocol() != nullptr);
    QVERIFY(state.protocol()->isPlotProtocol());
    QVERIFY(state.descriptor().plotProtocol);
}

void TestMainWindowProtocolState::receiveProtocolChoicesIncludeLuaScript()
{
    const QList<ProtocolDescriptor> choices =
        MainWindowProtocolState::receiveProtocolChoices();
    QStringList ids;
    for (const ProtocolDescriptor& descriptor : choices) {
        ids.append(descriptor.id);
    }

    QVERIFY(ids.contains(QStringLiteral("raw")));
    QVERIFY(ids.contains(QStringLiteral("plot.text")));
    QVERIFY(ids.contains(QStringLiteral("lua.script")));
    QVERIFY(!ids.contains(QString()));

    const int luaIndex = ids.indexOf(QStringLiteral("lua.script"));
    QVERIFY(luaIndex >= 0);
    QVERIFY(choices.at(luaIndex).scriptProtocol);
    QVERIFY(choices.at(luaIndex).creatable);
}
