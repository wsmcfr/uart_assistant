/**
 * @file TestConversionUtils.h
 * @brief 进制转换工具单元测试头文件
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef TESTCONVERSIONUTILS_H
#define TESTCONVERSIONUTILS_H

#include <QObject>
#include <QTest>

class TestConversionUtils : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 十进制转十六进制测试
    void testDecToHex_zero();
    void testDecToHex_positive();
    void testDecToHex_negative();
    void testDecToHex_withWidth();

    // 十进制转二进制测试
    void testDecToBin_zero();
    void testDecToBin_positive();
    void testDecToBin_withWidth();

    // 十六进制转十进制测试
    void testHexToDec_zero();
    void testHexToDec_lowercase();
    void testHexToDec_uppercase();
    void testHexToDec_withPrefix();

    // 二进制转十进制测试
    void testBinToDec_zero();
    void testBinToDec_positive();

    // 字节序转换测试
    void testReverseBytes();
    void testSwapBytes16();
    void testSwapBytes32();

    // IEEE754 浮点数转换测试
    void testBytesToFloat_littleEndian();
    void testBytesToFloat_bigEndian();
    void testFloatToBytes();
    void testBytesToDouble();
};

#endif // TESTCONVERSIONUTILS_H
