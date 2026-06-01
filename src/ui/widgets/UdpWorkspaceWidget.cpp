/**
 * @file UdpWorkspaceWidget.cpp
 * @brief UDP 专用工作台实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "UdpWorkspaceWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
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

    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("udpSummaryLabel"));
    m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_summaryLabel->setFrameShape(QFrame::StyledPanel);
    mainLayout->addWidget(m_summaryLabel);

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
    m_broadcastCheck->setObjectName(QStringLiteral("udpBroadcastCheck"));
    endpointLayout->addRow(QString(), m_broadcastCheck);

    m_recentRemoteCombo = new QComboBox;
    m_recentRemoteCombo->setObjectName(QStringLiteral("udpRecentRemoteCombo"));
    QPushButton* clearRecentButton = new QPushButton(tr("清空最近"));
    clearRecentButton->setObjectName(QStringLiteral("udpClearRecentRemoteButton"));
    QWidget* recentWidget = new QWidget;
    QHBoxLayout* recentLayout = new QHBoxLayout(recentWidget);
    recentLayout->setContentsMargins(0, 0, 0, 0);
    recentLayout->addWidget(m_recentRemoteCombo, 1);
    recentLayout->addWidget(clearRecentButton);
    endpointLayout->addRow(tr("最近远端:"), recentWidget);
    mainLayout->addWidget(endpointGroup);

    QWidget* logToolbar = new QWidget;
    QHBoxLayout* logToolbarLayout = new QHBoxLayout(logToolbar);
    logToolbarLayout->setContentsMargins(0, 0, 0, 0);
    logToolbarLayout->setSpacing(6);
    QLabel* logTitleLabel = new QLabel(tr("数据报日志"));
    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    m_autoScrollCheck->setObjectName(QStringLiteral("udpAutoScrollCheck"));
    m_autoScrollCheck->setChecked(true);
    QPushButton* copyLogButton = new QPushButton(tr("复制"));
    copyLogButton->setObjectName(QStringLiteral("udpCopyLogButton"));
    QPushButton* clearLogButton = new QPushButton(tr("清空"));
    clearLogButton->setObjectName(QStringLiteral("udpClearLogButton"));
    logToolbarLayout->addWidget(logTitleLabel);
    logToolbarLayout->addStretch(1);
    logToolbarLayout->addWidget(m_autoScrollCheck);
    logToolbarLayout->addWidget(copyLogButton);
    logToolbarLayout->addWidget(clearLogButton);
    mainLayout->addWidget(logToolbar);

    m_logEdit = new QPlainTextEdit;
    m_logEdit->setReadOnly(true);
    m_logEdit->setObjectName(QStringLiteral("udpLogEdit"));
    m_logEdit->setPlaceholderText(tr("UDP 数据报收发记录会显示在这里..."));
    mainLayout->addWidget(m_logEdit, 1);

    QWidget* sendPanel = new QWidget;
    QHBoxLayout* sendLayout = new QHBoxLayout(sendPanel);
    sendLayout->setContentsMargins(0, 0, 0, 0);
    m_sendEdit = new QPlainTextEdit;
    m_sendEdit->setObjectName(QStringLiteral("udpSendEdit"));
    m_sendEdit->setMaximumHeight(92);
    m_hexSendCheck = new QCheckBox(tr("HEX发送"));
    m_hexSendCheck->setObjectName(QStringLiteral("udpHexSendCheck"));
    m_appendNewlineCheck = new QCheckBox(tr("追加换行"));
    m_appendNewlineCheck->setObjectName(QStringLiteral("udpAppendNewlineCheck"));
    QPushButton* formatHexButton = new QPushButton(tr("HEX格式化"));
    formatHexButton->setObjectName(QStringLiteral("udpFormatHexButton"));
    m_byteCountLabel = new QLabel(tr("0 字节"));
    m_byteCountLabel->setObjectName(QStringLiteral("udpByteCountLabel"));
    m_byteCountLabel->setMinimumWidth(62);
    QPushButton* sendButton = new QPushButton(tr("发送数据报"));
    sendButton->setObjectName(QStringLiteral("udpSendButton"));
    sendLayout->addWidget(m_sendEdit, 1);
    sendLayout->addWidget(m_hexSendCheck);
    sendLayout->addWidget(m_appendNewlineCheck);
    sendLayout->addWidget(formatHexButton);
    sendLayout->addWidget(m_byteCountLabel);
    sendLayout->addWidget(sendButton);
    mainLayout->addWidget(sendPanel);

    m_sendEdit->installEventFilter(this);
    m_sendEdit->viewport()->installEventFilter(this);

    connect(sendButton, &QPushButton::clicked, this, &UdpWorkspaceWidget::onSendClicked);
    connect(m_recentRemoteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UdpWorkspaceWidget::onRecentRemoteChanged);
    connect(clearRecentButton, &QPushButton::clicked,
            this, &UdpWorkspaceWidget::onClearRecentRemoteClicked);
    connect(formatHexButton, &QPushButton::clicked, this, &UdpWorkspaceWidget::onFormatHexClicked);
    connect(copyLogButton, &QPushButton::clicked, this, &UdpWorkspaceWidget::onCopyLogClicked);
    connect(clearLogButton, &QPushButton::clicked, this, &UdpWorkspaceWidget::clear);
    connect(m_sendEdit, &QPlainTextEdit::textChanged, this, &UdpWorkspaceWidget::updateSendByteCount);
    connect(m_hexSendCheck, &QCheckBox::toggled, this, &UdpWorkspaceWidget::updateSendByteCount);
    connect(m_appendNewlineCheck, &QCheckBox::toggled, this, &UdpWorkspaceWidget::updateSendByteCount);
    connect(m_localPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UdpWorkspaceWidget::updateStatusSummary);
    connect(m_remoteIpEdit, &QLineEdit::textChanged, this, &UdpWorkspaceWidget::updateStatusSummary);
    connect(m_remotePortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UdpWorkspaceWidget::updateStatusSummary);
    connect(m_broadcastCheck, &QCheckBox::toggled, this, &UdpWorkspaceWidget::updateStatusSummary);

    updateSendByteCount();
    updateStatusSummary();
}

bool UdpWorkspaceWidget::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_sendEdit || watched == m_sendEdit->viewport())
        && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        /*
         * Ctrl+Enter 在 UDP 工作台中等价于点击“发送数据报”，不会影响普通
         * Enter 的多行 payload 编辑。
         */
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
            onSendClicked();
            return true;
        }
    }
    return CommunicationWorkspaceWidget::eventFilter(watched, event);
}

