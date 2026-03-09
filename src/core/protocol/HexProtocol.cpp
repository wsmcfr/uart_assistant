/**
 * @file HexProtocol.cpp
 * @brief 十六进制协议实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "HexProtocol.h"
#include "utils/Logger.h"

namespace ComAssistant {

HexProtocol::HexProtocol(QObject* parent)
    : IProtocol(parent)
{
}

void HexProtocol::setHexConfig(const HexConfig& config)
{
    m_hexConfig = config;
}

void HexProtocol::setFrameDelimiters(const QByteArray& head, const QByteArray& tail)
{
    m_hexConfig.frameHead = head;
    m_hexConfig.frameTail = tail;
}

void HexProtocol::setLengthField(int offset, int size, bool bigEndian, int adjustment)
{
    m_hexConfig.lengthFieldOffset = offset;
    m_hexConfig.lengthFieldSize = size;
    m_hexConfig.lengthBigEndian = bigEndian;
    m_hexConfig.lengthAdjustment = adjustment;
}

void HexProtocol::setChecksum(HexConfig::ChecksumType type, int size, bool bigEndian)
{
    m_hexConfig.useChecksum = true;
    m_hexConfig.checksumType = type;
    m_hexConfig.checksumSize = size;
    m_hexConfig.checksumBigEndian = bigEndian;
}

FrameResult HexProtocol::parse(const QByteArray& data)
{
    FrameResult result;

    if (data.isEmpty()) {
        return result;
    }

    m_buffer.append(data);

    int frameLen = findFrame();

    if (frameLen > 0) {
        result.valid = true;
        result.frame = m_buffer.left(frameLen);
        result.consumedBytes = frameLen;

        // 提取payload（去除帧头帧尾和校验）
        int headLen = m_hexConfig.frameHead.size();
        int tailLen = m_hexConfig.frameTail.size();
        int checkLen = m_hexConfig.useChecksum ? m_hexConfig.checksumSize : 0;

        int payloadStart = headLen;
        int payloadLen = frameLen - headLen - tailLen - checkLen;

        if (payloadLen > 0) {
            result.payload = result.frame.mid(payloadStart, payloadLen);
        }

        m_buffer.remove(0, frameLen);
        emit frameReceived(result);
    } else if (frameLen == -1) {
        // 无效数据，丢弃首字节
        m_buffer.remove(0, 1);
        result.consumedBytes = 1;
    }

    return result;
}

QByteArray HexProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    Q_UNUSED(metadata)

    QByteArray frame;

    // 帧头
    frame.append(m_hexConfig.frameHead);

    // 如果有长度字段，需要计算并插入
    if (m_hexConfig.lengthFieldOffset >= 0) {
        int dataLen = payload.size() + m_hexConfig.lengthAdjustment;
        int fieldSize = m_hexConfig.lengthFieldSize;

        QByteArray lenBytes;
        if (m_hexConfig.lengthBigEndian) {
            for (int i = fieldSize - 1; i >= 0; --i) {
                lenBytes.append(static_cast<char>((dataLen >> (i * 8)) & 0xFF));
            }
        } else {
            for (int i = 0; i < fieldSize; ++i) {
                lenBytes.append(static_cast<char>((dataLen >> (i * 8)) & 0xFF));
            }
        }
        frame.append(lenBytes);
    }

    // 数据
    frame.append(payload);

    // 校验和
    if (m_hexConfig.useChecksum) {
        QByteArray checksum = calculateChecksum(frame);
        frame.append(checksum);
    }

    // 帧尾
    frame.append(m_hexConfig.frameTail);

    return frame;
}

bool HexProtocol::validate(const QByteArray& frame)
{
    if (frame.isEmpty()) {
        return false;
    }

    // 检查帧头
    if (!m_hexConfig.frameHead.isEmpty()) {
        if (!frame.startsWith(m_hexConfig.frameHead)) {
            return false;
        }
    }

    // 检查帧尾
    if (!m_hexConfig.frameTail.isEmpty()) {
        if (!frame.endsWith(m_hexConfig.frameTail)) {
            return false;
        }
    }

    // 检查校验和
    if (m_hexConfig.useChecksum) {
        int checkLen = m_hexConfig.checksumSize;
        int tailLen = m_hexConfig.frameTail.size();
        int dataEnd = frame.size() - tailLen - checkLen;

        if (dataEnd <= 0) {
            return false;
        }

        QByteArray dataForCheck = frame.left(dataEnd);
        QByteArray expectedCheck = calculateChecksum(dataForCheck);
        QByteArray actualCheck = frame.mid(dataEnd, checkLen);

        if (expectedCheck != actualCheck) {
            return false;
        }
    }

    return true;
}

QByteArray HexProtocol::calculateChecksum(const QByteArray& data)
{
    switch (m_hexConfig.checksumType) {
        case HexConfig::ChecksumType::Sum8:
            return calcSum8(data);
        case HexConfig::ChecksumType::Sum16:
            return calcSum16(data);
        case HexConfig::ChecksumType::XOR:
            return calcXOR(data);
        case HexConfig::ChecksumType::CRC16:
            return calcCRC16(data);
        default:
            return calcSum8(data);
    }
}

void HexProtocol::reset()
{
    m_buffer.clear();
}

int HexProtocol::findFrame()
{
    // 查找帧头
    if (!m_hexConfig.frameHead.isEmpty()) {
        int headPos = m_buffer.indexOf(m_hexConfig.frameHead);
        if (headPos == -1) {
            return m_buffer.size() > 0 ? -1 : 0;
        }
        if (headPos > 0) {
            // 丢弃帧头之前的数据
            m_buffer.remove(0, headPos);
        }
    }

    int headLen = m_hexConfig.frameHead.size();

    // 使用长度字段确定帧长
    if (m_hexConfig.lengthFieldOffset >= 0) {
        int minLen = headLen + m_hexConfig.lengthFieldOffset + m_hexConfig.lengthFieldSize;
        if (m_buffer.size() < minLen) {
            return 0;
        }

        int dataLen = readLength(m_buffer, headLen + m_hexConfig.lengthFieldOffset);
        if (dataLen < 0 || dataLen > 65536) {
            return -1;  // 无效长度
        }

        int tailLen = m_hexConfig.frameTail.size();
        int checkLen = m_hexConfig.useChecksum ? m_hexConfig.checksumSize : 0;
        int frameLen = minLen + dataLen - m_hexConfig.lengthAdjustment + checkLen + tailLen;

        if (m_buffer.size() >= frameLen) {
            return frameLen;
        }
        return 0;
    }

    // 使用帧尾确定帧长
    if (!m_hexConfig.frameTail.isEmpty()) {
        int tailPos = m_buffer.indexOf(m_hexConfig.frameTail, headLen);
        if (tailPos == -1) {
            return 0;
        }
        return tailPos + m_hexConfig.frameTail.size();
    }

    // 无法确定帧边界
    return 0;
}

int HexProtocol::readLength(const QByteArray& data, int offset)
{
    int size = m_hexConfig.lengthFieldSize;
    if (offset + size > data.size()) {
        return -1;
    }

    int value = 0;
    const uchar* ptr = reinterpret_cast<const uchar*>(data.constData() + offset);

    if (m_hexConfig.lengthBigEndian) {
        for (int i = 0; i < size; ++i) {
            value = (value << 8) | ptr[i];
        }
    } else {
        for (int i = size - 1; i >= 0; --i) {
            value = (value << 8) | ptr[i];
        }
    }

    return value;
}

QByteArray HexProtocol::calcSum8(const QByteArray& data)
{
    uchar sum = 0;
    for (char c : data) {
        sum += static_cast<uchar>(c);
    }
    return QByteArray(1, static_cast<char>(sum));
}

QByteArray HexProtocol::calcSum16(const QByteArray& data)
{
    quint16 sum = 0;
    for (char c : data) {
        sum += static_cast<uchar>(c);
    }

    QByteArray result(2, 0);
    if (m_hexConfig.checksumBigEndian) {
        result[0] = static_cast<char>((sum >> 8) & 0xFF);
        result[1] = static_cast<char>(sum & 0xFF);
    } else {
        result[0] = static_cast<char>(sum & 0xFF);
        result[1] = static_cast<char>((sum >> 8) & 0xFF);
    }
    return result;
}

QByteArray HexProtocol::calcXOR(const QByteArray& data)
{
    uchar xorVal = 0;
    for (char c : data) {
        xorVal ^= static_cast<uchar>(c);
    }
    return QByteArray(1, static_cast<char>(xorVal));
}

QByteArray HexProtocol::calcCRC16(const QByteArray& data)
{
    // CRC-16/MODBUS 多项式: 0x8005, 初始值: 0xFFFF
    quint16 crc = 0xFFFF;
    const quint16 polynomial = 0x8005;

    for (char c : data) {
        crc ^= static_cast<uchar>(c);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
    }

    QByteArray result(2, 0);
    if (m_hexConfig.checksumBigEndian) {
        result[0] = static_cast<char>((crc >> 8) & 0xFF);
        result[1] = static_cast<char>(crc & 0xFF);
    } else {
        result[0] = static_cast<char>(crc & 0xFF);
        result[1] = static_cast<char>((crc >> 8) & 0xFF);
    }
    return result;
}

QByteArray HexProtocol::hexStringToBytes(const QString& hex)
{
    QString cleaned = hex.simplified().remove(' ');
    return QByteArray::fromHex(cleaned.toLatin1());
}

QString HexProtocol::bytesToHexString(const QByteArray& data, const QString& separator)
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0 && !separator.isEmpty()) {
            result += separator;
        }
        result += QString("%1").arg(static_cast<uchar>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return result;
}

} // namespace ComAssistant
