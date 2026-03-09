/**
 * @file ConversionUtils.h
 * @brief 进制转换工具类
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_CONVERSIONUTILS_H
#define COMASSISTANT_CONVERSIONUTILS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace ComAssistant {

/**
 * @brief 进制转换工具类
 * 提供进制转换、字节序转换、IEEE754浮点数转换等功能
 */
class ConversionUtils {
public:
    // ==================== 进制转换 ====================

    /**
     * @brief 十进制转十六进制
     * @param value 十进制值
     * @param width 输出宽度（字符数），0表示自动
     * @param uppercase 是否大写
     * @return 十六进制字符串
     */
    static QString decToHex(qint64 value, int width = 0, bool uppercase = true);

    /**
     * @brief 十进制转二进制
     * @param value 十进制值
     * @param width 输出宽度（位数），0表示自动
     * @return 二进制字符串
     */
    static QString decToBin(qint64 value, int width = 0);

    /**
     * @brief 十进制转八进制
     * @param value 十进制值
     * @param width 输出宽度，0表示自动
     * @return 八进制字符串
     */
    static QString decToOct(qint64 value, int width = 0);

    /**
     * @brief 十六进制转十进制
     * @param hex 十六进制字符串（可带0x前缀）
     * @param ok 转换是否成功
     * @return 十进制值
     */
    static qint64 hexToDec(const QString& hex, bool* ok = nullptr);

    /**
     * @brief 二进制转十进制
     * @param bin 二进制字符串（可带0b前缀）
     * @param ok 转换是否成功
     * @return 十进制值
     */
    static qint64 binToDec(const QString& bin, bool* ok = nullptr);

    /**
     * @brief 八进制转十进制
     * @param oct 八进制字符串（可带0o前缀）
     * @param ok 转换是否成功
     * @return 十进制值
     */
    static qint64 octToDec(const QString& oct, bool* ok = nullptr);

    /**
     * @brief 十六进制转二进制
     * @param hex 十六进制字符串
     * @return 二进制字符串
     */
    static QString hexToBin(const QString& hex);

    /**
     * @brief 二进制转十六进制
     * @param bin 二进制字符串
     * @param uppercase 是否大写
     * @return 十六进制字符串
     */
    static QString binToHex(const QString& bin, bool uppercase = true);

    // ==================== 字节序转换 ====================

    /**
     * @brief 反转字节数组
     * @param data 输入数据
     * @return 反转后的数据
     */
    static QByteArray reverseBytes(const QByteArray& data);

    /**
     * @brief 交换16位值的字节序
     * @param value 输入值
     * @return 交换后的值
     */
    static quint16 swapBytes16(quint16 value);

    /**
     * @brief 交换32位值的字节序
     * @param value 输入值
     * @return 交换后的值
     */
    static quint32 swapBytes32(quint32 value);

    /**
     * @brief 交换64位值的字节序
     * @param value 输入值
     * @return 交换后的值
     */
    static quint64 swapBytes64(quint64 value);

    // ==================== IEEE754浮点数转换 ====================

