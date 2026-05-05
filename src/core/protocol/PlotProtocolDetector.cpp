/**
 * @file PlotProtocolDetector.cpp
 * @brief 绘图协议自动检测器实现
 */

#include "PlotProtocolDetector.h"
#include <QDebug>

namespace ComAssistant {

/**
 * @brief 构造函数，初始化正则表达式
 */
PlotProtocolDetector::PlotProtocolDetector(QObject* parent)
    : QObject(parent)
    , m_stampRegex(QStringLiteral("<(\\d+)>\\{([a-zA-Z0-9_]+)\\}([-\\d.,]+)"))
    , m_textRegex(QStringLiteral("\\{([a-zA-Z0-9_]+)\\}([-\\d.,]+)"))
{
}

PlotProtocolDetector::~PlotProtocolDetector() = default;

/**
 * @brief 喂入原始串口数据进行检测
 *
 * 流程：
 * 1. 若已完成检测，直接返回
 * 2. 判断数据是二进制还是文本
 * 3. 二进制 → 仅尝试 JustFloat 检测
 * 4. 文本 → 行缓冲后按优先级逐行匹配 StampPlot > TextPlot > CsvPlot
 * 5. 任一协议命中次数达到阈值 → 发出 protocolDetected 信号
 */
void PlotProtocolDetector::feedData(const QByteArray& data)
{
    if (m_detected || data.isEmpty()) {
        return;
    }

    // 判断数据类型：二进制 or 文本
    if (isBinaryData(data)) {
        // === 二进制路径：仅尝试 JustFloat ===
        if (tryDetectJustFloat(data)) {
            m_justFloatHits++;
            if (m_justFloatHits >= m_confidenceThreshold) {
                m_detectedType = ProtocolType::JustFloat;
                m_detected = true;
                emit protocolDetected(ProtocolType::JustFloat);
            }
        }
    } else {
        // === 文本路径：行缓冲后按优先级匹配 ===
        m_lineBuffer.append(data);

        int pos;
        while ((pos = m_lineBuffer.indexOf('\n')) >= 0) {
            QByteArray line = m_lineBuffer.left(pos);
            m_lineBuffer.remove(0, pos + 1);

            // 移除行尾 \r
            if (line.endsWith('\r')) {
                line.chop(1);
            }
            if (line.isEmpty()) {
                continue;
            }

            // 按优先级匹配：StampPlot > TextPlot > CsvPlot
            if (tryDetectStampPlot(line)) {
                m_stampPlotHits++;
                // StampPlot 是 TextPlot 的超集，命中时重置低优先级计数
                m_textPlotHits = 0;
                m_csvPlotHits = 0;
                if (m_stampPlotHits >= m_confidenceThreshold) {
                    m_detectedType = ProtocolType::StampPlot;
                    m_detected = true;
                    emit protocolDetected(ProtocolType::StampPlot);
                    return;
                }
            } else if (tryDetectTextPlot(line)) {
                m_textPlotHits++;
                // TextPlot 命中时重置 CsvPlot 计数
                m_csvPlotHits = 0;
                if (m_textPlotHits >= m_confidenceThreshold) {
                    m_detectedType = ProtocolType::TextPlot;
                    m_detected = true;
                    emit protocolDetected(ProtocolType::TextPlot);
                    return;
                }
            } else if (tryDetectCsvPlot(line)) {
                m_csvPlotHits++;
                if (m_csvPlotHits >= m_confidenceThreshold) {
                    m_detectedType = ProtocolType::CsvPlot;
                    m_detected = true;
                    emit protocolDetected(ProtocolType::CsvPlot);
                    return;
                }
            }
            // 未匹配任何协议 → 计数器保持不变，允许跨多行累积
        }

        // 缓冲区溢出保护：防止非绘图数据无限累积
        if (m_lineBuffer.size() > 4096) {
            m_lineBuffer.clear();
        }
    }
}

/**
 * @brief 重置检测状态
 */
void PlotProtocolDetector::reset()
{
    m_detected = false;
    m_detectedType = ProtocolType::Raw;
    m_justFloatHits = 0;
    m_stampPlotHits = 0;
    m_textPlotHits = 0;
    m_csvPlotHits = 0;
    m_lineBuffer.clear();
    m_binaryBuffer.clear();
}

/**
 * @brief 设置确认阈值
 */
void PlotProtocolDetector::setConfidenceThreshold(int n)
{
    m_confidenceThreshold = qMax(1, n);
}

/**
 * @brief 检测 JustFloat 协议
 *
 * JustFloat 帧格式：[float1 LE][float2 LE]...[floatN LE][00 00 80 7F]
 * - 魔术尾：IEEE 754 正无穷大（+inf）的 little-endian 表示
 * - 数据区：每个值为 4 字节 little-endian float
 * - 数据区长度必须为 4 的倍数且 > 0
 */
bool PlotProtocolDetector::tryDetectJustFloat(const QByteArray& data)
{
    m_binaryBuffer.append(data);

    // 魔术尾：00 00 80 7F（IEEE 754 +inf little-endian）
    static const QByteArray tail("\x00\x00\x80\x7F", 4);
    int tailPos = m_binaryBuffer.indexOf(tail);

    if (tailPos < 0) {
        // 未找到尾标记，保持缓冲区有界
        if (m_binaryBuffer.size() > 8192) {
            m_binaryBuffer = m_binaryBuffer.right(4096);
        }
        return false;
    }

    // 尾标记前的数据长度
    int dataLen = tailPos;
    if (dataLen == 0 || dataLen % 4 != 0) {
        // 数据长度无效（不是4的倍数或为空），丢弃到尾标记之后
        m_binaryBuffer.remove(0, tailPos + 4);
        return false;
    }

    // 解析并验证每个 float 值
    int floatCount = dataLen / 4;
    const uchar* raw = reinterpret_cast<const uchar*>(m_binaryBuffer.constData());
    for (int i = 0; i < floatCount; ++i) {
        float val = qFromLittleEndian<float>(raw + i * 4);
        if (!isValidFloat(val)) {
            // 存在无效 float，丢弃到尾标记之后
            m_binaryBuffer.remove(0, tailPos + 4);
            return false;
        }
    }

    // 所有 float 有效，确认为 JustFloat 帧
    m_binaryBuffer.remove(0, tailPos + 4);
    return true;
}

/**
 * @brief 检测 StampPlot 协议
 *
 * 格式：<timestamp>{windowId}value1,value2,...
 * 正则：<(\d+)>\{([a-zA-Z0-9_]+)\}([-\d.,]+)
 */
bool PlotProtocolDetector::tryDetectStampPlot(const QByteArray& line)
{
    QRegularExpressionMatch match = m_stampRegex.match(QString::fromUtf8(line));
    if (!match.hasMatch()) {
        return false;
    }

    // 验证时间戳为合理正整数
    bool tsOk = false;
    qint64 ts = match.captured(1).toLongLong(&tsOk);
    if (!tsOk || ts < 0) {
        return false;
    }

    // 验证数值部分至少包含一个有效 double
    QString valuesStr = match.captured(3);
    QStringList valueList = valuesStr.split(',');
    if (valueList.isEmpty()) {
        return false;
    }
    bool valOk = false;
    valueList.first().toDouble(&valOk);
    return valOk;
}

/**
 * @brief 检测 TextPlot 协议
 *
 * 格式：{windowId}value1,value2,...
 * 正则：\{([a-zA-Z0-9_]+)\}([-\d.,]+)
 */
bool PlotProtocolDetector::tryDetectTextPlot(const QByteArray& line)
{
    QRegularExpressionMatch match = m_textRegex.match(QString::fromUtf8(line));
    if (!match.hasMatch()) {
        return false;
    }

    // 验证数值部分至少包含一个有效 double
    QString valuesStr = match.captured(2);
    QStringList valueList = valuesStr.split(',');
    if (valueList.isEmpty()) {
        return false;
    }
    bool valOk = false;
    valueList.first().toDouble(&valOk);
    return valOk;
}

/**
 * @brief 检测 CsvPlot 协议
 *
 * 格式：windowId,xValue,y1,y2,...
 * 要求：至少3列，首列为非数字字符串，其余列为数字
 */
bool PlotProtocolDetector::tryDetectCsvPlot(const QByteArray& line)
{
    QString text = QString::fromUtf8(line).trimmed();
    if (text.isEmpty()) {
        return false;
    }

    QStringList parts = text.split(',');
    if (parts.size() < 3) {
        return false;
    }

    // 首列必须为非数字（窗口ID）
    bool firstIsNumber = false;
    parts[0].trimmed().toDouble(&firstIsNumber);
    if (firstIsNumber) {
        return false;
    }

    // 第二列必须为数字（X轴值）
    bool xOk = false;
    parts[1].trimmed().toDouble(&xOk);
    if (!xOk) {
        return false;
    }

    // 第三列必须为数字（至少一个Y值）
    bool yOk = false;
    parts[2].trimmed().toDouble(&yOk);
    return yOk;
}

/**
 * @brief 检查 float 值是否为合理的传感器数据
 *
 * 排除 NaN、Inf 和绝对值过大的值（> 1e9），
 * 这些通常是随机二进制数据而非真实传感器读数。
 */
bool PlotProtocolDetector::isValidFloat(float value)
{
    if (std::isnan(value)) {
        return false;
    }
    if (std::isinf(value)) {
        return false;
    }
    if (qAbs(value) > 1e9f) {
        return false;
    }
    return true;
}

/**
 * @brief 判断数据是否包含二进制字节
 *
 * 规则：含字节 >0x7F 或控制字符（除 \t=0x09, \n=0x0A, \r=0x0D）视为二进制。
 * 这些字节不会出现在正常的文本绘图协议数据中。
 */
bool PlotProtocolDetector::isBinaryData(const QByteArray& data)
{
    for (int i = 0; i < data.size(); ++i) {
        uchar b = static_cast<uchar>(data[i]);
        // 高位字节（>0x7F）或非标准控制字符
        if (b > 0x7F) {
            return true;
        }
        if (b < 0x09) {
            return true;  // 0x00-0x08
        }
        if (b == 0x0B || b == 0x0C) {
            return true;  // VT, FF
        }
        if (b > 0x0D && b < 0x20) {
            return true;  // 0x0E-0x1F
        }
    }
    return false;
}

} // namespace ComAssistant
