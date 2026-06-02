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
                                             QObject* parent)
    : QObject(parent)
    , m_script(std::move(script))
    , m_interruptCallback(std::move(interruptCallback))
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
     * serial.send/serial.sendHex 在 worker 线程内被 Lua 调用。这里不直接操作 UI 或通信对象，
     * 只把非空数据通过 queued signal 交回对话框，保持线程边界清晰。
     */
    options.sendCallback = [this](const QByteArray& bytes) {
        if (bytes.isEmpty()) {
            return false;
        }

        emit sendRequested(bytes);
        return true;
    };

    /*
     * 4.7 第一版仍沿用 4.6 语义：脚本发送通道启用时 serial.isOpen() 返回 true。
     * 真实连接状态注入需要主窗口提供线程安全状态回调，留给后续小阶段处理。
     */
    options.isOpenCallback = []() {
        return true;
    };

    LuaSandbox sandbox;
    emit finished(sandbox.execute(m_script, options));
}

} // namespace ComAssistant
