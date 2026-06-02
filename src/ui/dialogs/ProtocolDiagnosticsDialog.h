/**
 * @file ProtocolDiagnosticsDialog.h
 * @brief 协议诊断对话框
 */

#ifndef COMASSISTANT_PROTOCOLDIAGNOSTICSDIALOG_H
#define COMASSISTANT_PROTOCOLDIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace ComAssistant {

/**
 * @brief 展示协议诊断摘要和完整 JSON 的只读对话框。
 *
 * 该对话框不编辑协议配置，只提供可读摘要、完整 JSON、复制和保存能力，
 * 避免与协议配置对话框职责重叠。
 */
class ProtocolDiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 创建协议诊断对话框。
     * @param diagnostics 已构建的诊断 JSON 对象。
     * @param parent 父窗口。
     *
     * 主要流程：格式化 JSON 文本，创建摘要区、JSON 区和操作按钮，
     * 然后从 JSON 中提取协议名、能力、配置版本和校验状态。
     */
    explicit ProtocolDiagnosticsDialog(const QJsonObject& diagnostics,
                                       QWidget* parent = nullptr);

    /**
     * @brief 返回格式化后的 JSON 文本。
     * @return 用于复制和保存的诊断 JSON 字符串。
     */
    QString jsonText() const;

private slots:
    /**
     * @brief 复制诊断 JSON 到剪贴板。
     */
    void copyJson();

    /**
     * @brief 选择路径并保存诊断 JSON。
     */
    void saveJson();

private:
    /**
     * @brief 创建只读摘要和 JSON 文本区。
     */
    void setupUi();

    /**
     * @brief 从 JSON 中刷新可读摘要标签。
     */
    void populateSummary();

    QJsonObject m_diagnostics;            ///< 完整诊断 JSON。
    QString m_jsonText;                   ///< 格式化 JSON 文本。
    QLabel* m_protocolLabel = nullptr;    ///< 协议概览标签。
    QLabel* m_capabilityLabel = nullptr;  ///< 能力摘要标签。
    QLabel* m_configLabel = nullptr;      ///< 配置摘要标签。
    QLabel* m_validationLabel = nullptr;  ///< 校验结果标签。
    QLabel* m_statusLabel = nullptr;      ///< 复制/保存操作状态提示。
    QPlainTextEdit* m_jsonEdit = nullptr; ///< 只读 JSON 文本框。
    QPushButton* m_copyButton = nullptr;  ///< 复制 JSON 按钮。
    QPushButton* m_saveButton = nullptr;  ///< 保存 JSON 按钮。
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLDIAGNOSTICSDIALOG_H
