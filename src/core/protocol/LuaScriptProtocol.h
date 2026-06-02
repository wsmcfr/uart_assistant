/**
 * @file LuaScriptProtocol.h
 * @brief Lua 脚本协议解析器
 */

#ifndef COMASSISTANT_LUASCRIPTPROTOCOL_H
#define COMASSISTANT_LUASCRIPTPROTOCOL_H

#include "IProtocol.h"

namespace ComAssistant {

/**
 * @brief Lua 脚本协议解析器。
 *
 * 该协议是 `lua.script` 的最小可创建原型。它不拥有接收线程或串口对象，
 * 只把调用方传入的当前接收缓冲交给 Lua `process(data, context)`，再把
 * Lua table 返回值映射成 FrameResult。第一版不开放 serial.receive，也不
 * 脚本化构帧，确保接收缓冲和阻塞语义继续留给后续阶段单独设计。
 */
class LuaScriptProtocol : public IProtocol
{
    Q_OBJECT

public:
    /**
     * @brief 构造 Lua 脚本协议实例。
     * @param parent Qt 父对象，非空时由父子关系管理生命周期。
     */
    explicit LuaScriptProtocol(QObject* parent = nullptr);
    ~LuaScriptProtocol() override = default;

    /**
     * @brief 返回旧版协议类型占位。
     * @return 固定返回 Raw，因为 lua.script 不参与旧版 ProtocolType 工作流。
     */
    ProtocolType type() const override { return ProtocolType::Raw; }

    /**
     * @brief 返回协议名称。
     * @return 面向用户和诊断显示的协议名称。
     */
    QString name() const override { return QStringLiteral("Lua Script"); }

    /**
     * @brief 返回协议说明。
     * @return 协议能力说明文本。
     */
    QString description() const override;

    /**
     * @brief 调用 Lua process(data, context) 解析当前输入。
     * @param data 本次传入的接收缓冲快照。
     * @return Lua table 映射出的帧解析结果。
     */
    FrameResult parse(const QByteArray& data) override;

    /**
     * @brief 构建发送帧。
     * @param payload 待发送载荷。
     * @param metadata 可选元数据，第一版暂不使用。
     * @return 第一版原样返回 payload。
     */
    QByteArray build(const QByteArray& payload,
                     const QVariantMap& metadata = QVariantMap()) override;

    /**
     * @brief 验证一段数据是否能被 Lua 脚本解析为有效帧。
     * @param frame 待验证数据。
     * @return 如果 Lua process() 返回 valid=true 则返回 true。
     */
    bool validate(const QByteArray& frame) override;

    /**
     * @brief 计算校验值。
     * @param data 待校验数据，第一版暂不使用。
     * @return 空字节数组；Lua 校验构帧留给后续阶段设计。
     */
    QByteArray calculateChecksum(const QByteArray& data) override;

    /**
     * @brief 重置协议内部状态。
     *
     * 当前最小原型不维护内部接收缓冲，但仍清空基类缓冲，避免后续扩展
     * 或调用方复用实例时留下旧数据。
     */
    void reset() override;
};

} // namespace ComAssistant

#endif // COMASSISTANT_LUASCRIPTPROTOCOL_H
