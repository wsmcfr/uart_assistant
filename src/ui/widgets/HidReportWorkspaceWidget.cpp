/**
 * @file HidReportWorkspaceWidget.cpp
 * @brief HID Report 专用调试工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "HidReportWorkspaceWidget.h"

#include "communication/HidReportCodec.h"

#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace ComAssistant {

HidReportWorkspaceWidget::HidReportWorkspaceWidget(QWidget* parent)
    : CommunicationWorkspaceWidget(parent)
{
    setupUi();
}

void HidReportWorkspaceWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(8);

    QGroupBox* reportGroup = new QGroupBox(tr("HID Report 参数"));
    QFormLayout* reportLayout = new QFormLayout(reportGroup);

    m_deviceSummaryLabel = new QLabel(tr("未选择 HID 设备"));
    m_deviceSummaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    reportLayout->addRow(tr("设备:"), m_deviceSummaryLabel);

    m_inputLengthSpin = new QSpinBox;
    m_inputLengthSpin->setObjectName(QStringLiteral("hidInputLengthSpin"));
    m_inputLengthSpin->setRange(1, 4096);
    m_inputLengthSpin->setValue(64);
    reportLayout->addRow(tr("Input长度:"), m_inputLengthSpin);

    m_outputLengthSpin = new QSpinBox;
    m_outputLengthSpin->setObjectName(QStringLiteral("hidOutputLengthSpin"));
    m_outputLengthSpin->setRange(1, 4096);
    m_outputLengthSpin->setValue(64);
    reportLayout->addRow(tr("Output长度:"), m_outputLengthSpin);

    m_featureLengthSpin = new QSpinBox;
    m_featureLengthSpin->setObjectName(QStringLiteral("hidFeatureLengthSpin"));
    m_featureLengthSpin->setRange(1, 4096);
    m_featureLengthSpin->setValue(64);
    reportLayout->addRow(tr("Feature长度:"), m_featureLengthSpin);

    m_outputReportIdSpin = new QSpinBox;
    m_outputReportIdSpin->setObjectName(QStringLiteral("hidOutputReportIdSpin"));
    m_outputReportIdSpin->setRange(0, 255);
    reportLayout->addRow(tr("Output Report ID:"), m_outputReportIdSpin);

    m_featureReportIdSpin = new QSpinBox;
    m_featureReportIdSpin->setObjectName(QStringLiteral("hidFeatureReportIdSpin"));
    m_featureReportIdSpin->setRange(0, 255);
    reportLayout->addRow(tr("Feature Report ID:"), m_featureReportIdSpin);

    m_firstDataIsLengthCheck = new QCheckBox(tr("Output payload 前追加长度字节"));
    reportLayout->addRow(QString(), m_firstDataIsLengthCheck);

    m_removeInputReportIdCheck = new QCheckBox(tr("接收时移除输入 Report ID"));
    reportLayout->addRow(QString(), m_removeInputReportIdCheck);
    mainLayout->addWidget(reportGroup);

    QWidget* historyToolbar = new QWidget;
    QHBoxLayout* historyToolbarLayout = new QHBoxLayout(historyToolbar);
    historyToolbarLayout->setContentsMargins(0, 0, 0, 0);
    historyToolbarLayout->setSpacing(6);
    QLabel* historyTitleLabel = new QLabel(tr("报告历史"));
    QPushButton* copyHistoryButton = new QPushButton(tr("复制"));
    copyHistoryButton->setObjectName(QStringLiteral("hidCopyHistoryButton"));
    QPushButton* clearHistoryButton = new QPushButton(tr("清空"));
    clearHistoryButton->setObjectName(QStringLiteral("hidClearHistoryButton"));
    historyToolbarLayout->addWidget(historyTitleLabel);
    historyToolbarLayout->addStretch(1);
    historyToolbarLayout->addWidget(copyHistoryButton);
    historyToolbarLayout->addWidget(clearHistoryButton);
    mainLayout->addWidget(historyToolbar);

    m_historyEdit = new QPlainTextEdit;
    m_historyEdit->setReadOnly(true);
    m_historyEdit->setObjectName(QStringLiteral("hidHistoryEdit"));
    m_historyEdit->setPlaceholderText(tr("HID Report 历史会显示在这里..."));
    mainLayout->addWidget(m_historyEdit, 1);

    QWidget* sendPanel = new QWidget;
    QVBoxLayout* sendLayout = new QVBoxLayout(sendPanel);
    sendLayout->setContentsMargins(0, 0, 0, 0);
    sendLayout->setSpacing(6);

    m_outputPayloadEdit = new QPlainTextEdit;
    m_outputPayloadEdit->setObjectName(QStringLiteral("hidOutputPayloadEdit"));
    m_outputPayloadEdit->setMaximumHeight(92);
    m_outputPayloadEdit->setPlaceholderText(tr("Output payload HEX，例如: 01 02 A0"));

    m_featurePayloadEdit = new QPlainTextEdit;
    m_featurePayloadEdit->setObjectName(QStringLiteral("hidFeaturePayloadEdit"));
    m_featurePayloadEdit->setMaximumHeight(92);
    m_featurePayloadEdit->setPlaceholderText(tr("Feature payload HEX，例如: 01 02 A0"));

    QPushButton* outputButton = new QPushButton(tr("发送 Output"));
    outputButton->setObjectName(QStringLiteral("hidOutputSendButton"));
    QPushButton* outputFormatButton = new QPushButton(tr("HEX格式化"));
    outputFormatButton->setObjectName(QStringLiteral("hidOutputFormatHexButton"));
    QPushButton* featureSetButton = new QPushButton(tr("Set Feature"));
    featureSetButton->setObjectName(QStringLiteral("hidFeatureSetButton"));
    QPushButton* featureGetButton = new QPushButton(tr("Get Feature"));
    featureGetButton->setObjectName(QStringLiteral("hidFeatureGetButton"));
    QPushButton* featureFormatButton = new QPushButton(tr("HEX格式化"));
    featureFormatButton->setObjectName(QStringLiteral("hidFeatureFormatHexButton"));

    m_outputByteCountLabel = new QLabel(tr("0 字节"));
    m_outputByteCountLabel->setObjectName(QStringLiteral("hidOutputByteCountLabel"));
    m_featureByteCountLabel = new QLabel(tr("0 字节"));
    m_featureByteCountLabel->setObjectName(QStringLiteral("hidFeatureByteCountLabel"));
    m_outputPreviewLabel = new QLabel(tr("Output Report:"));
    m_outputPreviewLabel->setObjectName(QStringLiteral("hidOutputPreviewLabel"));
    m_outputPreviewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_outputPreviewLabel->setWordWrap(true);
    m_featurePreviewLabel = new QLabel(tr("Feature Report:"));
    m_featurePreviewLabel->setObjectName(QStringLiteral("hidFeaturePreviewLabel"));
    m_featurePreviewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_featurePreviewLabel->setWordWrap(true);
    m_truncationWarningLabel = new QLabel;
    m_truncationWarningLabel->setObjectName(QStringLiteral("hidTruncationWarningLabel"));
    m_truncationWarningLabel->setStyleSheet(QStringLiteral("color: #b00020;"));
    m_truncationWarningLabel->setVisible(false);

    QGroupBox* outputGroup = new QGroupBox(tr("Output Report"));
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);
    QHBoxLayout* outputToolLayout = new QHBoxLayout;
    outputToolLayout->addWidget(m_outputByteCountLabel);
    outputToolLayout->addStretch(1);
    outputToolLayout->addWidget(outputFormatButton);
    outputToolLayout->addWidget(outputButton);
    outputLayout->addWidget(m_outputPayloadEdit);
    outputLayout->addLayout(outputToolLayout);
    outputLayout->addWidget(m_outputPreviewLabel);

    QGroupBox* featureGroup = new QGroupBox(tr("Feature Report"));
    QVBoxLayout* featureLayout = new QVBoxLayout(featureGroup);
    QHBoxLayout* featureToolLayout = new QHBoxLayout;
    featureToolLayout->addWidget(m_featureByteCountLabel);
    featureToolLayout->addStretch(1);
    featureToolLayout->addWidget(featureFormatButton);
    featureToolLayout->addWidget(featureSetButton);
    featureToolLayout->addWidget(featureGetButton);
    featureLayout->addWidget(m_featurePayloadEdit);
    featureLayout->addLayout(featureToolLayout);
    featureLayout->addWidget(m_featurePreviewLabel);

    QHBoxLayout* reportSendLayout = new QHBoxLayout;
    reportSendLayout->addWidget(outputGroup, 1);
    reportSendLayout->addWidget(featureGroup, 1);
    sendLayout->addLayout(reportSendLayout);
    sendLayout->addWidget(m_truncationWarningLabel);
    mainLayout->addWidget(sendPanel);

    connect(outputButton, &QPushButton::clicked, this, &HidReportWorkspaceWidget::onOutputSendClicked);
    connect(featureSetButton, &QPushButton::clicked, this, &HidReportWorkspaceWidget::onFeatureSetClicked);
    connect(featureGetButton, &QPushButton::clicked, this, &HidReportWorkspaceWidget::onFeatureGetClicked);
    connect(outputFormatButton, &QPushButton::clicked,
            this, &HidReportWorkspaceWidget::onOutputFormatHexClicked);
    connect(featureFormatButton, &QPushButton::clicked,
            this, &HidReportWorkspaceWidget::onFeatureFormatHexClicked);
    connect(copyHistoryButton, &QPushButton::clicked,
            this, &HidReportWorkspaceWidget::onCopyHistoryClicked);
    connect(clearHistoryButton, &QPushButton::clicked, this, &HidReportWorkspaceWidget::clear);
    connect(m_outputPayloadEdit, &QPlainTextEdit::textChanged,
            this, &HidReportWorkspaceWidget::updateReportPreviews);
    connect(m_featurePayloadEdit, &QPlainTextEdit::textChanged,
            this, &HidReportWorkspaceWidget::updateReportPreviews);
    connect(m_outputLengthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &HidReportWorkspaceWidget::updateReportPreviews);
    connect(m_featureLengthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &HidReportWorkspaceWidget::updateReportPreviews);
    connect(m_outputReportIdSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &HidReportWorkspaceWidget::updateReportPreviews);
    connect(m_featureReportIdSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &HidReportWorkspaceWidget::updateReportPreviews);
    connect(m_firstDataIsLengthCheck, &QCheckBox::toggled,
            this, &HidReportWorkspaceWidget::updateReportPreviews);

    updateReportPreviews();
}

void HidReportWorkspaceWidget::setConfig(const HidConfig& config)
{
    m_config = config;
    m_inputLengthSpin->setValue(config.inputReportLength);
    m_outputLengthSpin->setValue(config.outputReportLength);
    m_featureLengthSpin->setValue(config.featureReportLength);
    m_outputReportIdSpin->setValue(config.outReportId);
    m_featureReportIdSpin->setValue(config.featureReportId);
    m_firstDataIsLengthCheck->setChecked(config.firstDataIsLength);
    m_removeInputReportIdCheck->setChecked(config.removeInReportId);
    updateDeviceSummary();
    updateReportPreviews();
}

HidConfig HidReportWorkspaceWidget::config() const
{
    HidConfig config = m_config;
    config.inputReportLength = m_inputLengthSpin->value();
    config.outputReportLength = m_outputLengthSpin->value();
    config.featureReportLength = m_featureLengthSpin->value();
    config.outReportId = static_cast<quint8>(m_outputReportIdSpin->value());
    config.featureReportId = static_cast<quint8>(m_featureReportIdSpin->value());
    config.firstDataIsLength = m_firstDataIsLengthCheck->isChecked();
    config.removeInReportId = m_removeInputReportIdCheck->isChecked();
    return config;
}

void HidReportWorkspaceWidget::setConnected(bool connected)
{
    CommunicationWorkspaceWidget::setConnected(connected);
    Q_UNUSED(connected)
}

void HidReportWorkspaceWidget::appendReceivedData(const QByteArray& data)
{
    appendHistoryLine(QStringLiteral("RX"), QStringLiteral("Input"), data);
}

void HidReportWorkspaceWidget::appendSentData(const QByteArray& data)
{
    appendHistoryLine(QStringLiteral("TX"), QStringLiteral("Output payload"), data);
}

void HidReportWorkspaceWidget::clear()
{
    m_historyEdit->clear();
}

void HidReportWorkspaceWidget::appendFeatureReportData(const QByteArray& data)
{
    appendHistoryLine(QStringLiteral("RX"), QStringLiteral("Feature"), data);
}

void HidReportWorkspaceWidget::appendFeatureReportSentData(const QByteArray& data)
{
    appendHistoryLine(QStringLiteral("TX"), QStringLiteral("Feature"), data);
}

void HidReportWorkspaceWidget::onOutputSendClicked()
{
    const QByteArray payload = outputPayload();
    if (!payload.isEmpty()) {
        emit outputReportRequested(payload);
    }
}

void HidReportWorkspaceWidget::onFeatureSetClicked()
{
    const QByteArray report = HidReportCodec::buildFeatureReport(config(), featurePayload());
    emit featureReportSetRequested(report);
}

void HidReportWorkspaceWidget::onFeatureGetClicked()
{
    /*
     * hid_get_feature_report 的缓冲区首字节必须先填入要读取的 Report ID，
     * 后续字节留 0 作为接收空间。
     */
    const QByteArray report = HidReportCodec::buildFeatureReport(config(), QByteArray());
    emit featureReportGetRequested(report);
}

