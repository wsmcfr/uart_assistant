/**
 * @file ScriptExecutionWorker.cpp
 * @brief 脚本编辑器后台执行 worker 实现
 */

#include "ScriptExecutionWorker.h"

#include <utility>

namespace ComAssistant {

/**
 * @brief 创建脚本后台执行 worker。
 * @param script 要在后台线程中执行的 Lua 脚本文本。
 * @param interruptCallback Lua hook 查询取消状态时调用的函数。
 * @param parent Qt 父对象，通常为空，避免跨线程父子关系。
 *
 * 构造函数只保存脚本文本和取消回调，不启动线程，也不访问 UI。
 */
ScriptExecutionWorker::ScriptExecutionWorker(QString script,
                                             InterruptCallback interruptCallback,
                                             SendCallback sendCallback,
                                             ConnectionStateCallback connectionStateCallback,
                                             QObject* parent)
    : QObject(parent)
    , m_script(std::move(script))
    , m_interruptCallback(std::move(interruptCallback))
    , m_sendCallback(std::move(sendCallback))
    , m_connectionStateCallback(std::move(connectionStateCallback))
{
}

/**
 * @brief 执行脚本并通过信号返回结果。
 *
 * 主要流程是创建 LuaSandboxOptions、绑定发送信号回调、绑定取消回调，
 * 然后在当前 worker 线程中调用 LuaSandbox::execute()。函数没有返回值，
 * 执行完成后通过 finished() 把 LuaSandboxResult 交回 UI 线程。
 */
void ScriptExecutionWorker::run()
{
    LuaSandboxOptions options;
    options.timeoutMs = 3000;
    options.memoryLimitKb = 2048;
    options.maxOutputLines = 500;
    options.allowCommunicationApi = true;
    options.interruptCallback = m_interruptCallback;

    /*
     * serial.send/serial.sendHex 在 worker 线程内被 Lua 调用。这里仍不直接操作
     * UI 或通信对象，只调用对话框注入的线程安全 wrapper，并把拒绝原因写回 Lua。
     */
    options.sendWithErrorCallback = [this](const QByteArray& bytes, QString* error) {
        if (!m_sendCallback) {
            if (error) {
                *error = QStringLiteral("脚本发送通道未连接到主窗口");
            }
            return false;
        }

        const ScriptSendResult result = m_sendCallback(bytes);
        if (!result.accepted && error) {
            *error = result.error;
        }
        return result.accepted;
    };

    /*
     * 连接状态由 ScriptEditorDialog 注入的线程安全 wrapper 提供。没有回调时
     * 保守返回 false，避免脚本在未绑定主窗口发送入口时误判可以通信。
     */
    options.isOpenCallback = [this]() {
        return m_connectionStateCallback ? m_connectionStateCallback() : false;
    };

    LuaSandbox sandbox;
    emit finished(sandbox.execute(m_script, options));
}

} // namespace ComAssistant
