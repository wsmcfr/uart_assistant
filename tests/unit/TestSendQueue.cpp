/**
 * @file TestSendQueue.cpp
 * @brief 发送队列与调度器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestSendQueue.h"

#include "core/communication/SendDispatcher.h"
#include "core/communication/SendQueue.h"

#include <QSignalSpy>
#include <QtTest>

using namespace ComAssistant;

void TestSendQueue::testQueueAcceptsDataAndKeepsFifoOrder()
{
    /*
     * 队列是所有发送路径的入口，因此必须保证普通发送、脚本发送和
     * 文件块发送进入队列后仍按用户触发顺序消费。
     */
    SendQueue queue;

    const SendEnqueueResult first = queue.enqueue(QByteArray("first"), QStringLiteral("manual"));
    const SendEnqueueResult second = queue.enqueue(QByteArray("second"), QStringLiteral("script"));

    QVERIFY(first.accepted);
    QVERIFY(second.accepted);
    QCOMPARE(queue.size(), 2);
    QVERIFY(queue.hasPending());
    QCOMPARE(queue.peek().id, first.itemId);
    QCOMPARE(queue.peek().payload, QByteArray("first"));
    QCOMPARE(queue.peek().source, QStringLiteral("manual"));
}

void TestSendQueue::testQueueRejectsEmptyPayload()
{
    /*
     * 空负载通常来自 UI 输入为空或脚本误触发。直接拒绝可以避免调度器
     * 产生“成功发送 0 字节”的假进度。
     */
    SendQueue queue;

    const SendEnqueueResult result = queue.enqueue(QByteArray(), QStringLiteral("manual"));

    QVERIFY(!result.accepted);
    QVERIFY(result.error.contains(QStringLiteral("空")));
    QCOMPARE(queue.size(), 0);
}

void TestSendQueue::testQueueRejectsWhenItemCapacityReached()
{
    /*
     * 条数容量是最直观的背压保护。容量满时不能覆盖旧任务，否则文件块
     * 或脚本连续发送会悄悄丢数据。
     */
    SendQueueOptions options;
    options.maxItems = 1;
    SendQueue queue(options);

    QVERIFY(queue.enqueue(QByteArray("first"), QStringLiteral("manual")).accepted);
    const SendEnqueueResult rejected = queue.enqueue(QByteArray("second"), QStringLiteral("manual"));

    QVERIFY(!rejected.accepted);
    QVERIFY(rejected.error.contains(QStringLiteral("队列")));
    QCOMPARE(queue.size(), 1);
    QCOMPARE(queue.peek().payload, QByteArray("first"));
}

void TestSendQueue::testQueueKeepsFailedHeadUntilSuccess()
{
    /*
     * 发送失败时保留队首是可靠性的关键：断线或底层 write() 失败后，
     * 用户可以恢复连接并重试同一批数据，而不是从下一包继续造成缺口。
     */
    SendQueue queue;
    const SendEnqueueResult first = queue.enqueue(QByteArray("first"), QStringLiteral("file"));
    const SendEnqueueResult second = queue.enqueue(QByteArray("second"), QStringLiteral("file"));

    QVERIFY(first.accepted);
    QVERIFY(second.accepted);
    QVERIFY(!queue.completeHead(SendCompletion::failed(QStringLiteral("write failed"))));
    QCOMPARE(queue.size(), 2);
    QCOMPARE(queue.peek().id, first.itemId);
    QCOMPARE(queue.peek().lastError, QStringLiteral("write failed"));
    QCOMPARE(queue.peek().attemptCount, 1);

    QVERIFY(queue.completeHead(SendCompletion::success()));
    QCOMPARE(queue.size(), 1);
    QCOMPARE(queue.peek().id, second.itemId);
}

void TestSendQueue::testQueueCancelAllClearsPendingItems()
{
    /*
     * 关闭连接或用户取消传输时必须能一次性清空待发送任务，并把取消数量
     * 暴露给上层用于日志或诊断。
     */
    SendQueue queue;
    QVERIFY(queue.enqueue(QByteArray("first"), QStringLiteral("manual")).accepted);
    QVERIFY(queue.enqueue(QByteArray("second"), QStringLiteral("manual")).accepted);

    QCOMPARE(queue.cancelAll(QStringLiteral("user cancel")), 2);
    QCOMPARE(queue.size(), 0);
    QVERIFY(!queue.hasPending());
    QVERIFY(queue.lastError().contains(QStringLiteral("user cancel")));
}

void TestSendQueue::testDispatcherCompletesSuccessfulWrite()
{
    /*
     * 调度器把队列任务转成实际 write() 调用。写入返回非负值时，队首应
     * 被移除并通知上层“本地发送管道已接受”。
     */
    SendDispatcher dispatcher;
    QByteArray writtenPayload;
    dispatcher.setWriteHandler([&writtenPayload](const QByteArray& payload) {
        writtenPayload = payload;
        return static_cast<qint64>(payload.size());
    });

    QSignalSpy completedSpy(&dispatcher, SIGNAL(itemCompleted(qint64,QByteArray)));

    QVERIFY(dispatcher.enqueue(QByteArray("hello"), QStringLiteral("manual")).accepted);
    dispatcher.dispatchPending();

    QCOMPARE(writtenPayload, QByteArray("hello"));
    QCOMPARE(dispatcher.pendingCount(), 0);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.takeFirst().at(1).toByteArray(), QByteArray("hello"));
}

void TestSendQueue::testDispatcherKeepsHeadWhenWriteFails()
{
    /*
     * 底层 write() 返回负数表示没有可靠接收当前数据。调度器应停止消费，
     * 保留队首并发出失败信号，等待上层恢复后再重试。
     */
    SendDispatcher dispatcher;
    dispatcher.setWriteHandler([](const QByteArray&) {
        return static_cast<qint64>(-1);
    });

    QSignalSpy failedSpy(&dispatcher, SIGNAL(itemFailed(qint64,QByteArray,QString)));

    const SendEnqueueResult result = dispatcher.enqueue(QByteArray("hello"), QStringLiteral("manual"));
    QVERIFY(result.accepted);
    dispatcher.dispatchPending();

    QCOMPARE(dispatcher.pendingCount(), 1);
    QVERIFY(dispatcher.hasPending());
    QCOMPARE(dispatcher.peekPending().id, result.itemId);
    QCOMPARE(dispatcher.peekPending().attemptCount, 1);
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.takeFirst().at(1).toByteArray(), QByteArray("hello"));
}