void HidReportWorkspaceWidget::onOutputFormatHexClicked()
{
    m_outputPayloadEdit->setPlainText(normalizeHexText(m_outputPayloadEdit->toPlainText()));
}

void HidReportWorkspaceWidget::onFeatureFormatHexClicked()
{
    m_featurePayloadEdit->setPlainText(normalizeHexText(m_featurePayloadEdit->toPlainText()));
}

void HidReportWorkspaceWidget::onCopyHistoryClicked()
{
    /*
     * HID Report 历史经常需要和厂商文档或抓包结果对照，复制完整历史比只复制
     * 当前选区更适合调试复盘。
     */
    QApplication::clipboard()->setText(m_historyEdit->toPlainText());
}

void HidReportWorkspaceWidget::updateReportPreviews()
{
    const HidConfig currentConfig = config();
    const QByteArray output = outputPayload();
    const QByteArray feature = featurePayload();
    const QByteArray outputReport = HidReportCodec::buildOutputReport(currentConfig, output);
    const QByteArray featureReport = HidReportCodec::buildFeatureReport(currentConfig, feature);

    if (m_outputByteCountLabel) {
        m_outputByteCountLabel->setText(tr("payload %1 字节 / report %2 字节")
            .arg(output.size())
            .arg(outputReport.size()));
    }
    if (m_featureByteCountLabel) {
        m_featureByteCountLabel->setText(tr("payload %1 字节 / report %2 字节")
            .arg(feature.size())
            .arg(featureReport.size()));
    }
    if (m_outputPreviewLabel) {
        m_outputPreviewLabel->setText(tr("Output Report: %1").arg(bytesToHex(outputReport)));
    }
    if (m_featurePreviewLabel) {
        m_featurePreviewLabel->setText(tr("Feature Report: %1").arg(bytesToHex(featureReport)));
    }

    const bool outputTruncated = payloadWillBeTruncated(currentConfig.outputReportLength,
                                                        output.size(),
                                                        currentConfig.firstDataIsLength);
    const bool featureTruncated = payloadWillBeTruncated(currentConfig.featureReportLength,
                                                         feature.size(),
                                                         false);
    if (m_truncationWarningLabel) {
        m_truncationWarningLabel->setVisible(outputTruncated || featureTruncated);
        if (outputTruncated && featureTruncated) {
            m_truncationWarningLabel->setText(tr("Output 和 Feature payload 超过 Report 可用长度，发送时会截断。"));
        } else if (outputTruncated) {
            m_truncationWarningLabel->setText(tr("Output payload 超过 Report 可用长度，发送时会截断。"));
        } else if (featureTruncated) {
            m_truncationWarningLabel->setText(tr("Feature payload 超过 Report 可用长度，发送时会截断。"));
        } else {
            m_truncationWarningLabel->clear();
        }
    }
}

