/**
 * @file TestFileTransfer.cpp
 * @brief 文件传输核心逻辑单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TestFileTransfer.h"

#include <functional>

#include <QByteArray>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaType>
#include <QObject>
#include <QRadioButton>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>
#include <QVector>
#include <QtTest>

/*
 * 本测试需要确认标准 X/YMODEM 发送路径是否仍把整文件保存在 m_fileData。
 * 这里仅在当前测试编译单元把 private 暴露出来，不改变生产代码 ABI，也
 * 避免用进程内存读数做不稳定断言。
 */
#define private public
#include "core/transfer/FileTransfer.h"
#undef private
#include "ui/dialogs/FileTransferDialog.h"
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

/**
 * @brief 按 YMODEM/XMODEM CRC16 算法生成大端 CRC。
 * @param data 参与 CRC 的 payload。
 * @return CRC16-XMODEM 结果。
 *
 * YMODEM 数据包尾部使用高字节在前的 CRC16-XMODEM。测试独立实现该函数，
 * 避免直接复用被测类的私有 builder，从而真实验证协议帧解析。
 */
quint16 crc16XModemForTransferTest(const QByteArray& data)
{
    quint16 crc = 0;
    for (char byte : data) {
        crc ^= static_cast<quint16>(static_cast<quint8>(byte) << 8);
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = static_cast<quint16>((crc << 1) ^ 0x1021);
            } else {
                crc = static_cast<quint16>(crc << 1);
            }
        }
    }
    return crc;
}

/**
 * @brief 构造 YMODEM 0 号文件头包。
 * @param fileName 文件名；空字符串表示批次结束头。
 * @param fileSize 声明的文件大小。
 * @return 完整 133 字节 YMODEM 头包。
 */
QByteArray buildYModemHeaderPacketForTest(const QString& fileName, qint64 fileSize)
{
    QByteArray payload(128, '\0');
    if (!fileName.isEmpty()) {
        const QByteArray nameBytes = fileName.toUtf8();
        const QByteArray sizeBytes = QByteArray::number(fileSize);
        memcpy(payload.data(), nameBytes.constData(), qMin(nameBytes.size(), 100));
        const int sizeOffset = qMin(nameBytes.size(), 100) + 1;
        memcpy(payload.data() + sizeOffset,
               sizeBytes.constData(),
               qMin(sizeBytes.size(), payload.size() - sizeOffset));
    }

    QByteArray packet;
    packet.append(static_cast<char>(0x01));
    packet.append(static_cast<char>(0x00));
    packet.append(static_cast<char>(0xFF));
    packet.append(payload);

    const quint16 crc = crc16XModemForTransferTest(payload);
    packet.append(static_cast<char>((crc >> 8) & 0xFF));
    packet.append(static_cast<char>(crc & 0xFF));
    return packet;
}

/**
 * @brief 构造 YMODEM 1024 字节数据包。
 * @param packetNumber 1 开始的数据包序号。
 * @param data 数据内容，不足 1024 字节会用 CPMEOF 填充。
 * @return 完整 YMODEM 数据包。
 */
QByteArray buildYModemDataPacketForTest(int packetNumber, const QByteArray& data)
{
    QByteArray payload = data.left(1024);
    while (payload.size() < 1024) {
        payload.append(static_cast<char>(0x1A));
    }

    QByteArray packet;
    packet.append(static_cast<char>(0x02));
    packet.append(static_cast<char>(packetNumber & 0xFF));
    packet.append(static_cast<char>((~packetNumber) & 0xFF));
    packet.append(payload);

    const quint16 crc = crc16XModemForTransferTest(payload);
    packet.append(static_cast<char>((crc >> 8) & 0xFF));
    packet.append(static_cast<char>(crc & 0xFF));
    return packet;
}

/**
 * @brief 构造 XMODEM 数据包。
 * @param packetNumber 1 开始的数据包序号。
 * @param data 数据内容，不足块大小会用 CPMEOF 填充。
 * @param blockSize XMODEM 块大小，通常为 128 或 1024。
 * @param useCrc true 表示生成 CRC-16 包，false 表示生成基础 checksum 包。
 * @return 完整 XMODEM 包。
 *
 * 测试独立构造协议包，避免直接复用被测类的私有 builder。这样既能验证
 * 接收端包解析，也能明确最后一包填充行为。
 */
