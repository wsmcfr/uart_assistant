/**
 * @file ProtocolDiagnosticsDialog.cpp
 * @brief 协议诊断对话框实现
 */

#include "ProtocolDiagnosticsDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ComAssistant {
namespace {

/**
 * @brief 从 JSON 对象中安全读取字符串字段。
 * @param object 来源 JSON 对象。
 * @param key 字段名。
 * @param fallback 缺失或非字符串时使用的兜底文本。
 * @return 字段字符串或兜底文本。
 */
QString jsonString(const QJsonObject& object, const QString& key, const QString& fallback = QString())
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : fallback;
}

/**
 * @brief 从 JSON 对象中安全读取整数字段。
 * @param object 来源 JSON 对象。
 * @param key 字段名。
 * @param fallback 缺失或非数字时使用的兜底值。
 * @return 字段整数或兜底值。
 */
int jsonInt(const QJsonObject& object, const QString& key, int fallback = 0)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInt() : fallback;
}

/**
 * @brief 把布尔值转换为用户可读中文。
 * @param value 布尔值。
 * @return “是”或“否”。
 */
QString yesNoText(bool value)
{
    return value ? QStringLiteral("是") : QStringLiteral("否");
}

/**
 * @brief 把错误或警告数组合并为摘要文本。
 * @param values JSON 字符串数组。
 * @return 分号分隔的文本；为空时返回空字符串。
 */
QString joinJsonStringArray(const QJsonArray& values)
{
    QStringList text;
    for (const QJsonValue& value : values) {
        if (value.isString()) {
            text.append(value.toString());
        }
    }
    return text.join(QStringLiteral("; "));
}

} // namespace

ProtocolDiagnosticsDialog::ProtocolDiagnosticsDialog(const QJsonObject& diagnostics,
                                                     QWidget* parent)
    : QDialog(parent)
    , m_diagnostics(diagnostics)
    , m_jsonText(QString::fromUtf8(
          QJsonDocument(m_diagnostics).toJson(QJsonDocument::Indented)))
{
    setupUi();
    populateSummary();
}

QString ProtocolDiagnosticsDialog::jsonText() const
{
    return m_jsonText;
}

void ProtocolDiagnosticsDialog::copyJson()
{
    /*
     * Qt 桌面环境通常都提供剪贴板。这里仍然检查 QApplication::clipboard()
     * 返回值，防止极端测试环境下空指针导致崩溃。
     */
    QClipboard* clipboard = QApplication::clipboard();
    if (!clipboard) {
        m_statusLabel->setText(tr("剪贴板不可用，请手动复制 JSON。"));
        return;
    }

    clipboard->setText(m_jsonText);
    m_statusLabel->setText(tr("诊断 JSON 已复制。"));
}

void ProtocolDiagnosticsDialog::saveJson()
{
    /*
     * 保存路径由用户选择。用户取消时直接返回，不提示错误；
     * 只有文件打开或写入失败才弹出警告。
     */
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("保存协议诊断 JSON"),
        QStringLiteral("protocol-diagnostics.json"),
        tr("JSON 文件 (*.json);;所有文件 (*.*)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this,
                             tr("保存失败"),
                             tr("无法写入诊断文件：%1").arg(file.errorString()));
        return;
    }

    const QByteArray bytes = m_jsonText.toUtf8();
    if (file.write(bytes) != bytes.size()) {
        QMessageBox::warning(this,
                             tr("保存失败"),
                             tr("诊断文件未完整写入：%1").arg(file.errorString()));
        return;
    }

    m_statusLabel->setText(tr("诊断 JSON 已保存。"));
}

