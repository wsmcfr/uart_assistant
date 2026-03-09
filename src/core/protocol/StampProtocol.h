/**
 * @file StampProtocol.h
 * @brief STAMP绘图协议（带时间戳）
 * @author ComAssistant Team
 * @date 2026-01-16
 *
 * 协议格式：<timestamp>{windowId}data1,data2,...
 * 示例：<1000>{plot1}1.23,4.56
 */

#ifndef COMASSISTANT_STAMPPROTOCOL_H
#define COMASSISTANT_STAMPPROTOCOL_H

#include "IProtocol.h"
#include <QRegularExpression>

namespace ComAssistant {

/**
 * @brief STAMP绘图协议实现
 *
 * 格式：<timestamp>{windowId}value1,value2,...
 * - timestamp: Unix时间戳（毫秒），作为X轴
 * - windowId: 绘图窗口标识符
 * - values: 逗号分隔的数值，每个数值对应一条曲线
 */
class StampProtocol : public IProtocol
{
    Q_OBJECT

public:
    explicit StampProtocol(QObject* parent = nullptr);
    ~StampProtocol() override = default;

    // IProtocol 接口实现
    ProtocolType type() const override { return ProtocolType::StampPlot; }
    QString name() const override { return QStringLiteral("STAMP绘图"); }
    QString description() const override {
        return tr("STAMP绘图协议（带时间戳），格式：<timestamp>{windowId}value1,value2,...");
    }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload,
                    const QVariantMap& metadata = QVariantMap()) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    // 绘图协议支持
    bool isPlotProtocol() const override { return true; }
    PlotData parsePlotData(const QByteArray& data) override;

private:
    QRegularExpression m_regex;  ///< 协议匹配正则表达式
};

} // namespace ComAssistant

#endif // COMASSISTANT_STAMPPROTOCOL_H
