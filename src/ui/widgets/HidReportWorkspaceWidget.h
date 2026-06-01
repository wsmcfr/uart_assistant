/**
 * @file HidReportWorkspaceWidget.h
 * @brief HID Report 专用调试工作台
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_HIDREPORTWORKSPACEWIDGET_H
#define COMASSISTANT_HIDREPORTWORKSPACEWIDGET_H

#include "CommunicationWorkspaceWidget.h"
#include "config/AppConfig.h"

#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>

namespace ComAssistant {

/**
 * @brief HID Report 专用调试工作台。
 *
 * 提供设备信息、Input/Output/Feature Report 参数、Output 发送、
 * Feature Set/Get 和报告历史。
 */
class HidReportWorkspaceWidget : public CommunicationWorkspaceWidget
{
    Q_OBJECT

public:
    explicit HidReportWorkspaceWidget(QWidget* parent = nullptr);

    void setConfig(const HidConfig& config);
    HidConfig config() const;

    void setConnected(bool connected) override;
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;

    /**
     * @brief 追加 Feature Report 返回结果。
     * @param data Feature Report 原始字节。
     */
    void appendFeatureReportData(const QByteArray& data);

signals:
    /**
     * @brief 请求发送 Output Report 的 payload。
     */
    void outputReportRequested(const QByteArray& payload);

    /**
     * @brief 请求设置 Feature Report。
     */
    void featureReportSetRequested(const QByteArray& report);

    /**
     * @brief 请求读取 Feature Report。
     */
    void featureReportGetRequested(const QByteArray& report);

private slots:
    void onOutputSendClicked();
    void onFeatureSetClicked();
    void onFeatureGetClicked();

private:
    void setupUi();
    void updateDeviceSummary();
    void appendHistoryLine(const QString& direction, const QString& reportType, const QByteArray& data);

    HidConfig m_config;                    ///< 当前 HID 配置
    QLabel* m_deviceSummaryLabel = nullptr;///< 设备信息摘要
    QSpinBox* m_inputLengthSpin = nullptr; ///< 输入报告长度
    QSpinBox* m_outputLengthSpin = nullptr;///< 输出报告长度
    QSpinBox* m_featureLengthSpin = nullptr;///< Feature 报告长度
    QSpinBox* m_outputReportIdSpin = nullptr;///< Output Report ID
    QSpinBox* m_featureReportIdSpin = nullptr;///< Feature Report ID
    QCheckBox* m_firstDataIsLengthCheck = nullptr;///< payload 前写入长度
    QCheckBox* m_removeInputReportIdCheck = nullptr;///< 移除输入 Report ID
    QPlainTextEdit* m_historyEdit = nullptr;///< 报告历史
    QPlainTextEdit* m_outputPayloadEdit = nullptr;///< Output payload 输入
    QPlainTextEdit* m_featurePayloadEdit = nullptr;///< Feature payload 输入
};

} // namespace ComAssistant

#endif // COMASSISTANT_HIDREPORTWORKSPACEWIDGET_H
