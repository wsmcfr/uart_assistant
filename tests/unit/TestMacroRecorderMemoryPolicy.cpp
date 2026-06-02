/**
 * @file TestMacroRecorderMemoryPolicy.cpp
 * @brief 宏录制内存策略回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMacroRecorderMemoryPolicy.h"

#include "core/macro/MacroRecorder.h"

using namespace ComAssistant;

void TestMacroRecorderMemoryPolicy::testRecorderDropsOldestEventsAfterLimit()
{
    MacroRecorder recorder;
    recorder.setRecordReceive(true);
    recorder.startRecording(QStringLiteral("bounded_macro"));

    /*
     * 宏录制的高风险路径是勾选“录制接收数据”后持续接收。这里用事件数量
     * 上限验证裁剪策略：超过上限后删除最早事件，保留最新事件，避免录制器
     * 成为另一个无限增长的接收历史。
     */
    const int limit = MacroRecorder::maxRecordedEvents();
    for (int index = 0; index < limit + 12; ++index) {
        recorder.recordReceive(QStringLiteral("RX_%1").arg(index).toUtf8());
    }

    QCOMPARE(recorder.eventCount(), limit);

    const MacroData macro = recorder.stopRecording();
    QCOMPARE(macro.events.size(), limit);
    QVERIFY2(!macro.events.isEmpty(), "停止录制后应返回裁剪后的最近事件。");
    QVERIFY2(!macro.events.first().data.contains("RX_0"),
             "超过上限后最早的接收事件应被删除。");
    QVERIFY2(macro.events.last().data.contains(QStringLiteral("RX_%1").arg(limit + 11).toUtf8()),
             "裁剪策略必须保留最新接收事件。");
}

void TestMacroRecorderMemoryPolicy::testRecorderDropsOldestPayloadAfterByteLimit()
{
    MacroRecorder recorder;
    recorder.setRecordReceive(true);
    recorder.startRecording(QStringLiteral("bounded_bytes_macro"));

    /*
     * 字节上限用于约束“少量但很大的接收事件”。这里每个事件约 1MB，
     * 连续写入超过 4MB 后应从最早事件开始裁剪，同时保留最近 payload。
     */
    const QByteArray firstPayload = QByteArray("FIRST_") + QByteArray(1024 * 1024, 'A');
    const QByteArray middlePayload = QByteArray("MIDDLE_") + QByteArray(1024 * 1024, 'B');
    const QByteArray latestPayload = QByteArray("LATEST_") + QByteArray(1024 * 1024, 'C');

    recorder.recordReceive(firstPayload);
    recorder.recordReceive(middlePayload);
    recorder.recordReceive(middlePayload);
    recorder.recordReceive(middlePayload);
    recorder.recordReceive(latestPayload);

    const MacroData macro = recorder.stopRecording();
    QVERIFY2(!macro.events.isEmpty(), "字节裁剪后仍应保留最近的可保存事件。");
    QVERIFY2(macro.events.first().data.startsWith("MIDDLE_"),
             "超过字节上限后，应优先删除最早 payload。");
    QVERIFY2(macro.events.last().data.startsWith("LATEST_"),
             "字节裁剪必须保留最新 payload。");
}

void TestMacroRecorderMemoryPolicy::testRecorderSkipsSingleOversizedPayload()
{
    MacroRecorder recorder;
    recorder.setRecordReceive(true);
    recorder.startRecording(QStringLiteral("oversized_macro"));

    /*
     * 单条接收数据本身超过上限时，无法通过删除更早事件让它变小。
     * 正确行为是丢弃这条超大事件，避免保存出的宏马上突破内存策略。
     */
    const QByteArray oversizedPayload(static_cast<int>(MacroRecorder::maxRecordedBytes()) + 1, 'X');
    recorder.recordReceive(oversizedPayload);

    QCOMPARE(recorder.eventCount(), 0);

    const MacroData macro = recorder.stopRecording();
    QVERIFY2(macro.events.isEmpty(), "单条超大事件不应写入最终宏数据。");
}
