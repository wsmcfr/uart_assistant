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

    QVERIFY(queue.markHeadBytesWritten(queue.peek().payload.size()));
    QVERIFY(queue.completeHead(SendCompletion::success()));
    QCOMPARE(queue.size(), 1);
    QCOMPARE(queue.peek().id, second.itemId);
}

void TestSendQueue::testQueueTracksHeadWriteProgress()
{
    /*
     * SendQueue 现在不仅保存待发送任务，还保存队首已经被底层接受的
     * 字节偏移。这样调度器在部分写入后遇到失败时，可以保留原始任务
     * 用于信号和诊断，同时只重试未写完的尾部。
     */
    SendQueue queue;
    const SendEnqueueResult first = queue.enqueue(QByteArray("partial"), QStringLiteral("manual"));
    QVERIFY(first.accepted);

    QCOMPARE(queue.headRemainingPayload(), QByteArray("partial"));
    QCOMPARE(queue.queuedBytes(), static_cast<qint64>(7));
    QVERIFY(queue.markHeadBytesWritten(3));
    QCOMPARE(queue.peek().bytesWritten, static_cast<qint64>(3));
    QCOMPARE(queue.headRemainingPayload(), QByteArray("tial"));
    QCOMPARE(queue.queuedBytes(), static_cast<qint64>(4));

    QVERIFY(!queue.markHeadBytesWritten(100));
    QCOMPARE(queue.peek().bytesWritten, static_cast<qint64>(3));
    QCOMPARE(queue.queuedBytes(), static_cast<qint64>(4));
    QVERIFY(queue.peek().lastError.contains(QStringLiteral("超过")));

    QVERIFY(queue.markHeadBytesWritten(4));
    QCOMPARE(queue.headRemainingPayload(), QByteArray());
    QCOMPARE(queue.queuedBytes(), static_cast<qint64>(0));
    QVERIFY(queue.completeHead(SendCompletion::success()));
    QVERIFY(!queue.hasPending());
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

void TestSendQueue::testDispatcherCompletesOnlyAfterAllPartialWritesFinish()
{
    /*
     * 串口、TCP 和部分系统 Socket API 都可能只接受 payload 的前半段。
     * 调度器必须记录已写偏移，并在同一次调度中继续把剩余字节写完；
     * 如果把第一次部分写入当成完成，文件块和脚本长命令都会丢尾包。
     */
    SendDispatcher dispatcher;
    QVector<QByteArray> writeCalls;
    dispatcher.setWriteHandler([&writeCalls](const QByteArray& payload) {
        writeCalls.append(payload);

        /*
         * 第一次只接受 "ABC"，第二次只接受 "DE"，第三次接受最后的
         * "FG"。测试关注调度器传入的剩余片段，而不是底层自行缓存。
         */
        if (writeCalls.size() == 1) {
            return static_cast<qint64>(3);
        }
        if (writeCalls.size() == 2) {
            return static_cast<qint64>(2);
        }
        return static_cast<qint64>(payload.size());
    });

    QSignalSpy completedSpy(&dispatcher, SIGNAL(itemCompleted(qint64,QByteArray)));
    QSignalSpy failedSpy(&dispatcher, SIGNAL(itemFailed(qint64,QByteArray,QString)));

    QVERIFY(dispatcher.enqueue(QByteArray("ABCDEFG"), QStringLiteral("manual")).accepted);
    dispatcher.dispatchPending();

    QCOMPARE(writeCalls.size(), 3);
    QCOMPARE(writeCalls.at(0), QByteArray("ABCDEFG"));
    QCOMPARE(writeCalls.at(1), QByteArray("DEFG"));
    QCOMPARE(writeCalls.at(2), QByteArray("FG"));
    QCOMPARE(dispatcher.pendingCount(), 0);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.takeFirst().at(1).toByteArray(), QByteArray("ABCDEFG"));
    QCOMPARE(failedSpy.count(), 0);
}

