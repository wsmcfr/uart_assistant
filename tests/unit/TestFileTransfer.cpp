/**
 * @file TestFileTransfer.cpp
 * @brief 文件传输核心逻辑单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TestFileTransfer.h"

#include <QByteArray>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QVector>
#include <QtTest>

#include "core/transfer/FileTransfer.h"
#include "core/utils/ChecksumUtils.h"

using namespace ComAssistant;

namespace {

/**
 * @brief 从字节数组指定偏移读取小端 16 位整数
 * @param data 待读取的数据包。
 * @param offset 字段起始偏移。
 * @return 按小端顺序还原出的 16 位无符号整数。
 */
quint16 readLe16(const QByteArray& data, int offset)
{
    return static_cast<quint16>(static_cast<quint8>(data[offset])) |
           static_cast<quint16>(static_cast<quint8>(data[offset + 1]) << 8);
}

/**
 * @brief 从字节数组指定偏移读取小端 32 位整数
 * @param data 待读取的数据包。
 * @param offset 字段起始偏移。
 * @return 按小端顺序还原出的 32 位无符号整数。
 */
quint32 readLe32(const QByteArray& data, int offset)
{
    return static_cast<quint32>(static_cast<quint8>(data[offset])) |
           (static_cast<quint32>(static_cast<quint8>(data[offset + 1])) << 8) |
           (static_cast<quint32>(static_cast<quint8>(data[offset + 2])) << 16) |
           (static_cast<quint32>(static_cast<quint8>(data[offset + 3])) << 24);
}

} // namespace

void TestFileTransfer::testRawChunkerBuildsExpectedChunks()
{
    /*
     * 裸流模式不增加协议头，只负责按用户设置的块大小分片。
     * 这里使用固定内容验证分片边界，避免最后一个不足整块的数据被丢失。
     */
    const QByteArray data("ABCDEFGHIJ");
    const QVector<QByteArray> chunks = RawFileTransfer::splitForTest(data, 4);

    QCOMPARE(chunks.size(), 3);
    QCOMPARE(chunks[0], QByteArray("ABCD"));
    QCOMPARE(chunks[1], QByteArray("EFGH"));
    QCOMPARE(chunks[2], QByteArray("IJ"));
}

void TestFileTransfer::testOtaHeaderUsesLittleEndianMetadata()
{
    /*
     * 文件头是 MCU 识别整次 OTA 的第一包，因此字段顺序和大小端必须稳定。
     * 该测试用固定数字检查小端编码，便于下位机直接按字节解析。
     */
    const QByteArray packet = OtaFileTransfer::buildHeaderPacketForTest(
        "OTA1", "Project_ota.bin", 10, 0x12345678, 128);

    QVERIFY(packet.size() >= 4 + 4 + 4 + 2 + 1);
    QCOMPARE(packet.left(4), QByteArray("OTA1"));
    QCOMPARE(readLe32(packet, 4), static_cast<quint32>(10));
    QCOMPARE(readLe32(packet, 8), static_cast<quint32>(0x12345678));
    QCOMPARE(readLe16(packet, 12), static_cast<quint16>(128));
    QCOMPARE(static_cast<quint8>(packet[14]), static_cast<quint8>(15));
    QCOMPARE(packet.mid(15), QByteArray("Project_ota.bin"));
}

void TestFileTransfer::testOtaDataPacketIncludesIndexLengthPayloadAndCrc()
{
    /*
     * 数据包必须让下位机知道当前块序号和载荷长度，并在包尾提供 CRC16。
     * 这里对除尾部 CRC 外的所有字节重新计算 CRC，验证打包逻辑没有遗漏字段。
     */
    const QByteArray payload("AB");
    const QByteArray packet = OtaFileTransfer::buildDataPacketForTest(2, payload);

    QCOMPARE(readLe32(packet, 0), static_cast<quint32>(2));
    QCOMPARE(readLe16(packet, 4), static_cast<quint16>(payload.size()));
    QCOMPARE(packet.mid(6, payload.size()), payload);

    const QByteArray crcSource = packet.left(packet.size() - 2);
    const quint16 expectedCrc = ChecksumUtils::crc16XMODEM(crcSource);
    QCOMPARE(readLe16(packet, packet.size() - 2), expectedCrc);
}

