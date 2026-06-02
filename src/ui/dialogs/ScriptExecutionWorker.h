/**
 * @file ScriptExecutionWorker.h
 * @brief 脚本编辑器后台执行 worker
 */

#ifndef COMASSISTANT_SCRIPTEXECUTIONWORKER_H
#define COMASSISTANT_SCRIPTEXECUTIONWORKER_H

#include "core/script/LuaSandbox.h"

#include <QObject>
#include <QByteArray>
#include <QString>

#include <functional>

namespace ComAssistant {

/**
 * @brief 脚本后台执行 worker。
 *
 * 该对象会被移动到专用 QThread 中运行，只负责构造 LuaSandboxOptions 并执行脚本。
 * worker 不访问任何 UI 控件，也不直接持有串口或主窗口对象；所有发送请求和执行结果
 * 都通过信号交回 ScriptEditorDialog，由 UI 线程统一处理。
 */
class ScriptExecutionWorker : public QObject
{
    Q_OBJECT

public:
    using InterruptCallback = std::function<bool()>;

    /**
     * @brief 创建脚本后台执行 worker。
     * @param script 待执行的 Lua 脚本文本，会在构造时复制，避免后台读取 UI 控件。
     * @param interruptCallback 外部取消回调，由 Lua hook 周期性查询。
     * @param parent Qt 父对象；移动到线程前通常保持为空。
     */
    explicit ScriptExecutionWorker(QString script,
                                   InterruptCallback interruptCallback,
                                   QObject* parent = nullptr);

public slots:
    /**
     * @brief 在线程中执行脚本。
     *
     * 主要流程是配置 LuaSandbox 的资源限制、通信发送回调和取消回调，
     * 然后执行脚本并通过 finished() 返回结构化结果。
     */
    void run();

signals:
    /**
     * @brief 请求 UI 线程发送一段脚本产生的数据。
     * @param data 脚本通过 serial.send/serial.sendHex 产生的原始字节。
     */
    void sendRequested(const QByteArray& data);

    /**
     * @brief 脚本执行结束信号。
     * @param result LuaSandbox 返回的结构化执行结果。
     */
    void finished(const ComAssistant::LuaSandboxResult& result);

private:
    QString m_script;                    ///< 后台线程要执行的 Lua 脚本文本
    InterruptCallback m_interruptCallback; ///< 外部取消标记读取函数
};

} // namespace ComAssistant

#endif // COMASSISTANT_SCRIPTEXECUTIONWORKER_H
