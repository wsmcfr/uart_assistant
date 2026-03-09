/**
 * @file EncodingUtils.cpp
 * @brief 编码转换工具类实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "EncodingUtils.h"
#include <QUrl>
#include <QRegularExpression>
#include <QStringConverter>

namespace ComAssistant {

// ==================== 字符编码转换 ====================

QByteArray EncodingUtils::convertEncoding(const QByteArray& data,
                                           const QString& fromCodec,
                                           const QString& toCodec)
{
    // Qt6 使用 QStringConverter，但对 GBK 等编码支持有限
    // 简化实现：只处理 UTF-8 和 Latin-1

    if (fromCodec.compare("UTF-8", Qt::CaseInsensitive) == 0) {
        QString text = QString::fromUtf8(data);
        if (toCodec.compare("UTF-8", Qt::CaseInsensitive) == 0) {
            return text.toUtf8();
        } else if (toCodec.compare("ISO-8859-1", Qt::CaseInsensitive) == 0 ||
                   toCodec.compare("Latin-1", Qt::CaseInsensitive) == 0) {
            return text.toLatin1();
        } else if (toCodec.compare("UTF-16", Qt::CaseInsensitive) == 0) {
            QStringEncoder encoder(QStringConverter::Utf16);
            return encoder.encode(text);
        }
    } else if (fromCodec.compare("ISO-8859-1", Qt::CaseInsensitive) == 0 ||
               fromCodec.compare("Latin-1", Qt::CaseInsensitive) == 0) {
        QString text = QString::fromLatin1(data);
        if (toCodec.compare("UTF-8", Qt::CaseInsensitive) == 0) {
            return text.toUtf8();
        }
    } else if (fromCodec.compare("UTF-16", Qt::CaseInsensitive) == 0) {
        QStringDecoder decoder(QStringConverter::Utf16);
        QString text = decoder.decode(data);
        if (toCodec.compare("UTF-8", Qt::CaseInsensitive) == 0) {
            return text.toUtf8();
        }
    }

    // 其他编码暂不支持，返回原数据
    return data;
}

QStringList EncodingUtils::supportedCodecs()
{
    QStringList codecs;
    // Qt6 支持的编码（通过 QStringConverter）
    codecs << "UTF-8" << "UTF-16" << "UTF-16BE" << "UTF-16LE"
           << "UTF-32" << "UTF-32BE" << "UTF-32LE"
           << "ISO-8859-1" << "Latin-1"
           << "System";
    return codecs;
}

QString EncodingUtils::detectEncoding(const QByteArray& data)
{
    // 检测UTF-8 BOM
    if (data.startsWith("\xEF\xBB\xBF")) {
        return "UTF-8";
    }
    // 检测UTF-16 LE BOM
    if (data.startsWith("\xFF\xFE")) {
        return "UTF-16LE";
    }
    // 检测UTF-16 BE BOM
    if (data.startsWith("\xFE\xFF")) {
        return "UTF-16BE";
    }
    // 检测UTF-32 LE BOM
    if (data.startsWith("\xFF\xFE\x00\x00")) {
        return "UTF-32LE";
    }
    // 检测UTF-32 BE BOM
    if (data.startsWith("\x00\x00\xFE\xFF")) {
        return "UTF-32BE";
    }

    // 尝试UTF-8验证
    if (isValidUtf8(data)) {
        // 检查是否有非ASCII字符
        bool hasNonAscii = false;
        for (char c : data) {
            if (static_cast<unsigned char>(c) > 127) {
                hasNonAscii = true;
                break;
            }
        }
        if (hasNonAscii) {
            return "UTF-8";
        }
    }

    // 默认返回ASCII/Latin-1
    return "ISO-8859-1";
}

QByteArray EncodingUtils::utf8ToGbk(const QByteArray& utf8Data)
{
    // Qt6 不直接支持 GBK，返回 UTF-8
    return utf8Data;
}

QByteArray EncodingUtils::gbkToUtf8(const QByteArray& gbkData)
{
    // Qt6 不直接支持 GBK，假设输入是 Latin-1
    return QString::fromLatin1(gbkData).toUtf8();
}

QString EncodingUtils::utf8ToUnicode(const QByteArray& utf8Data)
{
    return QString::fromUtf8(utf8Data);
}

QByteArray EncodingUtils::unicodeToUtf8(const QString& text)
{
    return text.toUtf8();
}

// ==================== Base64编码 ====================

QString EncodingUtils::toBase64(const QByteArray& data)
{
    return QString::fromLatin1(data.toBase64());
}

QByteArray EncodingUtils::fromBase64(const QString& base64)
{
    return QByteArray::fromBase64(base64.toLatin1());
}

QString EncodingUtils::stringToBase64(const QString& text, const QString& codec)
{
    Q_UNUSED(codec)
    return toBase64(text.toUtf8());
}

QString EncodingUtils::base64ToString(const QString& base64, const QString& codec)
{
    Q_UNUSED(codec)
    QByteArray data = fromBase64(base64);
    return QString::fromUtf8(data);
}

// ==================== URL编码 ====================

QString EncodingUtils::urlEncode(const QString& text)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(text));
}

QString EncodingUtils::urlDecode(const QString& encoded)
{
    return QUrl::fromPercentEncoding(encoded.toUtf8());
}

// ==================== 转义序列 ====================

QByteArray EncodingUtils::processEscapeSequences(const QString& text)
{
    QByteArray result;
    result.reserve(text.length());

    int i = 0;
    while (i < text.length()) {
        if (text[i] == '\\' && i + 1 < text.length()) {
            QChar next = text[i + 1];
            if (next == 'n') {
                result.append('\n');
                i += 2;
            } else if (next == 'r') {
                result.append('\r');
                i += 2;
            } else if (next == 't') {
                result.append('\t');
                i += 2;
            } else if (next == '\\') {
                result.append('\\');
                i += 2;
            } else if (next == '0') {
                result.append('\0');
                i += 2;
            } else if (next == 'x' && i + 3 < text.length()) {
                // \xHH格式
                QString hexStr = text.mid(i + 2, 2);
                bool ok;
                int value = hexStr.toInt(&ok, 16);
                if (ok) {
                    result.append(static_cast<char>(value));
                    i += 4;
                } else {
                    result.append(text[i].toLatin1());
                    ++i;
                }
            } else {
                result.append(text[i].toLatin1());
                ++i;
            }
        } else {
            result.append(text[i].toLatin1());
            ++i;
        }
    }

    return result;
}

QString EncodingUtils::toEscapeString(const QByteArray& data)
{
    QString result;
    result.reserve(data.length() * 2);

    for (char byte : data) {
        unsigned char c = static_cast<unsigned char>(byte);
        if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else if (c == '\t') {
            result += "\\t";
        } else if (c == '\\') {
            result += "\\\\";
        } else if (c == '\0') {
            result += "\\0";
        } else if (c >= 32 && c <= 126) {
            result += QChar(c);
        } else {
            result += QString("\\x%1").arg(c, 2, 16, QChar('0'));
        }
    }

    return result;
}

// ==================== Unicode转义 ====================

QString EncodingUtils::toUnicodeEscape(const QString& text)
{
    QString result;
    result.reserve(text.length() * 6);

    for (const QChar& ch : text) {
        if (ch.unicode() < 128 && ch.isPrint()) {
            result += ch;
        } else {
            result += QString("\\u%1").arg(ch.unicode(), 4, 16, QChar('0'));
        }
    }

    return result;
}

QString EncodingUtils::fromUnicodeEscape(const QString& escaped)
{
    QString result;
    QRegularExpression re("\\\\u([0-9A-Fa-f]{4})");
    QString working = escaped;

    QRegularExpressionMatchIterator it = re.globalMatch(working);
    int lastEnd = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result += working.mid(lastEnd, match.capturedStart() - lastEnd);
        bool ok;
        ushort code = match.captured(1).toUShort(&ok, 16);
        if (ok) {
            result += QChar(code);
        } else {
            result += match.captured(0);
        }
        lastEnd = match.capturedEnd();
    }
    result += working.mid(lastEnd);

    return result;
}

// ==================== HTML实体编码 ====================

QString EncodingUtils::htmlEncode(const QString& text)
{
    QString result;
    result.reserve(text.length() * 2);

    for (const QChar& ch : text) {
        switch (ch.unicode()) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        case '\'':
            result += "&#39;";
            break;
        default:
            if (ch.unicode() > 127) {
                result += QString("&#%1;").arg(ch.unicode());
            } else {
                result += ch;
            }
            break;
        }
    }

    return result;
}

QString EncodingUtils::htmlDecode(const QString& encoded)
{
    QString result = encoded;

    // 命名实体
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&#39;", "'");
    result.replace("&apos;", "'");
    result.replace("&nbsp;", " ");

    // 数字实体
    QRegularExpression numericRe("&#(\\d+);");
    QRegularExpressionMatchIterator it = numericRe.globalMatch(result);

    QList<QPair<int, int>> replacements;
    QStringList replaceWith;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        bool ok;
        uint code = match.captured(1).toUInt(&ok);
        if (ok && code <= 0x10FFFF) {
            replacements.append(qMakePair(match.capturedStart(), match.capturedLength()));
            replaceWith.append(QString::fromUcs4(&code, 1));
        }
    }

    // 从后向前替换以保持索引正确
    for (int i = replacements.size() - 1; i >= 0; --i) {
        result.replace(replacements[i].first, replacements[i].second, replaceWith[i]);
    }

    // 十六进制实体
    QRegularExpression hexRe("&#x([0-9A-Fa-f]+);");
    it = hexRe.globalMatch(result);

    replacements.clear();
    replaceWith.clear();

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        bool ok;
        uint code = match.captured(1).toUInt(&ok, 16);
        if (ok && code <= 0x10FFFF) {
            replacements.append(qMakePair(match.capturedStart(), match.capturedLength()));
            replaceWith.append(QString::fromUcs4(&code, 1));
        }
    }

    for (int i = replacements.size() - 1; i >= 0; --i) {
        result.replace(replacements[i].first, replacements[i].second, replaceWith[i]);
    }

    return result;
}

// ==================== 工具方法 ====================

bool EncodingUtils::isValidUtf8(const QByteArray& data)
{
    int i = 0;
    while (i < data.size()) {
        unsigned char c = static_cast<unsigned char>(data[i]);

        if (c < 0x80) {
            // ASCII
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            // 2字节序列
            if (i + 1 >= data.size()) return false;
            if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3字节序列
            if (i + 2 >= data.size()) return false;
            if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) != 0x80) return false;
            if ((static_cast<unsigned char>(data[i + 2]) & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4字节序列
            if (i + 3 >= data.size()) return false;
            if ((static_cast<unsigned char>(data[i + 1]) & 0xC0) != 0x80) return false;
            if ((static_cast<unsigned char>(data[i + 2]) & 0xC0) != 0x80) return false;
            if ((static_cast<unsigned char>(data[i + 3]) & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

bool EncodingUtils::isValidBase64(const QString& text)
{
    QString cleanText = text.trimmed();
    // 移除换行和空格
    cleanText.remove(QRegularExpression("\\s"));

    if (cleanText.isEmpty()) return false;

    // Base64只包含 A-Z, a-z, 0-9, +, /, =
    static QRegularExpression base64Re("^[A-Za-z0-9+/]*={0,2}$");
    if (!base64Re.match(cleanText).hasMatch()) return false;

    // 长度必须是4的倍数
    return (cleanText.length() % 4 == 0);
}

QList<uint> EncodingUtils::getCodePoints(const QString& text)
{
    QList<uint> codePoints;
    int i = 0;
    while (i < text.length()) {
        uint cp = text[i].unicode();
        if (text[i].isHighSurrogate() && i + 1 < text.length() && text[i + 1].isLowSurrogate()) {
            cp = QChar::surrogateToUcs4(text[i], text[i + 1]);
            i += 2;
        } else {
            ++i;
        }
        codePoints.append(cp);
    }
    return codePoints;
}

QString EncodingUtils::fromCodePoints(const QList<uint>& codePoints)
{
    QString result;
    for (uint cp : codePoints) {
        if (cp <= 0xFFFF) {
            result += QChar(static_cast<ushort>(cp));
        } else {
            // 使用代理对
            result += QString::fromUcs4(&cp, 1);
        }
    }
    return result;
}

} // namespace ComAssistant
