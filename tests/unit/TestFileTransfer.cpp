/**
 * @file TestFileTransfer.cpp
 * @brief 文件传输核心逻辑单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TestFileTransfer.h"

#include <QByteArray>
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
