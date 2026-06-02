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

void AsciiProtocol::setConfig(const QVariantMap& config)
{
    /*
     * Factory 会先通过 Schema 完成类型校验、默认值补全和枚举约束。
     * 这里仍保留温和的字符串分支，是为了让直接调用 setConfig() 的旧代码
     * 也能得到可预期的内部状态；未知值保持当前默认值，不主动制造无效状态。
     */
    m_config = config;

    const QString lineEnding = config.value(QStringLiteral("lineEnding"), QStringLiteral("CRLF")).toString();
    if (lineEnding == QStringLiteral("None")) {
        m_asciiConfig.lineEnding = LineEnding::None;
    } else if (lineEnding == QStringLiteral("CR")) {
        m_asciiConfig.lineEnding = LineEnding::CR;
    } else if (lineEnding == QStringLiteral("LF")) {
        m_asciiConfig.lineEnding = LineEnding::LF;
    } else if (lineEnding == QStringLiteral("CRLF")) {
        m_asciiConfig.lineEnding = LineEnding::CRLF;
    }

    m_asciiConfig.appendLineEnding =
        config.value(QStringLiteral("appendLineEnding"), m_asciiConfig.appendLineEnding).toBool();
    m_asciiConfig.timeoutMs =
        config.value(QStringLiteral("timeoutMs"), m_asciiConfig.timeoutMs).toInt();
    m_asciiConfig.encoding =
        config.value(QStringLiteral("encoding"), m_asciiConfig.encoding).toString();
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
