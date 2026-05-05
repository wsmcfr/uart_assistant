/**
 * @file PlotProtocolDetector.h
 * @brief 绘图协议自动检测器 — 通过数据格式签名自动识别绘图协议类型
 * @author ComAssistant Team
 *
 * 当用户未手动选择协议时，分析串口数据的格式特征，
 * 通过置信度计数确认协议类型后自动切换，无需人工干预。
 *
 * 检测优先级：JustFloat > StampPlot > TextPlot > CsvPlot
 */

#ifndef COMASSISTANT_PLOTPROTOCOLDETECTOR_H
#define COMASSISTANT_PLOTPROTOCOLDETECTOR_H

#include <QObject>
#include <QByteArray>
#include <QRegularExpression>
#include <QtEndian>
#include <QtMath>
#include "IProtocol.h"

namespace ComAssistant {

/**
 * @brief 绘图协议自动检测器
 *
 * 通过分析串口数据的格式特征，自动识别以下4种绘图协议：
 * - JustFloat: 二进制float数组 + 魔术尾 00 00 80 7F
 * - StampPlot: <timestamp>{windowId}value1,value2,...
 * - TextPlot:  {windowId}value1,value2,...
 * - CsvPlot:   windowId,xValue,y1,y2,...
 *
 * 使用方法：
 * 1. 创建实例，连接 protocolDetected 信号
 * 2. 在 onDataReceived 中调用 feedData()
 * 3. 检测到协议后会发出 protocolDetected 信号
 * 4. 手动切换协议或断开连接时调用 reset()
 */
class PlotProtocolDetector : public QObject
{
    Q_OBJECT

public:
    explicit PlotProtocolDetector(QObject* parent = nullptr);
    ~PlotProtocolDetector() override;

    /**
     * @brief 喂入原始串口数据进行检测
     * @param data 原始字节数据
     *
     * 内部会缓冲数据，按行提取后逐行匹配。
     * 检测完成后不再处理后续数据。
     */
    void feedData(const QByteArray& data);

    /**
     * @brief 重置检测状态
     *
     * 在以下情况调用：
     * - 用户手动切换协议
     * - 断开连接
     * - 需要重新开始检测
     */
    void reset();

    /**
     * @brief 设置确认阈值（连续匹配次数）
     * @param n 阈值，默认3次
     */
    void setConfidenceThreshold(int n);

signals:
    /**
     * @brief 检测到绘图协议时发出
     * @param type 检测到的协议类型
     */
    void protocolDetected(ProtocolType type);

private:
    /**
     * @brief 尝试检测 JustFloat 协议（二进制路径）
     * @param data 原始二进制数据
     * @return 是否匹配成功
     *
     * 搜索魔术尾 00 00 80 7F，验证前面数据为4字节对齐的有效float
     */
    bool tryDetectJustFloat(const QByteArray& data);

    /**
     * @brief 尝试检测 StampPlot 协议
     * @param line 一行文本数据
     * @return 是否匹配成功
     *
     * 正则：<(\d+)>\{([a-zA-Z0-9_]+)\}([-\d.,]+)
     */
    bool tryDetectStampPlot(const QByteArray& line);

    /**
     * @brief 尝试检测 TextPlot 协议
     * @param line 一行文本数据
     * @return 是否匹配成功
     *
     * 正则：\{([a-zA-Z0-9_]+)\}([-\d.,]+)
     */
    bool tryDetectTextPlot(const QByteArray& line);

    /**
     * @brief 尝试检测 CsvPlot 协议
     * @param line 一行文本数据
     * @return 是否匹配成功
     *
     * 逗号分隔3+列，首列为非数字字符串，其余为数字
     */
    bool tryDetectCsvPlot(const QByteArray& line);

    /**
     * @brief 检查float值是否为合理的传感器数据
     * @param value 待检查的float值
     * @return 是否有效（非NaN、非Inf、绝对值 < 1e9）
     */
    static bool isValidFloat(float value);

    /**
     * @brief 判断数据是否包含二进制字节
     * @param data 待检查的数据
     * @return 是否为二进制数据
     *
     * 含字节 >0x7F 或 0x00-0x08/0x0B/0x0C/0x0E-0x1F 视为二进制
     */
    static bool isBinaryData(const QByteArray& data);

    // === 检测状态 ===
    bool m_detected = false;               ///< 是否已完成检测
    ProtocolType m_detectedType = ProtocolType::Raw;  ///< 检测到的协议类型

    // === 置信度配置 ===
    int m_confidenceThreshold = 3;         ///< 连续匹配次数阈值

    // === 各协议命中计数器 ===
    int m_justFloatHits = 0;
    int m_stampPlotHits = 0;
    int m_textPlotHits = 0;
    int m_csvPlotHits = 0;

    // === 数据缓冲区 ===
    QByteArray m_lineBuffer;               ///< 文本行缓冲
    QByteArray m_binaryBuffer;             ///< 二进制帧缓冲

    // === 预编译正则 ===
    QRegularExpression m_stampRegex;       ///< StampPlot: <(\d+)>\{...\}values
    QRegularExpression m_textRegex;        ///< TextPlot:  \{...\}values
};

} // namespace ComAssistant

#endif // COMASSISTANT_PLOTPROTOCOLDETECTOR_H