QByteArray buildXModemPacketForTest(int packetNumber,
                                    const QByteArray& data,
                                    int blockSize = 128,
                                    bool useCrc = true)
{
    QByteArray payload = data.left(blockSize);
    while (payload.size() < blockSize) {
        payload.append(static_cast<char>(0x1A));
    }

    QByteArray packet;
    packet.append(static_cast<char>(blockSize == 1024 ? 0x02 : 0x01));
    packet.append(static_cast<char>(packetNumber & 0xFF));
    packet.append(static_cast<char>((~packetNumber) & 0xFF));
    packet.append(payload);

    if (useCrc) {
        const quint16 crc = crc16XModemForTransferTest(payload);
        packet.append(static_cast<char>((crc >> 8) & 0xFF));
        packet.append(static_cast<char>(crc & 0xFF));
    } else {
        quint8 checksum = 0;
        for (char value : payload) {
            checksum = static_cast<quint8>(checksum + static_cast<quint8>(value));
        }
        packet.append(static_cast<char>(checksum));
    }

    return packet;
}

/**
 * @brief 克隆并破坏包尾 CRC，构造校验失败场景。
 * @param packet 原始合法数据包。
 * @return CRC 尾字节被翻转后的非法包。
 */
QByteArray corruptPacketCrcForTest(const QByteArray& packet)
{
    QByteArray corrupt = packet;
    corrupt[corrupt.size() - 1] = static_cast<char>(
        static_cast<quint8>(corrupt.at(corrupt.size() - 1)) ^ 0x5A);
    return corrupt;
}

/**
 * @brief 读取 sendData 信号最后一次发送的字节数组。
 * @param spy 已连接到 FileTransfer::sendData 的 QSignalSpy。
 * @return 最后一次信号携带的数据。
 */
QByteArray lastSentPacket(const QSignalSpy& spy)
{
    return spy.isEmpty() ? QByteArray() : spy.last().at(0).toByteArray();
}

/**
 * @brief 判断指定发送序号的 sendData 是否等于目标字节串。
 * @param spy 已连接到 FileTransfer::sendData 的信号记录器。
 * @param index 需要读取的发送序号。
 * @param expected 期望字节串。
 * @return 数据存在且完全匹配时返回 true。
 */
bool sentPacketEquals(const QSignalSpy& spy, int index, const QByteArray& expected)
{
    if (index < 0 || index >= spy.count()) {
        return false;
    }
    return spy.at(index).at(0).toByteArray() == expected;
}

/**
 * @brief 创建包含可识别块边界的大文件。
 * @param file 已打开的临时文件。
 * @param size 需要写入的总字节数。
 * @return 写入文件的完整内容，供测试验证协议包 payload。
 *
 * 测试文件内容按 A..Z 循环生成，既能保证超过多个协议块，又能让断言
 * 明确判断发送包来自文件对应位置，而不是来自固定填充数据。
 */
