/**
 * @file TestLuaScriptProtocol.cpp
 * @brief Lua 脚本协议解析器单元测试
 */

#include "TestLuaScriptProtocol.h"

#include "core/protocol/LuaScriptProtocol.h"

using namespace ComAssistant;

namespace {

/**
 * @brief 构造只设置内联 Lua 源码的协议配置。
 * @param scriptSource Lua 脚本源码，测试中直接定义 process() 入口。
 * @return 可传给 LuaScriptProtocol::setConfig() 的配置表。
 *
 * 测试只关心脚本契约本身，其他沙箱限制使用协议默认值即可。
 */
QVariantMap makeScriptConfig(const QString& scriptSource)
{
    QVariantMap config;
    config.insert(QStringLiteral("scriptSource"), scriptSource);
    return config;
}

/**
 * @brief 构造完整 Lua 协议配置，允许测试覆盖资源边界。
 * @param scriptSource Lua 脚本源码。
 * @param maxOutputLines LuaSandbox 保留的最大输出行数。
 * @return 可传给 LuaScriptProtocol::setConfig() 的配置表。
 */
QVariantMap makeScriptConfigWithOutputLimit(const QString& scriptSource,
                                            int maxOutputLines)
{
    QVariantMap config = makeScriptConfig(scriptSource);
    config.insert(QStringLiteral("maxOutputLines"), maxOutputLines);
    return config;
}

} // namespace

void TestLuaScriptProtocol::parsesFrameFromProcessResult()
{
    /*
     * 这里用非 ASCII 的帧头验证二进制字节不会在 Lua wrapper 与 C++ 结果
     * 之间被当作普通文本损坏；metadata 同时覆盖字符串、数字和布尔值。
     */
    LuaScriptProtocol protocol;
    protocol.setConfig(makeScriptConfig(
        QStringLiteral("function process(data, context)\n"
                       "  return {\n"
                       "    valid = true,\n"
                       "    consumedBytes = #data,\n"
                       "    frame = data,\n"
                       "    payload = string.sub(data, 2),\n"
                       "    metadata = {\n"
                       "      protocol = context.protocolId,\n"
                       "      length = context.dataLength,\n"
                       "      ok = true\n"
                       "    }\n"
                       "  }\n"
                       "end\n")));

    const FrameResult result = protocol.parse(QByteArray::fromHex("A10203"));

    QVERIFY(result.valid);
    QCOMPARE(result.consumedBytes, 3);
    QCOMPARE(result.frame.toHex(' ').toUpper(), QByteArray("A1 02 03"));
    QCOMPARE(result.payload.toHex(' ').toUpper(), QByteArray("02 03"));
    QCOMPARE(result.metadata.value(QStringLiteral("protocol")).toString(),
             QStringLiteral("lua.script"));
    QCOMPARE(result.metadata.value(QStringLiteral("length")).toInt(), 3);
    QCOMPARE(result.metadata.value(QStringLiteral("ok")).toBool(), true);
    QVERIFY(result.errorMessage.isEmpty());
}

void TestLuaScriptProtocol::returnsIncompleteFrameWithoutConsumingBytes()
{
    /*
     * 半包时脚本可以返回 valid=false 且 consumedBytes=0，调用方据此保留
     * 接收缓冲，等待更多字节进入下一轮 parse()。
     */
    LuaScriptProtocol protocol;
    protocol.setConfig(makeScriptConfig(
        QStringLiteral("function process(data, context)\n"
                       "  return { valid = false, consumedBytes = 0 }\n"
                       "end\n")));

    const FrameResult result = protocol.parse(QByteArray::fromHex("A1"));

    QVERIFY(!result.valid);
    QCOMPARE(result.consumedBytes, 0);
    QVERIFY(result.frame.isEmpty());
    QVERIFY(result.payload.isEmpty());
    QVERIFY(result.errorMessage.isEmpty());
}

