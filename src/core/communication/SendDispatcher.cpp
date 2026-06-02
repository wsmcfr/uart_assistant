/**
 * @file SendDispatcher.cpp
 * @brief 发送队列调度器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "SendDispatcher.h"

namespace ComAssistant {

SendDispatcher::SendDispatcher(QObject* parent)
    : QObject(parent)
    , m_queue(SendQueueOptions())
{
    /*
     * 默认队列使用 SendQueue 的安全容量。控制器如果需要按通信类型调整
     * 背压边界，可以在创建后调用 setQueueOptions()。
     */
}

void SendDispatcher::setQueueOptions(const SendQueueOptions& options)
{
    /*
     * 容量策略改变时清空旧队列，避免旧任务在更严格的新容量下处于
     * “不可重新入队但仍存在”的模糊状态。
     */
    m_queue = SendQueue(options);
    m_lastError.clear();
    emitQueueChanged();
}

void SendDispatcher::setWriteHandler(WriteHandler handler)
{
    /*
     * 写入回调不拥有通信对象。MainWindowCommunicationController 关闭连接
     * 前会先取消队列，再清空回调，避免调度器调用悬空对象。
     */
    m_writeHandler = std::move(handler);
}

void SendDispatcher::setErrorProvider(ErrorProvider provider)
{
    /*
     * 错误提供器和写入回调同样不拥有通信对象。关闭连接时控制器会清空
     * 它，避免调度器在失败路径读取已释放对象的 lastError()。
     */
    m_errorProvider = std::move(provider);
}

SendEnqueueResult SendDispatcher::enqueue(const QByteArray& payload, const QString& source)
{
    const SendEnqueueResult result = m_queue.enqueue(payload, source);
    if (!result.accepted) {
        m_lastError = result.error;
    } else {
        m_lastError.clear();
    }
    emitQueueChanged();
    return result;
}

void SendDispatcher::dispatchPending()
{
    if (!m_writeHandler) {
        /*
         * 没有写入回调通常表示尚未连接或已经关闭。这里不弹出任务，确保
         * 上层恢复回调后可以继续发送同一批数据。
         */
        m_lastError = tr("发送通道不可用");
        if (m_queue.hasPending()) {
            const SendQueueItem item = m_queue.peek();
            m_queue.completeHead(SendCompletion::failed(m_lastError));
            emit itemFailed(item.id, item.payload, m_lastError);
            emitQueueChanged();
        }
        return;
    }

    while (m_queue.hasPending()) {
        const SendQueueItem item = m_queue.peek();
        const qint64 written = m_writeHandler(item.payload);
        if (written < 0) {
            /*
             * 底层明确失败时保留队首。这里先复制 item，是为了 completeHead()
             * 更新队首 attemptCount 后仍能用原始 payload 发出失败信号。
             */
            m_lastError = m_errorProvider ? m_errorProvider() : QString();
            if (m_lastError.isEmpty()) {
                m_lastError = tr("发送失败");
            }
            m_queue.completeHead(SendCompletion::failed(m_lastError));
            emit itemFailed(item.id, item.payload, m_lastError);
            emitQueueChanged();
            return;
        }

        m_queue.completeHead(SendCompletion::success());
        m_lastError.clear();
        emit itemCompleted(item.id, item.payload);
        emitQueueChanged();
    }
}

int SendDispatcher::cancelAll(const QString& reason)
{
    const int cancelled = m_queue.cancelAll(reason);
    m_lastError = reason;
    emit queueCancelled(cancelled, reason);
    emitQueueChanged();
    return cancelled;
}

bool SendDispatcher::hasPending() const
{
    return m_queue.hasPending();
}

int SendDispatcher::pendingCount() const
{
    return m_queue.size();
}

const SendQueueItem& SendDispatcher::peekPending() const
{
    return m_queue.peek();
}

QString SendDispatcher::lastError() const
{
    return m_lastError.isEmpty() ? m_queue.lastError() : m_lastError;
}

void SendDispatcher::emitQueueChanged()
{
    emit queueChanged(m_queue.size(), m_queue.queuedBytes());
}

} // namespace ComAssistant
