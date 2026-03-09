/**
 * @file TestConversionUtils.cpp
 * @brief 进制转换工具单元测试
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "TestConversionUtils.h"
#include <QByteArray>
#include "core/utils/ConversionUtils.h"

using namespace ComAssistant;

void TestConversionUtils::initTestCase()
{
    qDebug() << "Starting ConversionUtils tests...";
}

void TestConversionUtils::cleanupTestCase()
{
    qDebug() << "ConversionUtils tests completed.";
}

// ==================== 十进制转十六进制 ====================

void TestConversionUtils::testDecToHex_zero()
{
    QString result = ConversionUtils::decToHex(0);
    QCOMPARE(result, QString("0"));
}

void TestConversionUtils::testDecToHex_positive()
{
    QCOMPARE(ConversionUtils::decToHex(255), QString("FF"));
    QCOMPARE(ConversionUtils::decToHex(16), QString("10"));
    QCOMPARE(ConversionUtils::decToHex(4096), QString("1000"));
}

void TestConversionUtils::testDecToHex_negative()
{
    // 负数应该返回补码形式
    QString result = ConversionUtils::decToHex(-1);
    QVERIFY(!result.isEmpty());
}

void TestConversionUtils::testDecToHex_withWidth()
{
    QCOMPARE(ConversionUtils::decToHex(15, 4), QString("000F"));
    QCOMPARE(ConversionUtils::decToHex(255, 4), QString("00FF"));
    QCOMPARE(ConversionUtils::decToHex(4095, 4), QString("0FFF"));
}

// ==================== 十进制转二进制 ====================

void TestConversionUtils::testDecToBin_zero()
{
    QString result = ConversionUtils::decToBin(0);
    QCOMPARE(result, QString("0"));
}

void TestConversionUtils::testDecToBin_positive()
{
    QCOMPARE(ConversionUtils::decToBin(1), QString("1"));
    QCOMPARE(ConversionUtils::decToBin(5), QString("101"));
    QCOMPARE(ConversionUtils::decToBin(255), QString("11111111"));
}

void TestConversionUtils::testDecToBin_withWidth()
{
    QCOMPARE(ConversionUtils::decToBin(5, 8), QString("00000101"));
    QCOMPARE(ConversionUtils::decToBin(1, 4), QString("0001"));
}

// ==================== 十六进制转十进制 ====================

void TestConversionUtils::testHexToDec_zero()
{
    QCOMPARE(ConversionUtils::hexToDec("0"), static_cast<qint64>(0));
    QCOMPARE(ConversionUtils::hexToDec("00"), static_cast<qint64>(0));
}

void TestConversionUtils::testHexToDec_lowercase()
{
    QCOMPARE(ConversionUtils::hexToDec("ff"), static_cast<qint64>(255));
    QCOMPARE(ConversionUtils::hexToDec("abc"), static_cast<qint64>(2748));
}

void TestConversionUtils::testHexToDec_uppercase()
{
    QCOMPARE(ConversionUtils::hexToDec("FF"), static_cast<qint64>(255));
    QCOMPARE(ConversionUtils::hexToDec("ABC"), static_cast<qint64>(2748));
}

void TestConversionUtils::testHexToDec_withPrefix()
{
    QCOMPARE(ConversionUtils::hexToDec("0xFF"), static_cast<qint64>(255));
    QCOMPARE(ConversionUtils::hexToDec("0x10"), static_cast<qint64>(16));
}

// ==================== 二进制转十进制 ====================

void TestConversionUtils::testBinToDec_zero()
{
    QCOMPARE(ConversionUtils::binToDec("0"), static_cast<qint64>(0));
    QCOMPARE(ConversionUtils::binToDec("00000000"), static_cast<qint64>(0));
}

void TestConversionUtils::testBinToDec_positive()
{
    QCOMPARE(ConversionUtils::binToDec("1"), static_cast<qint64>(1));
    QCOMPARE(ConversionUtils::binToDec("101"), static_cast<qint64>(5));
    QCOMPARE(ConversionUtils::binToDec("11111111"), static_cast<qint64>(255));
}

// ==================== 字节序转换 ====================

void TestConversionUtils::testReverseBytes()
{
    QByteArray data = QByteArray::fromHex("01020304");
    QByteArray reversed = ConversionUtils::reverseBytes(data);
    QCOMPARE(reversed, QByteArray::fromHex("04030201"));
}

void TestConversionUtils::testSwapBytes16()
{
    QCOMPARE(ConversionUtils::swapBytes16(0x1234), static_cast<quint16>(0x3412));
    QCOMPARE(ConversionUtils::swapBytes16(0xABCD), static_cast<quint16>(0xCDAB));
}

void TestConversionUtils::testSwapBytes32()
{
    QCOMPARE(ConversionUtils::swapBytes32(0x12345678), static_cast<quint32>(0x78563412));
    QCOMPARE(ConversionUtils::swapBytes32(0xABCDEF01), static_cast<quint32>(0x01EFCDAB));
}

// ==================== IEEE754 浮点数 ====================

void TestConversionUtils::testBytesToFloat_littleEndian()
{
    // 1.0f 的小端序表示：00 00 80 3F
    QByteArray data = QByteArray::fromHex("0000803F");
    float result = ConversionUtils::bytesToFloat(data, false);
    QCOMPARE(result, 1.0f);
}

void TestConversionUtils::testBytesToFloat_bigEndian()
{
    // 1.0f 的大端序表示：3F 80 00 00
    QByteArray data = QByteArray::fromHex("3F800000");
    float result = ConversionUtils::bytesToFloat(data, true);
    QCOMPARE(result, 1.0f);
}

void TestConversionUtils::testFloatToBytes()
{
    QByteArray result = ConversionUtils::floatToBytes(1.0f, false);
    QCOMPARE(result.size(), 4);
    QCOMPARE(result, QByteArray::fromHex("0000803F"));
}

void TestConversionUtils::testBytesToDouble()
{
    // 1.0 的小端序表示
    QByteArray data = QByteArray::fromHex("000000000000F03F");
    double result = ConversionUtils::bytesToDouble(data, false);
    QCOMPARE(result, 1.0);
}