void UdpWorkspaceWidget::setConfig(const NetworkConfig& config)
{
    m_localPortSpin->setValue(config.listenPort);
    m_remoteIpEdit->setText(config.remoteIp.isEmpty() ? QStringLiteral("127.0.0.1") : config.remoteIp);
    if (config.remotePort > 0) {
        m_remotePortSpin->setValue(config.remotePort);
    }
    updateStatusSummary();
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
    updateStatusSummary();
}

void UdpWorkspaceWidget::setConnected(bool connected)
{
    CommunicationWorkspaceWidget::setConnected(connected);
    m_stateLabel->setText(connected ? tr("已绑定") : tr("未绑定"));
    m_localPortSpin->setEnabled(!connected);
    updateStatusSummary();
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
    const QByteArray payload = currentPayload();
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

void UdpWorkspaceWidget::onClearRecentRemoteClicked()
{
    m_recentRemoteCombo->clear();
    updateStatusSummary();
}

void UdpWorkspaceWidget::onFormatHexClicked()
{
    /*
     * 格式化 HEX 后同步切换为 HEX 发送，避免字节数提示和实际发送内容不一致。
     */
    m_hexSendCheck->setChecked(true);
    m_sendEdit->setPlainText(normalizeHexText(m_sendEdit->toPlainText()));
}

void UdpWorkspaceWidget::onCopyLogClicked()
{
    /*
     * UDP 报文通常用于复现网络问题，复制完整日志比只复制选中片段更适合
     * 后续粘贴到测试记录。
     */
    QApplication::clipboard()->setText(m_logEdit->toPlainText());
}

void UdpWorkspaceWidget::updateSendByteCount()
{
    if (!m_byteCountLabel) {
        return;
    }

    m_byteCountLabel->setText(tr("%1 字节").arg(currentPayload().size()));
}

void UdpWorkspaceWidget::updateStatusSummary()
{
    if (!m_summaryLabel) {
        return;
    }

    const QString targetIp = m_broadcastCheck && m_broadcastCheck->isChecked()
        ? QStringLiteral("255.255.255.255")
        : m_remoteIpEdit->text().trimmed();
    m_summaryLabel->setText(tr("%1  本地:%2  目标:%3:%4  最近远端:%5")
        .arg(m_connected ? tr("已绑定") : tr("未绑定"),
             m_localPortSpin->value() == 0 ? tr("自动") : QString::number(m_localPortSpin->value()),
             targetIp,
             QString::number(m_remotePortSpin->value()),
             QString::number(m_recentRemoteCombo ? m_recentRemoteCombo->count() : 0)));
}

QByteArray UdpWorkspaceWidget::currentPayload() const
{
    QByteArray payload = parsePayload(m_sendEdit->toPlainText(), m_hexSendCheck->isChecked());
    if (!m_hexSendCheck->isChecked() && m_appendNewlineCheck->isChecked()) {
        payload.append('\n');
    }
    return payload;
}

void UdpWorkspaceWidget::appendLogLine(const QString& direction, const QByteArray& data)
{
    CommunicationWorkspaceWidget::appendLogLine(m_logEdit,
                                                direction,
                                                QStringLiteral("UDP"),
                                                data,
                                                m_autoScrollCheck && m_autoScrollCheck->isChecked());
}

} // namespace ComAssistant
