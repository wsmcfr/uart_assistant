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
}

void ProtocolConfigEditor::setSchema(const ProtocolConfigSchema& schema)
{
    /*
     * Schema 是编辑器的唯一输入来源。每次设置都完整重建，避免协议切换后
     * 旧控件的取值、范围或枚举选项继续影响新协议。
     */
    m_schema = schema;
    clearForm();

    const bool hasFields = !m_schema.fields.isEmpty();
    m_emptyLabel->setVisible(!hasFields);

    for (const ProtocolConfigField& field : m_schema.fields) {
        QWidget* fieldWidget = createFieldWidget(field);
        m_formLayout->addRow(field.displayName, fieldWidget);
    }
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
    return widget;
}

} // namespace ComAssistant
