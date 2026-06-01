/**
 * @file TcpClientWorkspaceWidget.cpp
 * @brief TCP 客户端专用工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TcpClientWorkspaceWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
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

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("tcpClientSummaryLabel"));
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_summaryLabel->setFrameShape(QFrame::StyledPanel);
    mainLayout->addWidget(m_summaryLabel);

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
    m_autoReconnectCheck->setObjectName(QStringLiteral("tcpClientAutoReconnectCheck"));
    connectionLayout->addRow(QString(), m_autoReconnectCheck);

    m_reconnectIntervalSpin = new QSpinBox;
    m_reconnectIntervalSpin->setObjectName(QStringLiteral("tcpClientReconnectIntervalSpin"));
    m_reconnectIntervalSpin->setRange(500, 60000);
    m_reconnectIntervalSpin->setValue(3000);
    m_reconnectIntervalSpin->setSuffix(QStringLiteral(" ms"));
    connectionLayout->addRow(tr("重连间隔:"), m_reconnectIntervalSpin);

    mainLayout->addWidget(connectionGroup);

    QWidget* logToolbar = new QWidget;
    QHBoxLayout* logToolbarLayout = new QHBoxLayout(logToolbar);
    logToolbarLayout->setContentsMargins(0, 0, 0, 0);
    logToolbarLayout->setSpacing(6);
    QLabel* logTitleLabel = new QLabel(tr("网络流日志"));
    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    m_autoScrollCheck->setObjectName(QStringLiteral("tcpClientAutoScrollCheck"));
    m_autoScrollCheck->setChecked(true);
    QPushButton* copyLogButton = new QPushButton(tr("复制"));
    copyLogButton->setObjectName(QStringLiteral("tcpClientCopyLogButton"));
    QPushButton* clearLogButton = new QPushButton(tr("清空"));
    clearLogButton->setObjectName(QStringLiteral("tcpClientClearLogButton"));
    logToolbarLayout->addWidget(logTitleLabel);
    logToolbarLayout->addStretch(1);
    logToolbarLayout->addWidget(m_autoScrollCheck);
    logToolbarLayout->addWidget(copyLogButton);
    logToolbarLayout->addWidget(clearLogButton);
    mainLayout->addWidget(logToolbar);

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
    m_hexSendCheck->setObjectName(QStringLiteral("tcpClientHexSendCheck"));
    m_appendNewlineCheck = new QCheckBox(tr("追加换行"));
    m_appendNewlineCheck->setObjectName(QStringLiteral("tcpClientAppendNewlineCheck"));
    QPushButton* formatHexButton = new QPushButton(tr("HEX格式化"));
    formatHexButton->setObjectName(QStringLiteral("tcpClientFormatHexButton"));
    m_byteCountLabel = new QLabel(tr("0 字节"));
    m_byteCountLabel->setObjectName(QStringLiteral("tcpClientByteCountLabel"));
    m_byteCountLabel->setMinimumWidth(62);
    m_sendButton = new QPushButton(tr("发送"));
    m_sendButton->setObjectName(QStringLiteral("tcpClientSendButton"));
    sendLayout->addWidget(m_sendEdit, 1);
    sendLayout->addWidget(m_hexSendCheck);
    sendLayout->addWidget(m_appendNewlineCheck);
    sendLayout->addWidget(formatHexButton);
    sendLayout->addWidget(m_byteCountLabel);
    sendLayout->addWidget(m_sendButton);
    mainLayout->addWidget(sendPanel);

    m_sendEdit->installEventFilter(this);
    m_sendEdit->viewport()->installEventFilter(this);

    connect(m_sendButton, &QPushButton::clicked, this, &TcpClientWorkspaceWidget::onSendClicked);
    connect(formatHexButton, &QPushButton::clicked, this, &TcpClientWorkspaceWidget::onFormatHexClicked);
    connect(copyLogButton, &QPushButton::clicked, this, &TcpClientWorkspaceWidget::onCopyLogClicked);
    connect(clearLogButton, &QPushButton::clicked, this, &TcpClientWorkspaceWidget::clear);
    connect(m_sendEdit, &QPlainTextEdit::textChanged, this, &TcpClientWorkspaceWidget::updateSendByteCount);
    connect(m_hexSendCheck, &QCheckBox::toggled, this, &TcpClientWorkspaceWidget::updateSendByteCount);
    connect(m_appendNewlineCheck, &QCheckBox::toggled, this, &TcpClientWorkspaceWidget::updateSendByteCount);
    connect(m_serverIpEdit, &QLineEdit::textChanged, this, &TcpClientWorkspaceWidget::updateStatusSummary);
    connect(m_serverPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TcpClientWorkspaceWidget::updateStatusSummary);
    connect(m_autoReconnectCheck, &QCheckBox::toggled,
            this, &TcpClientWorkspaceWidget::updateStatusSummary);
    connect(m_reconnectIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TcpClientWorkspaceWidget::updateStatusSummary);

    updateSendByteCount();
    updateStatusSummary();
}

bool TcpClientWorkspaceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_sendEdit || watched == m_sendEdit->viewport())
        && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        /*
         * Ctrl+Enter 只在发送输入框里消费，普通 Enter 仍然保留多行编辑能力。
         */
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            onSendClicked();
            return true;
        }
    }
    return CommunicationWorkspaceWidget::eventFilter(watched, event);
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
    updateStatusSummary();
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
    const QByteArray payload = currentPayload();
    if (!payload.isEmpty()) {
        emit sendDataRequested(payload);
    }
}