void ProtocolDiagnosticsDialog::setupUi()
{
    setWindowTitle(tr("协议诊断"));
    resize(720, 560);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    auto* titleLabel = new QLabel(tr("当前协议诊断"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    m_protocolLabel = new QLabel(this);
    m_protocolLabel->setWordWrap(true);
    mainLayout->addWidget(m_protocolLabel);

    m_capabilityLabel = new QLabel(this);
    m_capabilityLabel->setWordWrap(true);
    mainLayout->addWidget(m_capabilityLabel);

    m_configLabel = new QLabel(this);
    m_configLabel->setWordWrap(true);
    mainLayout->addWidget(m_configLabel);

    m_validationLabel = new QLabel(this);
    m_validationLabel->setWordWrap(true);
    mainLayout->addWidget(m_validationLabel);

    m_jsonEdit = new QPlainTextEdit(this);
    m_jsonEdit->setObjectName(QStringLiteral("protocolDiagnosticsJsonEdit"));
    m_jsonEdit->setReadOnly(true);
    m_jsonEdit->setPlainText(m_jsonText);
    m_jsonEdit->setMinimumHeight(260);
    mainLayout->addWidget(m_jsonEdit, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addStretch();

    m_copyButton = new QPushButton(tr("复制 JSON"), this);
    m_copyButton->setObjectName(QStringLiteral("protocolDiagnosticsCopyButton"));
    connect(m_copyButton, &QPushButton::clicked, this, &ProtocolDiagnosticsDialog::copyJson);
    buttonRow->addWidget(m_copyButton);

    m_saveButton = new QPushButton(tr("保存 JSON..."), this);
    m_saveButton->setObjectName(QStringLiteral("protocolDiagnosticsSaveButton"));
    connect(m_saveButton, &QPushButton::clicked, this, &ProtocolDiagnosticsDialog::saveJson);
    buttonRow->addWidget(m_saveButton);

    auto* closeButton = new QPushButton(tr("关闭"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonRow->addWidget(closeButton);

    mainLayout->addLayout(buttonRow);
}

void ProtocolDiagnosticsDialog::populateSummary()
{
    const QJsonObject protocol = m_diagnostics.value(QStringLiteral("protocol")).toObject();
    const QJsonObject capabilities = m_diagnostics.value(QStringLiteral("capabilities")).toObject();
    const QJsonObject configuration = m_diagnostics.value(QStringLiteral("configuration")).toObject();
    const QJsonObject validation = m_diagnostics.value(QStringLiteral("validation")).toObject();

    const QString protocolName = jsonString(protocol, QStringLiteral("displayName"), tr("未知协议"));
    const QString protocolId = jsonString(protocol, QStringLiteral("id"), QStringLiteral("unknown"));
    const QString category = jsonString(protocol, QStringLiteral("category"), QStringLiteral("Unknown"));
    const QString description = jsonString(protocol, QStringLiteral("description"));

    m_protocolLabel->setText(tr("协议：%1（ID: %2，分类: %3）%4")
                                 .arg(protocolName,
                                      protocolId,
                                      category,
                                      description.isEmpty()
                                          ? QString()
                                          : tr("\n说明：%1").arg(description)));

    m_capabilityLabel->setText(tr("能力：内置=%1，绘图=%2，构帧=%3")
                                   .arg(yesNoText(capabilities.value(QStringLiteral("builtin")).toBool()),
                                        yesNoText(capabilities.value(QStringLiteral("plotProtocol")).toBool()),
                                        yesNoText(capabilities.value(QStringLiteral("frameBuilder")).toBool())));

    m_configLabel->setText(tr("配置：configVersion=%1，schemaVersion=%2，字段数=%3")
                               .arg(jsonInt(configuration, QStringLiteral("configVersion")))
                               .arg(jsonInt(configuration, QStringLiteral("schemaVersion")))
                               .arg(jsonInt(configuration, QStringLiteral("fieldCount"))));

    const bool valid = validation.value(QStringLiteral("valid")).toBool();
    const QString errors = joinJsonStringArray(validation.value(QStringLiteral("errors")).toArray());
    const QString warnings = joinJsonStringArray(validation.value(QStringLiteral("warnings")).toArray());
    QString validationText = valid ? tr("校验：通过") : tr("校验：失败");
    if (!errors.isEmpty()) {
        validationText.append(tr("\n错误：%1").arg(errors));
    }
    if (!warnings.isEmpty()) {
        validationText.append(tr("\n警告：%1").arg(warnings));
    }
    m_validationLabel->setText(validationText);
}

} // namespace ComAssistant