QByteArray writePatternFileForTransferTest(QTemporaryFile& file, int size)
{
    QByteArray data;
    data.reserve(size);
    for (int i = 0; i < size; ++i) {
        data.append(static_cast<char>('A' + (i % 26)));
    }

    const qint64 bytesWritten = file.write(data);
    if (bytesWritten != static_cast<qint64>(data.size())) {
        return QByteArray();
    }
    file.flush();
    file.close();
    return data;
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

void TestFileTransfer::testXModemSendStreamsFileWithoutWholeFileCache()
{
    /*
     * XMODEM 发送以前会在 startSend() 阶段 readAll() 整个文件，导致固件
     * 越大峰值内存越高。完整实现应只保留当前待 ACK 的协议包，文件 payload
     * 由 QFile 按包读取。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray originalData = writePatternFileForTransferTest(file, 128 * 3 + 17);
    QVERIFY(!originalData.isEmpty());

    XModemTransfer transfer(true, false);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startSend(file.fileName()));
    QCOMPARE(transfer.m_fileData.size(), 0);
    QCOMPARE(transfer.progress().fileSize, static_cast<qint64>(originalData.size()));
    QCOMPARE(transfer.progress().totalPackets, 4);

    transfer.processReceivedData(QByteArray("C"));

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(transfer.m_fileData.size(), 0);
    const QByteArray firstPacket = lastSentPacket(sendSpy);
    QCOMPARE(firstPacket.size(), 3 + 128 + 2);
    QCOMPARE(static_cast<quint8>(firstPacket.at(0)), static_cast<quint8>(0x01));
    QCOMPARE(static_cast<quint8>(firstPacket.at(1)), static_cast<quint8>(1));
    QCOMPARE(firstPacket.mid(3, 128), originalData.left(128));
}

void TestFileTransfer::testXModemSendRetransmitsEotOnTimeout()
{
    /*
     * 数据包 ACK 后 XMODEM 会发送 EOT 等待最终 ACK。该阶段超时必须重发
     * EOT 控制字节，不能重发 m_lastPacket 中保存的最后一个数据包。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray originalData = writePatternFileForTransferTest(file, 16);
    QVERIFY(!originalData.isEmpty());

    XModemTransfer transfer(true, false);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startSend(file.fileName()));
    transfer.processReceivedData(QByteArray("C"));
    QCOMPARE(sendSpy.count(), 1);
    QVERIFY(lastSentPacket(sendSpy).size() > 1);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(sendSpy.count(), 2);
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x04)));

    transfer.onTimeout();

    QCOMPARE(sendSpy.count(), 3);
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x04)));
}

void TestFileTransfer::testYModemSendStreamsFileWithoutWholeFileCache()
{
    /*
     * YMODEM 发送以前会在发送 0 号头包时 readAll() 整个文件。新的发送
     * 路径应在头包只读取元数据，在数据包阶段按 1024 字节块读取。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray originalData = writePatternFileForTransferTest(file, 1024 * 2 + 31);
    QVERIFY(!originalData.isEmpty());

    YModemTransfer transfer(false);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startSend(file.fileName()));
    QCOMPARE(transfer.m_fileData.size(), 0);

    transfer.processReceivedData(QByteArray("C"));

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(transfer.m_fileData.size(), 0);
    const QByteArray headerPacket = lastSentPacket(sendSpy);
    QCOMPARE(headerPacket.size(), 3 + 128 + 2);
    QCOMPARE(static_cast<quint8>(headerPacket.at(0)), static_cast<quint8>(0x01));
    QVERIFY(headerPacket.mid(3, 128).contains(QFileInfo(file.fileName()).fileName().toUtf8()));
    QVERIFY(headerPacket.mid(3, 128).contains(QByteArray::number(originalData.size())));

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x06)));
    transfer.processReceivedData(QByteArray("C"));

    QCOMPARE(sendSpy.count(), 2);
    QCOMPARE(transfer.m_fileData.size(), 0);
    const QByteArray firstDataPacket = lastSentPacket(sendSpy);
    QCOMPARE(firstDataPacket.size(), 3 + 1024 + 2);
    QCOMPARE(static_cast<quint8>(firstDataPacket.at(0)), static_cast<quint8>(0x02));
    QCOMPARE(static_cast<quint8>(firstDataPacket.at(1)), static_cast<quint8>(1));
    QCOMPARE(firstDataPacket.mid(3, 1024), originalData.left(1024));
}

void TestFileTransfer::testYModemSendRetransmitsCurrentPacketOnNak()
{
    /*
     * 流式发送不能依赖整文件缓存来重发，因此发送器必须保留当前待 ACK
     * 的完整协议包。收到 NAK 时重发该包，不读取下一块文件数据。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray originalData = writePatternFileForTransferTest(file, 1024 + 19);
    QVERIFY(!originalData.isEmpty());

    YModemTransfer transfer(false);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startSend(file.fileName()));
    transfer.processReceivedData(QByteArray("C"));
    QCOMPARE(sendSpy.count(), 1);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x06)));
    transfer.processReceivedData(QByteArray("C"));
    QCOMPARE(sendSpy.count(), 2);
    const QByteArray firstDataPacket = lastSentPacket(sendSpy);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x15)));

    QCOMPARE(sendSpy.count(), 3);
    QCOMPARE(lastSentPacket(sendSpy), firstDataPacket);
    QCOMPARE(transfer.progress().currentPacket, 1);
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(1024));
}

void TestFileTransfer::testYModemSendResetsRetryCountAfterPacketAck()
{
    /*
     * 重试次数应属于当前待 ACK 的数据包。如果上一包曾经 NAK 后成功 ACK，
     * 下一包的首次 NAK 仍应允许重发，不能把多个包的偶发错误累加。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray originalData = writePatternFileForTransferTest(file, 1024 * 2);
    QVERIFY(!originalData.isEmpty());

    YModemTransfer transfer(false);
    transfer.setMaxRetries(1);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startSend(file.fileName()));
    transfer.processReceivedData(QByteArray("C"));
    QCOMPARE(sendSpy.count(), 1);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x06)));
    transfer.processReceivedData(QByteArray("C"));
    QCOMPARE(sendSpy.count(), 2);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x15)));
    QCOMPARE(sendSpy.count(), 3);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(sendSpy.count(), 4);
    const QByteArray secondDataPacket = lastSentPacket(sendSpy);
    QCOMPARE(static_cast<quint8>(secondDataPacket.at(1)), static_cast<quint8>(2));

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x15)));

    QCOMPARE(sendSpy.count(), 5);
    QCOMPARE(lastSentPacket(sendSpy), secondDataPacket);
    QCOMPARE(transfer.state(), TransferState::Running);
}

void TestFileTransfer::testYModemGSendStreamsPacketsWithoutPerPacketAck()
{
    /*
     * YMODEM-G 是流式变体：接收端用 G 请求头包和数据流，发送端在数据
     * 包之间不等 ACK。该测试覆盖完整单文件发送路径，避免仅有枚举和
     * 工厂入口但仍按普通 YMODEM 等待逐包 ACK。
     */
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray originalData = writePatternFileForTransferTest(file, 1024 * 2 + 19);
    QVERIFY(!originalData.isEmpty());

    YModemTransfer transfer(true);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startSend(file.fileName()));
    QCOMPARE(transfer.protocol(), TransferProtocol::YModemG);

    transfer.processReceivedData(QByteArray("G"));

    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(static_cast<quint8>(lastSentPacket(sendSpy).at(0)), static_cast<quint8>(0x01));
    QVERIFY(lastSentPacket(sendSpy).mid(3, 128).contains(QFileInfo(file.fileName()).fileName().toUtf8()));

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x06)));
    transfer.processReceivedData(QByteArray("G"));

    QTRY_VERIFY(sendSpy.count() >= 5);
    QTest::qWait(20);
    QCOMPARE(sendSpy.count(), 5);
    const QByteArray firstDataPacket = sendSpy.at(1).at(0).toByteArray();
    const QByteArray secondDataPacket = sendSpy.at(2).at(0).toByteArray();
    const QByteArray thirdDataPacket = sendSpy.at(3).at(0).toByteArray();
    QCOMPARE(firstDataPacket.size(), 3 + 1024 + 2);
    QCOMPARE(secondDataPacket.size(), 3 + 1024 + 2);
    QCOMPARE(thirdDataPacket.size(), 3 + 1024 + 2);
    QCOMPARE(static_cast<quint8>(firstDataPacket.at(1)), static_cast<quint8>(1));
    QCOMPARE(static_cast<quint8>(secondDataPacket.at(1)), static_cast<quint8>(2));
    QCOMPARE(static_cast<quint8>(thirdDataPacket.at(1)), static_cast<quint8>(3));
    QCOMPARE(firstDataPacket.mid(3, 1024), originalData.left(1024));
    QCOMPARE(secondDataPacket.mid(3, 1024), originalData.mid(1024, 1024));
    QCOMPARE(thirdDataPacket.mid(3, 19), originalData.mid(2048));
    QCOMPARE(static_cast<quint8>(thirdDataPacket.at(3 + 19)), static_cast<quint8>(0x1A));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x04)));
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(originalData.size()));
    QCOMPARE(completedSpy.count(), 0);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x06)));
    transfer.processReceivedData(QByteArray("G"));

    QTRY_COMPARE(sendSpy.count(), 6);
    QCOMPARE(static_cast<quint8>(lastSentPacket(sendSpy).at(0)), static_cast<quint8>(0x01));
    QCOMPARE(lastSentPacket(sendSpy).mid(3, 128).at(0), static_cast<char>(0x00));
    QCOMPARE(transfer.state(), TransferState::Completed);
    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), true);
}

