/**
 * @file ProtocolConfigDialog.cpp
 * @brief 协议配置对话框实现
 */

#include "ProtocolConfigDialog.h"

#include "ui/widgets/ProtocolConfigEditor.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ComAssistant {

ProtocolConfigDialog::ProtocolConfigDialog(const ProtocolDescriptor& descriptor,
                                           const QVariantMap& initialConfig,
                                           QWidget* parent)
    : QDialog(parent)
    , m_descriptor(descriptor)
    , m_normalizedConfig(initialConfig)
{
    setupUi(initialConfig);
}

QVariantMap ProtocolConfigDialog::normalizedConfig() const
{
    return m_normalizedConfig;
}

bool ProtocolConfigDialog::acceptConfig()
{
    /*
     * 对话框只接受 Schema 校验通过后的 normalizedConfig。
     * 这样主窗口不需要再理解字段类型，也不会应用未规范化的十六进制文本。
     */
    const ProtocolConfigValidationResult result = m_editor->validateConfig();
    if (!result.valid) {
        return false;
    }

    m_normalizedConfig = result.normalizedConfig;
    return true;
}

void ProtocolConfigDialog::onAccepted()
{
    if (!acceptConfig()) {
        return;
    }

    accept();
}

void ProtocolConfigDialog::onRestoreDefaults()
{
    /*
     * 恢复默认值只影响编辑器当前控件；用户仍需点击确定才会真正返回配置。
     */
    m_editor->restoreDefaults();
}

void ProtocolConfigDialog::setupUi(const QVariantMap& initialConfig)
{
    setWindowTitle(tr("协议配置"));
    resize(520, 420);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    m_titleLabel = new QLabel(m_descriptor.displayName, this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    m_descriptionLabel = new QLabel(m_descriptor.description, this);
    m_descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(m_descriptionLabel);

    m_editor = new ProtocolConfigEditor(this);
    m_editor->setSchema(m_descriptor.configSchema);
    m_editor->setConfig(initialConfig.isEmpty() ? m_descriptor.defaultConfig : initialConfig);
    mainLayout->addWidget(m_editor);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_restoreButton = buttonBox->addButton(tr("恢复默认"), QDialogButtonBox::ResetRole);
    m_restoreButton->setEnabled(!m_descriptor.configSchema.fields.isEmpty());

    connect(m_restoreButton, &QPushButton::clicked, this, &ProtocolConfigDialog::onRestoreDefaults);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ProtocolConfigDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

} // namespace ComAssistant
