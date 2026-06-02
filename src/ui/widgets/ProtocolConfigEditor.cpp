/**
 * @file ProtocolConfigEditor.cpp
 * @brief 协议配置 Schema 编辑器实现
 */

#include "ProtocolConfigEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ComAssistant {

ProtocolConfigEditor::ProtocolConfigEditor(QWidget* parent)
    : QWidget(parent)
{
    /*
     * 主布局只承载表单和空状态提示。表单不额外包卡片，保持它能被
     * 对话框或未来插件页面复用，并让外层容器决定视觉边界。
     */
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    m_formLayout = new QFormLayout();
    m_formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    m_formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mainLayout->addLayout(m_formLayout);

    m_emptyLabel = new QLabel(tr("当前协议没有可配置项"), this);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setVisible(false);
    mainLayout->addWidget(m_emptyLabel);

    /*
     * 错误提示由 validateConfig() 统一维护。默认隐藏，只有 Schema
     * 返回错误时才显示，避免空表单或有效配置占用额外视觉空间。
     */
    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setVisible(false);
    mainLayout->addWidget(m_errorLabel);
}

void ProtocolConfigEditor::setSchema(const ProtocolConfigSchema& schema)
{
    /*
     * Schema 是编辑器的唯一输入来源。每次设置都完整重建，避免协议切换后
     * 旧控件的取值、范围或枚举选项继续影响新协议。
     */
    m_schema = schema;
    clearForm();
    clearErrors();

    const bool hasFields = !m_schema.fields.isEmpty();
    m_emptyLabel->setVisible(!hasFields);

    for (const ProtocolConfigField& field : m_schema.fields) {
        QWidget* fieldWidget = createFieldWidget(field);
        m_formLayout->addRow(field.displayName, fieldWidget);
    }
}

void ProtocolConfigEditor::setConfig(const QVariantMap& config)
{
    /*
     * 加载配置时对缺失字段回退到 Schema 默认值，保证旧会话或部分配置
     * 也能在 UI 中显示完整、可编辑的当前值。
     */
    clearErrors();
    for (const ProtocolConfigField& field : m_schema.fields) {
        const QVariant value = config.contains(field.key)
            ? config.value(field.key)
            : field.defaultValue;
        setFieldValue(field, value);
    }
}

QVariantMap ProtocolConfigEditor::config() const
{
    QVariantMap result;

    /*
     * 只导出 Schema 已知字段。未知字段由核心 Schema 校验层处理，
     * 编辑器第一版不展示也不编辑未知扩展字段。
     */
    for (const ProtocolConfigField& field : m_schema.fields) {
        result.insert(field.key, fieldValue(field));
    }

    return result;
}

void ProtocolConfigEditor::restoreDefaults()
{
    /*
     * 默认值来源必须保持在 Schema 层，尤其是 BytesHex 默认值会在
     * schema.defaults() 中规范化，避免 UI 与核心规则分叉。
     */
    setConfig(m_schema.defaults());
}

ProtocolConfigValidationResult ProtocolConfigEditor::validateConfig()
{
    /*
     * UI 不自行重写校验规则，而是把当前控件值交给 Schema。
     * 这样脚本、会话恢复、工厂创建和配置对话框走同一套规则。
     */
    const ProtocolConfigValidationResult result = m_schema.validate(config());
    if (result.valid) {
        clearErrors();
    } else {
        m_errorLabel->setText(result.errors.join(QStringLiteral("\n")));
        m_errorLabel->setVisible(true);
    }
    return result;
}

QString ProtocolConfigEditor::errorText() const
{
    return m_errorLabel ? m_errorLabel->text() : QString();
}

void ProtocolConfigEditor::clearErrors()
{
    if (!m_errorLabel) {
        return;
    }

    m_errorLabel->clear();
    m_errorLabel->setVisible(false);
}

void ProtocolConfigEditor::clearForm()
{
    /*
     * QFormLayout::takeAt() 会返回行内 label 和 field 的布局项。
     * 先检查 count() 可以避免空表单首次清理时触发 Qt 的无效索引警告；
     * 直接 delete widget 可确保连续 setSchema() 时旧控件不会继续被 findChild() 找到。
     */
    while (m_formLayout->count() > 0) {
        QLayoutItem* item = m_formLayout->takeAt(0);
        if (QWidget* widget = item->widget()) {
            delete widget;
        }
        delete item;
    }
    m_fieldWidgets.clear();
}

