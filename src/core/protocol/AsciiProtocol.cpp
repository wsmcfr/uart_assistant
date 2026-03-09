/**
 * @file AsciiProtocol.cpp
 * @brief ASCII文本协议实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "AsciiProtocol.h"

namespace ComAssistant {

AsciiProtocol::AsciiProtocol(QObject* parent)
    : IProtocol(parent)
{
}

void AsciiProtocol::setAsciiConfig(const AsciiConfig& config)
{
    m_asciiConfig = config;
}

void AsciiProtocol::setLineEnding(LineEnding ending)
{
    m_asciiConfig.lineEnding = ending;
}

QByteArray AsciiProtocol::lineEndingString(LineEnding ending)
{
    switch (ending) {
        case LineEnding::CR:    return QByteArray("\r");
        case LineEnding::LF:    return QByteArray("\n");
        case LineEnding::CRLF:  return QByteArray("\r\n");
        default:                return QByteArray();
    }
}

FrameResult AsciiProtocol::parse(const QByteArray& data)
{
    FrameResult result;

    if (data.isEmpty()) {
        return result;
    }

    // 追加到内部缓冲区
    m_buffer.append(data);

    // 查找行结束符
    int endPos = findLineEnding(m_buffer);

    if (endPos >= 0) {
        QByteArray lineEnd = lineEndingString(m_asciiConfig.lineEnding);
        int frameLen = endPos + lineEnd.size();

        result.valid = true;
        result.frame = m_buffer.left(frameLen);
        result.payload = m_buffer.left(endPos);  // 不包含行结束符
        result.consumedBytes = frameLen;

        m_buffer.remove(0, frameLen);

        emit frameReceived(result);
    }

    return result;
}

QByteArray AsciiProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    Q_UNUSED(metadata)

    QByteArray frame = payload;

    if (m_asciiConfig.appendLineEnding) {
        frame.append(lineEndingString(m_asciiConfig.lineEnding));
    }

    return frame;
}

bool AsciiProtocol::validate(const QByteArray& frame)
{
    // ASCII协议一般不需要校验，只检查是否为有效文本
    for (char c : frame) {
        // 允许可打印字符和常见控制字符
        if (c < 0 && c != '\r' && c != '\n' && c != '\t') {
            return false;
        }
    }
    return true;
}

QByteArray AsciiProtocol::calculateChecksum(const QByteArray& data)
{
    // ASCII协议通常不使用校验和
    Q_UNUSED(data)
    return QByteArray();
}

void AsciiProtocol::reset()
{
    m_buffer.clear();
}

QByteArray AsciiProtocol::textToFrame(const QString& text)
{
    QByteArray payload = text.toUtf8();
    return build(payload, QVariantMap());
}

QString AsciiProtocol::frameToText(const QByteArray& frame)
{
    QByteArray data = frame;

    // 移除各种可能的行结束符
    while (data.endsWith('\r') || data.endsWith('\n')) {
        data.chop(1);
    }

    return QString::fromUtf8(data);
}

int AsciiProtocol::findLineEnding(const QByteArray& data)
{
    LineEnding ending = m_asciiConfig.lineEnding;

    switch (ending) {
        case LineEnding::CR:
            return data.indexOf('\r');

        case LineEnding::LF:
            return data.indexOf('\n');

        case LineEnding::CRLF: {
            int pos = data.indexOf("\r\n");
            return pos;
        }

        case LineEnding::None:
        default:
            // 无行结束符，不分帧
            return -1;
    }
}

} // namespace ComAssistant
