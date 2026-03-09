/**
 * @file ChecksumUtils.h
 * @brief 校验和工具类
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_CHECKSUMUTILS_H
#define COMASSISTANT_CHECKSUMUTILS_H

#include <QByteArray>
#include <QString>

namespace ComAssistant {

/**
 * @brief 校验和工具类
 * 提供各种校验和算法：CRC、XOR、SUM、LRC、BCC等
 */
class ChecksumUtils {
public:
    // ==================== CRC算法 ====================

    /**
     * @brief CRC-16 Modbus算法
     * @param data 输入数据
     * @return CRC-16值（低字节在前）
     */
    static quint16 crc16Modbus(const QByteArray& data);

    /**
     * @brief CRC-16 CCITT算法（多项式0x1021，初值0xFFFF）
     * @param data 输入数据
     * @return CRC-16值
     */
    static quint16 crc16CCITT(const QByteArray& data);

    /**
     * @brief CRC-16 CCITT-FALSE算法
     * @param data 输入数据
     * @return CRC-16值
     */
    static quint16 crc16CCITT_FALSE(const QByteArray& data);

    /**
     * @brief CRC-16 XMODEM算法（多项式0x1021，初值0x0000）
     * @param data 输入数据
     * @return CRC-16值
     */
    static quint16 crc16XMODEM(const QByteArray& data);

    /**
     * @brief CRC-32算法（IEEE 802.3）
     * @param data 输入数据
     * @return CRC-32值
     */
    static quint32 crc32(const QByteArray& data);

    /**
     * @brief CRC-8算法（多项式0x07）
     * @param data 输入数据
     * @return CRC-8值
     */
    static quint8 crc8(const QByteArray& data);

    /**
     * @brief CRC-8 MAXIM/Dallas算法（多项式0x31）
     * @param data 输入数据
     * @return CRC-8值
     */
    static quint8 crc8Maxim(const QByteArray& data);

    // ==================== 简单校验和 ====================

    /**
     * @brief XOR校验和（异或校验）
     * @param data 输入数据
     * @return 所有字节异或结果
     */
    static quint8 xorChecksum(const QByteArray& data);

    /**
     * @brief SUM校验和（累加和，取低8位）
     * @param data 输入数据
     * @return 累加和低8位
     */
    static quint8 sumChecksum(const QByteArray& data);

    /**
     * @brief SUM16校验和（累加和，取低16位）
     * @param data 输入数据
     * @return 累加和低16位
     */
    static quint16 sum16Checksum(const QByteArray& data);

    /**
     * @brief LRC校验和（纵向冗余校验）
     * @param data 输入数据
     * @return LRC值（累加和取反加1）
     */
    static quint8 lrcChecksum(const QByteArray& data);

    /**
     * @brief BCC校验（块校验字符，等同于XOR）
     * @param data 输入数据
     * @return BCC值
     */
    static quint8 bcc(const QByteArray& data);

    // ==================== 工具方法 ====================

    /**
     * @brief 将校验和结果转换为十六进制字符串
     * @param value 校验和值
     * @param width 输出宽度（字节数）
     * @param uppercase 是否大写
     * @return 十六进制字符串
     */
    static QString toHexString(quint32 value, int width = 1, bool uppercase = true);

    /**
     * @brief 验证CRC-16 Modbus
     * @param data 数据（包含2字节CRC）
     * @return 校验是否通过
     */
    static bool verifyCrc16Modbus(const QByteArray& data);

    /**
     * @brief 验证CRC-32
     * @param data 数据（包含4字节CRC）
     * @return 校验是否通过
     */
    static bool verifyCrc32(const QByteArray& data);

private:
    // CRC查找表
    static const quint16 CRC16_MODBUS_TABLE[256];
    static const quint32 CRC32_TABLE[256];
    static const quint8 CRC8_TABLE[256];

    // 初始化查找表
    static void initTables();
    static bool s_tablesInitialized;
};

} // namespace ComAssistant

#endif // COMASSISTANT_CHECKSUMUTILS_H