QWidget* ProtocolConfigEditor::createFieldWidget(const ProtocolConfigField& field)
{
    QWidget* widget = nullptr;

    switch (field.type) {
    case ProtocolConfigFieldType::Bool: {
        auto* checkBox = new QCheckBox(this);
        checkBox->setChecked(field.defaultValue.toBool());
        widget = checkBox;
        break;
    }

    case ProtocolConfigFieldType::Integer: {
        auto* spinBox = new QSpinBox(this);
        spinBox->setMinimum(field.minValue.isValid() ? field.minValue.toInt() : -2147483647);
        spinBox->setMaximum(field.maxValue.isValid() ? field.maxValue.toInt() : 2147483647);
        spinBox->setValue(field.defaultValue.toInt());
        widget = spinBox;
        break;
    }

    case ProtocolConfigFieldType::Double: {
        auto* spinBox = new QDoubleSpinBox(this);
        spinBox->setDecimals(6);
        spinBox->setMinimum(field.minValue.isValid() ? field.minValue.toDouble() : -999999999.0);
        spinBox->setMaximum(field.maxValue.isValid() ? field.maxValue.toDouble() : 999999999.0);
        spinBox->setValue(field.defaultValue.toDouble());
        widget = spinBox;
        break;
    }

    case ProtocolConfigFieldType::String:
    case ProtocolConfigFieldType::BytesHex: {
        auto* lineEdit = new QLineEdit(this);
        lineEdit->setText(field.defaultValue.toString());
        widget = lineEdit;
        break;
    }

    case ProtocolConfigFieldType::Enum: {
        auto* comboBox = new QComboBox(this);
        comboBox->addItems(field.enumValues);
        const int defaultIndex = comboBox->findText(field.defaultValue.toString());
        if (defaultIndex >= 0) {
            comboBox->setCurrentIndex(defaultIndex);
        }
        widget = comboBox;
        break;
    }
    }

    /*
     * objectName 是测试、自动化和后续字段级提示的稳定锚点，必须只由
     * Schema key 派生，不能随翻译或显示名变化。
     */
    widget->setObjectName(QStringLiteral("protocolConfig_%1").arg(field.key));
    widget->setToolTip(field.description);
    m_fieldWidgets.insert(field.key, widget);
    return widget;
}

void ProtocolConfigEditor::setFieldValue(const ProtocolConfigField& field, const QVariant& value)
{
    QWidget* widget = m_fieldWidgets.value(field.key, nullptr);
    if (!widget) {
        return;
    }

    /*
     * 根据字段类型写入对应控件。数值控件会按控件范围自动夹取，
     * 自由文本字段则保留原始输入，等待 Schema 做最终校验和规范化。
     */
    switch (field.type) {
    case ProtocolConfigFieldType::Bool:
        if (auto* checkBox = qobject_cast<QCheckBox*>(widget)) {
            checkBox->setChecked(value.toBool());
        }
        break;

    case ProtocolConfigFieldType::Integer:
        if (auto* spinBox = qobject_cast<QSpinBox*>(widget)) {
            spinBox->setValue(value.toInt());
        }
        break;

    case ProtocolConfigFieldType::Double:
        if (auto* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
            spinBox->setValue(value.toDouble());
        }
        break;

    case ProtocolConfigFieldType::String:
    case ProtocolConfigFieldType::BytesHex:
        if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            lineEdit->setText(value.toString());
        }
        break;

    case ProtocolConfigFieldType::Enum:
        if (auto* comboBox = qobject_cast<QComboBox*>(widget)) {
            const int index = comboBox->findText(value.toString());
            if (index >= 0) {
                comboBox->setCurrentIndex(index);
            } else {
                const int defaultIndex = comboBox->findText(field.defaultValue.toString());
                comboBox->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 0);
            }
        }
        break;
    }
}

QVariant ProtocolConfigEditor::fieldValue(const ProtocolConfigField& field) const
{
    QWidget* widget = m_fieldWidgets.value(field.key, nullptr);
    if (!widget) {
        return field.defaultValue;
    }

    /*
     * 控件读取保持 QVariantMap 的基础类型稳定：Bool 输出 bool，
     * Integer 输出 int，Double 输出 double，其余输出 QString。
     */
    switch (field.type) {
    case ProtocolConfigFieldType::Bool:
        if (const auto* checkBox = qobject_cast<const QCheckBox*>(widget)) {
            return checkBox->isChecked();
        }
        break;

    case ProtocolConfigFieldType::Integer:
        if (const auto* spinBox = qobject_cast<const QSpinBox*>(widget)) {
            return spinBox->value();
        }
        break;

    case ProtocolConfigFieldType::Double:
        if (const auto* spinBox = qobject_cast<const QDoubleSpinBox*>(widget)) {
            return spinBox->value();
        }
        break;

    case ProtocolConfigFieldType::String:
    case ProtocolConfigFieldType::BytesHex:
        if (const auto* lineEdit = qobject_cast<const QLineEdit*>(widget)) {
            return lineEdit->text();
        }
        break;

    case ProtocolConfigFieldType::Enum:
        if (const auto* comboBox = qobject_cast<const QComboBox*>(widget)) {
            return comboBox->currentText();
        }
        break;
    }

    return field.defaultValue;
}

} // namespace ComAssistant
