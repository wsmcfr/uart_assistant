/**
 * @file HexProtocol.h
 * @brief 十六进制协议
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_HEXPROTOCOL_H
#define COMASSISTANT_HEXPROTOCOL_H

#include "IProtocol.h"

namespace ComAssistant {

/**
 * @brief 十六进制协议配置
 */
struct HexConfig {
    QByteArray frameHead;          ///< 帧头
    QByteArray frameTail;          ///< 帧尾
    int lengthFieldOffset = -1;    ///< 长度字段偏移（-1表示不使用）
    int lengthFieldSize = 1;       ///< 长度字段字节数
    bool lengthBigEndian = true;   ///< 长度字段大端序
    int lengthAdjustment = 0;      ///< 长度调整值
    bool useChecksum = false;      ///< 是否使用校验和
    int checksumSize = 1;          ///< 校验和字节数
    bool checksumBigEndian = true; ///< 校验和大端序

    enum class ChecksumType {
        Sum8,       ///< 8位累加和
        Sum16,      ///< 16位累加和
        XOR,        ///< 异或校验
        CRC8,       ///< CRC-8
        CRC16,      ///< CRC-16
        CRC32       ///< CRC-32
    };
    ChecksumType checksumType = ChecksumType::Sum8;
};

/**
 * @brief 十六进制协议
 *
 * 支持帧头帧尾、长度字段、校验和等功能
 */
class HexProtocol : public IProtocol {
    Q_OBJECT

public:
    explicit HexProtocol(QObject* parent = nullptr);
    ~HexProtocol() override = default;

    //=========================================================================
    // IProtocol 接口实现
    //=========================================================================

    ProtocolType type() const override { return ProtocolType::Hex; }
    QString name() const override { return QStringLiteral("HEX"); }
    QString description() const override { return tr("Hexadecimal protocol with frame detection"); }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload, const QVariantMap& metadata) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    void reset() override;

    //=========================================================================
    // HEX协议特有方法
    //=========================================================================

    /**
     * @brief 设置HEX配置
     */
    void setHexConfig(const HexConfig& config);
    HexConfig hexConfig() const { return m_hexConfig; }

    /**
     * @brief 设置帧头帧尾
     */
    void setFrameDelimiters(const QByteArray& head, const QByteArray& tail);

    /**
     * @brief 设置长度字段
     */
    void setLengthField(int offset, int size, bool bigEndian = true, int adjustment = 0);

    /**
     * @brief 设置校验和
     */
    void setChecksum(HexConfig::ChecksumType type, int size = 1, bool bigEndian = true);

    //=========================================================================
    // 工具方法
    //=========================================================================

    /**
     * @brief 十六进制字符串转字节数组
     * @param hex 十六进制字符串（如 "AA BB CC"）
     * @return 字节数组
     */
    static QByteArray hexStringToBytes(const QString& hex);

    /**
     * @brief 字节数组转十六进制字符串
     * @param data 字节数组
     * @param separator 分隔符
     * @return 十六进制字符串
     */
    static QString bytesToHexString(const QByteArray& data, const QString& separator = " ");

private:
    int findFrame();
    int readLength(const QByteArray& data, int offset);

    // 校验和计算
    QByteArray calcSum8(const QByteArray& data);
    QByteArray calcSum16(const QByteArray& data);
    QByteArray calcXOR(const QByteArray& data);
    QByteArray calcCRC16(const QByteArray& data);

private:
    HexConfig m_hexConfig;
};

} // namespace ComAssistant

#endif // COMASSISTANT_HEXPROTOCOL_H
