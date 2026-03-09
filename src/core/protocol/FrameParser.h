/**
 * @file FrameParser.h
 * @brief 通用帧解析器
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_FRAMEPARSER_H
#define COMASSISTANT_FRAMEPARSER_H

#include <QObject>
#include <QByteArray>
#include <QTimer>
#include <functional>

namespace ComAssistant {

/**
 * @brief 帧定界方式
 */
enum class FrameDelimiter {
    None,           ///< 无定界（原始数据）
    FixedLength,    ///< 固定长度
    Timeout,        ///< 超时分帧
    StartEnd,       ///< 起始/结束标志
    LengthField,    ///< 长度字段
    Custom          ///< 自定义规则
};

/**
 * @brief 帧配置
 */
struct FrameConfig {
    FrameDelimiter delimiter = FrameDelimiter::None;

    // 固定长度模式
    int fixedLength = 0;

    // 超时分帧模式
    int timeoutMs = 50;

    // 起始/结束标志模式
    QByteArray startFlag;
    QByteArray endFlag;
    bool includeFlags = true;  // 结果是否包含标志

    // 长度字段模式
    int lengthFieldOffset = 0;     // 长度字段偏移
    int lengthFieldSize = 1;       // 长度字段字节数（1, 2, 4）
    bool lengthFieldBigEndian = true;
    int lengthAdjustment = 0;      // 长度调整值
    int headerLength = 0;          // 帧头长度（长度字段之前）

    // 通用配置
    int maxFrameSize = 65536;      // 最大帧长度
};

/**
 * @brief 通用帧解析器
 *
 * 支持多种分帧方式，可组合使用
 */
class FrameParser : public QObject {
    Q_OBJECT

public:
    explicit FrameParser(QObject* parent = nullptr);
    ~FrameParser() override;

    /**
     * @brief 设置帧配置
     */
    void setConfig(const FrameConfig& config);
    FrameConfig config() const { return m_config; }

    /**
     * @brief 输入数据进行解析
     * @param data 输入数据
     */
    void feed(const QByteArray& data);

    /**
     * @brief 清空缓冲区
     */
    void clear();

    /**
     * @brief 获取当前缓冲区大小
     */
    int bufferSize() const { return m_buffer.size(); }

    /**
     * @brief 设置自定义解析回调
     * @param parser 解析函数，返回帧长度（0表示未找到完整帧，-1表示需要丢弃首字节）
     */
    void setCustomParser(std::function<int(const QByteArray&)> parser);

signals:
    /**
     * @brief 解析出完整帧
     */
    void frameReady(const QByteArray& frame);

    /**
     * @brief 缓冲区溢出
     */
    void bufferOverflow(int size);

private slots:
    void onTimeout();

private:
    void processBuffer();
    int findFrame();

    // 各种分帧方法
    int parseFixedLength();
    int parseStartEnd();
    int parseLengthField();
    int parseCustom();

    // 工具方法
    int readLengthField(const QByteArray& data, int offset, int size, bool bigEndian);

private:
    FrameConfig m_config;
    QByteArray m_buffer;
    QTimer* m_timeoutTimer = nullptr;
    std::function<int(const QByteArray&)> m_customParser;
};

} // namespace ComAssistant

#endif // COMASSISTANT_FRAMEPARSER_H