void HidReportWorkspaceWidget::updateDeviceSummary()
{
    m_deviceSummaryLabel->setText(QStringLiteral("%1  VID:%2 PID:%3 IF:%4 Usage:%5/%6")
        .arg(m_config.name.isEmpty() ? tr("未命名 HID") : m_config.name)
        .arg(m_config.vendorId, 4, 16, QLatin1Char('0'))
        .arg(m_config.productId, 4, 16, QLatin1Char('0'))
        .arg(m_config.interfaceNumber)
        .arg(m_config.usagePage, 4, 16, QLatin1Char('0'))
        .arg(m_config.usage, 4, 16, QLatin1Char('0'))
        .toUpper());
}

void HidReportWorkspaceWidget::appendHistoryLine(const QString& direction,
                                                 const QString& reportType,
                                                 const QByteArray& data)
{
    m_historyEdit->appendPlainText(formatLogLine(direction, reportType, data));
}

QByteArray HidReportWorkspaceWidget::outputPayload() const
{
    return parsePayload(m_outputPayloadEdit->toPlainText(), true);
}

QByteArray HidReportWorkspaceWidget::featurePayload() const
{
    return parsePayload(m_featurePayloadEdit->toPlainText(), true);
}

bool HidReportWorkspaceWidget::payloadWillBeTruncated(int reportLength,
                                                      int payloadLength,
                                                      bool firstDataIsLength) const
{
    /*
     * 固定长度报告的首字节总是 Report ID；Output 可选再占用一个长度字节。
     * 负载长度超过剩余空间时，HidReportCodec 会 truncate，这里提前给出提示。
     */
    const int fixedLength = qMax(1, reportLength);
    const int availablePayloadLength = qMax(0, fixedLength - 1 - (firstDataIsLength ? 1 : 0));
    return payloadLength > availablePayloadLength;
}

} // namespace ComAssistant
