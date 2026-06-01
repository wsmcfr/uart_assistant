/**
 * @file SendQueue.cpp
 * @brief 统一发送队列与背压策略实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "SendQueue.h"

#include <QtGlobal>

namespace ComAssistant {

SendCompletion SendCompletion::success()
{
    /*
     * 成功结果不携带错误文本，调度器收到后会让队列弹出队首任务。
     */
    SendCompletion completion;
    completion.ok = true;
    return completion;
}

SendCompletion SendCompletion::failed(const QString& error)
{
    /*
     * 失败结果保留上层给出的错误。如果底层没有错误文本，队列实现会补
     * 一个通用错误，避免 lastError 为空导致 UI 无法给出原因。
     */
    SendCompletion completion;
    completion.ok = false;
    completion.error = error;
    return completion;
}

SendQueue::SendQueue(const SendQueueOptions& options)
    : m_options(options)
{
    /*
     * 容量配置来自控制器或测试。这里做一次下限保护，避免 0 或负数让
     * 队列永久不可用；字节上限至少为 1，保证单字节命令仍可发送。
     */
    m_options.maxItems = qMax(1, m_options.maxItems);
    m_options.maxBytes = qMax<qint64>(1, m_options.maxBytes);
}

SendEnqueueResult SendQueue::enqueue(const QByteArray& payload, const QString& source)
{
    SendEnqueueResult result;

    if (payload.isEmpty()) {
        m_lastError = QStringLiteral("发送数据为空，已拒绝入队");
        result.error = m_lastError;
        return result;
    }

    if (m_items.size() >= m_options.maxItems) {
        m_lastError = QStringLiteral("发送队列已满，已拒绝新的发送任务");
        result.error = m_lastError;
        return result;
    }

    if (m_queuedBytes + payload.size() > m_options.maxBytes) {
        m_lastError = QStringLiteral("发送队列字节容量不足，已拒绝新的发送任务");
        result.error = m_lastError;
        return result;
    }

    SendItem item;
    item.id = nextId();
    item.payload = payload;
    item.source = source;
    m_items.enqueue(item);
    m_queuedBytes += payload.size();

    result.accepted = true;
    result.itemId = item.id;
    m_lastError.clear();
    return result;
}

bool SendQueue::completeHead(const SendCompletion& completion)
{
    if (m_items.isEmpty()) {
        m_lastError = QStringLiteral("发送队列为空，无法完成队首任务");
        return false;
    }

    if (!completion.ok) {
        /*
         * 失败时只更新队首元数据，不移动队列。attemptCount 记录失败次数，
         * 后续重试或诊断可以判断是否已经多次卡在同一 payload。
         */
        SendItem& item = m_items.head();
        item.attemptCount++;
        item.lastError = completion.error.isEmpty()
            ? QStringLiteral("发送失败")
            : completion.error;
        m_lastError = item.lastError;
        return false;
    }

    const SendItem item = m_items.dequeue();
    m_queuedBytes -= item.payload.size();
    if (m_queuedBytes < 0) {
        /*
         * 理论上不会出现负数，但这里做兜底，避免未来维护者手动调整
         * payload 统计时把队列带入不可恢复状态。
         */
        m_queuedBytes = 0;
    }
    m_lastError.clear();
    return true;
}

int SendQueue::cancelAll(const QString& reason)
{
    const int cancelled = m_items.size();
    m_items.clear();
    m_queuedBytes = 0;
    m_lastError = reason;
    return cancelled;
}

bool SendQueue::hasPending() const
{
    return !m_items.isEmpty();
}

const SendItem& SendQueue::peek() const
{
    return m_items.head();
}

int SendQueue::size() const
{
    return m_items.size();
}

qint64 SendQueue::queuedBytes() const
{
    return m_queuedBytes;
}

QString SendQueue::lastError() const
{
    return m_lastError;
}

SendQueueOptions SendQueue::options() const
{
    return m_options;
}

qint64 SendQueue::nextId()
{
    return m_nextId++;
}

} // namespace ComAssistant
