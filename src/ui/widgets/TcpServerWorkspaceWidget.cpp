/**
 * @file TcpServerWorkspaceWidget.cpp
 * @brief TCP 服务器专用工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TcpServerWorkspaceWidget.h"

#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

namespace ComAssistant {

TcpServerWorkspaceWidget::TcpServerWorkspaceWidget(QWidget* parent)
    : CommunicationWorkspaceWidget(parent)
{
    setupUi();
}

void TcpServerWorkspaceWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(8);

    QGroupBox* listenGroup = new QGroupBox(tr("TCP服务器监听"));
    QFormLayout* listenLayout = new QFormLayout(listenGroup);

    m_stateLabel = new QLabel(tr("未监听"));
    listenLayout->addRow(tr("状态:"), m_stateLabel);

    m_listenPortSpin = new QSpinBox;
    m_listenPortSpin->setObjectName(QStringLiteral("tcpServerListenPortSpin"));
    m_listenPortSpin->setRange(1, 65535);
    m_listenPortSpin->setValue(8080);
    listenLayout->addRow(tr("监听端口:"), m_listenPortSpin);

    m_maxConnectionsSpin = new QSpinBox;
    m_maxConnectionsSpin->setObjectName(QStringLiteral("tcpServerMaxConnectionsSpin"));
    m_maxConnectionsSpin->setRange(1, 100);
    m_maxConnectionsSpin->setValue(10);
    listenLayout->addRow(tr("最大连接数:"), m_maxConnectionsSpin);

    m_clientCombo = new QComboBox;
    m_clientCombo->setObjectName(QStringLiteral("tcpServerClientCombo"));
    listenLayout->addRow(tr("目标客户端:"), m_clientCombo);
    mainLayout->addWidget(listenGroup);

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setPlaceholderText(tr("客户端连接、断开和收发记录会显示在这里..."));
    mainLayout->addWidget(m_logEdit, 1);

    QWidget* sendPanel = new QWidget;
    QHBoxLayout* sendLayout = new QHBoxLayout(sendPanel);
    sendLayout->setContentsMargins(0, 0, 0, 0);
    m_sendEdit = new QPlainTextEdit;
    m_sendEdit->setObjectName(QStringLiteral("tcpServerSendEdit"));
    m_sendEdit->setMaximumHeight(92);
    m_hexSendCheck = new QCheckBox(tr("HEX发送"));
    QPushButton* sendClientButton = new QPushButton(tr("发送到客户端"));
    sendClientButton->setObjectName(QStringLiteral("tcpServerSendClientButton"));
    QPushButton* broadcastButton = new QPushButton(tr("广播"));
    broadcastButton->setObjectName(QStringLiteral("tcpServerBroadcastButton"));
    sendLayout->addWidget(m_sendEdit, 1);
    sendLayout->addWidget(m_hexSendCheck);
    sendLayout->addWidget(sendClientButton);
    sendLayout->addWidget(broadcastButton);
    mainLayout->addWidget(sendPanel);

    connect(sendClientButton, &QPushButton::clicked, this, &TcpServerWorkspaceWidget::onSendClientClicked);
    connect(broadcastButton, &QPushButton::clicked, this, &TcpServerWorkspaceWidget::onBroadcastClicked);
}

void TcpServerWorkspaceWidget::setConfig(const NetworkConfig& config)
{
    m_listenPortSpin->setValue(config.listenPort);
    m_maxConnectionsSpin->setValue(config.maxConnections);
}

NetworkConfig TcpServerWorkspaceWidget::config() const
{
    NetworkConfig config;
    config.mode = NetworkMode::TcpServer;
    config.listenPort = m_listenPortSpin->value();
    config.maxConnections = m_maxConnectionsSpin->value();
    return config;
}

void TcpServerWorkspaceWidget::setClients(const QStringList& clients)
{
    const QString current = m_clientCombo->currentData().toString();
    m_clientCombo->clear();
    for (const QString& clientId : clients) {
        m_clientCombo->addItem(clientId, clientId);
    }
    const int currentIndex = m_clientCombo->findData(current);
    if (currentIndex >= 0) {
        m_clientCombo->setCurrentIndex(currentIndex);
    }
}

void TcpServerWorkspaceWidget::addClient(const QString& clientId)
{
    if (m_clientCombo->findData(clientId) < 0) {
        m_clientCombo->addItem(clientId, clientId);
    }
    m_logEdit->appendPlainText(tr("[客户端连接] %1").arg(clientId));
}

void TcpServerWorkspaceWidget::removeClient(const QString& clientId)
{
    const int index = m_clientCombo->findData(clientId);
    if (index >= 0) {
        m_clientCombo->removeItem(index);
    }
    m_logEdit->appendPlainText(tr("[客户端断开] %1").arg(clientId));
}

void TcpServerWorkspaceWidget::setConnected(bool connected)
{
    CommunicationWorkspaceWidget::setConnected(connected);
    m_stateLabel->setText(connected ? tr("监听中") : tr("未监听"));
    m_listenPortSpin->setEnabled(!connected);
    m_maxConnectionsSpin->setEnabled(!connected);
}

void TcpServerWorkspaceWidget::appendReceivedData(const QByteArray& data)
{
    appendLogLine(QStringLiteral("RX"), data);
}

void TcpServerWorkspaceWidget::appendSentData(const QByteArray& data)
{
    appendLogLine(QStringLiteral("TX"), data);
}

void TcpServerWorkspaceWidget::clear()
{
    m_logEdit->clear();
}

void TcpServerWorkspaceWidget::onSendClientClicked()
{
    const QByteArray payload = currentPayload();
    const QString clientId = m_clientCombo->currentData().toString();
    if (!payload.isEmpty() && !clientId.isEmpty()) {
        emit sendToClientRequested(clientId, payload);
    }
}

void TcpServerWorkspaceWidget::onBroadcastClicked()
{
    const QByteArray payload = currentPayload();
    if (!payload.isEmpty()) {
        emit broadcastDataRequested(payload);
    }
}

QByteArray TcpServerWorkspaceWidget::currentPayload() const
{
    return parsePayload(m_sendEdit->toPlainText(), m_hexSendCheck->isChecked());
}

void TcpServerWorkspaceWidget::appendLogLine(const QString& direction, const QByteArray& data)
{
    m_logEdit->appendPlainText(QStringLiteral("[%1] %2 %3  %4")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             direction,
             QString::number(data.size()),
             bytesToHex(data)));
}

} // namespace ComAssistant
