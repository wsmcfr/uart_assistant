/**
 * @file EasyHexProtocol.h
 * @brief EasyHEX协议 - 简易十六进制协议
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_EASYHEXPROTOCOL_H
#define COMASSISTANT_EASYHEXPROTOCOL_H

#include "IProtocol.h"

namespace ComAssistant {

/**
 * @brief EasyHEX协议配置
 */
struct EasyHexConfig {
    QByteArray frameHeader;     ///< 帧头 (默认: AA 55)
    QByteArray frameTail;       ///< 帧尾 (默认: 无)
    bool useChecksum = true;    ///< 使用校验和
    enum ChecksumType {
        SUM8,   ///< 累加和取低8位
        XOR8,   ///< 异或校验
        CRC8    ///< CRC-8
    };
    ChecksumType checksumType = SUM8;
    int lengthFieldOffset = -1; ///< 长度字段偏移 (-1表示无长度字段)
    int lengthFieldSize = 1;    ///< 长度字段大小 (1或2字节)
    bool lengthIncludesHeader = false;  ///< 长度是否包含帧头
};

/**
 * @brief EasyHEX协议
 *
 * 简易十六进制协议，支持灵活配置帧结构：
 * - 帧头 + 数据 + 校验和 + 帧尾
 * - 帧头 + 长度 + 数据 + 校验和
 */
class EasyHexProtocol : public IProtocol {
    Q_OBJECT

public:
    explicit EasyHexProtocol(QObject* parent = nullptr);
    ~EasyHexProtocol() override = default;

    //=========================================================================
    // IProtocol 接口实现
    //=========================================================================

    ProtocolType type() const override { return ProtocolType::Custom; }
    QString name() const override { return QStringLiteral("EasyHEX"); }
    QString description() const override { return tr("Easy Hexadecimal Protocol"); }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload, const QVariantMap& metadata) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    void reset() override;

    //=========================================================================
    // EasyHEX特有方法
    //=========================================================================

    /**
     * @brief 设置协议配置
     */
    void setConfig(const EasyHexConfig& config);
    EasyHexConfig easyHexConfig() const { return m_easyConfig; }

    /**
     * @brief 设置帧头
     */
    void setFrameHeader(const QByteArray& header);

    /**
     * @brief 设置帧尾
     */
    void setFrameTail(const QByteArray& tail);

    /**
     * @brief 设置是否使用校验和
     */
    void setUseChecksum(bool use);

    /**
     * @brief 快速解析HEX字符串
     * @param hexString 十六进制字符串 (例如: "AA 55 01 02 03")
     * @return 字节数组
     */
    static QByteArray fromHexString(const QString& hexString);

    /**
     * @brief 转换为HEX字符串
     * @param data 字节数组
     * @param separator 分隔符
     * @return 十六进制字符串
     */
    static QString toHexString(const QByteArray& data, const QString& separator = " ");

private:
    int findFrame();
    quint8 calcSum8(const QByteArray& data);
    quint8 calcXor8(const QByteArray& data);
    quint8 calcCrc8(const QByteArray& data);

private:
    EasyHexConfig m_easyConfig;
};

} // namespace ComAssistant

#endif // COMASSISTANT_EASYHEXPROTOCOL_H
