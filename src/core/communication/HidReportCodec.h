/**
 * @file HidReportCodec.h
 * @brief HID Report 编解码辅助类
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_HIDREPORTCODEC_H
#define COMASSISTANT_HIDREPORTCODEC_H

#include "config/AppConfig.h"

#include <QByteArray>

namespace ComAssistant {

/**
 * @brief HID Report 编解码辅助类。
 *
 * 该类只处理字节格式，不持有设备句柄，因此可以在单元测试中直接验证
 * Report ID、长度字节、补零和输入 Report ID 移除等规则。
 */
class HidReportCodec
{
public:
    /**
     * @brief 构造 Output Report。
     *
     * 主要流程：首字节写入 Output Report ID；按配置可追加 payload 长度；
     * 再写入 payload，并按 outputReportLength 补零或截断。
     *
     * @param config HID 配置，提供报告长度、Report ID 和长度字节策略。
     * @param payload 用户要发送的原始负载。
     * @return 可直接传给 hid_write 的 Output Report。
     */
    static QByteArray buildOutputReport(const HidConfig& config, const QByteArray& payload);

    /**
     * @brief 构造 Feature Report。
     *
     * Feature Report 使用独立的 Report ID 和长度，避免 Output Report 的参数
     * 误套到控制传输上。
     *
     * @param config HID 配置，提供 Feature Report 参数。
     * @param payload Feature Report 的负载，不包含 Report ID。
     * @return 可直接传给 hid_send_feature_report 或 hid_get_feature_report 的缓冲区。
     */
    static QByteArray buildFeatureReport(const HidConfig& config, const QByteArray& payload);

    /**
     * @brief 归一化输入报告。
     *
     * @param config HID 配置，决定是否移除输入 Report ID。
     * @param report hidapi 读取到的完整输入报告。
     * @return 传给上层 UI 和协议层的 payload。
     */
    static QByteArray normalizeInputReport(const HidConfig& config, const QByteArray& report);

private:
    /**
     * @brief 按指定 Report ID、长度和 payload 构造固定长度报告。
     *
     * @param reportId 报告首字节 Report ID；无 Report ID 的设备使用 0。
     * @param reportLength 固定报告长度，至少为 1。
     * @param payload 报告负载，不包含 Report ID。
     * @param firstDataIsLength true 表示在 payload 前追加一字节负载长度。
     * @return 固定长度报告。
     */
    static QByteArray buildFixedLengthReport(quint8 reportId,
                                             int reportLength,
                                             const QByteArray& payload,
                                             bool firstDataIsLength);
};

} // namespace ComAssistant

#endif // COMASSISTANT_HIDREPORTCODEC_H
