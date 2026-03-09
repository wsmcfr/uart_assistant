/**
 * @file EncodingUtils.h
 * @brief 编码转换工具类
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_ENCODINGUTILS_H
#define COMASSISTANT_ENCODINGUTILS_H

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace ComAssistant {

/**
 * @brief 编码转换工具类
 * 提供字符编码转换、Base64、URL编码等功能
 */
class EncodingUtils {
public:
    // ==================== 字符编码转换 ====================

    /**
     * @brief 转换字符编码
     * @param data 输入数据
     * @param fromCodec 源编码（如 "UTF-8", "GBK", "ISO-8859-1"）
     * @param toCodec 目标编码
     * @return 转换后的数据
     */
    static QByteArray convertEncoding(const QByteArray& data,
                                       const QString& fromCodec,
                                       const QString& toCodec);

    /**
     * @brief 获取支持的编码列表
     * @return 编码名称列表
     */
    static QStringList supportedCodecs();

    /**
     * @brief 检测字节数组的可能编码
     * @param data 输入数据
     * @return 可能的编码名称（猜测）
     */
    static QString detectEncoding(const QByteArray& data);

    /**
     * @brief UTF-8转GBK
     * @param utf8Data UTF-8编码数据
     * @return GBK编码数据
     */
    static QByteArray utf8ToGbk(const QByteArray& utf8Data);

    /**
     * @brief GBK转UTF-8
     * @param gbkData GBK编码数据
     * @return UTF-8编码数据
     */
    static QByteArray gbkToUtf8(const QByteArray& gbkData);

    /**
     * @brief UTF-8转Unicode
     * @param utf8Data UTF-8编码数据
     * @return Unicode字符串
     */
    static QString utf8ToUnicode(const QByteArray& utf8Data);

    /**
     * @brief Unicode转UTF-8
     * @param text Unicode字符串
     * @return UTF-8编码数据
     */
    static QByteArray unicodeToUtf8(const QString& text);

    // ==================== Base64编码 ====================

    /**
     * @brief 字节数组转Base64
     * @param data 输入数据
     * @return Base64字符串
     */
    static QString toBase64(const QByteArray& data);

    /**
     * @brief Base64转字节数组
     * @param base64 Base64字符串
     * @return 解码后的数据
     */
    static QByteArray fromBase64(const QString& base64);

    /**
     * @brief 字符串转Base64
     * @param text 输入字符串
     * @param codec 编码（默认UTF-8）
     * @return Base64字符串
     */
    static QString stringToBase64(const QString& text, const QString& codec = "UTF-8");

    /**
     * @brief Base64转字符串
     * @param base64 Base64字符串
     * @param codec 编码（默认UTF-8）
     * @return 解码后的字符串
     */
    static QString base64ToString(const QString& base64, const QString& codec = "UTF-8");

    // ==================== URL编码 ====================

    /**
     * @brief URL编码
     * @param text 输入字符串
     * @return URL编码后的字符串
     */
    static QString urlEncode(const QString& text);

    /**
     * @brief URL解码
     * @param encoded URL编码的字符串
     * @return 解码后的字符串
     */
    static QString urlDecode(const QString& encoded);

    // ==================== 转义序列 ====================

    /**
     * @brief 处理转义序列
     * @param text 包含转义序列的字符串（如 \n, \r, \t, \x00）
     * @return 处理后的字节数组
     */
    static QByteArray processEscapeSequences(const QString& text);

    /**
     * @brief 将字节数组转为带转义序列的字符串
     * @param data 输入数据
     * @return 带转义序列的字符串
     */
    static QString toEscapeString(const QByteArray& data);

    // ==================== Unicode转义 ====================

    /**
     * @brief Unicode转义编码（如 \u4e2d\u6587）
     * @param text 输入字符串
     * @return Unicode转义字符串
     */
    static QString toUnicodeEscape(const QString& text);

    /**
     * @brief Unicode转义解码
     * @param escaped Unicode转义字符串
     * @return 解码后的字符串
     */
    static QString fromUnicodeEscape(const QString& escaped);

    // ==================== HTML实体编码 ====================

    /**
     * @brief HTML实体编码
     * @param text 输入字符串
     * @return HTML编码后的字符串
     */
    static QString htmlEncode(const QString& text);

    /**
     * @brief HTML实体解码
     * @param encoded HTML编码的字符串
     * @return 解码后的字符串
     */
    static QString htmlDecode(const QString& encoded);

    // ==================== 工具方法 ====================

    /**
     * @brief 检查是否为有效的UTF-8
     * @param data 输入数据
     * @return 是否为有效UTF-8
     */
    static bool isValidUtf8(const QByteArray& data);

    /**
     * @brief 检查是否为有效的Base64
     * @param text 输入字符串
     * @return 是否为有效Base64
     */
    static bool isValidBase64(const QString& text);

    /**
     * @brief 获取字符串的Unicode码点
     * @param text 输入字符串
     * @return 码点列表
     */
    static QList<uint> getCodePoints(const QString& text);

    /**
     * @brief 从Unicode码点构建字符串
     * @param codePoints 码点列表
     * @return 字符串
     */
    static QString fromCodePoints(const QList<uint>& codePoints);
};

} // namespace ComAssistant

#endif // COMASSISTANT_ENCODINGUTILS_H
