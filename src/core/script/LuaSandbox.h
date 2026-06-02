/**
 * @file LuaSandbox.h
 * @brief Lua 安全沙箱执行器
 */

#ifndef COMASSISTANT_LUASANDBOX_H
#define COMASSISTANT_LUASANDBOX_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMetaType>

#include <functional>

namespace ComAssistant {

/**
 * @brief Lua 沙箱执行选项。
 *
 * 该结构集中描述脚本资源边界和能力开关。UI、协议脚本和自动化测试
 * 都应通过这里传入限制，避免在执行器内部散落魔法数字。
 */
struct LuaSandboxOptions
{
    using SendCallback = std::function<bool(const QByteArray&)>;
    using IsOpenCallback = std::function<bool()>;
    using InterruptCallback = std::function<bool()>;

    int timeoutMs = 1000;               ///< 最大执行时长，<=0 表示不限制时间
    int memoryLimitKb = 1024;           ///< Lua state 内存预算，<=0 表示不限制内存
    int maxOutputLines = 200;           ///< print 输出最多保留的行数
    bool allowCommunicationApi = false; ///< 是否启用后续通信 API，4.5 默认关闭
    SendCallback sendCallback;          ///< 受控发送回调，仅通信 API 显式启用时使用
    IsOpenCallback isOpenCallback;      ///< 连接状态回调，仅用于 serial.isOpen()
    InterruptCallback interruptCallback; ///< 外部取消回调，返回 true 时 hook 中断脚本
};

/**
 * @brief Lua 沙箱执行结果。
 *
 * 结果用显式布尔值区分普通 Lua 错误、超时、中断和内存超限，
 * 方便后续 UI 展示、协议诊断导出和自动化测试做稳定判断。
 */
struct LuaSandboxResult
{
    bool success = false;         ///< 脚本是否完整执行成功
    bool timedOut = false;        ///< 是否被超时 hook 中断
    bool interrupted = false;     ///< 是否被外部取消中断
    bool memoryExceeded = false;  ///< 是否触发 Lua 内存预算
    QString errorMessage;         ///< 失败原因或 Lua 错误文本
    QStringList outputLines;      ///< print 收集到的输出行
    qint64 elapsedMs = 0;         ///< 本次执行耗时
};

/**
 * @brief Lua 安全沙箱执行器。
 *
 * 每次 execute() 都创建全新的 Lua state，只打开白名单标准库并注册受控 API。
 * 这样一次脚本的全局变量、hook 或库表修改不会污染下一次执行。
 */
class LuaSandbox
{
public:
    /**
     * @brief 执行一段 Lua 脚本文本。
     * @param script Lua 脚本文本，按 UTF-8 传入 Lua 运行时。
     * @param options 本次执行使用的资源限制和能力开关。
     * @return 结构化执行结果，包含输出、耗时和失败原因。
     */
    LuaSandboxResult execute(const QString& script,
                             const LuaSandboxOptions& options = LuaSandboxOptions());
};

} // namespace ComAssistant

Q_DECLARE_METATYPE(ComAssistant::LuaSandboxResult)

#endif // COMASSISTANT_LUASANDBOX_H
