/**
 * @file TestSendQueue.h
 * @brief 发送队列与调度器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTSENDQUEUE_H
#define TESTSENDQUEUE_H

#include <QObject>
#include <QTest>

/**
 * @brief 发送队列与调度器可靠性测试。
 *
 * 这些测试覆盖第三阶段发送背压的核心边界：容量限制、失败保持、
 * 取消清空，以及调度器把底层写入结果正确反馈给队列。
 */
class TestSendQueue : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 非空数据应按 FIFO 顺序入队并可查看队首。
     */
    void testQueueAcceptsDataAndKeepsFifoOrder();

    /**
     * @brief 空数据应被拒绝，避免产生无意义发送任务。
     */
    void testQueueRejectsEmptyPayload();

    /**
     * @brief 达到条数容量时应拒绝新任务，并保持已有队列不变。
     */
    void testQueueRejectsWhenItemCapacityReached();

    /**
     * @brief 发送失败时队首必须保留，发送成功时才弹出。
     */
    void testQueueKeepsFailedHeadUntilSuccess();

    /**
     * @brief 队列应记录队首部分写入偏移，并只暴露剩余 payload。
     */
    void testQueueTracksHeadWriteProgress();

    /**
     * @brief 取消队列应清空所有待发送任务并返回取消数量。
     */
    void testQueueCancelAllClearsPendingItems();

    /**
     * @brief 调度器写入成功后应弹出队首并发出完成信号。
     */
    void testDispatcherCompletesSuccessfulWrite();

    /**
     * @brief 调度器遇到部分写入时应继续发送剩余数据，直到完整 payload 被接受。
     */
    void testDispatcherCompletesOnlyAfterAllPartialWritesFinish();

    /**
     * @brief 调度器部分写入后失败时应保留队首，并从剩余字节继续重试。
     */
    void testDispatcherRetriesRemainingBytesAfterPartialWriteFailure();

    /**
     * @brief 调度器遇到 0 字节写入时应按停滞失败处理，避免误报成功或死循环。
     */
    void testDispatcherTreatsZeroByteWriteAsStalledFailure();

    /**
     * @brief 调度器遇到底层返回超过剩余长度的异常值时应保留队首。
     */
    void testDispatcherRejectsWriteCountLargerThanRemainingPayload();

    /**
     * @brief 调度器写入失败时应保留队首并发出失败信号。
     */
    void testDispatcherKeepsHeadWhenWriteFails();
};

#endif // TESTSENDQUEUE_H
