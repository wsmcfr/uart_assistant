/**
 * @file CsvProtocol.h
 * @brief CSV绘图协议
 * @author ComAssistant Team
 * @date 2026-01-16
 *
 * 协议格式：windowId,x,y1,y2,y3,...
 * 示例：plot3,0.1,100,200,300
 */

#ifndef COMASSISTANT_CSVPROTOCOL_H
#define COMASSISTANT_CSVPROTOCOL_H

#include "IProtocol.h"

namespace ComAssistant {

/**
 * @brief CSV绘图协议实现
 *
 * 格式：windowId,xValue,y1,y2,y3,...
 * - windowId: 绘图窗口标识符
 * - xValue: X轴值
 * - y1,y2,...: Y轴值，每个对应一条曲线（支持多曲线）
 */
class CsvProtocol : public IProtocol
{
    Q_OBJECT

public:
    explicit CsvProtocol(QObject* parent = nullptr);
    ~CsvProtocol() override = default;

    // IProtocol 接口实现
    ProtocolType type() const override { return ProtocolType::CsvPlot; }
    QString name() const override { return QStringLiteral("CSV绘图"); }
    QString description() const override {
        return tr("CSV绘图协议，格式：windowId,x,y1,y2,y3,...（支持多曲线）");
    }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload,
                    const QVariantMap& metadata = QVariantMap()) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    // 绘图协议支持
    bool isPlotProtocol() const override { return true; }
    PlotData parsePlotData(const QByteArray& data) override;
};

} // namespace ComAssistant

#endif // COMASSISTANT_CSVPROTOCOL_H
