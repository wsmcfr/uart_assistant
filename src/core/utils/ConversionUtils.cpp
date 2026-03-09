/**
 * @file ConversionUtils.cpp
 * @brief 进制转换工具类实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "ConversionUtils.h"
#include <QRegularExpression>
#include <QtEndian>
#include <cstring>

namespace ComAssistant {

// ==================== 进制转换 ====================

QString ConversionUtils::decToHex(qint64 value, int width, bool uppercase)
{
    QString result = QString::number(value, 16);
    if (width > 0 && result.length() < width) {
        result = result.rightJustified(width, '0');
    }
    return uppercase ? result.toUpper() : result.toLower();
}

QString ConversionUtils::decToBin(qint64 value, int width)
{
    QString result = QString::number(value, 2);
    if (width > 0 && result.length() < width) {
        result = result.rightJustified(width, '0');
    }
    return result;
}

QString ConversionUtils::decToOct(qint64 value, int width)
{
    QString result = QString::number(value, 8);
    if (width > 0 && result.length() < width) {
        result = result.rightJustified(width, '0');
    }
    return result;
}

qint64 ConversionUtils::hexToDec(const QString& hex, bool* ok)
{
    QString cleanHex = hex.trimmed();
    // 移除0x或0X前缀
    if (cleanHex.startsWith("0x", Qt::CaseInsensitive)) {
        cleanHex = cleanHex.mid(2);
    }
    return cleanHex.toLongLong(ok, 16);
}

qint64 ConversionUtils::binToDec(const QString& bin, bool* ok)
{
    QString cleanBin = bin.trimmed();
    // 移除0b或0B前缀
    if (cleanBin.startsWith("0b", Qt::CaseInsensitive)) {
        cleanBin = cleanBin.mid(2);
    }
    return cleanBin.toLongLong(ok, 2);
}

qint64 ConversionUtils::octToDec(const QString& oct, bool* ok)
{
    QString cleanOct = oct.trimmed();
    // 移除0o或0O前缀
    if (cleanOct.startsWith("0o", Qt::CaseInsensitive)) {
        cleanOct = cleanOct.mid(2);
    }
    return cleanOct.toLongLong(ok, 8);
}

QString ConversionUtils::hexToBin(const QString& hex)
{
    bool ok;
    qint64 value = hexToDec(hex, &ok);
    if (!ok) return QString();
    return decToBin(value);
}

QString ConversionUtils::binToHex(const QString& bin, bool uppercase)
{
    bool ok;
    qint64 value = binToDec(bin, &ok);
    if (!ok) return QString();
    return decToHex(value, 0, uppercase);
}

// ==================== 字节序转换 ====================

QByteArray ConversionUtils::reverseBytes(const QByteArray& data)
{
    QByteArray result = data;
    std::reverse(result.begin(), result.end());
    return result;
}

quint16 ConversionUtils::swapBytes16(quint16 value)
{
    return ((value & 0xFF00) >> 8) | ((value & 0x00FF) << 8);
}

quint32 ConversionUtils::swapBytes32(quint32 value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x000000FF) << 24);
}

quint64 ConversionUtils::swapBytes64(quint64 value)
{
    return ((value & 0xFF00000000000000ULL) >> 56) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x000000FF00000000ULL) >> 8) |
           ((value & 0x00000000FF000000ULL) << 8) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x00000000000000FFULL) << 56);
}

// ==================== IEEE754浮点数转换 ====================

float ConversionUtils::bytesToFloat(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 4) return 0.0f;

    quint32 intValue;
    if (bigEndian) {
        intValue = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        intValue = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()));
    }

    float result;
    std::memcpy(&result, &intValue, sizeof(float));
    return result;
}

double ConversionUtils::bytesToDouble(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 8) return 0.0;

    quint64 intValue;
    if (bigEndian) {
        intValue = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        intValue = qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(bytes.constData()));
    }

    double result;
    std::memcpy(&result, &intValue, sizeof(double));
    return result;
}

QByteArray ConversionUtils::floatToBytes(float value, bool bigEndian)
{
    quint32 intValue;
    std::memcpy(&intValue, &value, sizeof(float));

    QByteArray result(4, 0);
    if (bigEndian) {
        qToBigEndian(intValue, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(intValue, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

QByteArray ConversionUtils::doubleToBytes(double value, bool bigEndian)
{
    quint64 intValue;
    std::memcpy(&intValue, &value, sizeof(double));

    QByteArray result(8, 0);
    if (bigEndian) {
        qToBigEndian(intValue, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(intValue, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

float ConversionUtils::hexToFloat(const QString& hex, bool bigEndian, bool* ok)
{
    QByteArray bytes = hexStringToBytes(hex);
    if (bytes.size() < 4) {
        if (ok) *ok = false;
        return 0.0f;
    }
    if (ok) *ok = true;
    return bytesToFloat(bytes.left(4), bigEndian);
}

QString ConversionUtils::floatToHex(float value, bool bigEndian, bool uppercase)
{
    QByteArray bytes = floatToBytes(value, bigEndian);
    return bytesToHexString(bytes, "", uppercase);
}

double ConversionUtils::hexToDouble(const QString& hex, bool bigEndian, bool* ok)
{
    QByteArray bytes = hexStringToBytes(hex);
    if (bytes.size() < 8) {
        if (ok) *ok = false;
        return 0.0;
    }
    if (ok) *ok = true;
    return bytesToDouble(bytes.left(8), bigEndian);
}

QString ConversionUtils::doubleToHex(double value, bool bigEndian, bool uppercase)
{
    QByteArray bytes = doubleToBytes(value, bigEndian);
    return bytesToHexString(bytes, "", uppercase);
}

// ==================== 整数转换 ====================

quint16 ConversionUtils::bytesToUint16(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 2) return 0;

    if (bigEndian) {
        return qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(bytes.constData()));
    }
}

quint32 ConversionUtils::bytesToUint32(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 4) return 0;

    if (bigEndian) {
        return qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()));
    }
}

quint64 ConversionUtils::bytesToUint64(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 8) return 0;

    if (bigEndian) {
        return qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        return qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(bytes.constData()));
    }
}

QByteArray ConversionUtils::uint16ToBytes(quint16 value, bool bigEndian)
{
    QByteArray result(2, 0);
    if (bigEndian) {
        qToBigEndian(value, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(value, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

QByteArray ConversionUtils::uint32ToBytes(quint32 value, bool bigEndian)
{
    QByteArray result(4, 0);
    if (bigEndian) {
        qToBigEndian(value, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(value, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

QByteArray ConversionUtils::uint64ToBytes(quint64 value, bool bigEndian)
{
    QByteArray result(8, 0);
    if (bigEndian) {
        qToBigEndian(value, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(value, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

// ==================== 有符号整数转换 ====================

qint8 ConversionUtils::bytesToInt8(const QByteArray& bytes)
{
    if (bytes.isEmpty()) return 0;
    return static_cast<qint8>(bytes.at(0));
}

qint16 ConversionUtils::bytesToInt16(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 2) return 0;

    if (bigEndian) {
        return qFromBigEndian<qint16>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        return qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(bytes.constData()));
    }
}

qint32 ConversionUtils::bytesToInt32(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 4) return 0;

    if (bigEndian) {
        return qFromBigEndian<qint32>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        return qFromLittleEndian<qint32>(reinterpret_cast<const uchar*>(bytes.constData()));
    }
}

qint64 ConversionUtils::bytesToInt64(const QByteArray& bytes, bool bigEndian)
{
    if (bytes.size() < 8) return 0;

    if (bigEndian) {
        return qFromBigEndian<qint64>(reinterpret_cast<const uchar*>(bytes.constData()));
    } else {
        return qFromLittleEndian<qint64>(reinterpret_cast<const uchar*>(bytes.constData()));
    }
}

QByteArray ConversionUtils::int8ToBytes(qint8 value)
{
    QByteArray result(1, 0);
    result[0] = static_cast<char>(value);
    return result;
}

QByteArray ConversionUtils::int16ToBytes(qint16 value, bool bigEndian)
{
    QByteArray result(2, 0);
    if (bigEndian) {
        qToBigEndian(value, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(value, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

QByteArray ConversionUtils::int32ToBytes(qint32 value, bool bigEndian)
{
    QByteArray result(4, 0);
    if (bigEndian) {
        qToBigEndian(value, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(value, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

QByteArray ConversionUtils::int64ToBytes(qint64 value, bool bigEndian)
{
    QByteArray result(8, 0);
    if (bigEndian) {
        qToBigEndian(value, reinterpret_cast<uchar*>(result.data()));
    } else {
        qToLittleEndian(value, reinterpret_cast<uchar*>(result.data()));
    }
    return result;
}

// ==================== 字符串/字节数组转换 ====================

QByteArray ConversionUtils::hexStringToBytes(const QString& hex)
{
    // 移除所有非十六进制字符
    QString cleanHex = hex;
    cleanHex.remove(QRegularExpression("[^0-9A-Fa-f]"));

    // 如果长度为奇数，前面补0
    if (cleanHex.length() % 2 != 0) {
        cleanHex.prepend('0');
    }

    return QByteArray::fromHex(cleanHex.toLatin1());
}

QString ConversionUtils::bytesToHexString(const QByteArray& data,
                                           const QString& separator,
                                           bool uppercase)
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0 && !separator.isEmpty()) {
            result += separator;
        }
        QString hex = QString::number(static_cast<quint8>(data[i]), 16);
        if (hex.length() == 1) {
            hex.prepend('0');
        }
        result += uppercase ? hex.toUpper() : hex.toLower();
    }
    return result;
}

QByteArray ConversionUtils::asciiToBytes(const QString& text)
{
    return text.toLatin1();
}

QString ConversionUtils::bytesToAscii(const QByteArray& data,
                                       bool replaceNonPrintable,
                                       QChar replacement)
{
    QString result;
    result.reserve(data.size());

    for (char byte : data) {
        unsigned char c = static_cast<unsigned char>(byte);
        if (replaceNonPrintable && (c < 32 || c > 126)) {
            result += replacement;
        } else {
            result += QChar(c);
        }
    }

    return result;
}

// ==================== 工具方法 ====================

QString ConversionUtils::formatNumber(qint64 value, int base,
                                       int groupSize, const QString& separator)
{
    QString result = QString::number(value, base);

    if (groupSize <= 0 || separator.isEmpty()) {
        return result;
    }

    // 从右向左插入分隔符
    QString formatted;
    int count = 0;
    for (int i = result.length() - 1; i >= 0; --i) {
        if (count > 0 && count % groupSize == 0) {
            formatted.prepend(separator);
        }
        formatted.prepend(result[i]);
        ++count;
    }

    return formatted;
}

bool ConversionUtils::isValidHex(const QString& text)
{
    QString cleanText = text.trimmed();
    // 移除可能的0x前缀
    if (cleanText.startsWith("0x", Qt::CaseInsensitive)) {
        cleanText = cleanText.mid(2);
    }
    // 移除空格
    cleanText.remove(' ');

    if (cleanText.isEmpty()) return false;

    static QRegularExpression hexRegex("^[0-9A-Fa-f]+$");
    return hexRegex.match(cleanText).hasMatch();
}

bool ConversionUtils::isValidBin(const QString& text)
{
    QString cleanText = text.trimmed();
    // 移除可能的0b前缀
    if (cleanText.startsWith("0b", Qt::CaseInsensitive)) {
        cleanText = cleanText.mid(2);
    }
    // 移除空格
    cleanText.remove(' ');

    if (cleanText.isEmpty()) return false;

    static QRegularExpression binRegex("^[01]+$");
    return binRegex.match(cleanText).hasMatch();
}

bool ConversionUtils::isValidOct(const QString& text)
{
    QString cleanText = text.trimmed();
    // 移除可能的0o前缀
    if (cleanText.startsWith("0o", Qt::CaseInsensitive)) {
        cleanText = cleanText.mid(2);
    }
    // 移除空格
    cleanText.remove(' ');

    if (cleanText.isEmpty()) return false;

    static QRegularExpression octRegex("^[0-7]+$");
    return octRegex.match(cleanText).hasMatch();
}

bool ConversionUtils::isValidDec(const QString& text)
{
    QString cleanText = text.trimmed();
    // 移除空格
    cleanText.remove(' ');

    if (cleanText.isEmpty()) return false;

    // 允许负号开头
    static QRegularExpression decRegex("^-?[0-9]+$");
    return decRegex.match(cleanText).hasMatch();
}

} // namespace ComAssistant
