/**
 * @file TestChecksumUtils.h
 * @brief 校验和工具单元测试头文件
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef TESTCHECKSUMUTILS_H
#define TESTCHECKSUMUTILS_H

#include <QObject>
#include <QTest>

class TestChecksumUtils : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // CRC16 Modbus 测试
    void testCrc16Modbus_empty();
    void testCrc16Modbus_standard();
    void testCrc16Modbus_knownValues();

    // CRC16 CCITT 测试
    void testCrc16CCITT_empty();
    void testCrc16CCITT_standard();

    // CRC32 测试
    void testCrc32_empty();
    void testCrc32_standard();

    // XOR 校验和测试
    void testXorChecksum_empty();
    void testXorChecksum_standard();

    // 累加和测试
    void testSumChecksum_empty();
    void testSumChecksum_standard();

    // BCC 校验测试
    void testBcc_empty();
    void testBcc_standard();
};

#endif // TESTCHECKSUMUTILS_H
