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

    // 定期压缩缓冲区（当偏移量超过 64KB 时）
    if (m_bufferOffset > 65536) {
        compactBuffer();
    }

    m_buffer.append(data);

    // 检查缓冲区溢出（使用有效数据大小）
    int effectiveSize = m_buffer.size() - m_bufferOffset;
    if (effectiveSize > m_config.maxFrameSize) {
        LOG_WARN(QString("Frame buffer overflow: %1 bytes").arg(effectiveSize));
        emit bufferOverflow(effectiveSize);
        m_buffer.clear();
        m_bufferOffset = 0;
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
    m_bufferOffset = 0;
    m_timeoutTimer->stop();
}

void FrameParser::setCustomParser(std::function<int(const QByteArray&)> parser)
{
    m_customParser = parser;
}

void FrameParser::onTimeout()
{
    // 超时到达，将缓冲区有效内容作为一帧发出
    int effectiveSize = m_buffer.size() - m_bufferOffset;
    if (effectiveSize > 0) {
        emit frameReady(m_buffer.mid(m_bufferOffset));
        m_buffer.clear();
        m_bufferOffset = 0;
    }
}

void FrameParser::compactBuffer()
{
    // 移除已处理的数据，重置偏移量
    if (m_bufferOffset > 0) {
        m_buffer.remove(0, m_bufferOffset);
        m_bufferOffset = 0;
    }
}

void FrameParser::processBuffer()
{
    while (m_bufferOffset < m_buffer.size()) {
        int frameLen = findFrame();

        if (frameLen > 0) {
            // 找到完整帧
            QByteArray frame = m_buffer.mid(m_bufferOffset, frameLen);
            m_bufferOffset += frameLen;
            emit frameReady(frame);

            // 如果偏移量过大，立即压缩
            if (m_bufferOffset > 65536) {
                compactBuffer();
            }
        } else if (frameLen == -1) {
            // 需要丢弃首字节（无效数据）
            ++m_bufferOffset;
        } else {
            // 未找到完整帧，等待更多数据
            break;
        }
    }

    // 处理完成后，如果缓冲区已全部处理，清空
    if (m_bufferOffset >= m_buffer.size()) {
        m_buffer.clear();
        m_bufferOffset = 0;
    }
}

int FrameParser::findFrame()
{
    // 获取有效数据大小
    int effectiveSize = m_buffer.size() - m_bufferOffset;

    switch (m_config.delimiter) {
        case FrameDelimiter::None:
            // 无分帧，直接返回所有有效数据
            return effectiveSize;

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
            return effectiveSize;
    }
}

int FrameParser::parseFixedLength()
{
    int effectiveSize = m_buffer.size() - m_bufferOffset;
    if (effectiveSize >= m_config.fixedLength) {
        return m_config.fixedLength;
    }
    return 0;
}

int FrameParser::parseStartEnd()
{
    const QByteArray& start = m_config.startFlag;
    const QByteArray& end = m_config.endFlag;

    // 获取有效数据的起始指针
    const char* dataPtr = m_buffer.constData() + m_bufferOffset;
    int effectiveSize = m_buffer.size() - m_bufferOffset;

    // 查找起始标志
    int startPos = 0;
    if (!start.isEmpty()) {
        // 在有效数据中查找起始标志
        QByteArray effectiveData(dataPtr, effectiveSize);
        startPos = effectiveData.indexOf(start);
        if (startPos == -1) {
            // 未找到起始标志，丢弃所有有效数据
            return effectiveSize > 0 ? -1 : 0;
        }
        // 丢弃起始标志之前的数据（通过增加偏移量）
        if (startPos > 0) {
            m_bufferOffset += startPos;
            effectiveSize -= startPos;
            dataPtr = m_buffer.constData() + m_bufferOffset;
        }
    }

    // 查找结束标志
    if (!end.isEmpty()) {
        int searchStart = start.isEmpty() ? 0 : start.size();
        QByteArray effectiveData(dataPtr, effectiveSize);
        int endPos = effectiveData.indexOf(end, searchStart);

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
    int effectiveSize = m_buffer.size() - m_bufferOffset;

    // 检查是否有足够数据读取长度字段
    int minLen = headerLen + fieldOffset + fieldSize;
    if (effectiveSize < minLen) {
        return 0;
    }

    // 读取长度字段（使用偏移后的数据）
    int dataLen = readLengthField(m_buffer, m_bufferOffset + headerLen + fieldOffset,
                                   fieldSize, m_config.lengthFieldBigEndian);

    if (dataLen < 0 || dataLen > m_config.maxFrameSize) {
        // 无效长度，丢弃首字节
        return -1;
    }

    // 计算完整帧长度
    int frameLen = minLen + dataLen + m_config.lengthAdjustment;

    if (effectiveSize >= frameLen) {
        return frameLen;
    }

    return 0;
}

int FrameParser::parseCustom()
{
    if (m_customParser) {
        // 传递有效数据给自定义解析器
        QByteArray effectiveData = m_buffer.mid(m_bufferOffset);
        return m_customParser(effectiveData);
    }
    return m_buffer.size() - m_bufferOffset;
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