void TestSendQueue::testDispatcherRetriesRemainingBytesAfterPartialWriteFailure()
{
    /*
     * 部分写入后如果下一次 write() 失败，已经被底层接受的前缀不能在
     * 重试时再次发送，否则接收端会看到重复字节。调度器应保留原始队首
     * 用于完成/失败信号，同时内部只重试未写完的尾部。
     */
    SendDispatcher dispatcher;
    QVector<QByteArray> writeCalls;
    dispatcher.setWriteHandler([&writeCalls](const QByteArray& payload) {
        writeCalls.append(payload);
        if (writeCalls.size() == 1) {
            return static_cast<qint64>(2);
        }
        if (writeCalls.size() == 2) {
            return static_cast<qint64>(-1);
        }
        return static_cast<qint64>(payload.size());
    });
    dispatcher.setErrorProvider([]() {
        return QStringLiteral("temporary transport error");
    });

    QSignalSpy completedSpy(&dispatcher, SIGNAL(itemCompleted(qint64,QByteArray)));
    QSignalSpy failedSpy(&dispatcher, SIGNAL(itemFailed(qint64,QByteArray,QString)));

    const SendEnqueueResult result = dispatcher.enqueue(QByteArray("HELLO"), QStringLiteral("manual"));
    QVERIFY(result.accepted);

    dispatcher.dispatchPending();
    QCOMPARE(dispatcher.pendingCount(), 1);
    QVERIFY(dispatcher.hasPending());
    QCOMPARE(dispatcher.peekPending().id, result.itemId);
    QCOMPARE(dispatcher.peekPending().payload, QByteArray("HELLO"));
    QCOMPARE(dispatcher.peekPending().attemptCount, 1);
    QCOMPARE(writeCalls.size(), 2);
    QCOMPARE(writeCalls.at(0), QByteArray("HELLO"));
    QCOMPARE(writeCalls.at(1), QByteArray("LLO"));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.takeFirst().at(1).toByteArray(), QByteArray("HELLO"));
    QCOMPARE(completedSpy.count(), 0);

    dispatcher.dispatchPending();
    QCOMPARE(writeCalls.size(), 3);
    QCOMPARE(writeCalls.at(2), QByteArray("LLO"));
    QCOMPARE(dispatcher.pendingCount(), 0);
    QCOMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.takeFirst().at(1).toByteArray(), QByteArray("HELLO"));
}

void TestSendQueue::testDispatcherTreatsZeroByteWriteAsStalledFailure()
{
    /*
     * 0 字节写入既不是成功，也不是可推进的部分写入。若继续 while 循环
     * 会卡死；若按成功处理则直接丢整包。因此它应作为“写入停滞”失败，
     * 保留队首给上层恢复连接后重试。
     */
    SendDispatcher dispatcher;
    int writeCallCount = 0;
    dispatcher.setWriteHandler([&writeCallCount](const QByteArray&) {
        ++writeCallCount;
        return static_cast<qint64>(0);
    });

    QSignalSpy completedSpy(&dispatcher, SIGNAL(itemCompleted(qint64,QByteArray)));
    QSignalSpy failedSpy(&dispatcher, SIGNAL(itemFailed(qint64,QByteArray,QString)));

    QVERIFY(dispatcher.enqueue(QByteArray("stalled"), QStringLiteral("manual")).accepted);
    dispatcher.dispatchPending();

    QCOMPARE(writeCallCount, 1);
    QCOMPARE(dispatcher.pendingCount(), 1);
    QCOMPARE(dispatcher.peekPending().payload, QByteArray("stalled"));
    QCOMPARE(dispatcher.peekPending().attemptCount, 1);
    QVERIFY(dispatcher.lastError().contains(QStringLiteral("0"))
            || dispatcher.lastError().contains(QStringLiteral("停滞"))
            || dispatcher.lastError().contains(QStringLiteral("未写入")));
    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
}

void TestSendQueue::testDispatcherRejectsWriteCountLargerThanRemainingPayload()
{
    /*
     * 合法 write() 返回值不能超过本次传入的 payload 长度。若底层返回
     * 这种异常值，调度器不能为了“完成发送”而信任它，否则队列偏移和
     * 进度统计会被破坏。
     */
    SendDispatcher dispatcher;
    dispatcher.setWriteHandler([](const QByteArray& payload) {
        return static_cast<qint64>(payload.size() + 1);
    });

    QSignalSpy completedSpy(&dispatcher, SIGNAL(itemCompleted(qint64,QByteArray)));
    QSignalSpy failedSpy(&dispatcher, SIGNAL(itemFailed(qint64,QByteArray,QString)));

    QVERIFY(dispatcher.enqueue(QByteArray("overflow"), QStringLiteral("manual")).accepted);
    dispatcher.dispatchPending();

    QCOMPARE(dispatcher.pendingCount(), 1);
    QCOMPARE(dispatcher.peekPending().bytesWritten, static_cast<qint64>(0));
    QVERIFY(dispatcher.lastError().contains(QStringLiteral("超过")));
    QCOMPARE(completedSpy.count(), 0);
    QCOMPARE(failedSpy.count(), 1);
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
