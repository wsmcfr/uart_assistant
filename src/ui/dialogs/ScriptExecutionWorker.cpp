/**
 * @file ScriptExecutionWorker.cpp
 * @brief 脚本编辑器后台执行 worker 实现
 */

#include "ScriptExecutionWorker.h"

namespace ComAssistant {

ScriptExecutionWorker::ScriptExecutionWorker(QString script,
                                             InterruptCallback interruptCallback,
                                             QObject* parent)
    : QObject(parent)
    , m_script(std::move(script))
    , m_interruptCallback(std::move(interruptCallback))
{
}

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
