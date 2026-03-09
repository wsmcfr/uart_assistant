/**
 * @file JustFloatProtocol.h
 * @brief JustFloat二进制绘图协议
 * @author ComAssistant Team
 * @date 2026-01-21
 *
 * JustFloat协议格式：
 * [float1][float2]...[floatN][帧尾]
 * - 每个float为4字节小端序IEEE754格式
 * - 帧尾固定为 00 00 80 7F（float的正无穷 +inf）
 */

#ifndef COMASSISTANT_JUSTFLOATPROTOCOL_H
#define COMASSISTANT_JUSTFLOATPROTOCOL_H

#include "IProtocol.h"

namespace ComAssistant {

/**
 * @brief JustFloat二进制绘图协议
 *
 * VOFA+风格的高效二进制浮点协议
 */
class JustFloatProtocol : public IProtocol
{
    Q_OBJECT

public:
    explicit JustFloatProtocol(QObject* parent = nullptr);
    ~JustFloatProtocol() override = default;

    // IProtocol 接口实现
    ProtocolType type() const override { return ProtocolType::JustFloat; }
    QString name() const override { return QStringLiteral("JustFloat"); }
    QString description() const override { return tr("VOFA+风格二进制浮点协议"); }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload,
                    const QVariantMap& metadata = QVariantMap()) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    void reset() override;

    // 绘图协议支持
    bool isPlotProtocol() const override { return true; }
    PlotData parsePlotData(const QByteArray& data) override;

    /**
     * @brief 设置绘图窗口ID
     * @param windowId 窗口ID
     */
    void setWindowId(const QString& windowId) { m_windowId = windowId; }

    /**
     * @brief 获取绘图窗口ID
     * @return 窗口ID
     */
    QString windowId() const { return m_windowId; }

    /**
     * @brief 构建JustFloat数据帧
     * @param values 浮点数列表
     * @return 完整的帧数据
     */
    static QByteArray buildFrame(const QVector<float>& values);

    /**
     * @brief 解析JustFloat帧数据
     * @param frame 帧数据（不含帧尾）
     * @return 浮点数列表
     */
    static QVector<float> parseFrame(const QByteArray& frame);

private:
    /**
     * @brief 查找帧尾位置
     * @param data 数据
     * @return 帧尾起始位置，-1表示未找到
     */
    int findFrameTail(const QByteArray& data) const;

    QString m_windowId = "JustFloat";  ///< 绘图窗口ID

    // 帧尾标记（00 00 80 7F = float +inf）
    static constexpr char FRAME_TAIL[4] = {0x00, 0x00, static_cast<char>(0x80), 0x7F};
    static constexpr int FRAME_TAIL_SIZE = 4;
    static constexpr int FLOAT_SIZE = 4;
};

} // namespace ComAssistant

#endif // COMASSISTANT_JUSTFLOATPROTOCOL_H