void TestFileTransfer::testYModemGReceiveUsesGHandshakeAndSkipsDataAcks()
{
    /*
     * YMODEM-G 接收端不通过 NAK 请求重传，数据包校验成功后也不 ACK。
     * 它只在头包阶段发送 G，并在文件 EOT 后 ACK+G 请求下一个批次头。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    YModemTransfer transfer(true);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(tempDir.path()));
    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(lastSentPacket(sendSpy), QByteArray("G"));

    const QByteArray payload("YMODEM-G receive payload");
    transfer.processReceivedData(buildYModemHeaderPacketForTest(QStringLiteral("ymodem-g.bin"),
                                                                payload.size()));
    QVERIFY(sendSpy.count() >= 3);
    QCOMPARE(sendSpy.at(sendSpy.count() - 2).at(0).toByteArray(),
             QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray("G"));
    QCOMPARE(transfer.state(), TransferState::Running);

    const int sendsBeforeData = sendSpy.count();
    transfer.processReceivedData(buildYModemDataPacketForTest(1, payload));

    QCOMPARE(sendSpy.count(), sendsBeforeData);
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(payload.size()));

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x04)));
    QVERIFY(sendSpy.count() >= sendsBeforeData + 2);
    QCOMPARE(sendSpy.at(sendSpy.count() - 2).at(0).toByteArray(),
             QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray("G"));
    QCOMPARE(completedSpy.count(), 0);

    transfer.processReceivedData(buildYModemHeaderPacketForTest(QString(), 0));

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), true);
    QFile outputFile(QDir(tempDir.path()).filePath(QStringLiteral("ymodem-g.bin")));
    QVERIFY(outputFile.open(QIODevice::ReadOnly));
    QCOMPARE(outputFile.readAll(), payload);
}

void TestFileTransfer::testYModemGReceiveAbortsOnCorruptDataPacket()
{
    /*
     * G 模式没有 NAK 重传语义。发现数据包损坏时，接收端必须用 CAN
     * 明确中止，避免对端继续高速发送后续块而本地文件已不可恢复。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    YModemTransfer transfer(true);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(tempDir.path()));
    transfer.processReceivedData(buildYModemHeaderPacketForTest(QStringLiteral("bad-g.bin"), 4));

    const QByteArray corruptPacket =
        corruptPacketCrcForTest(buildYModemDataPacketForTest(1, QByteArray("bad")));
    transfer.processReceivedData(corruptPacket);

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), false);
    QCOMPARE(transfer.state(), TransferState::Failed);
    QCOMPARE(transfer.m_file, nullptr);
    QVERIFY(transfer.progress().errorMessage.contains(QStringLiteral("YMODEM-G")));
    QVERIFY(lastSentPacket(sendSpy).size() >= 2);
    QVERIFY(lastSentPacket(sendSpy).startsWith(QByteArray(2, static_cast<char>(0x18))));
}

void TestFileTransfer::testFileTransferDialogExposesYModemGProtocol()
{
    /*
     * UI 必须和工厂能力一致。该测试不启动真实传输，只检查标准协议模式
     * 下拉框包含 YMODEM-G，避免用户只能从代码枚举看到这个协议。
     */
    FileTransferDialog dialog;

    const QList<QComboBox*> combos = dialog.findChildren<QComboBox*>();
    QComboBox* modeCombo = nullptr;
    QComboBox* protocolCombo = nullptr;
    for (QComboBox* combo : combos) {
        if (combo->findData(static_cast<int>(TransferProtocol::YModem)) >= 0) {
            protocolCombo = combo;
        } else if (combo->findText(QStringLiteral("XMODEM/YMODEM")) >= 0) {
            modeCombo = combo;
        }
    }

    QVERIFY(modeCombo != nullptr);
    QVERIFY(protocolCombo != nullptr);

    const int standardModeIndex = modeCombo->findText(QStringLiteral("XMODEM/YMODEM"));
    QVERIFY(standardModeIndex >= 0);
    modeCombo->setCurrentIndex(standardModeIndex);

    QVERIFY(protocolCombo->findData(static_cast<int>(TransferProtocol::YModemG)) >= 0);
    QVERIFY(protocolCombo->findText(QStringLiteral("YMODEM-G")) >= 0);

    const QList<QRadioButton*> radioButtons = dialog.findChildren<QRadioButton*>();
    bool receiveRadioEnabled = false;
    for (QRadioButton* radio : radioButtons) {
        if (radio->text() == QStringLiteral("接收文件")) {
            receiveRadioEnabled = radio->isEnabled();
            break;
        }
    }
    QVERIFY(receiveRadioEnabled);
}