void TcpClientWorkspaceWidget::onFormatHexClicked()
{
    /*
     * 格式化 HEX 代表用户准备按字节发送，因此同步打开 HEX 发送模式，
     * 字节数提示和后续发送解析才会与格式化结果一致。
     */
    m_hexSendCheck->setChecked(true);
    m_sendEdit->setPlainText(normalizeHexText(m_sendEdit->toPlainText()));
}

void TcpClientWorkspaceWidget::onCopyLogClicked()
{
    /*
     * 日志复制走系统剪贴板，保留用户当前选择状态；空日志复制为空字符串，
     * 行为和常见调试工具一致。
     */
    QApplication::clipboard()->setText(m_logEdit->toPlainText());
}

void TcpClientWorkspaceWidget::updateSendByteCount()
{
    if (!m_byteCountLabel) {
        return;
    }

    m_byteCountLabel->setText(tr("%1 字节").arg(currentPayload().size()));
}

void TcpClientWorkspaceWidget::updateStatusSummary()
{
    if (!m_summaryLabel) {
        return;
    }

    m_summaryLabel->setText(tr("%1  目标 %2:%3  自动重连:%4  间隔:%5 ms")
        .arg(m_connected ? tr("已连接") : tr("未连接"),
             m_serverIpEdit->text().trimmed(),
             QString::number(m_serverPortSpin->value()),
             m_autoReconnectCheck->isChecked() ? tr("开") : tr("关"),
             QString::number(m_reconnectIntervalSpin->value())));
}

QByteArray TcpClientWorkspaceWidget::currentPayload() const
{
    QByteArray payload = parsePayload(m_sendEdit->toPlainText(), m_hexSendCheck->isChecked());
    if (!m_hexSendCheck->isChecked() && m_appendNewlineCheck->isChecked()) {
        payload.append('\n');
    }
    return payload;
}

void TcpClientWorkspaceWidget::appendLogLine(const QString& direction, const QByteArray& data)
{
    CommunicationWorkspaceWidget::appendLogLine(m_logEdit,
                                                direction,
                                                QStringLiteral("TCP"),
                                                data,
                                                m_autoScrollCheck && m_autoScrollCheck->isChecked());
}

} // namespace ComAssistant
