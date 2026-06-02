/**
 * @file ProtocolConfigDialog.h
 * @brief 协议配置对话框
 */

#ifndef COMASSISTANT_PROTOCOLCONFIGDIALOG_H
#define COMASSISTANT_PROTOCOLCONFIGDIALOG_H

#include "core/protocol/ProtocolDescriptor.h"

#include <QDialog>
#include <QVariantMap>

class QLabel;
class QPushButton;

namespace ComAssistant {

class ProtocolConfigEditor;

/**
 * @brief 包装 ProtocolConfigEditor 的协议配置对话框。
 *
 * 对话框负责展示协议名称、说明、恢复默认值和确认/取消按钮；
 * 具体字段控件生成与 Schema 校验由 ProtocolConfigEditor 完成。
 */
class ProtocolConfigDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 创建协议配置对话框。
     * @param descriptor 当前协议描述，包含显示名、说明、Schema 和默认配置。
     * @param initialConfig 打开时加载的初始配置。
     * @param parent 父级窗口。
     */
    explicit ProtocolConfigDialog(const ProtocolDescriptor& descriptor,
                                  const QVariantMap& initialConfig,
                                  QWidget* parent = nullptr);

    /**
     * @brief 读取确认后的规范化配置。
     * @return 最近一次 acceptConfig() 校验通过得到的 normalizedConfig。
     */
    QVariantMap normalizedConfig() const;

    /**
     * @brief 校验并接受当前配置。
     * @return 校验通过返回 true；失败返回 false 且对话框保持打开。
     */
    bool acceptConfig();

private slots:
    /**
     * @brief 响应确定按钮，校验通过后关闭对话框。
     */
    void onAccepted();

    /**
     * @brief 恢复当前协议 Schema 的默认配置。
     */
    void onRestoreDefaults();

private:
    /**
     * @brief 创建对话框界面并连接按钮信号。
     * @param initialConfig 初始配置，用于初始化编辑器控件。
     */
    void setupUi(const QVariantMap& initialConfig);

    ProtocolDescriptor m_descriptor;        ///< 当前协议描述。
    QVariantMap m_normalizedConfig;         ///< 最近一次校验通过的规范化配置。
    QLabel* m_titleLabel = nullptr;         ///< 协议名称标题。
    QLabel* m_descriptionLabel = nullptr;   ///< 协议说明文本。
    ProtocolConfigEditor* m_editor = nullptr; ///< Schema 驱动配置编辑器。
    QPushButton* m_restoreButton = nullptr; ///< 恢复默认值按钮。
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLCONFIGDIALOG_H
