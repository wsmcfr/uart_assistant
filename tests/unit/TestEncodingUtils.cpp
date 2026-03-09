/**
 * @file TestEncodingUtils.cpp
 * @brief 编码转换工具单元测试
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "TestEncodingUtils.h"
#include <QByteArray>
#include <QString>
#include "core/utils/EncodingUtils.h"

using namespace ComAssistant;

void TestEncodingUtils::initTestCase()
{
    qDebug() << "Starting EncodingUtils tests...";
}

void TestEncodingUtils::cleanupTestCase()
{
    qDebug() << "EncodingUtils tests completed.";
}

// ==================== Base64 ====================

void TestEncodingUtils::testToBase64_empty()
{
    QByteArray data;
    QString result = EncodingUtils::toBase64(data);
    QCOMPARE(result, QString(""));
}

void TestEncodingUtils::testToBase64_simple()
{
    QByteArray data("Hello");
    QString result = EncodingUtils::toBase64(data);
    QCOMPARE(result, QString("SGVsbG8="));
}

void TestEncodingUtils::testFromBase64_empty()
{
    QString base64;
    QByteArray result = EncodingUtils::fromBase64(base64);
    QVERIFY(result.isEmpty());
}

void TestEncodingUtils::testFromBase64_simple()
{
    QString base64("SGVsbG8=");
    QByteArray result = EncodingUtils::fromBase64(base64);
    QCOMPARE(result, QByteArray("Hello"));
}

void TestEncodingUtils::testBase64_roundTrip()
{
    QByteArray original("The quick brown fox jumps over the lazy dog.");
    QString encoded = EncodingUtils::toBase64(original);
    QByteArray decoded = EncodingUtils::fromBase64(encoded);
    QCOMPARE(decoded, original);
}

// ==================== URL 编码 ====================

void TestEncodingUtils::testUrlEncode_empty()
{
    QString text;
    QString result = EncodingUtils::urlEncode(text);
    QCOMPARE(result, QString(""));
}

void TestEncodingUtils::testUrlEncode_simple()
{
    QString text("Hello World");
    QString result = EncodingUtils::urlEncode(text);
    QCOMPARE(result, QString("Hello%20World"));
}

void TestEncodingUtils::testUrlEncode_special()
{
    QString text("a=b&c=d");
    QString result = EncodingUtils::urlEncode(text);
    QCOMPARE(result, QString("a%3Db%26c%3Dd"));
}

void TestEncodingUtils::testUrlDecode_empty()
{
    QString encoded;
    QString result = EncodingUtils::urlDecode(encoded);
    QCOMPARE(result, QString(""));
}

void TestEncodingUtils::testUrlDecode_simple()
{
    QString encoded("Hello%20World");
    QString result = EncodingUtils::urlDecode(encoded);
    QCOMPARE(result, QString("Hello World"));
}

void TestEncodingUtils::testUrl_roundTrip()
{
    QString original("Hello World! 你好世界");
    QString encoded = EncodingUtils::urlEncode(original);
    QString decoded = EncodingUtils::urlDecode(encoded);
    QCOMPARE(decoded, original);
}

// ==================== 支持的编码 ====================

void TestEncodingUtils::testSupportedCodecs()
{
    QStringList codecs = EncodingUtils::supportedCodecs();
    QVERIFY(!codecs.isEmpty());
    QVERIFY(codecs.contains("UTF-8"));
}
