/**
 * @file HidReportWorkspaceWidget.cpp
 * @brief HID Report 专用调试工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "HidReportWorkspaceWidget.h"

#include "communication/HidReportCodec.h"

#include <QDateTime>
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

    m_historyEdit = new QPlainTextEdit;
    m_historyEdit->setReadOnly(true);
    m_historyEdit->setPlaceholderText(tr("HID Report 历史会显示在这里..."));
    mainLayout->addWidget(m_historyEdit, 1);

    QWidget* sendPanel = new QWidget;
    QHBoxLayout* sendLayout = new QHBoxLayout(sendPanel);
    sendLayout->setContentsMargins(0, 0, 0, 0);

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
    QPushButton* featureSetButton = new QPushButton(tr("Set Feature"));
    featureSetButton->setObjectName(QStringLiteral("hidFeatureSetButton"));
    QPushButton* featureGetButton = new QPushButton(tr("Get Feature"));
    featureGetButton->setObjectName(QStringLiteral("hidFeatureGetButton"));

    sendLayout->addWidget(m_outputPayloadEdit, 1);
    sendLayout->addWidget(outputButton);
    sendLayout->addWidget(m_featurePayloadEdit, 1);
    sendLayout->addWidget(featureSetButton);
    sendLayout->addWidget(featureGetButton);
    mainLayout->addWidget(sendPanel);

    connect(outputButton, &QPushButton::clicked, this, &HidReportWorkspaceWidget::onOutputSendClicked);
    connect(featureSetButton, &QPushButton::clicked, this, &HidReportWorkspaceWidget::onFeatureSetClicked);
    connect(featureGetButton, &QPushButton::clicked, this, &HidReportWorkspaceWidget::onFeatureGetClicked);
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

void HidReportWorkspaceWidget::onOutputSendClicked()
{
    const QByteArray payload = parsePayload(m_outputPayloadEdit->toPlainText(), true);
    if (!payload.isEmpty()) {
        emit outputReportRequested(payload);
    }
}

void HidReportWorkspaceWidget::onFeatureSetClicked()
{
    const QByteArray payload = parsePayload(m_featurePayloadEdit->toPlainText(), true);
    const QByteArray report = HidReportCodec::buildFeatureReport(config(), payload);
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
    m_historyEdit->appendPlainText(QStringLiteral("[%1] %2 %3 %4  %5")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             direction,
             reportType,
             QString::number(data.size()),
             bytesToHex(data)));
}

} // namespace ComAssistant
