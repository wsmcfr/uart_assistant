/**
 * @file TcpServerWorkspaceWidget.cpp
 * @brief TCP 服务器专用工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TcpServerWorkspaceWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
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

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("tcpServerSummaryLabel"));
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_summaryLabel->setFrameShape(QFrame::StyledPanel);
    mainLayout->addWidget(m_summaryLabel);

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

    m_clientCountLabel = new QLabel(tr("0 个客户端"));
    m_clientCountLabel->setObjectName(QStringLiteral("tcpServerClientCountLabel"));
    m_disconnectClientButton = new QPushButton(tr("断开选中"));
    m_disconnectClientButton->setObjectName(QStringLiteral("tcpServerDisconnectClientButton"));
    QWidget* clientToolsWidget = new QWidget;
    QHBoxLayout* clientToolsLayout = new QHBoxLayout(clientToolsWidget);
    clientToolsLayout->setContentsMargins(0, 0, 0, 0);
    clientToolsLayout->addWidget(m_clientCountLabel);
    clientToolsLayout->addStretch(1);
    clientToolsLayout->addWidget(m_disconnectClientButton);
    listenLayout->addRow(tr("客户端:"), clientToolsWidget);
    mainLayout->addWidget(listenGroup);

    QWidget* logToolbar = new QWidget;
    QHBoxLayout* logToolbarLayout = new QHBoxLayout(logToolbar);
    logToolbarLayout->setContentsMargins(0, 0, 0, 0);
    logToolbarLayout->setSpacing(6);
    QLabel* logTitleLabel = new QLabel(tr("服务器日志"));
    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    m_autoScrollCheck->setObjectName(QStringLiteral("tcpServerAutoScrollCheck"));
    m_autoScrollCheck->setChecked(true);
    QPushButton* copyLogButton = new QPushButton(tr("复制"));
    copyLogButton->setObjectName(QStringLiteral("tcpServerCopyLogButton"));
    QPushButton* clearLogButton = new QPushButton(tr("清空"));
    clearLogButton->setObjectName(QStringLiteral("tcpServerClearLogButton"));
    logToolbarLayout->addWidget(logTitleLabel);
    logToolbarLayout->addStretch(1);
    logToolbarLayout->addWidget(m_autoScrollCheck);
    logToolbarLayout->addWidget(copyLogButton);
    logToolbarLayout->addWidget(clearLogButton);
    mainLayout->addWidget(logToolbar);

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setObjectName(QStringLiteral("tcpServerLogEdit"));
    m_logEdit->setPlaceholderText(tr("客户端连接、断开和收发记录会显示在这里..."));
    configureLogEdit(m_logEdit);
    mainLayout->addWidget(m_logEdit, 1);

    QWidget* sendPanel = new QWidget;
    QHBoxLayout* sendLayout = new QHBoxLayout(sendPanel);
    sendLayout->setContentsMargins(0, 0, 0, 0);
    m_sendEdit = new QPlainTextEdit;
    m_sendEdit->setObjectName(QStringLiteral("tcpServerSendEdit"));
    m_sendEdit->setMaximumHeight(92);
    m_hexSendCheck = new QCheckBox(tr("HEX发送"));
    m_hexSendCheck->setObjectName(QStringLiteral("tcpServerHexSendCheck"));
    m_appendNewlineCheck = new QCheckBox(tr("追加换行"));
    m_appendNewlineCheck->setObjectName(QStringLiteral("tcpServerAppendNewlineCheck"));
    QPushButton* formatHexButton = new QPushButton(tr("HEX格式化"));
    formatHexButton->setObjectName(QStringLiteral("tcpServerFormatHexButton"));
    m_byteCountLabel = new QLabel(tr("0 字节"));
    m_byteCountLabel->setObjectName(QStringLiteral("tcpServerByteCountLabel"));
    m_byteCountLabel->setMinimumWidth(62);
    m_sendClientButton = new QPushButton(tr("发送到客户端"));
    m_sendClientButton->setObjectName(QStringLiteral("tcpServerSendClientButton"));
    QPushButton* broadcastButton = new QPushButton(tr("广播"));
    broadcastButton->setObjectName(QStringLiteral("tcpServerBroadcastButton"));
    sendLayout->addWidget(m_sendEdit, 1);
    sendLayout->addWidget(m_hexSendCheck);
    sendLayout->addWidget(m_appendNewlineCheck);
    sendLayout->addWidget(formatHexButton);
    sendLayout->addWidget(m_byteCountLabel);
    sendLayout->addWidget(m_sendClientButton);
    sendLayout->addWidget(broadcastButton);
    mainLayout->addWidget(sendPanel);

    m_sendEdit->installEventFilter(this);
    m_sendEdit->viewport()->installEventFilter(this);

    connect(m_sendClientButton, &QPushButton::clicked, this, &TcpServerWorkspaceWidget::onSendClientClicked);
    connect(broadcastButton, &QPushButton::clicked, this, &TcpServerWorkspaceWidget::onBroadcastClicked);
    connect(m_disconnectClientButton, &QPushButton::clicked,
            this, &TcpServerWorkspaceWidget::onDisconnectClientClicked);
    connect(formatHexButton, &QPushButton::clicked, this, &TcpServerWorkspaceWidget::onFormatHexClicked);
    connect(copyLogButton, &QPushButton::clicked, this, &TcpServerWorkspaceWidget::onCopyLogClicked);
    connect(clearLogButton, &QPushButton::clicked, this, &TcpServerWorkspaceWidget::clear);
    connect(m_clientCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TcpServerWorkspaceWidget::updateClientState);
    connect(m_sendEdit, &QPlainTextEdit::textChanged, this, &TcpServerWorkspaceWidget::updateSendByteCount);
    connect(m_hexSendCheck, &QCheckBox::toggled, this, &TcpServerWorkspaceWidget::updateSendByteCount);
    connect(m_appendNewlineCheck, &QCheckBox::toggled, this, &TcpServerWorkspaceWidget::updateSendByteCount);
    connect(m_listenPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TcpServerWorkspaceWidget::updateStatusSummary);
    connect(m_maxConnectionsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TcpServerWorkspaceWidget::updateStatusSummary);

    updateClientState();
    updateSendByteCount();
    updateStatusSummary();
}

bool TcpServerWorkspaceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_sendEdit || watched == m_sendEdit->viewport())
        && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        /*
         * TCP 服务端的 Ctrl+Enter 语义是向当前选中客户端发送，广播仍保留显式按钮。
         */
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            onSendClientClicked();
            return true;
        }
    }
    return CommunicationWorkspaceWidget::eventFilter(watched, event);
}

