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

    SendQueueItem item;
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
         * 后续重试或诊断可以判断是否已经多次卡在同一 payload。bytesWritten
         * 不在失败时清零，因为部分写入后的恢复应从未写完的尾部继续，
         * 避免接收端看到重复的已接受前缀。
         */
        SendQueueItem& item = m_items.head();
        item.attemptCount++;
        item.lastError = completion.error.isEmpty()
            ? QStringLiteral("发送失败")
            : completion.error;
        m_lastError = item.lastError;
        return false;
    }

    if (m_items.head().bytesWritten < m_items.head().payload.size()) {
        /*
         * 调度器只有在整包所有字节都被底层接受后才允许完成队首。这里
         * 作为队列层的最后防线，防止未来其他调用者绕过 markHeadBytesWritten()
         * 直接 completeHead(success) 造成部分写入丢尾包。
         */
        SendQueueItem& item = m_items.head();
        item.attemptCount++;
        item.lastError = QStringLiteral("发送任务尚未完整写入，不能标记完成");
        m_lastError = item.lastError;
        return false;
    }

    const SendQueueItem item = m_items.dequeue();
    /*
     * m_queuedBytes 统计的是“仍需写入底层”的剩余字节数。正常完整写入
     * 后 remaining 为 0；若未来有调用者在偏移未满时绕过完成保护，这里
     * 仍按剩余字节扣减，避免统计出现负数或重复扣整包。
     */
    const qint64 remainingBytes = item.payload.size() - item.bytesWritten;
    m_queuedBytes -= qMax<qint64>(0, remainingBytes);
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

const SendQueueItem& SendQueue::peek() const
{
    return m_items.head();
}

QByteArray SendQueue::headRemainingPayload() const
{
    /*
     * 调度器只应把未写完的尾部交给底层 write()。这里集中计算剩余片段，
     * 可以保证部分写入后的失败重试不会重复发送已经被接收端接受的前缀。
     */
    if (m_items.isEmpty()) {
        return QByteArray();
    }

    const SendQueueItem& item = m_items.head();
    const qint64 safeOffset = qBound<qint64>(0, item.bytesWritten, item.payload.size());
    return item.payload.mid(static_cast<int>(safeOffset));
}

bool SendQueue::markHeadBytesWritten(qint64 bytes)
{
    /*
     * 该函数是队首写入进度的唯一推进入口，同时维护 m_queuedBytes。
     * 只有确认底层接受了正数且不超过剩余长度的字节，才允许更新偏移。
     */
    if (m_items.isEmpty()) {
        m_lastError = QStringLiteral("发送队列为空，无法推进写入偏移");
        return false;
    }

    if (bytes <= 0) {
        m_lastError = QStringLiteral("写入字节数必须大于 0");
        return false;
    }

    SendQueueItem& item = m_items.head();
    const qint64 remaining = item.payload.size() - item.bytesWritten;
    if (bytes > remaining) {
        /*
         * 底层 write() 理论上不应返回大于传入 payload 的值。这里拒绝推进，
         * 让调度器把它当作通信后端异常处理，而不是把队列统计带偏。
         */
        item.attemptCount++;
        item.lastError = QStringLiteral("底层写入返回值超过剩余字节数");
        m_lastError = item.lastError;
        return false;
    }

    item.bytesWritten += bytes;
    m_queuedBytes -= bytes;
    if (m_queuedBytes < 0) {
        /*
         * 正常路径不会小于 0。保留兜底是为了防止后续维护时手动调整
         * bytesWritten 或 payload 导致队列统计进入不可恢复状态。
         */
        m_queuedBytes = 0;
    }
    item.lastError.clear();
    m_lastError.clear();
    return true;
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
