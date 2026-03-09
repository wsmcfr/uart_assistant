/**
 * @file FrameParser.cpp
 * @brief 通用帧解析器实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "FrameParser.h"
#include "utils/Logger.h"

namespace ComAssistant {

FrameParser::FrameParser(QObject* parent)
    : QObject(parent)
{
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &FrameParser::onTimeout);
}

FrameParser::~FrameParser()
{
    m_timeoutTimer->stop();
}

void FrameParser::setConfig(const FrameConfig& config)
{
    m_config = config;

    if (m_config.delimiter == FrameDelimiter::Timeout) {
        m_timeoutTimer->setInterval(m_config.timeoutMs);
    }
}

void FrameParser::feed(const QByteArray& data)
{
    if (data.isEmpty()) {
        return;
    }

    m_buffer.append(data);

    // 检查缓冲区溢出
    if (m_buffer.size() > m_config.maxFrameSize) {
        LOG_WARN(QString("Frame buffer overflow: %1 bytes").arg(m_buffer.size()));
        emit bufferOverflow(m_buffer.size());
        m_buffer.clear();
        return;
    }

    // 超时模式：重置定时器
    if (m_config.delimiter == FrameDelimiter::Timeout) {
        m_timeoutTimer->start();
    } else {
        // 其他模式：立即处理
        processBuffer();
    }
}

void FrameParser::clear()
{
    m_buffer.clear();
    m_timeoutTimer->stop();
}

void FrameParser::setCustomParser(std::function<int(const QByteArray&)> parser)
{
    m_customParser = parser;
}

void FrameParser::onTimeout()
{
    // 超时到达，将缓冲区内容作为一帧发出
    if (!m_buffer.isEmpty()) {
        emit frameReady(m_buffer);
        m_buffer.clear();
    }
}

void FrameParser::processBuffer()
{
    while (!m_buffer.isEmpty()) {
        int frameLen = findFrame();

        if (frameLen > 0) {
            // 找到完整帧
            QByteArray frame = m_buffer.left(frameLen);
            m_buffer.remove(0, frameLen);
            emit frameReady(frame);
        } else if (frameLen == -1) {
            // 需要丢弃首字节（无效数据）
            m_buffer.remove(0, 1);
        } else {
            // 未找到完整帧，等待更多数据
            break;
        }
    }
}

int FrameParser::findFrame()
{
    switch (m_config.delimiter) {
        case FrameDelimiter::None:
            // 无分帧，直接返回所有数据
            return m_buffer.size();

        case FrameDelimiter::FixedLength:
            return parseFixedLength();

        case FrameDelimiter::Timeout:
            // 超时模式在onTimeout中处理
            return 0;

        case FrameDelimiter::StartEnd:
            return parseStartEnd();

        case FrameDelimiter::LengthField:
            return parseLengthField();

        case FrameDelimiter::Custom:
            return parseCustom();

        default:
            return m_buffer.size();
    }
}

int FrameParser::parseFixedLength()
{
    if (m_buffer.size() >= m_config.fixedLength) {
        return m_config.fixedLength;
    }
    return 0;
}

int FrameParser::parseStartEnd()
{
    const QByteArray& start = m_config.startFlag;
    const QByteArray& end = m_config.endFlag;

    // 查找起始标志
    int startPos = 0;
    if (!start.isEmpty()) {
        startPos = m_buffer.indexOf(start);
        if (startPos == -1) {
            // 未找到起始标志，丢弃之前的数据
            return m_buffer.size() > 0 ? -1 : 0;
        }
        // 丢弃起始标志之前的数据
        if (startPos > 0) {
            m_buffer.remove(0, startPos);
            startPos = 0;
        }
    }

    // 查找结束标志
    if (!end.isEmpty()) {
        int searchStart = start.isEmpty() ? 0 : start.size();
        int endPos = m_buffer.indexOf(end, searchStart);

        if (endPos == -1) {
            // 未找到结束标志
            return 0;
        }

        int frameLen = endPos + end.size();

        if (!m_config.includeFlags) {
            // 不包含标志时，需要调整返回的帧
            // 这里返回完整长度，由上层处理
        }

        return frameLen;
    }

    return 0;
}

int FrameParser::parseLengthField()
{
    int headerLen = m_config.headerLength;
    int fieldOffset = m_config.lengthFieldOffset;
    int fieldSize = m_config.lengthFieldSize;

    // 检查是否有足够数据读取长度字段
    int minLen = headerLen + fieldOffset + fieldSize;
    if (m_buffer.size() < minLen) {
        return 0;
    }

    // 读取长度字段
    int dataLen = readLengthField(m_buffer, headerLen + fieldOffset,
                                   fieldSize, m_config.lengthFieldBigEndian);

    if (dataLen < 0 || dataLen > m_config.maxFrameSize) {
        // 无效长度，丢弃首字节
        return -1;
    }

    // 计算完整帧长度
    int frameLen = minLen + dataLen + m_config.lengthAdjustment;

    if (m_buffer.size() >= frameLen) {
        return frameLen;
    }

    return 0;
}

int FrameParser::parseCustom()
{
    if (m_customParser) {
        return m_customParser(m_buffer);
    }
    return m_buffer.size();
}

int FrameParser::readLengthField(const QByteArray& data, int offset, int size, bool bigEndian)
{
    if (offset + size > data.size()) {
        return -1;
    }

    int value = 0;
    const uchar* ptr = reinterpret_cast<const uchar*>(data.constData() + offset);

    if (bigEndian) {
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

} // namespace ComAssistant
