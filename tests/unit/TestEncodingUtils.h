/**
 * @file TestEncodingUtils.h
 * @brief 编码转换工具单元测试头文件
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef TESTENCODINGUTILS_H
#define TESTENCODINGUTILS_H

#include <QObject>
#include <QTest>

class TestEncodingUtils : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Base64 编码测试
    void testToBase64_empty();
    void testToBase64_simple();
    void testFromBase64_empty();
    void testFromBase64_simple();
    void testBase64_roundTrip();

    // URL 编码测试
    void testUrlEncode_empty();
    void testUrlEncode_simple();
    void testUrlEncode_special();
    void testUrlDecode_empty();
    void testUrlDecode_simple();
    void testUrl_roundTrip();

    // 支持的编码列表测试
    void testSupportedCodecs();
};

#endif // TESTENCODINGUTILS_H
