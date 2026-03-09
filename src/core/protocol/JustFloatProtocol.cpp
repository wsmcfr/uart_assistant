/**
 * @file JustFloatProtocol.cpp
 * @brief JustFloat二进制绘图协议实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "JustFloatProtocol.h"
#include <cstring>
#include <QtEndian>

namespace ComAssistant {

// 静态成员定义
constexpr char JustFloatProtocol::FRAME_TAIL[];

JustFloatProtocol::JustFloatProtocol(QObject* parent)
    : IProtocol(parent)
{
}

FrameResult JustFloatProtocol::parse(const QByteArray& data)
{
    FrameResult result;

    // 将数据添加到缓冲区
    m_buffer.append(data);

    // 查找帧尾
    int tailPos = findFrameTail(m_buffer);
    if (tailPos < 0) {
        // 未找到帧尾，需要更多数据
        // 如果缓冲区太大，丢弃旧数据（保留最后1KB）
        if (m_buffer.size() > 1024) {
            m_buffer = m_buffer.right(1024);
        }
        result.consumedBytes = 0;
        return result;
    }

    // 检查数据长度是否为4的倍数
    if (tailPos % FLOAT_SIZE != 0) {
        // 数据不完整或格式错误，丢弃到帧尾
        result.errorMessage = tr("帧数据长度不是4的倍数");
        result.consumedBytes = tailPos + FRAME_TAIL_SIZE;
        m_buffer.remove(0, result.consumedBytes);
        return result;
    }

    // 提取帧数据（不含帧尾）
    result.frame = m_buffer.left(tailPos);
    result.payload = result.frame;
    result.valid = true;
    result.consumedBytes = tailPos + FRAME_TAIL_SIZE;

    // 从缓冲区移除已处理的数据
    m_buffer.remove(0, result.consumedBytes);

    return result;
}

QByteArray JustFloatProtocol::build(const QByteArray& payload,
                                     const QVariantMap& metadata)
{
    Q_UNUSED(metadata);

    // payload应该已经是正确格式的浮点数据
    QByteArray frame = payload;

    // 添加帧尾
    frame.append(FRAME_TAIL, FRAME_TAIL_SIZE);

    return frame;
}

bool JustFloatProtocol::validate(const QByteArray& frame)
{
    // 至少需要一个float和帧尾
    if (frame.size() < FLOAT_SIZE + FRAME_TAIL_SIZE) {
        return false;
    }

    // 数据部分必须是4的倍数
    int dataSize = frame.size() - FRAME_TAIL_SIZE;
    if (dataSize % FLOAT_SIZE != 0) {
        return false;
    }

    // 检查帧尾
    return frame.right(FRAME_TAIL_SIZE) == QByteArray(FRAME_TAIL, FRAME_TAIL_SIZE);
}

QByteArray JustFloatProtocol::calculateChecksum(const QByteArray& data)
{
    Q_UNUSED(data);
    // JustFloat协议不使用校验和
    return QByteArray();
}

void JustFloatProtocol::reset()
{
    m_buffer.clear();
}

PlotData JustFloatProtocol::parsePlotData(const QByteArray& data)
{
    PlotData plotData;
    plotData.windowId = m_windowId;

    // data应该是不含帧尾的纯浮点数据
    if (data.isEmpty() || data.size() % FLOAT_SIZE != 0) {
        plotData.errorMessage = tr("数据为空或长度不正确");
        return plotData;
    }

    // 解析浮点数
    QVector<float> values = parseFrame(data);
    if (values.isEmpty()) {
        plotData.errorMessage = tr("无法解析浮点数据");
        return plotData;
    }

    // 转换为double
    plotData.yValues.reserve(values.size());
    for (float val : values) {
        plotData.yValues.append(static_cast<double>(val));
    }

    plotData.valid = true;
    plotData.useTimestamp = false;
    plotData.useCustomX = false;

    return plotData;
}

QByteArray JustFloatProtocol::buildFrame(const QVector<float>& values)
{
    QByteArray frame;
    frame.reserve(values.size() * FLOAT_SIZE + FRAME_TAIL_SIZE);

    // 写入浮点数（小端序）
    for (float val : values) {
        char bytes[FLOAT_SIZE];
        qToLittleEndian(val, bytes);
        frame.append(bytes, FLOAT_SIZE);
    }

    // 添加帧尾
    frame.append(FRAME_TAIL, FRAME_TAIL_SIZE);

    return frame;
}

QVector<float> JustFloatProtocol::parseFrame(const QByteArray& frame)
{
    QVector<float> values;
    int count = frame.size() / FLOAT_SIZE;
    values.reserve(count);

    for (int i = 0; i < count; ++i) {
        float val = qFromLittleEndian<float>(
            reinterpret_cast<const uchar*>(frame.constData() + i * FLOAT_SIZE));
        values.append(val);
    }

    return values;
}

int JustFloatProtocol::findFrameTail(const QByteArray& data) const
{
    // 搜索帧尾标记
    QByteArray tail(FRAME_TAIL, FRAME_TAIL_SIZE);
    return data.indexOf(tail);
}

} // namespace ComAssistant
