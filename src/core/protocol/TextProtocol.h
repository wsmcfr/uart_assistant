/**
 * @file TextProtocol.h
 * @brief TEXT绘图协议
 * @author ComAssistant Team
 * @date 2026-01-16
 *
 * 协议格式：{windowId}data1,data2,data3,...
 * 示例：{plot1}1.23,4.56,7.89
 */

#ifndef COMASSISTANT_TEXTPROTOCOL_H
#define COMASSISTANT_TEXTPROTOCOL_H

#include "IProtocol.h"
#include <QRegularExpression>

namespace ComAssistant {

/**
 * @brief TEXT绘图协议实现
 *
 * 格式：{windowId}value1,value2,value3,...
 * - windowId: 绘图窗口标识符
 * - values: 逗号分隔的数值，每个数值对应一条曲线
 * - X轴自动递增
 */
class TextProtocol : public IProtocol
{
    Q_OBJECT

public:
    explicit TextProtocol(QObject* parent = nullptr);
    ~TextProtocol() override = default;

    // IProtocol 接口实现
    ProtocolType type() const override { return ProtocolType::TextPlot; }
    QString name() const override { return QStringLiteral("TEXT绘图"); }
    QString description() const override {
        return tr("TEXT绘图协议，格式：{windowId}value1,value2,...");
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

#endif // COMASSISTANT_TEXTPROTOCOL_H
