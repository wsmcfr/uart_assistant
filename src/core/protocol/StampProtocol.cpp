/**
 * @file StampProtocol.cpp
 * @brief STAMP绘图协议实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "StampProtocol.h"
#include "utils/Logger.h"

namespace ComAssistant {

StampProtocol::StampProtocol(QObject* parent)
    : IProtocol(parent)
{
    // 匹配格式：<timestamp>{windowId}value1,value2,...
    // timestamp: 数字（毫秒时间戳）
    // windowId: 字母数字下划线
    // values: 逗号分隔的数值
    m_regex = QRegularExpression(R"(<(\d+)>\{([a-zA-Z0-9_]+)\}([-\d.,]+))");
}

FrameResult StampProtocol::parse(const QByteArray& data)
{
    FrameResult result;
    QString text = QString::fromUtf8(data).trimmed();

    if (text.isEmpty()) {
        result.valid = false;
        result.consumedBytes = data.size();
        return result;
    }

    // 按行分割，找到第一个完整行
    int lineEnd = text.indexOf('\n');
    QString line = (lineEnd >= 0) ? text.left(lineEnd) : text;

    QRegularExpressionMatch match = m_regex.match(line);
    if (match.hasMatch()) {
        result.valid = true;
        result.frame = line.toUtf8();
        result.payload = match.captured(3).toUtf8();
        result.metadata["timestamp"] = match.captured(1).toLongLong();
        result.metadata["windowId"] = match.captured(2);
        result.consumedBytes = line.toUtf8().size() + (lineEnd >= 0 ? 1 : 0);
    } else {
        result.valid = false;
        result.errorMessage = tr("无效的STAMP协议格式");
        result.consumedBytes = line.toUtf8().size() + (lineEnd >= 0 ? 1 : 0);
    }

    return result;
}

QByteArray StampProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    qint64 timestamp = metadata.value("timestamp", 0).toLongLong();
    QString windowId = metadata.value("windowId", "plot1").toString();
    return QString("<%1>{%2}%3\n")
        .arg(timestamp)
        .arg(windowId)
        .arg(QString::fromUtf8(payload))
        .toUtf8();
}

bool StampProtocol::validate(const QByteArray& frame)
{
    QString text = QString::fromUtf8(frame).trimmed();
    return m_regex.match(text).hasMatch();
}

QByteArray StampProtocol::calculateChecksum(const QByteArray& data)
{
    Q_UNUSED(data);
    return QByteArray();  // STAMP协议不使用校验和
}

PlotData StampProtocol::parsePlotData(const QByteArray& data)
{
    PlotData result;
    QString text = QString::fromUtf8(data).trimmed();

    if (text.isEmpty()) {
        result.valid = false;
        result.errorMessage = tr("数据为空");
        return result;
    }

    QRegularExpressionMatch match = m_regex.match(text);
    if (!match.hasMatch()) {
        result.valid = false;
        result.errorMessage = tr("无效的STAMP协议格式");
        return result;
    }

    // 解析时间戳
    bool timestampOk;
    result.timestamp = match.captured(1).toDouble(&timestampOk);
    if (!timestampOk) {
        result.valid = false;
        result.errorMessage = tr("无效的时间戳");
        return result;
    }

    result.windowId = match.captured(2);
    QString valuesStr = match.captured(3);

    // 解析数值
    QStringList valueList = valuesStr.split(',', Qt::SkipEmptyParts);
    for (const QString& valueStr : valueList) {
        bool ok;
        double value = valueStr.trimmed().toDouble(&ok);
        if (ok) {
            result.yValues.append(value);
        } else {
            LOG_WARN(QString("Invalid number in STAMP protocol: %1").arg(valueStr));
        }
    }

    if (result.yValues.isEmpty()) {
        result.valid = false;
        result.errorMessage = tr("没有有效的数值");
        return result;
    }

    result.valid = true;
    result.useTimestamp = true;
    result.useCustomX = false;

    return result;
}

} // namespace ComAssistant
