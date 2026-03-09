/**
 * @file CsvProtocol.cpp
 * @brief CSV绘图协议实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "CsvProtocol.h"
#include "utils/Logger.h"

namespace ComAssistant {

CsvProtocol::CsvProtocol(QObject* parent)
    : IProtocol(parent)
{
}

FrameResult CsvProtocol::parse(const QByteArray& data)
{
    FrameResult result;
    QString text = QString::fromUtf8(data).trimmed();

    if (text.isEmpty()) {
        result.valid = false;
        result.consumedBytes = data.size();
        return result;
    }

    // 按行分割
    int lineEnd = text.indexOf('\n');
    QString line = (lineEnd >= 0) ? text.left(lineEnd) : text;

    // CSV格式：windowId,x,y1,y2,...（至少3列）
    QStringList parts = line.split(',');
    if (parts.size() >= 3) {
        result.valid = true;
        result.frame = line.toUtf8();
        result.metadata["windowId"] = parts[0].trimmed();
        result.metadata["x"] = parts[1].trimmed().toDouble();
        result.payload = parts.mid(2).join(',').toUtf8();
        result.consumedBytes = line.toUtf8().size() + (lineEnd >= 0 ? 1 : 0);
    } else {
        result.valid = false;
        result.errorMessage = tr("无效的CSV协议格式（至少需要3列：窗口ID,X值,Y值）");
        result.consumedBytes = line.toUtf8().size() + (lineEnd >= 0 ? 1 : 0);
    }

    return result;
}

QByteArray CsvProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    QString windowId = metadata.value("windowId", "plot1").toString();
    double x = metadata.value("x", 0.0).toDouble();
    return QString("%1,%2,%3\n")
        .arg(windowId)
        .arg(x)
        .arg(QString::fromUtf8(payload))
        .toUtf8();
}

bool CsvProtocol::validate(const QByteArray& frame)
{
    QString text = QString::fromUtf8(frame).trimmed();
    QStringList parts = text.split(',');
    return parts.size() >= 3;
}

QByteArray CsvProtocol::calculateChecksum(const QByteArray& data)
{
    Q_UNUSED(data);
    return QByteArray();  // CSV协议不使用校验和
}

PlotData CsvProtocol::parsePlotData(const QByteArray& data)
{
    PlotData result;
    QString text = QString::fromUtf8(data).trimmed();

    if (text.isEmpty()) {
        result.valid = false;
        result.errorMessage = tr("数据为空");
        return result;
    }

    // CSV格式：windowId,x,y1,y2,y3,...
    QStringList parts = text.split(',');

    if (parts.size() < 3) {
        result.valid = false;
        result.errorMessage = tr("CSV格式错误：至少需要窗口ID、X值和一个Y值");
        return result;
    }

    // 第一列：窗口ID
    result.windowId = parts[0].trimmed();
    if (result.windowId.isEmpty()) {
        result.valid = false;
        result.errorMessage = tr("窗口ID为空");
        return result;
    }

    // 第二列：X值
    bool xOk;
    result.xValue = parts[1].trimmed().toDouble(&xOk);
    if (!xOk) {
        result.valid = false;
        result.errorMessage = tr("无效的X值: %1").arg(parts[1]);
        return result;
    }

    // 后续列：Y值（多条曲线）
    for (int i = 2; i < parts.size(); ++i) {
        QString valueStr = parts[i].trimmed();
        if (valueStr.isEmpty()) continue;

        bool ok;
        double value = valueStr.toDouble(&ok);
        if (ok) {
            result.yValues.append(value);
        } else {
            LOG_WARN(QString("Invalid Y value in CSV protocol at column %1: %2")
                    .arg(i + 1).arg(valueStr));
        }
    }

    if (result.yValues.isEmpty()) {
        result.valid = false;
        result.errorMessage = tr("没有有效的Y值");
        return result;
    }

    result.valid = true;
    result.useTimestamp = false;
    result.useCustomX = true;

    return result;
}

} // namespace ComAssistant
