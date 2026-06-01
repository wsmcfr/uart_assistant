/**
 * @file UdpWorkspaceWidget.cpp
 * @brief UDP 专用工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "UdpWorkspaceWidget.h"

#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace ComAssistant {

UdpWorkspaceWidget::UdpWorkspaceWidget(QWidget* parent)
    : CommunicationWorkspaceWidget(parent)
{
    setupUi();
}

void UdpWorkspaceWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(8);

    QGroupBox* endpointGroup = new QGroupBox(tr("UDP端点"));
    QFormLayout* endpointLayout = new QFormLayout(endpointGroup);

    m_stateLabel = new QLabel(tr("未绑定"));
    endpointLayout->addRow(tr("状态:"), m_stateLabel);

    m_localPortSpin = new QSpinBox;
    m_localPortSpin->setObjectName(QStringLiteral("udpLocalPortSpin"));
    m_localPortSpin->setRange(0, 65535);
    m_localPortSpin->setSpecialValueText(tr("自动"));
    m_localPortSpin->setValue(8080);
    endpointLayout->addRow(tr("本地端口:"), m_localPortSpin);

    m_remoteIpEdit = new QLineEdit(QStringLiteral("127.0.0.1"));
    m_remoteIpEdit->setObjectName(QStringLiteral("udpRemoteIpEdit"));
    endpointLayout->addRow(tr("目标 IP:"), m_remoteIpEdit);

    m_remotePortSpin = new QSpinBox;
    m_remotePortSpin->setObjectName(QStringLiteral("udpRemotePortSpin"));
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(8080);
    endpointLayout->addRow(tr("目标端口:"), m_remotePortSpin);

    m_broadcastCheck = new QCheckBox(tr("广播发送"));
    endpointLayout->addRow(QString(), m_broadcastCheck);

    m_recentRemoteCombo = new QComboBox;
    m_recentRemoteCombo->setObjectName(QStringLiteral("udpRecentRemoteCombo"));
    endpointLayout->addRow(tr("最近远端:"), m_recentRemoteCombo);
    mainLayout->addWidget(endpointGroup);

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setPlaceholderText(tr("UDP 数据报收发记录会显示在这里..."));
    mainLayout->addWidget(m_logEdit, 1);

    QWidget* sendPanel = new QWidget;
    QHBoxLayout* sendLayout = new QHBoxLayout(sendPanel);
    sendLayout->setContentsMargins(0, 0, 0, 0);
    m_sendEdit = new QPlainTextEdit;
    m_sendEdit->setObjectName(QStringLiteral("udpSendEdit"));
    m_sendEdit->setMaximumHeight(92);
    m_hexSendCheck = new QCheckBox(tr("HEX发送"));
    QPushButton* sendButton = new QPushButton(tr("发送数据报"));
    sendButton->setObjectName(QStringLiteral("udpSendButton"));
    sendLayout->addWidget(m_sendEdit, 1);
    sendLayout->addWidget(m_hexSendCheck);
    sendLayout->addWidget(sendButton);
    mainLayout->addWidget(sendPanel);

    connect(sendButton, &QPushButton::clicked, this, &UdpWorkspaceWidget::onSendClicked);
    connect(m_recentRemoteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UdpWorkspaceWidget::onRecentRemoteChanged);
}

void UdpWorkspaceWidget::setConfig(const NetworkConfig& config)
{
    m_localPortSpin->setValue(config.listenPort);
    m_remoteIpEdit->setText(config.remoteIp.isEmpty() ? QStringLiteral("127.0.0.1") : config.remoteIp);
    if (config.remotePort > 0) {
        m_remotePortSpin->setValue(config.remotePort);
    }
}

NetworkConfig UdpWorkspaceWidget::config() const
{
    NetworkConfig config;
    config.mode = NetworkMode::Udp;
    config.listenPort = m_localPortSpin->value();
    config.remoteIp = m_remoteIpEdit->text().trimmed();
    config.remotePort = m_remotePortSpin->value();
    return config;
}

void UdpWorkspaceWidget::addRecentRemote(const QString& ip, int port)
{
    const QString label = QStringLiteral("%1:%2").arg(ip).arg(port);
    if (m_recentRemoteCombo->findText(label) < 0) {
        m_recentRemoteCombo->insertItem(0, label, QStringList() << ip << QString::number(port));
    }
    m_recentRemoteCombo->setCurrentIndex(m_recentRemoteCombo->findText(label));
}

void UdpWorkspaceWidget::setConnected(bool connected)
{
    CommunicationWorkspaceWidget::setConnected(connected);
    m_stateLabel->setText(connected ? tr("已绑定") : tr("未绑定"));
    m_localPortSpin->setEnabled(!connected);
}

void UdpWorkspaceWidget::appendReceivedData(const QByteArray& data)
{
    appendLogLine(QStringLiteral("RX"), data);
}

void UdpWorkspaceWidget::appendSentData(const QByteArray& data)
{
    appendLogLine(QStringLiteral("TX"), data);
}

void UdpWorkspaceWidget::clear()
{
    m_logEdit->clear();
}

void UdpWorkspaceWidget::onSendClicked()
{
    const QByteArray payload = parsePayload(m_sendEdit->toPlainText(), m_hexSendCheck->isChecked());
    QString ip = m_remoteIpEdit->text().trimmed();
    if (m_broadcastCheck->isChecked()) {
        ip = QStringLiteral("255.255.255.255");
    }
    if (!payload.isEmpty() && !ip.isEmpty()) {
        emit sendDatagramRequested(payload, ip, m_remotePortSpin->value());
    }
}

void UdpWorkspaceWidget::onRecentRemoteChanged(int index)
{
    if (index < 0) {
        return;
    }
    const QStringList parts = m_recentRemoteCombo->itemData(index).toStringList();
    if (parts.size() != 2) {
        return;
    }
    m_remoteIpEdit->setText(parts.at(0));
    m_remotePortSpin->setValue(parts.at(1).toInt());
}

void UdpWorkspaceWidget::appendLogLine(const QString& direction, const QByteArray& data)
{
    m_logEdit->appendPlainText(QStringLiteral("[%1] %2 %3  %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             direction,
             QString::number(data.size()),
             bytesToHex(data)));
}

} // namespace ComAssistant
