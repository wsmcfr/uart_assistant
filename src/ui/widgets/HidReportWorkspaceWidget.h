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

    /**
     * @brief 追加 Feature Report 发送记录。
     * @param data Feature Report 原始字节。
     */
    void appendFeatureReportSentData(const QByteArray& data);

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
    /**
     * @brief 发送 Output Report payload。
     */
    void onOutputSendClicked();

    /**
     * @brief 设置 Feature Report。
     */
    void onFeatureSetClicked();

    /**
     * @brief 读取 Feature Report。
     */
    void onFeatureGetClicked();

    /**
     * @brief 把 Output payload 输入规范化为大写分组 HEX。
     */
    void onOutputFormatHexClicked();

    /**
     * @brief 把 Feature payload 输入规范化为大写分组 HEX。
     */
    void onFeatureFormatHexClicked();

    /**
     * @brief 复制 HID 历史到系统剪贴板。
     */
    void onCopyHistoryClicked();

    /**
     * @brief 刷新 Output/Feature Report 预览和截断提示。
     */
    void updateReportPreviews();

private:
    /**
     * @brief 创建 HID Report 工作台界面并建立控件信号。
     */
    void setupUi();

    /**
     * @brief 刷新设备摘要。
     */
    void updateDeviceSummary();

    /**
     * @brief 追加一行 HID 报告历史。
     * @param direction 方向，例如 RX 或 TX。
     * @param reportType 报告类型，例如 Input、Output、Feature。
     * @param data 原始报告或 payload 字节。
     */
    void appendHistoryLine(const QString& direction, const QString& reportType, const QByteArray& data);

    /**
     * @brief 解析 Output payload 输入框。
     * @return 用户输入的 Output payload 字节。
     */
    QByteArray outputPayload() const;

    /**
     * @brief 解析 Feature payload 输入框。
     * @return 用户输入的 Feature payload 字节。
     */
    QByteArray featurePayload() const;

    /**
     * @brief 判断 payload 是否会被固定长度报告截断。
     * @param reportLength 完整报告长度，包含 Report ID。
     * @param payloadLength payload 字节数。
     * @param firstDataIsLength true 表示 Output payload 前会追加长度字节。
     * @return true 表示构造报告时会截断。
     */
    bool payloadWillBeTruncated(int reportLength, int payloadLength, bool firstDataIsLength) const;

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
    QLabel* m_outputPreviewLabel = nullptr;///< Output Report 预览
    QLabel* m_featurePreviewLabel = nullptr;///< Feature Report 预览
    QLabel* m_outputByteCountLabel = nullptr;///< Output payload 字节数
    QLabel* m_featureByteCountLabel = nullptr;///< Feature payload 字节数
    QLabel* m_truncationWarningLabel = nullptr;///< 截断提示
};

} // namespace ComAssistant

#endif // COMASSISTANT_HIDREPORTWORKSPACEWIDGET_H
