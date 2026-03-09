/**
 * @file IProtocol.h
 * @brief 协议接口定义
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_IPROTOCOL_H
#define COMASSISTANT_IPROTOCOL_H

#include <QObject>
#include <QByteArray>
#include <QVariantMap>
#include <QString>
#include <QVector>
#include <functional>

namespace ComAssistant {

/**
 * @brief 协议类型枚举
 */
enum class ProtocolType {
    Raw,        ///< 原始数据（无协议）
    Ascii,      ///< ASCII文本协议
    Hex,        ///< 十六进制协议
    Modbus,     ///< Modbus协议
    Custom,     ///< 自定义协议
    EasyHex,    ///< EasyHEX协议
    TextPlot,   ///< TEXT绘图协议
    StampPlot,  ///< STAMP绘图协议（带时间戳）
    CsvPlot,    ///< CSV绘图协议
    JustFloat   ///< JustFloat二进制绘图协议
};

/**
 * @brief 帧解析结果
 */
struct FrameResult {
    bool valid = false;              ///< 帧是否有效
    QByteArray frame;                ///< 完整帧数据
    QByteArray payload;              ///< 有效载荷
    QVariantMap metadata;            ///< 元数据（如地址、功能码等）
    QString errorMessage;            ///< 错误信息
    int consumedBytes = 0;           ///< 消耗的字节数
};

/**
 * @brief 绘图数据结构
 */
struct PlotData {
    bool valid = false;              ///< 数据是否有效
    QString windowId;                ///< 绘图窗口ID
    double timestamp = 0;            ///< 时间戳（毫秒），用于STAMP协议
    double xValue = 0;               ///< X轴值，用于CSV协议
    QVector<double> yValues;         ///< Y轴值列表（多曲线）
    bool useTimestamp = false;       ///< 是否使用时间戳作为X轴
    bool useCustomX = false;         ///< 是否使用自定义X值
    QString errorMessage;            ///< 错误信息
};

/**
 * @brief 协议接口
 *
 * 所有协议实现必须继承此接口
 */
class IProtocol : public QObject {
    Q_OBJECT

public:
    explicit IProtocol(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IProtocol() = default;

    //=========================================================================
    // 基本信息
    //=========================================================================

    /**
     * @brief 获取协议类型
     */
    virtual ProtocolType type() const = 0;

    /**
     * @brief 获取协议名称
     */
    virtual QString name() const = 0;

    /**
     * @brief 获取协议描述
     */
    virtual QString description() const = 0;

    //=========================================================================
    // 帧解析
    //=========================================================================

    /**
     * @brief 解析数据，提取完整帧
     * @param data 输入数据
     * @return 解析结果
     *
     * 协议实现应从data中识别完整帧，并在FrameResult中返回：
     * - valid: 是否找到有效帧
     * - frame: 完整帧数据
     * - payload: 去除帧头尾后的有效载荷
     * - consumedBytes: 已处理的字节数（包括无效数据）
     */
    virtual FrameResult parse(const QByteArray& data) = 0;

    /**
     * @brief 构建发送帧
     * @param payload 有效载荷
     * @param metadata 元数据（可选）
     * @return 完整帧数据
     */
    virtual QByteArray build(const QByteArray& payload,
                            const QVariantMap& metadata = QVariantMap()) = 0;

    //=========================================================================
    // 校验功能
    //=========================================================================

    /**
     * @brief 验证帧完整性
     * @param frame 待验证的帧
     * @return 是否有效
     */
    virtual bool validate(const QByteArray& frame) = 0;

    /**
     * @brief 计算校验值
     * @param data 待校验数据
     * @return 校验值
     */
    virtual QByteArray calculateChecksum(const QByteArray& data) = 0;

    //=========================================================================
    // 配置
    //=========================================================================

    /**
     * @brief 设置协议配置
     * @param config 配置参数
     */
    virtual void setConfig(const QVariantMap& config) { m_config = config; }

    /**
     * @brief 获取协议配置
     */
    virtual QVariantMap config() const { return m_config; }

    /**
     * @brief 重置协议状态
     */
    virtual void reset() {}

    //=========================================================================
    // 绘图协议支持
    //=========================================================================

    /**
     * @brief 是否为绘图协议
     * @return 如果协议支持绘图数据解析返回true
     */
    virtual bool isPlotProtocol() const { return false; }

    /**
     * @brief 解析绘图数据
     * @param data 输入数据（通常是一行）
     * @return 绘图数据结果
     */
    virtual PlotData parsePlotData(const QByteArray& data) {
        Q_UNUSED(data);
        return PlotData();
    }

signals:
    /**
     * @brief 解析出完整帧时发出
     */
    void frameReceived(const FrameResult& result);

    /**
     * @brief 解析错误时发出
     */
    void parseError(const QString& error);

protected:
    QVariantMap m_config;     ///< 配置参数
    QByteArray m_buffer;      ///< 内部缓冲区
};

/**
 * @brief 协议工厂函数类型
 */
using ProtocolCreator = std::function<IProtocol*(QObject*)>;

} // namespace ComAssistant

#endif // COMASSISTANT_IPROTOCOL_H
