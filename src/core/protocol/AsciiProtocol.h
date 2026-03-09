/**
 * @file AsciiProtocol.h
 * @brief ASCII文本协议
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_ASCIIPROTOCOL_H
#define COMASSISTANT_ASCIIPROTOCOL_H

#include "IProtocol.h"

namespace ComAssistant {

/**
 * @brief 行结束符类型
 */
enum class LineEnding {
    None,       ///< 无
    CR,         ///< \r
    LF,         ///< \n
    CRLF        ///< \r\n
};

/**
 * @brief ASCII协议配置
 */
struct AsciiConfig {
    LineEnding lineEnding = LineEnding::CRLF;   ///< 行结束符
    bool appendLineEnding = true;               ///< 发送时追加行结束符
    int timeoutMs = 100;                        ///< 分帧超时（无行结束符时）
    QString encoding = "UTF-8";                 ///< 字符编码
};

/**
 * @brief ASCII文本协议
 *
 * 按行结束符分帧，适用于AT指令、调试信息等文本协议
 */
class AsciiProtocol : public IProtocol {
    Q_OBJECT

public:
    explicit AsciiProtocol(QObject* parent = nullptr);
    ~AsciiProtocol() override = default;

    //=========================================================================
    // IProtocol 接口实现
    //=========================================================================

    ProtocolType type() const override { return ProtocolType::Ascii; }
    QString name() const override { return QStringLiteral("ASCII"); }
    QString description() const override { return tr("ASCII text protocol with line ending"); }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload, const QVariantMap& metadata) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    void reset() override;

    //=========================================================================
    // ASCII协议特有方法
    //=========================================================================

    /**
     * @brief 设置ASCII配置
     */
    void setAsciiConfig(const AsciiConfig& config);
    AsciiConfig asciiConfig() const { return m_asciiConfig; }

    /**
     * @brief 设置行结束符
     */
    void setLineEnding(LineEnding ending);
    LineEnding lineEnding() const { return m_asciiConfig.lineEnding; }

    /**
     * @brief 获取行结束符字符串
     */
    static QByteArray lineEndingString(LineEnding ending);

    /**
     * @brief 将文本转换为带行结束符的帧
     */
    QByteArray textToFrame(const QString& text);

    /**
     * @brief 将帧转换为文本（去除行结束符）
     */
    QString frameToText(const QByteArray& frame);

private:
    int findLineEnding(const QByteArray& data);

private:
    AsciiConfig m_asciiConfig;
};

} // namespace ComAssistant

#endif // COMASSISTANT_ASCIIPROTOCOL_H
