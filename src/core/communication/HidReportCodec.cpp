/**
 * @file HidReportCodec.cpp
 * @brief HID Report 编解码辅助类实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "HidReportCodec.h"

#include <QtGlobal>

namespace ComAssistant {

QByteArray HidReportCodec::buildOutputReport(const HidConfig& config, const QByteArray& payload)
{
    /*
     * Output Report 的长度来自设备协议描述或厂商文档。即使设备没有
     * 非零 Report ID，hidapi 也要求首字节保留为 0。
     */
    return buildFixedLengthReport(config.outReportId,
                                  config.outputReportLength,
                                  payload,
                                  config.firstDataIsLength);
}

QByteArray HidReportCodec::buildFeatureReport(const HidConfig& config, const QByteArray& payload)
{
    /*
     * Feature Report 走控制传输，很多设备会把 Feature ID 和 Output ID
     * 分开定义，因此这里不复用 outReportId。
     */
    return buildFixedLengthReport(config.featureReportId,
                                  config.featureReportLength,
                                  payload,
                                  false);
}

QByteArray HidReportCodec::normalizeInputReport(const HidConfig& config, const QByteArray& report)
{
    QByteArray payload = report;
    if (config.removeInReportId && !payload.isEmpty()) {
        payload.remove(0, 1);
    }
    return payload;
}

QByteArray HidReportCodec::buildFixedLengthReport(quint8 reportId,
                                                  int reportLength,
                                                  const QByteArray& payload,
                                                  bool firstDataIsLength)
{
    const int fixedLength = qMax(1, reportLength);
    QByteArray report;

    report.reserve(fixedLength);
    report.append(static_cast<char>(reportId));

    if (firstDataIsLength) {
        report.append(static_cast<char>(qMin(payload.size(), 255)));
    }
    report.append(payload);

    if (report.size() < fixedLength) {
        report.append(QByteArray(fixedLength - report.size(), '\0'));
    } else if (report.size() > fixedLength) {
        report.truncate(fixedLength);
    }

    return report;
}

} // namespace ComAssistant
