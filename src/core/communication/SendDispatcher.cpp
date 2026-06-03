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
        const QByteArray remainingPayload = m_queue.headRemainingPayload();
        if (remainingPayload.isEmpty()) {
            /*
             * 如果队首已没有剩余字节，说明上一次部分写入刚好写完但还没
             * 走到完成分支。这里直接完成队首，保证恢复调度时状态能收敛。
             */
            if (m_queue.completeHead(SendCompletion::success())) {
                m_lastError.clear();
                emit itemCompleted(item.id, item.payload);
                emitQueueChanged();
                continue;
            }

            m_lastError = m_queue.lastError().isEmpty()
                ? tr("发送任务状态异常")
                : m_queue.lastError();
            emit itemFailed(item.id, item.payload, m_lastError);
            emitQueueChanged();
            return;
        }

        const qint64 written = m_writeHandler(remainingPayload);
        if (written < 0) {
            /*
             * 底层明确失败时保留队首。这里先复制 item，是为了 completeHead()
             * 更新队首 attemptCount 后仍能用原始 payload 发出失败信号。
             */
            m_lastError = resolveWriteError(tr("发送失败"));
            m_queue.completeHead(SendCompletion::failed(m_lastError));
            emit itemFailed(item.id, item.payload, m_lastError);
            emitQueueChanged();
            return;
        }

        if (written == 0) {
            /*
             * 0 字节写入不会推进状态。如果继续循环会形成忙等，如果当成
             * 成功则会丢整包；因此明确按停滞失败处理，等待上层恢复后重试。
             */
            m_lastError = resolveWriteError(tr("底层写入返回 0 字节，发送停滞"));
            m_queue.completeHead(SendCompletion::failed(m_lastError));
            emit itemFailed(item.id, item.payload, m_lastError);
            emitQueueChanged();
            return;
        }

        if (!m_queue.markHeadBytesWritten(written)) {
            /*
             * 返回值超过剩余长度等异常会由队列记录具体错误。这里保留
             * 队首并通知上层，避免把未知后端行为误判为发送成功。
             */
            m_lastError = m_queue.lastError().isEmpty()
                ? tr("发送写入状态异常")
                : m_queue.lastError();
            emit itemFailed(item.id, item.payload, m_lastError);
            emitQueueChanged();
            return;
        }

        if (m_queue.peek().bytesWritten < m_queue.peek().payload.size()) {
            /*
             * 部分写入是正常情况。调度器继续 while 循环，下一轮只把剩余
             * 尾部传给底层 write()，避免重复发送已被接受的前缀。
             */
            emitQueueChanged();
            continue;
        }

        if (!m_queue.completeHead(SendCompletion::success())) {
            m_lastError = m_queue.lastError().isEmpty()
                ? tr("发送任务无法完成")
                : m_queue.lastError();
            emit itemFailed(item.id, item.payload, m_lastError);
            emitQueueChanged();
            return;
        }

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

QString SendDispatcher::resolveWriteError(const QString& fallback) const
{
    /*
     * 不同通信后端的 write() 只提供数字结果，真实原因通常保存在
     * lastError()。这里集中处理错误文本，避免每个失败分支重复兜底逻辑。
     */
    QString error = m_errorProvider ? m_errorProvider() : QString();
    if (error.isEmpty()) {
        error = fallback;
    }
    return error;
}

} // namespace ComAssistant