    /**
     * @brief 字节数组转32位浮点数
     * @param bytes 4字节数据
     * @param bigEndian 是否大端序
     * @return 浮点数值
     */
    static float bytesToFloat(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 字节数组转64位双精度浮点数
     * @param bytes 8字节数据
     * @param bigEndian 是否大端序
     * @return 双精度浮点数值
     */
    static double bytesToDouble(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 32位浮点数转字节数组
     * @param value 浮点数值
     * @param bigEndian 是否大端序
     * @return 4字节数据
     */
    static QByteArray floatToBytes(float value, bool bigEndian = false);

    /**
     * @brief 64位双精度浮点数转字节数组
     * @param value 双精度浮点数值
     * @param bigEndian 是否大端序
     * @return 8字节数据
     */
    static QByteArray doubleToBytes(double value, bool bigEndian = false);

    /**
     * @brief 十六进制字符串转浮点数
     * @param hex 8字符十六进制（32位IEEE754）
     * @param bigEndian 是否大端序
     * @param ok 转换是否成功
     * @return 浮点数值
     */
    static float hexToFloat(const QString& hex, bool bigEndian = false, bool* ok = nullptr);

    /**
     * @brief 浮点数转十六进制字符串
     * @param value 浮点数值
     * @param bigEndian 是否大端序
     * @param uppercase 是否大写
     * @return 8字符十六进制字符串
     */
    static QString floatToHex(float value, bool bigEndian = false, bool uppercase = true);

    /**
     * @brief 十六进制字符串转双精度浮点数
     * @param hex 16字符十六进制（64位IEEE754）
     * @param bigEndian 是否大端序
     * @param ok 转换是否成功
     * @return 双精度浮点数值
     */
    static double hexToDouble(const QString& hex, bool bigEndian = false, bool* ok = nullptr);

    /**
     * @brief 双精度浮点数转十六进制字符串
     * @param value 双精度浮点数值
     * @param bigEndian 是否大端序
     * @param uppercase 是否大写
     * @return 16字符十六进制字符串
     */
    static QString doubleToHex(double value, bool bigEndian = false, bool uppercase = true);

    // ==================== 整数转换 ====================

    /**
     * @brief 字节数组转16位无符号整数
     * @param bytes 2字节数据
     * @param bigEndian 是否大端序
     * @return 16位无符号整数
     */
    static quint16 bytesToUint16(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 字节数组转32位无符号整数
     * @param bytes 4字节数据
     * @param bigEndian 是否大端序
     * @return 32位无符号整数
     */
    static quint32 bytesToUint32(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 字节数组转64位无符号整数
     * @param bytes 8字节数据
     * @param bigEndian 是否大端序
     * @return 64位无符号整数
     */
    static quint64 bytesToUint64(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 16位无符号整数转字节数组
     * @param value 16位无符号整数
     * @param bigEndian 是否大端序
     * @return 2字节数据
     */
    static QByteArray uint16ToBytes(quint16 value, bool bigEndian = false);

    /**
     * @brief 32位无符号整数转字节数组
     * @param value 32位无符号整数
     * @param bigEndian 是否大端序
     * @return 4字节数据
     */
    static QByteArray uint32ToBytes(quint32 value, bool bigEndian = false);

    /**
     * @brief 64位无符号整数转字节数组
     * @param value 64位无符号整数
     * @param bigEndian 是否大端序
     * @return 8字节数据
     */
    static QByteArray uint64ToBytes(quint64 value, bool bigEndian = false);

    // ==================== 有符号整数转换 ====================

    /**
     * @brief 字节数组转8位有符号整数
     * @param bytes 1字节数据
     * @return 8位有符号整数
     */
    static qint8 bytesToInt8(const QByteArray& bytes);

    /**
     * @brief 字节数组转16位有符号整数
     * @param bytes 2字节数据
     * @param bigEndian 是否大端序
     * @return 16位有符号整数
     */
    static qint16 bytesToInt16(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 字节数组转32位有符号整数
     * @param bytes 4字节数据
     * @param bigEndian 是否大端序
     * @return 32位有符号整数
     */
    static qint32 bytesToInt32(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 字节数组转64位有符号整数
     * @param bytes 8字节数据
     * @param bigEndian 是否大端序
     * @return 64位有符号整数
     */
    static qint64 bytesToInt64(const QByteArray& bytes, bool bigEndian = false);

    /**
     * @brief 8位有符号整数转字节数组
     * @param value 8位有符号整数
     * @return 1字节数据
     */
    static QByteArray int8ToBytes(qint8 value);

    /**
     * @brief 16位有符号整数转字节数组
     * @param value 16位有符号整数
     * @param bigEndian 是否大端序
     * @return 2字节数据
     */
    static QByteArray int16ToBytes(qint16 value, bool bigEndian = false);

    /**
     * @brief 32位有符号整数转字节数组
     * @param value 32位有符号整数
     * @param bigEndian 是否大端序
     * @return 4字节数据
     */
    static QByteArray int32ToBytes(qint32 value, bool bigEndian = false);

    /**
     * @brief 64位有符号整数转字节数组
     * @param value 64位有符号整数
     * @param bigEndian 是否大端序
     * @return 8字节数据
     */
    static QByteArray int64ToBytes(qint64 value, bool bigEndian = false);

    // ==================== 字符串/字节数组转换 ====================

    /**
     * @brief 十六进制字符串转字节数组
     * @param hex 十六进制字符串（空格分隔或连续）
     * @return 字节数组
     */
    static QByteArray hexStringToBytes(const QString& hex);

    /**
     * @brief 字节数组转十六进制字符串
     * @param data 字节数组
     * @param separator 分隔符（默认空格）
     * @param uppercase 是否大写
     * @return 十六进制字符串
     */
    static QString bytesToHexString(const QByteArray& data,
                                     const QString& separator = " ",
                                     bool uppercase = true);

    /**
     * @brief ASCII字符串转字节数组
     * @param text ASCII字符串
     * @return 字节数组
     */
    static QByteArray asciiToBytes(const QString& text);

    /**
     * @brief 字节数组转ASCII字符串
     * @param data 字节数组
     * @param replaceNonPrintable 是否替换不可打印字符
     * @param replacement 替换字符
     * @return ASCII字符串
     */
    static QString bytesToAscii(const QByteArray& data,
                                 bool replaceNonPrintable = true,
                                 QChar replacement = '.');

    // ==================== 工具方法 ====================

    /**
     * @brief 格式化数字（添加分组分隔符）
     * @param value 数值
     * @param base 进制（2、8、10、16）
     * @param groupSize 分组大小
     * @param separator 分隔符
     * @return 格式化的字符串
     */
    static QString formatNumber(qint64 value, int base = 10,
                                 int groupSize = 4, const QString& separator = " ");

    /**
     * @brief 检查字符串是否为有效的十六进制
     * @param text 输入字符串
     * @return 是否有效
     */
    static bool isValidHex(const QString& text);

    /**
     * @brief 检查字符串是否为有效的二进制
     * @param text 输入字符串
     * @return 是否有效
     */
    static bool isValidBin(const QString& text);

    /**
     * @brief 检查字符串是否为有效的八进制
     * @param text 输入字符串
     * @return 是否有效
     */
    static bool isValidOct(const QString& text);

    /**
     * @brief 检查字符串是否为有效的十进制
     * @param text 输入字符串
     * @return 是否有效
     */
    static bool isValidDec(const QString& text);
};

} // namespace ComAssistant

#endif // COMASSISTANT_CONVERSIONUTILS_H
