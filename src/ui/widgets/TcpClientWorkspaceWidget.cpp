/**
 * @file TcpClientWorkspaceWidget.cpp
 * @brief TCP 客户端专用工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TcpClientWorkspaceWidget.h"

#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace ComAssistant {

TcpClientWorkspaceWidget::TcpClientWorkspaceWidget(QWidget* parent)
    : CommunicationWorkspaceWidget(parent)
{
    setupUi();
}

void TcpClientWorkspaceWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(8);

    QGroupBox* connectionGroup = new QGroupBox(tr("TCP客户端连接"));
    QFormLayout* connectionLayout = new QFormLayout(connectionGroup);

    m_stateLabel = new QLabel(tr("未连接"));
    connectionLayout->addRow(tr("状态:"), m_stateLabel);

    m_serverIpEdit = new QLineEdit(QStringLiteral("127.0.0.1"));
    m_serverIpEdit->setObjectName(QStringLiteral("tcpClientServerIpEdit"));
    connectionLayout->addRow(tr("服务器 IP:"), m_serverIpEdit);

    m_serverPortSpin = new QSpinBox;
    m_serverPortSpin->setObjectName(QStringLiteral("tcpClientServerPortSpin"));
    m_serverPortSpin->setRange(1, 65535);
    m_serverPortSpin->setValue(8080);
    connectionLayout->addRow(tr("服务器端口:"), m_serverPortSpin);

    m_timeoutSpin = new QSpinBox;
    m_timeoutSpin->setObjectName(QStringLiteral("tcpClientTimeoutSpin"));
    m_timeoutSpin->setRange(100, 60000);
    m_timeoutSpin->setValue(5000);
    m_timeoutSpin->setSuffix(QStringLiteral(" ms"));
    connectionLayout->addRow(tr("连接超时:"), m_timeoutSpin);

    m_autoReconnectCheck = new QCheckBox(tr("断开后自动重连"));
    connectionLayout->addRow(QString(), m_autoReconnectCheck);

    m_reconnectIntervalSpin = new QSpinBox;
    m_reconnectIntervalSpin->setRange(500, 60000);
    m_reconnectIntervalSpin->setValue(3000);
    m_reconnectIntervalSpin->setSuffix(QStringLiteral(" ms"));
    connectionLayout->addRow(tr("重连间隔:"), m_reconnectIntervalSpin);

    mainLayout->addWidget(connectionGroup);

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setObjectName(QStringLiteral("tcpClientLogEdit"));
    m_logEdit->setPlaceholderText(tr("网络流收发日志会显示在这里..."));
    mainLayout->addWidget(m_logEdit, 1);

    QWidget* sendPanel = new QWidget;
    QHBoxLayout* sendLayout = new QHBoxLayout(sendPanel);
    sendLayout->setContentsMargins(0, 0, 0, 0);
    m_sendEdit = new QPlainTextEdit;
    m_sendEdit->setObjectName(QStringLiteral("tcpClientSendEdit"));
    m_sendEdit->setMaximumHeight(92);
    m_hexSendCheck = new QCheckBox(tr("HEX发送"));
    QPushButton* sendButton = new QPushButton(tr("发送"));
    sendButton->setObjectName(QStringLiteral("tcpClientSendButton"));
    sendLayout->addWidget(m_sendEdit, 1);
    sendLayout->addWidget(m_hexSendCheck);
    sendLayout->addWidget(sendButton);
    mainLayout->addWidget(sendPanel);

    connect(sendButton, &QPushButton::clicked, this, &TcpClientWorkspaceWidget::onSendClicked);
}

void TcpClientWorkspaceWidget::setConfig(const NetworkConfig& config)
{
    m_serverIpEdit->setText(config.serverIp);
    m_serverPortSpin->setValue(config.serverPort);
    m_timeoutSpin->setValue(config.connectTimeout);
}

NetworkConfig TcpClientWorkspaceWidget::config() const
{
    NetworkConfig config;
    config.mode = NetworkMode::TcpClient;
    config.serverIp = m_serverIpEdit->text().trimmed();
    config.serverPort = m_serverPortSpin->value();
    config.connectTimeout = m_timeoutSpin->value();
    return config;
}

bool TcpClientWorkspaceWidget::autoReconnectEnabled() const
{
    return m_autoReconnectCheck->isChecked();
}

int TcpClientWorkspaceWidget::reconnectIntervalMs() const
{
    return m_reconnectIntervalSpin->value();
}

void TcpClientWorkspaceWidget::setConnected(bool connected)
{
    CommunicationWorkspaceWidget::setConnected(connected);
    m_stateLabel->setText(connected ? tr("已连接") : tr("未连接"));
    m_serverIpEdit->setEnabled(!connected);
    m_serverPortSpin->setEnabled(!connected);
    m_timeoutSpin->setEnabled(!connected);
}

void TcpClientWorkspaceWidget::appendReceivedData(const QByteArray& data)
{
    appendLogLine(QStringLiteral("RX"), data);
}

void TcpClientWorkspaceWidget::appendSentData(const QByteArray& data)
{
    appendLogLine(QStringLiteral("TX"), data);
}

void TcpClientWorkspaceWidget::clear()
{
    m_logEdit->clear();
}

void TcpClientWorkspaceWidget::onSendClicked()
{
    const QByteArray payload = parsePayload(m_sendEdit->toPlainText(), m_hexSendCheck->isChecked());
    if (!payload.isEmpty()) {
        emit sendDataRequested(payload);
    }
}

void TcpClientWorkspaceWidget::appendLogLine(const QString& direction, const QByteArray& data)
{
    const QString line = QStringLiteral("[%1] %2 %3  %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             direction,
             QString::number(data.size()),
             bytesToHex(data));
    m_logEdit->appendPlainText(line);
}

} // namespace ComAssistant