void TestFileTransfer::testOtaEndPacketCarriesDoneAndCrc32()
{
    /*
     * 结束包提供明确 DONE 标识，并再次携带整文件 CRC32，方便 MCU 在写入
     * 完成后进行最终一致性校验。
     */
    const QByteArray packet = OtaFileTransfer::buildEndPacketForTest(0x12345678);

    QCOMPARE(packet.left(4), QByteArray("DONE"));
    QCOMPARE(readLe32(packet, 4), static_cast<quint32>(0x12345678));
    QCOMPARE(packet.size(), 8);
}

void TestFileTransfer::testRawTransferWaitsForLocalSendResultBeforeNextChunk()
{
    /*
     * Raw 大文件发送不能在 emit sendData() 后立刻推进下一块，否则主窗口
     * 发送队列尚未确认本地写入时，文件传输进度会提前跳动甚至丢块。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    QCOMPARE(file.write(QByteArray("ABCDEFGH")), static_cast<qint64>(8));
    file.close();

    RawFileTransfer transfer;
    RawTransferOptions options;
    options.blockSize = 4;
    options.intervalMs = 0;
    transfer.setOptions(options);

    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startSend(file.fileName()));
    QTRY_COMPARE(sendSpy.count(), 1);
    QCOMPARE(sendSpy.at(0).at(0).toByteArray(), QByteArray("ABCD"));
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(0));
    QCOMPARE(completedSpy.count(), 0);

    transfer.notifyLocalSendResult(true, QString());
    QTRY_COMPARE(sendSpy.count(), 2);
    QCOMPARE(sendSpy.at(1).at(0).toByteArray(), QByteArray("EFGH"));
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(4));

    transfer.notifyLocalSendResult(true, QString());
    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(8));
    QCOMPARE(transfer.state(), TransferState::Completed);
}

void TestFileTransfer::testRawTransferFailsWhenLocalSendFails()
{
    /*
     * 主窗口发送队列拒绝或底层 write() 失败时，Raw 传输必须停止在当前块，
     * 不继续读取文件。这样用户恢复连接后能明确看到失败原因，而不是误以为
     * 大文件已经继续发送。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    QCOMPARE(file.write(QByteArray("ABCDEFGH")), static_cast<qint64>(8));
    file.close();

    RawFileTransfer transfer;
    RawTransferOptions options;
    options.blockSize = 4;
    options.intervalMs = 0;
    transfer.setOptions(options);

    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startSend(file.fileName()));
    QTRY_COMPARE(sendSpy.count(), 1);

    transfer.notifyLocalSendResult(false, QStringLiteral("queue rejected"));

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(transfer.state(), TransferState::Error);
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(0));
    QVERIFY(transfer.progress().errorMessage.contains(QStringLiteral("queue rejected")));
}

void TestFileTransfer::testOtaTransferWaitsForLocalSendResultBeforeNextPacket()
{
    /*
     * OTA 无 ACK 模式虽然不等设备 ACK，但仍必须等待本地发送队列确认。
     * 该测试用 4 字节文件和 2 字节块大小验证 header/data/data/end 都由
     * notifyLocalSendResult() 一步步推进。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    QCOMPARE(file.write(QByteArray("ABCD")), static_cast<qint64>(4));
    file.close();

    OtaFileTransfer transfer;
    OtaTransferOptions options;
    options.blockSize = 2;
    options.intervalMs = 0;
    options.waitAck = false;
    transfer.setOptions(options);

    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startSend(file.fileName()));
    QTRY_COMPARE(sendSpy.count(), 1);
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(0));

    transfer.notifyLocalSendResult(true, QString());
    QTRY_COMPARE(sendSpy.count(), 2);
    QCOMPARE(readLe32(sendSpy.at(1).at(0).toByteArray(), 0), static_cast<quint32>(0));
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(0));

    transfer.notifyLocalSendResult(true, QString());
    QTRY_COMPARE(sendSpy.count(), 3);
    QCOMPARE(readLe32(sendSpy.at(2).at(0).toByteArray(), 0), static_cast<quint32>(1));
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(2));

    transfer.notifyLocalSendResult(true, QString());
    QTRY_COMPARE(sendSpy.count(), 4);
    QCOMPARE(sendSpy.at(3).at(0).toByteArray().left(4), QByteArray("DONE"));
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(4));
    QCOMPARE(completedSpy.count(), 0);

    transfer.notifyLocalSendResult(true, QString());
    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(transfer.state(), TransferState::Completed);
}

void TestFileTransfer::testRawTransferUsesRunningPausedStates()
{
    /*
     * 第三阶段把文件传输 UI 所需状态收敛为 Running/Paused。该测试确保
     * 裸流发送不再只靠单独 bool 标记暂停，而是把暂停暴露到统一状态机。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    QCOMPARE(file.write(QByteArray("ABCDEFGH")), static_cast<qint64>(8));
    file.close();

    RawFileTransfer transfer;
    RawTransferOptions options;
    options.blockSize = 4;
    options.intervalMs = 50;
    transfer.setOptions(options);

    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startSend(file.fileName()));
    QTRY_COMPARE(sendSpy.count(), 1);
    QCOMPARE(transfer.state(), TransferState::Running);

    transfer.pause();
    QCOMPARE(transfer.state(), TransferState::Paused);

    transfer.resume();
    QCOMPARE(transfer.state(), TransferState::Running);

    transfer.cancel();
}

void TestFileTransfer::testRawTransferCancelTransitionsThroughCancelling()
{
    /*
     * Cancelling 是资源释放中的瞬时状态。即使最终会很快进入 Cancelled，
     * 信号中也应能看到该状态，方便 UI 禁用重复按钮并记录诊断。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    QCOMPARE(file.write(QByteArray("ABCDEFGH")), static_cast<qint64>(8));
    file.close();

    RawFileTransfer transfer;
    RawTransferOptions options;
    options.blockSize = 4;
    transfer.setOptions(options);

    QSignalSpy stateSpy(&transfer, &FileTransfer::stateChanged);

    QVERIFY(transfer.startSend(file.fileName()));
    transfer.cancel();

    QList<int> states;
    for (const QList<QVariant>& arguments : stateSpy) {
        states.append(static_cast<int>(arguments.at(0).value<TransferState>()));
    }

    QVERIFY(states.contains(static_cast<int>(TransferState::Cancelling)));
    QVERIFY(states.contains(static_cast<int>(TransferState::Cancelled)));
    QCOMPARE(transfer.state(), TransferState::Cancelled);
}

void TestFileTransfer::testRawTransferLocalFailureUsesFailedState()
{
    /*
     * Failed 与 Cancelled 分开，能让 UI 和诊断日志区分“用户主动取消”和
     * “本地发送失败”。错误文本必须保存在 progress 中供界面展示。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    QCOMPARE(file.write(QByteArray("ABCDEFGH")), static_cast<qint64>(8));
    file.close();

    RawFileTransfer transfer;
    RawTransferOptions options;
    options.blockSize = 4;
    transfer.setOptions(options);

    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startSend(file.fileName()));
    QTRY_COMPARE(sendSpy.count(), 1);

    transfer.notifyLocalSendResult(false, QStringLiteral("local queue failed"));

    QCOMPARE(transfer.state(), TransferState::Failed);
    QVERIFY(transfer.progress().errorMessage.contains(QStringLiteral("local queue failed")));
}
