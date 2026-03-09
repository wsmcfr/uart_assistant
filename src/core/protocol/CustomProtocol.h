/**
 * @file CustomProtocol.h
 * @brief 自定义协议
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_CUSTOMPROTOCOL_H
#define COMASSISTANT_CUSTOMPROTOCOL_H

#include "IProtocol.h"
#include "FrameParser.h"
#include <functional>

namespace ComAssistant {

/**
 * @brief 自定义协议配置
 */
struct CustomProtocolConfig {
    QString name = "Custom";           ///< 协议名称
    QString description;               ///< 协议描述

    // 帧解析配置
    FrameConfig frameConfig;

    // 校验和配置
    bool useChecksum = false;
    int checksumOffset = -1;           ///< 校验和位置（-1表示末尾）
    int checksumSize = 1;
    enum class ChecksumAlgo {
        Sum8, Sum16, XOR, CRC8, CRC16, Custom
    };
    ChecksumAlgo checksumAlgo = ChecksumAlgo::Sum8;
};

/**
 * @brief 自定义协议
 *
 * 支持通过配置或回调函数自定义协议解析逻辑
 */
class CustomProtocol : public IProtocol {
    Q_OBJECT

public:
    explicit CustomProtocol(QObject* parent = nullptr);
    ~CustomProtocol() override;

    //=========================================================================
    // IProtocol 接口实现
    //=========================================================================

    ProtocolType type() const override { return ProtocolType::Custom; }
    QString name() const override { return m_config.name; }
    QString description() const override { return m_config.description; }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload, const QVariantMap& metadata) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    void reset() override;

    //=========================================================================
    // 自定义协议特有方法
    //=========================================================================

    /**
     * @brief 设置自定义协议配置
     */
    void setCustomConfig(const CustomProtocolConfig& config);
    CustomProtocolConfig customConfig() const { return m_config; }

    /**
     * @brief 设置自定义解析回调
     * @param parser 解析函数，返回FrameResult
     */
    void setParseCallback(std::function<FrameResult(const QByteArray&)> parser);

    /**
     * @brief 设置自定义构建回调
     * @param builder 构建函数，返回完整帧
     */
    void setBuildCallback(std::function<QByteArray(const QByteArray&, const QVariantMap&)> builder);

    /**
     * @brief 设置自定义校验回调
     * @param validator 校验函数
     */
    void setValidateCallback(std::function<bool(const QByteArray&)> validator);

    /**
     * @brief 设置自定义校验和计算回调
     * @param calculator 校验和计算函数
     */
    void setChecksumCallback(std::function<QByteArray(const QByteArray&)> calculator);

    /**
     * @brief 加载协议配置文件
     * @param filePath 配置文件路径（JSON格式）
     * @return 是否成功
     */
    bool loadFromFile(const QString& filePath);

    /**
     * @brief 保存协议配置到文件
     * @param filePath 配置文件路径
     * @return 是否成功
     */
    bool saveToFile(const QString& filePath);

private slots:
    void onFrameReady(const QByteArray& frame);

private:
    CustomProtocolConfig m_config;
    FrameParser* m_frameParser = nullptr;

    // 自定义回调
    std::function<FrameResult(const QByteArray&)> m_parseCallback;
    std::function<QByteArray(const QByteArray&, const QVariantMap&)> m_buildCallback;
    std::function<bool(const QByteArray&)> m_validateCallback;
    std::function<QByteArray(const QByteArray&)> m_checksumCallback;
};

} // namespace ComAssistant

#endif // COMASSISTANT_CUSTOMPROTOCOL_H