void TestFileTransfer::testXModemReceiveWritesPacketsWithoutAccumulatingFileData()
{
    /*
     * 旧接收路径会把每个合法数据块 append 到 m_fileData，最后 EOT 时再
     * 一次性写入目标文件。新路径应校验通过后立即写入 QFile，避免大文件
     * 接收期间多占一份完整内存。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString targetPath = QDir(tempDir.path()).filePath(QStringLiteral("xmodem.bin"));

    XModemTransfer transfer(true, false);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(targetPath));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray("C"));

    const QByteArray payload("XMODEM receive payload");
    const QByteArray packet = buildXModemPacketForTest(1, payload);
    const QByteArray expectedBlock = packet.mid(3, 128);

    transfer.processReceivedData(packet);

    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(transfer.m_fileData.size(), 0);
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(128));

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x04)));

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), true);
    QCOMPARE(transfer.state(), TransferState::Completed);

    QFile outputFile(targetPath);
    QVERIFY(outputFile.open(QIODevice::ReadOnly));
    QCOMPARE(outputFile.readAll(), expectedBlock);
}

void TestFileTransfer::testXModemReceiveKeepsPartialPacketUntilComplete()
{
    /*
     * 串口可能把一个 XMODEM 包拆成多次 readyRead。接收缓存必须保留半包，
     * 否则第二段数据到达时已经丢失包头，无法完成校验。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString targetPath = QDir(tempDir.path()).filePath(QStringLiteral("partial.bin"));

    XModemTransfer transfer(true, false);
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));

    QVERIFY(transfer.startReceive(targetPath));
    QCOMPARE(sendSpy.count(), 1);

    const QByteArray packet = buildXModemPacketForTest(1, QByteArray("partial"));
    transfer.processReceivedData(packet.left(17));

    QCOMPARE(sendSpy.count(), 1);
    QVERIFY(transfer.m_receiveBuffer.size() > 0);

    transfer.processReceivedData(packet.mid(17));

    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(transfer.m_fileData.size(), 0);
}

void TestFileTransfer::testXModemReceiveSenderCancelReleasesOpenFile()
{
    /*
     * 对端发送 CAN 时，接收端可能刚打开目标文件但尚未写完。必须立即
     * 释放 QFile，避免取消后文件仍被本进程占用。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString targetPath = QDir(tempDir.path()).filePath(QStringLiteral("cancel-xmodem.bin"));

    XModemTransfer transfer(true, false);
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(targetPath));
    QVERIFY(transfer.m_file != nullptr);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x18)));

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), false);
    QCOMPARE(transfer.state(), TransferState::Cancelled);
    QCOMPARE(transfer.m_file, nullptr);
}

void TestFileTransfer::testXModemReceiveFailureReleasesOpenFile()
{
    /*
     * 校验失败超过重试次数后，接收端必须释放目标文件并进入 Failed。
     * 这里把最大重试设为 0，使第一包 CRC 错误即可触发失败路径。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString targetPath = QDir(tempDir.path()).filePath(QStringLiteral("bad-crc.bin"));

    XModemTransfer transfer(true, false);
    transfer.setMaxRetries(0);
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(targetPath));
    const QByteArray corruptPacket =
        corruptPacketCrcForTest(buildXModemPacketForTest(1, QByteArray("bad")));

    transfer.processReceivedData(corruptPacket);

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), false);
    QCOMPARE(transfer.state(), TransferState::Failed);
    QCOMPARE(transfer.m_file, nullptr);
    QVERIFY(transfer.progress().errorMessage.contains(QStringLiteral("校验")));
}

void TestFileTransfer::testYModemReceiveCompletesSingleFileBatch()
{
    /*
     * 这是 YMODEM 接收的完整成功路径。测试按协议顺序喂入头包、数据包、
     * 双 EOT 和空头结束，验证握手字节、状态、进度和保存文件内容。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    YModemTransfer transfer;
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(tempDir.path()));
    QCOMPARE(sendSpy.count(), 1);
    QCOMPARE(lastSentPacket(sendSpy), QByteArray("C"));
    QCOMPARE(transfer.state(), TransferState::WaitingStart);

    const QByteArray payload("Hello YMODEM receive");
    transfer.processReceivedData(buildYModemHeaderPacketForTest(QStringLiteral("firmware.bin"),
                                                                payload.size()));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x43)));
    QVERIFY(sendSpy.count() >= 3);
    QCOMPARE(sendSpy.at(sendSpy.count() - 2).at(0).toByteArray(),
             QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(transfer.state(), TransferState::Running);
    QCOMPARE(transfer.progress().fileName, QStringLiteral("firmware.bin"));
    QCOMPARE(transfer.progress().fileSize, static_cast<qint64>(payload.size()));

    transfer.processReceivedData(buildYModemDataPacketForTest(1, payload));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(transfer.progress().bytesTransferred, static_cast<qint64>(payload.size()));

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x04)));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x15)));
    QCOMPARE(completedSpy.count(), 0);

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x04)));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x43)));
    QVERIFY(sendSpy.count() >= 2);
    QCOMPARE(sendSpy.at(sendSpy.count() - 2).at(0).toByteArray(),
             QByteArray(1, static_cast<char>(0x06)));
    QCOMPARE(completedSpy.count(), 0);

    transfer.processReceivedData(buildYModemHeaderPacketForTest(QString(), 0));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x06)));
    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), true);
    QCOMPARE(transfer.state(), TransferState::Completed);

    QFile outputFile(QDir(tempDir.path()).filePath(QStringLiteral("firmware.bin")));
    QVERIFY(outputFile.open(QIODevice::ReadOnly));
    QCOMPARE(outputFile.readAll(), payload);
}

void TestFileTransfer::testYModemReceiveRejectsCorruptHeaderAndAcceptsRetry()
{
    /*
     * 头包可能因线路噪声损坏。接收端应 NAK 并继续等待正确头包，而不是
     * 直接进入失败或创建错误文件。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    YModemTransfer transfer;
    QSignalSpy sendSpy(&transfer, SIGNAL(sendData(QByteArray)));
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(tempDir.path()));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray("C"));

    const QByteArray corruptHeader =
        corruptPacketCrcForTest(buildYModemHeaderPacketForTest(QStringLiteral("retry.bin"), 4));
    transfer.processReceivedData(corruptHeader);
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x15)));
    QCOMPARE(completedSpy.count(), 0);
    QVERIFY(!QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral("retry.bin"))));

    transfer.processReceivedData(buildYModemHeaderPacketForTest(QStringLiteral("retry.bin"), 4));
    QCOMPARE(lastSentPacket(sendSpy), QByteArray(1, static_cast<char>(0x43)));
    QCOMPARE(transfer.progress().fileName, QStringLiteral("retry.bin"));
    QCOMPARE(completedSpy.count(), 0);
}

void TestFileTransfer::testYModemReceiveRejectsUnsafeFileName()
{
    /*
     * YMODEM 文件名来自对端设备，不能直接拼接路径。包含目录分隔符或
     * 上级目录的文件名必须拒绝，避免把文件写到用户选择保存目录之外。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    YModemTransfer transfer;
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(tempDir.path()));
    transfer.processReceivedData(buildYModemHeaderPacketForTest(QStringLiteral("../escape.bin"), 4));

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), false);
    QCOMPARE(transfer.state(), TransferState::Failed);
    QVERIFY(!QFileInfo::exists(QDir(tempDir.path()).filePath(QStringLiteral("../escape.bin"))));
}

void TestFileTransfer::testYModemReceiveSenderCancelReleasesOpenFile()
{
    /*
     * 对端取消时，YMODEM 接收端可能已经打开了目标文件。必须立即关闭文件
     * 句柄，否则 Windows 下用户无法删除或覆盖该文件。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    YModemTransfer transfer;
    QSignalSpy completedSpy(&transfer, SIGNAL(transferCompleted(bool,QString)));

    QVERIFY(transfer.startReceive(tempDir.path()));
    transfer.processReceivedData(buildYModemHeaderPacketForTest(QStringLiteral("cancel.bin"), 8));

    const QString targetPath = QDir(tempDir.path()).filePath(QStringLiteral("cancel.bin"));
    QVERIFY(QFileInfo::exists(targetPath));

    transfer.processReceivedData(QByteArray(1, static_cast<char>(0x18)));

    QTRY_COMPARE(completedSpy.count(), 1);
    QCOMPARE(completedSpy.at(0).at(0).toBool(), false);
    QCOMPARE(transfer.state(), TransferState::Cancelled);
    QVERIFY2(QFile::remove(targetPath),
             "发送方取消后目标文件句柄必须释放，确保用户能删除或覆盖残留文件。");
}
