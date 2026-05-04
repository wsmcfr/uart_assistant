/**
 * @file TextProtocol.cpp
 * @brief TEXT绘图协议实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "TextProtocol.h"
#include "utils/Logger.h"

namespace ComAssistant {

TextProtocol::TextProtocol(QObject* parent)
    : IProtocol(parent)
{
    // 匹配格式：{windowId}value1,value2,...
    // windowId: 字母数字下划线，至少1个字符
    // values: 数值，可以是整数或浮点数，可以有负号
    m_regex = QRegularExpression(R"(\{([a-zA-Z0-9_]+)\}([-\d.,]+))");
}

FrameResult TextProtocol::parse(const QByteArray& data)
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
        result.payload = match.captured(2).toUtf8();
        result.metadata["windowId"] = match.captured(1);
        result.consumedBytes = line.toUtf8().size() + (lineEnd >= 0 ? 1 : 0);
    } else {
        result.valid = false;
        result.errorMessage = tr("无效的TEXT协议格式");
        result.consumedBytes = line.toUtf8().size() + (lineEnd >= 0 ? 1 : 0);
    }

    return result;
}

QByteArray TextProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    QString windowId = metadata.value("windowId", "plot1").toString();
    return QString("{%1}%2\n").arg(windowId, QString::fromUtf8(payload)).toUtf8();
}

bool TextProtocol::validate(const QByteArray& frame)
{
    QString text = QString::fromUtf8(frame).trimmed();
    return m_regex.match(text).hasMatch();
}

QByteArray TextProtocol::calculateChecksum(const QByteArray& data)
{
    Q_UNUSED(data);
    return QByteArray();  // TEXT协议不使用校验和
}

PlotData TextProtocol::parsePlotData(const QByteArray& data)
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
        result.errorMessage = tr("无效的TEXT协议格式");
        return result;
    }

    result.windowId = match.captured(1);
    QString valuesStr = match.captured(2);

    // 解析数值
    QStringList valueList = valuesStr.split(',', QString::SkipEmptyParts);
    for (const QString& valueStr : valueList) {
        bool ok;
        double value = valueStr.trimmed().toDouble(&ok);
        if (ok) {
            result.yValues.append(value);
        } else {
            LOG_WARN(QString("Invalid number in TEXT protocol: %1").arg(valueStr));
        }
    }

    if (result.yValues.isEmpty()) {
        result.valid = false;
        result.errorMessage = tr("没有有效的数值");
        return result;
    }

    result.valid = true;
    result.useTimestamp = false;
    result.useCustomX = false;

    return result;
}

} // namespace ComAssistant