void TcpServerWorkspaceWidget::setConfig(const NetworkConfig& config)
{
    m_listenPortSpin->setValue(config.listenPort);
    m_maxConnectionsSpin->setValue(config.maxConnections);
    updateStatusSummary();
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
    updateClientState();
}

void TcpServerWorkspaceWidget::addClient(const QString& clientId)
{
    if (m_clientCombo->findData(clientId) < 0) {
        m_clientCombo->addItem(clientId, clientId);
    }
    configureLogEdit(m_logEdit);
    m_logEdit->appendPlainText(tr("[客户端连接] %1").arg(clientId));
    updateClientState();
}

void TcpServerWorkspaceWidget::removeClient(const QString& clientId)
{
    const int index = m_clientCombo->findData(clientId);
    if (index >= 0) {
        m_clientCombo->removeItem(index);
    }
    configureLogEdit(m_logEdit);
    m_logEdit->appendPlainText(tr("[客户端断开] %1").arg(clientId));
    updateClientState();
}

void TcpServerWorkspaceWidget::setConnected(bool connected)
{
    CommunicationWorkspaceWidget::setConnected(connected);
    m_stateLabel->setText(connected ? tr("监听中") : tr("未监听"));
    m_listenPortSpin->setEnabled(!connected);
    m_maxConnectionsSpin->setEnabled(!connected);
    updateStatusSummary();
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

void TcpServerWorkspaceWidget::onDisconnectClientClicked()
{
    const QString clientId = m_clientCombo->currentData().toString();
    if (!clientId.isEmpty()) {
        emit disconnectClientRequested(clientId);
    }
}

void TcpServerWorkspaceWidget::onFormatHexClicked()
{
    /*
     * 格式化 HEX 后同步切换为 HEX 发送，避免界面显示的是字节串但实际按文本发送。
     */
    m_hexSendCheck->setChecked(true);
    m_sendEdit->setPlainText(normalizeHexText(m_sendEdit->toPlainText()));
}

void TcpServerWorkspaceWidget::onCopyLogClicked()
{
    /*
     * 复制全部服务器日志，便于用户把连接/断开/收发历史粘贴到 issue 或报告中。
     */
    QApplication::clipboard()->setText(m_logEdit->toPlainText());
}

void TcpServerWorkspaceWidget::updateClientState()
{
    const int clientCount = m_clientCombo ? m_clientCombo->count() : 0;
    if (m_clientCountLabel) {
        m_clientCountLabel->setText(tr("%1 个客户端").arg(clientCount));
    }
    const bool hasSelectedClient = clientCount > 0 && !m_clientCombo->currentData().toString().isEmpty();
    if (m_sendClientButton) {
        m_sendClientButton->setEnabled(hasSelectedClient);
    }
    if (m_disconnectClientButton) {
        m_disconnectClientButton->setEnabled(hasSelectedClient);
    }
    updateStatusSummary();
}

void TcpServerWorkspaceWidget::updateSendByteCount()
{
    if (!m_byteCountLabel) {
        return;
    }

    m_byteCountLabel->setText(tr("%1 字节").arg(currentPayload().size()));
}

void TcpServerWorkspaceWidget::updateStatusSummary()
{
    if (!m_summaryLabel) {
        return;
    }

    m_summaryLabel->setText(tr("%1  监听端口:%2  客户端:%3/%4")
        .arg(m_connected ? tr("监听中") : tr("未监听"),
             QString::number(m_listenPortSpin->value()),
             QString::number(m_clientCombo ? m_clientCombo->count() : 0),
             QString::number(m_maxConnectionsSpin->value())));
}

QByteArray TcpServerWorkspaceWidget::currentPayload() const
{
    QByteArray payload = parsePayload(m_sendEdit->toPlainText(), m_hexSendCheck->isChecked());
    if (!m_hexSendCheck->isChecked() && m_appendNewlineCheck->isChecked()) {
        payload.append('\n');
    }
    return payload;
}

void TcpServerWorkspaceWidget::appendLogLine(const QString& direction, const QByteArray& data)
{
    CommunicationWorkspaceWidget::appendLogLine(m_logEdit,
                                                direction,
                                                QStringLiteral("TCP"),
                                                data,
                                                m_autoScrollCheck && m_autoScrollCheck->isChecked());
}

} // namespace ComAssistant
