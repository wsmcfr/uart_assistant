/**
 * @file EasyHexProtocol.cpp
 * @brief EasyHEX协议实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "EasyHexProtocol.h"
#include "core/utils/Logger.h"
#include <QRegularExpression>

namespace ComAssistant {

EasyHexProtocol::EasyHexProtocol(QObject* parent)
    : IProtocol(parent)
{
    // 默认配置: AA 55 帧头
    m_easyConfig.frameHeader = QByteArray::fromHex("AA55");
    m_easyConfig.frameTail = QByteArray();
    m_easyConfig.useChecksum = true;
    m_easyConfig.checksumType = EasyHexConfig::SUM8;
    m_easyConfig.lengthFieldOffset = 2;  // 帧头后面是长度
    m_easyConfig.lengthFieldSize = 1;
}

void EasyHexProtocol::setConfig(const EasyHexConfig& config)
{
    m_easyConfig = config;
}

void EasyHexProtocol::setFrameHeader(const QByteArray& header)
{
    m_easyConfig.frameHeader = header;
}

void EasyHexProtocol::setFrameTail(const QByteArray& tail)
{
    m_easyConfig.frameTail = tail;
}

void EasyHexProtocol::setUseChecksum(bool use)
{
    m_easyConfig.useChecksum = use;
}

FrameResult EasyHexProtocol::parse(const QByteArray& data)
{
    FrameResult result;
    m_buffer.append(data);

    int frameEnd = findFrame();
    if (frameEnd < 0) {
        result.valid = false;
        result.consumedBytes = 0;
        return result;
    }

    // 提取完整帧
    QByteArray frame = m_buffer.left(frameEnd);
    m_buffer = m_buffer.mid(frameEnd);

    // 校验
    if (validate(frame)) {
        // 提取数据部分
        int dataStart = m_easyConfig.frameHeader.size();
        if (m_easyConfig.lengthFieldOffset >= 0) {
            dataStart = m_easyConfig.lengthFieldOffset + m_easyConfig.lengthFieldSize;
        }
        int dataEnd = frame.size();
        if (m_easyConfig.useChecksum) {
            dataEnd -= 1;  // 减去校验和
        }
        if (!m_easyConfig.frameTail.isEmpty()) {
            dataEnd -= m_easyConfig.frameTail.size();
        }

        QByteArray payload = frame.mid(dataStart, dataEnd - dataStart);

        result.valid = true;
        result.frame = frame;
        result.payload = payload;
        result.consumedBytes = frameEnd;

        emit frameReceived(result);
        LOG_DEBUG(QString("EasyHEX frame parsed: %1").arg(toHexString(payload)));
    } else {
        result.valid = false;
        result.errorMessage = "Frame validation failed";
        result.consumedBytes = frameEnd;
        LOG_WARN("EasyHEX frame validation failed");
    }

    return result;
}

QByteArray EasyHexProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    Q_UNUSED(metadata)

    QByteArray frame;

    // 帧头
    frame.append(m_easyConfig.frameHeader);

    // 长度字段
    if (m_easyConfig.lengthFieldOffset >= 0) {
        int length = payload.size();
        if (m_easyConfig.useChecksum) {
            length += 1;
        }
        if (!m_easyConfig.frameTail.isEmpty()) {
            length += m_easyConfig.frameTail.size();
        }

        if (m_easyConfig.lengthFieldSize == 1) {
            frame.append(static_cast<char>(length & 0xFF));
        } else {
            frame.append(static_cast<char>((length >> 8) & 0xFF));
            frame.append(static_cast<char>(length & 0xFF));
        }
    }

    // 数据
    frame.append(payload);

    // 校验和
    if (m_easyConfig.useChecksum) {
        QByteArray checkData = frame.mid(m_easyConfig.frameHeader.size());
        frame.append(calculateChecksum(checkData));
    }

    // 帧尾
    if (!m_easyConfig.frameTail.isEmpty()) {
        frame.append(m_easyConfig.frameTail);
    }

    LOG_DEBUG(QString("EasyHEX frame built: %1").arg(toHexString(frame)));
    return frame;
}

bool EasyHexProtocol::validate(const QByteArray& frame)
{
    // 检查帧头
    if (!frame.startsWith(m_easyConfig.frameHeader)) {
        return false;
    }

    // 检查帧尾
    if (!m_easyConfig.frameTail.isEmpty() && !frame.endsWith(m_easyConfig.frameTail)) {
        return false;
    }

    // 检查校验和
    if (m_easyConfig.useChecksum) {
        int checksumPos = frame.size() - 1;
        if (!m_easyConfig.frameTail.isEmpty()) {
            checksumPos -= m_easyConfig.frameTail.size();
        }

        quint8 receivedChecksum = static_cast<quint8>(frame[checksumPos]);
        QByteArray checkData = frame.mid(m_easyConfig.frameHeader.size(), checksumPos - m_easyConfig.frameHeader.size());
        QByteArray calculatedChecksum = calculateChecksum(checkData);

        if (calculatedChecksum.isEmpty() || static_cast<quint8>(calculatedChecksum[0]) != receivedChecksum) {
            return false;
        }
    }

    return true;
}

QByteArray EasyHexProtocol::calculateChecksum(const QByteArray& data)
{
    quint8 checksum = 0;

    switch (m_easyConfig.checksumType) {
    case EasyHexConfig::SUM8:
        checksum = calcSum8(data);
        break;
    case EasyHexConfig::XOR8:
        checksum = calcXor8(data);
        break;
    case EasyHexConfig::CRC8:
        checksum = calcCrc8(data);
        break;
    }

    return QByteArray(1, static_cast<char>(checksum));
}

void EasyHexProtocol::reset()
{
    m_buffer.clear();
}

int EasyHexProtocol::findFrame()
{
    // 查找帧头
    int headerPos = m_buffer.indexOf(m_easyConfig.frameHeader);
    if (headerPos < 0) {
        m_buffer.clear();
        return -1;
    }

    // 丢弃帧头之前的数据
    if (headerPos > 0) {
        m_buffer = m_buffer.mid(headerPos);
    }

    // 计算最小帧长度
    int minFrameLen = m_easyConfig.frameHeader.size();
    if (m_easyConfig.lengthFieldOffset >= 0) {
        minFrameLen += m_easyConfig.lengthFieldSize;
    }
    if (m_easyConfig.useChecksum) {
        minFrameLen += 1;
    }
    minFrameLen += m_easyConfig.frameTail.size();

    if (m_buffer.size() < minFrameLen) {
        return -1;  // 数据不足
    }

    int frameLen = 0;

    // 根据长度字段或帧尾确定帧长度
    if (m_easyConfig.lengthFieldOffset >= 0) {
        // 有长度字段
        int lengthPos = m_easyConfig.lengthFieldOffset;
        if (m_buffer.size() < lengthPos + m_easyConfig.lengthFieldSize) {
            return -1;
        }

        if (m_easyConfig.lengthFieldSize == 1) {
            frameLen = static_cast<quint8>(m_buffer[lengthPos]);
        } else {
            frameLen = (static_cast<quint8>(m_buffer[lengthPos]) << 8) |
                       static_cast<quint8>(m_buffer[lengthPos + 1]);
        }

        // 加上帧头和长度字段本身
        frameLen += m_easyConfig.frameHeader.size() + m_easyConfig.lengthFieldSize;

    } else if (!m_easyConfig.frameTail.isEmpty()) {
        // 有帧尾，查找帧尾
        int tailPos = m_buffer.indexOf(m_easyConfig.frameTail, m_easyConfig.frameHeader.size());
        if (tailPos < 0) {
            return -1;
        }
        frameLen = tailPos + m_easyConfig.frameTail.size();

    } else {
        // 无长度字段也无帧尾，假设固定长度或使用超时
        return -1;
    }

    if (m_buffer.size() < frameLen) {
        return -1;  // 数据不足
    }

    return frameLen;
}

quint8 EasyHexProtocol::calcSum8(const QByteArray& data)
{
    quint8 sum = 0;
    for (char c : data) {
        sum += static_cast<quint8>(c);
    }
    return sum;
}

quint8 EasyHexProtocol::calcXor8(const QByteArray& data)
{
    quint8 xorVal = 0;
    for (char c : data) {
        xorVal ^= static_cast<quint8>(c);
    }
    return xorVal;
}

quint8 EasyHexProtocol::calcCrc8(const QByteArray& data)
{
    // CRC-8 (多项式: 0x07)
    quint8 crc = 0;
    for (char c : data) {
        crc ^= static_cast<quint8>(c);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

QByteArray EasyHexProtocol::fromHexString(const QString& hexString)
{
    QString cleaned = hexString;
    cleaned.remove(QRegularExpression("[^0-9A-Fa-f]"));

    QByteArray result;
    for (int i = 0; i + 1 < cleaned.length(); i += 2) {
        bool ok;
        int byte = cleaned.mid(i, 2).toInt(&ok, 16);
        if (ok) {
            result.append(static_cast<char>(byte));
        }
    }
    return result;
}

QString EasyHexProtocol::toHexString(const QByteArray& data, const QString& separator)
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0 && !separator.isEmpty()) {
            result += separator;
        }
        result += QString("%1").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return result;
}

} // namespace ComAssistant
