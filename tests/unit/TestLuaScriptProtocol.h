/**
 * @file TestLuaScriptProtocol.h
 * @brief Lua 脚本协议解析器单元测试头文件
 */

#ifndef TESTLUASCRIPTPROTOCOL_H
#define TESTLUASCRIPTPROTOCOL_H

#include <QObject>
#include <QTest>

/**
 * @brief Lua 脚本协议解析器测试类。
 *
 * 该测试类验证 `lua.script` 从协议元数据推进到真实 `IProtocol` 后，
 * 能否通过 `process(data, context)` 契约把 Lua table 解析结果映射为
 * FrameResult，同时保护第一版边界：不脚本化构帧、不开放接收 API。
 */
class TestLuaScriptProtocol : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证 process() 返回的有效帧会映射为 FrameResult。
     *
     * 该用例覆盖二进制 frame/payload、消耗字节数和 string/number/bool
     * 三类 metadata 标量，确保 Lua 协议结果不会退化成纯文本解析。
     */
    void parsesFrameFromProcessResult();

    /**
     * @brief 验证脚本可以返回未完成帧且不消耗输入。
     *
     * 接收链路经常会传入半包数据，Lua 解析器必须允许脚本明确声明
     * 当前没有完整帧，避免错误丢弃缓冲区内容。
     */
    void returnsIncompleteFrameWithoutConsumingBytes();

    /**
     * @brief 验证入口函数抛出的 Lua 错误会进入 FrameResult。
     *
     * 错误文本是后续诊断和 UI 提示的基础，不能在协议封装层被吞掉。
     */
    void reportsLuaErrorsFromProcess();

    /**
     * @brief 验证 Lua 运行错误会记录为最近错误。
     *
     * 最近错误是 MainWindow 状态栏提示和协议诊断 JSON 的运行期事实源，
     * 不能只临时存在于单次 FrameResult 中。
     */
    void recordsRecentError();

    /**
     * @brief 验证成功解析有效帧后会清空最近错误。
     *
     * 旧错误如果一直残留，会让诊断包误报当前协议仍处于失败状态。
     */
    void clearsRecentErrorAfterValidFrame();

    /**
     * @brief 验证输出行数限制导致结果区缺失时会记录最近错误。
     *
     * 用户把 maxOutputLines 设置过低时，LuaSandbox 会截断 wrapper 输出；
     * 协议层必须把该状态暴露出来，方便用户调整性能边界。
     */
    void recordsResultBlockErrorWhenOutputLimitIsTooLow();

    /**
     * @brief 验证脚本返回的 consumedBytes 会被限制在输入长度范围内。
     *
     * Lua 脚本可能写错消耗字节数，C++ 层必须兜底，避免调用方误删
     * 尚未真正解析过的接收缓冲内容。
     */
    void clampsConsumedBytesToInputSize();

    /**
     * @brief 验证第一版 build() 原样返回 payload。
     *
     * 4.10 只实现接收解析最小原型，构帧脚本化留到后续阶段。
     */
    void buildReturnsPayloadForFirstPrototype();
};

#endif // TESTLUASCRIPTPROTOCOL_H