void TestLuaScriptProtocol::reportsLuaErrorsFromProcess()
{
    /*
     * Lua 错误必须稳定回传到 FrameResult::errorMessage，便于后续诊断包
     * 或 UI 对脚本作者显示真实失败原因。
     */
    LuaScriptProtocol protocol;
    protocol.setConfig(makeScriptConfig(
        QStringLiteral("function process(data, context)\n"
                       "  error('bad lua frame')\n"
                       "end\n")));

    const FrameResult result = protocol.parse(QByteArray::fromHex("A102"));

    QVERIFY(!result.valid);
    QCOMPARE(result.consumedBytes, 0);
    QVERIFY(result.errorMessage.contains(QStringLiteral("bad lua frame")));
}

void TestLuaScriptProtocol::recordsRecentError()
{
    /*
     * 协议诊断对话框不会持有上一轮 FrameResult，因此 LuaScriptProtocol
     * 自身需要保存最近一次错误，供 MainWindow 后续读取。
     */
    LuaScriptProtocol protocol;
    protocol.setConfig(makeScriptConfig(
        QStringLiteral("function process(data, context)\n"
                       "  error('bad lua frame')\n"
                       "end\n")));

    const FrameResult result = protocol.parse(QByteArray::fromHex("A102"));

    QVERIFY(!result.valid);
    QVERIFY(protocol.recentError().contains(QStringLiteral("bad lua frame")));
}

void TestLuaScriptProtocol::clearsRecentErrorAfterValidFrame()
{
    /*
     * 最近错误只描述当前最近一次失败状态。有效帧解析成功后需要清空，
     * 避免用户修好脚本后诊断仍展示旧错误。
     */
    LuaScriptProtocol protocol;
    protocol.setConfig(makeScriptConfig(
        QStringLiteral("function process(data, context)\n"
                       "  error('first failure')\n"
                       "end\n")));
    protocol.parse(QByteArray::fromHex("A102"));
    QVERIFY(!protocol.recentError().isEmpty());

    protocol.setConfig(makeScriptConfig(
        QStringLiteral("function process(data, context)\n"
                       "  return { valid = true, consumedBytes = #data, frame = data, payload = data }\n"
                       "end\n")));
    const FrameResult result = protocol.parse(QByteArray::fromHex("A102"));

    QVERIFY(result.valid);
    QVERIFY(protocol.recentError().isEmpty());
}

void TestLuaScriptProtocol::recordsResultBlockErrorWhenOutputLimitIsTooLow()
{
    /*
     * wrapper 需要输出固定哨兵和字段。如果 maxOutputLines 过低导致哨兵区
     * 缺失，协议层应把错误保存下来，帮助用户从诊断中发现输出边界问题。
     */
    LuaScriptProtocol protocol;
    protocol.setConfig(makeScriptConfigWithOutputLimit(
        QStringLiteral("function process(data, context)\n"
                       "  return { valid = true, consumedBytes = #data, frame = data, payload = data }\n"
                       "end\n"),
        1));

    const FrameResult result = protocol.parse(QByteArray::fromHex("A102"));

    QVERIFY(!result.valid);
    QVERIFY(result.errorMessage.contains(QStringLiteral("result block")));
    QVERIFY(protocol.recentError().contains(QStringLiteral("result block")));
}

void TestLuaScriptProtocol::clampsConsumedBytesToInputSize()
{
    /*
     * 脚本返回越界消耗值时由 C++ 层兜底夹紧，避免一个脚本错误扩大为
     * 接收缓冲区状态错误。
     */
    LuaScriptProtocol protocol;
    protocol.setConfig(makeScriptConfig(
        QStringLiteral("function process(data, context)\n"
                       "  return { valid = true, consumedBytes = 999, frame = data, payload = data }\n"
                       "end\n")));

    const FrameResult result = protocol.parse(QByteArray::fromHex("01020304"));

    QVERIFY(result.valid);
    QCOMPARE(result.consumedBytes, 4);
    QCOMPARE(result.frame.toHex(' ').toUpper(), QByteArray("01 02 03 04"));
}

void TestLuaScriptProtocol::buildReturnsPayloadForFirstPrototype()
{
    /*
     * 4.10 只承诺接收解析最小原型。build() 保持原样返回 payload，
     * 后续如果要脚本化构帧，需要先补独立设计和红测。
     */
    LuaScriptProtocol protocol;
    const QByteArray payload = QByteArray::fromHex("112233");

    QCOMPARE(protocol.build(payload), payload);
}
