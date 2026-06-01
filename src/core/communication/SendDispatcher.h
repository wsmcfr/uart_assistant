/**
 * @file SendDispatcher.h
 * @brief 发送队列调度器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_SENDDISPATCHER_H
#define COMASSISTANT_SENDDISPATCHER_H

#include "SendQueue.h"

#include <QObject>

#include <functional>

namespace ComAssistant {

/**
 * @brief 发送队列调度器。
 *
 * SendDispatcher 负责把 SendQueue 中的任务串行写入底层通信对象。它不
 * 持有通信对象所有权，只保存一个写入回调，因此可以被主窗口控制器用
 * 于串口、TCP Client、普通 UDP 或后续其他字节流出口。
 */
class SendDispatcher : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 底层写入函数类型。
     *
     * 回调返回值遵循 ICommunication::write() 语义：非负表示本地写入
     * 被接受，负数表示失败。
     */
    using WriteHandler = std::function<qint64(const QByteArray&)>;

    /**
     * @brief 构造发送调度器。
     * @param parent Qt 父对象。
     */
    explicit SendDispatcher(QObject* parent = nullptr);

    /**
     * @brief 设置队列容量选项。
     * @param options 新队列容量。调用会清空旧队列，避免旧任务跨策略残留。
     */
    void setQueueOptions(const SendQueueOptions& options);

    /**
     * @brief 设置底层写入回调。
     * @param handler 写入函数；传空函数会让调度器进入不可发送状态。
     */
    void setWriteHandler(WriteHandler handler);

    /**
     * @brief 添加发送任务。
     * @param payload 待发送原始字节。
     * @param source 发送来源标签。
     * @return 入队结果。
     */
    SendEnqueueResult enqueue(const QByteArray& payload, const QString& source = QString());

    /**
     * @brief 调度当前所有可发送任务。
     *
     * 调度器会连续消费队列，直到队列为空、写入失败或没有写入回调。
     * 写入失败时队首保持不动，方便恢复后重试。
     */
    void dispatchPending();

    /**
     * @brief 取消并清空待发送任务。
     * @param reason 取消原因。
     * @return 被取消任务数量。
     */
    int cancelAll(const QString& reason = QString());

    /**
     * @brief 判断是否存在待发送任务。
     * @return 有待发送任务返回 true。
     */
    bool hasPending() const;

    /**
     * @brief 获取待发送任务数量。
     * @return 当前队列长度。
     */
    int pendingCount() const;

    /**
     * @brief 获取队首待发送任务。
     * @return 队首任务引用；调用前需确保 hasPending()。
     */
    const SendItem& peekPending() const;

    /**
     * @brief 获取最近一次错误。
     * @return 调度器或队列最近一次错误文本。
     */
    QString lastError() const;

signals:
    /**
     * @brief 队列数量或字节数发生变化。
     * @param pendingItems 当前待发送任务数量。
     * @param pendingBytes 当前待发送 payload 总字节数。
     */
    void queueChanged(int pendingItems, qint64 pendingBytes);

    /**
     * @brief 单个任务写入成功。
     * @param itemId 任务编号。
     * @param payload 成功写入的原始字节。
     */
    void itemCompleted(qint64 itemId, const QByteArray& payload);

    /**
     * @brief 单个任务写入失败。
     * @param itemId 任务编号。
     * @param payload 失败任务的原始字节。
     * @param error 失败原因。
     */
    void itemFailed(qint64 itemId, const QByteArray& payload, const QString& error);

    /**
     * @brief 队列被取消。
     * @param cancelledCount 被清空的任务数量。
     * @param reason 取消原因。
     */
    void queueCancelled(int cancelledCount, const QString& reason);

private:
    /**
     * @brief 发出当前队列统计信号。
     */
    void emitQueueChanged();

private:
    SendQueue m_queue;          ///< 纯发送队列，负责容量和失败保持。
    WriteHandler m_writeHandler;///< 底层通信写入回调。
    QString m_lastError;        ///< 最近一次调度器错误。
};

} // namespace ComAssistant

#endif // COMASSISTANT_SENDDISPATCHER_H
