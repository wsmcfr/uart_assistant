/**
 * @file TestChecksumUtils.cpp
 * @brief 校验和工具单元测试
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "TestChecksumUtils.h"
#include <QByteArray>
#include "core/utils/ChecksumUtils.h"

using namespace ComAssistant;

void TestChecksumUtils::initTestCase()
{
    qDebug() << "Starting ChecksumUtils tests...";
}

void TestChecksumUtils::cleanupTestCase()
{
    qDebug() << "ChecksumUtils tests completed.";
}

// ==================== CRC16 Modbus ====================

void TestChecksumUtils::testCrc16Modbus_empty()
{
    QByteArray data;
    quint16 crc = ChecksumUtils::crc16Modbus(data);
    QCOMPARE(crc, static_cast<quint16>(0xFFFF));  // Modbus CRC 初始值
}

void TestChecksumUtils::testCrc16Modbus_standard()
{
    // 标准 Modbus 测试数据
    QByteArray data = QByteArray::fromHex("01030000000A");
    quint16 crc = ChecksumUtils::crc16Modbus(data);
    // 验证 CRC 不为 0
    QVERIFY(crc != 0);
}

void TestChecksumUtils::testCrc16Modbus_knownValues()
{
    // "123456789" 的 CRC16-Modbus 应该是 0x4B37
    QByteArray data("123456789");
    quint16 crc = ChecksumUtils::crc16Modbus(data);
    QCOMPARE(crc, static_cast<quint16>(0x4B37));
}

// ==================== CRC16 CCITT ====================

void TestChecksumUtils::testCrc16CCITT_empty()
{
    QByteArray data;
    quint16 crc = ChecksumUtils::crc16CCITT(data);
    QCOMPARE(crc, static_cast<quint16>(0xFFFF));  // CCITT 初始值
}

void TestChecksumUtils::testCrc16CCITT_standard()
{
    QByteArray data("Hello");
    quint16 crc = ChecksumUtils::crc16CCITT(data);
    QVERIFY(crc != 0);
}

// ==================== CRC32 ====================

void TestChecksumUtils::testCrc32_empty()
{
    QByteArray data;
    quint32 crc = ChecksumUtils::crc32(data);
    QCOMPARE(crc, static_cast<quint32>(0));
}

void TestChecksumUtils::testCrc32_standard()
{
    // "123456789" 的 CRC32 应该是 0xCBF43926
    QByteArray data("123456789");
    quint32 crc = ChecksumUtils::crc32(data);
    QCOMPARE(crc, static_cast<quint32>(0xCBF43926));
}

// ==================== XOR Checksum ====================

void TestChecksumUtils::testXorChecksum_empty()
{
    QByteArray data;
    quint8 xorSum = ChecksumUtils::xorChecksum(data);
    QCOMPARE(xorSum, static_cast<quint8>(0));
}

void TestChecksumUtils::testXorChecksum_standard()
{
    // 0x01 ^ 0x02 ^ 0x03 ^ 0x04 = 0x04
    QByteArray data = QByteArray::fromHex("01020304");
    quint8 xorSum = ChecksumUtils::xorChecksum(data);
    QCOMPARE(xorSum, static_cast<quint8>(0x04));
}

// ==================== Sum Checksum ====================

void TestChecksumUtils::testSumChecksum_empty()
{
    QByteArray data;
    quint8 sum = ChecksumUtils::sumChecksum(data);
    QCOMPARE(sum, static_cast<quint8>(0));
}

void TestChecksumUtils::testSumChecksum_standard()
{
    // (0x01 + 0x02 + 0x03 + 0x04) & 0xFF = 0x0A
    QByteArray data = QByteArray::fromHex("01020304");
    quint8 sum = ChecksumUtils::sumChecksum(data);
    QCOMPARE(sum, static_cast<quint8>(0x0A));
}

// ==================== BCC ====================

void TestChecksumUtils::testBcc_empty()
{
    QByteArray data;
    quint8 bcc = ChecksumUtils::bcc(data);
    QCOMPARE(bcc, static_cast<quint8>(0));
}

void TestChecksumUtils::testBcc_standard()
{
    QByteArray data = QByteArray::fromHex("01020304");
    quint8 bcc = ChecksumUtils::bcc(data);
    // BCC 应该等于 XOR
    quint8 xorSum = ChecksumUtils::xorChecksum(data);
    QCOMPARE(bcc, xorSum);
}
