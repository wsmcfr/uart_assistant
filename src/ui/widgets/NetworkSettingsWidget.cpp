/**
 * @file NetworkSettingsWidget.cpp
 * @brief 网络设置组件实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "NetworkSettingsWidget.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>

namespace ComAssistant {

NetworkSettingsWidget::NetworkSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void NetworkSettingsWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 远程设置
    m_remoteGroup = new QGroupBox(tr("Remote Settings"));
    QFormLayout* remoteLayout = new QFormLayout(m_remoteGroup);

    m_remoteIpEdit = new QLineEdit;
    m_remoteIpEdit->setPlaceholderText("192.168.1.100");
    m_remoteIpEdit->setText("127.0.0.1");
    m_remoteIpLabel = new QLabel(tr("Remote IP:"));
    remoteLayout->addRow(m_remoteIpLabel, m_remoteIpEdit);

    m_remotePortSpin = new QSpinBox;
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(8080);
    m_remotePortLabel = new QLabel(tr("Remote Port:"));
    remoteLayout->addRow(m_remotePortLabel, m_remotePortSpin);

    mainLayout->addWidget(m_remoteGroup);

    // 本地设置
    m_localGroup = new QGroupBox(tr("Local Settings"));
    QFormLayout* localLayout = new QFormLayout(m_localGroup);

    m_localPortSpin = new QSpinBox;
    m_localPortSpin->setRange(0, 65535);
    m_localPortSpin->setValue(0);
    m_localPortSpin->setSpecialValueText(tr("Auto"));
    m_localPortLabel = new QLabel(tr("Local Port:"));
    localLayout->addRow(m_localPortLabel, m_localPortSpin);

    m_maxConnectionsSpin = new QSpinBox;
    m_maxConnectionsSpin->setRange(1, 100);
    m_maxConnectionsSpin->setValue(10);
    m_maxConnectionsLabel = new QLabel(tr("Max Connections:"));
    localLayout->addRow(m_maxConnectionsLabel, m_maxConnectionsSpin);

    mainLayout->addWidget(m_localGroup);

    mainLayout->addStretch();

    // 连接信号
    connect(m_remoteIpEdit, &QLineEdit::textChanged, this, &NetworkSettingsWidget::onConfigChanged);
    connect(m_remotePortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NetworkSettingsWidget::onConfigChanged);
    connect(m_localPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NetworkSettingsWidget::onConfigChanged);
    connect(m_maxConnectionsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NetworkSettingsWidget::onConfigChanged);
}

void NetworkSettingsWidget::setConfig(const NetworkConfig& config)
{
    m_config = config;

    m_remoteIpEdit->setText(config.serverIp);
    m_remotePortSpin->setValue(config.serverPort);
    m_localPortSpin->setValue(config.listenPort);
    m_maxConnectionsSpin->setValue(config.maxConnections);
}

NetworkConfig NetworkSettingsWidget::config() const
{
    return m_config;
}

void NetworkSettingsWidget::setMode(NetworkMode mode)
{
    m_config.mode = mode;

    // 根据模式显示/隐藏相关选项
    bool isServer = (mode == NetworkMode::TcpServer);
    bool isClient = (mode == NetworkMode::TcpClient);

    // 服务器模式不需要远程IP
    m_remoteIpEdit->setEnabled(!isServer);

    // 客户端模式不需要最大连接数
    m_maxConnectionsSpin->setEnabled(isServer);
}

void NetworkSettingsWidget::onConfigChanged()
{
    m_config.serverIp = m_remoteIpEdit->text();
    m_config.serverPort = m_remotePortSpin->value();
    m_config.listenPort = m_localPortSpin->value();
    m_config.maxConnections = m_maxConnectionsSpin->value();

    emit configChanged(m_config);
}

void NetworkSettingsWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void NetworkSettingsWidget::retranslateUi()
{
    m_remoteGroup->setTitle(tr("Remote Settings"));
    m_remoteIpLabel->setText(tr("Remote IP:"));
    m_remotePortLabel->setText(tr("Remote Port:"));

    m_localGroup->setTitle(tr("Local Settings"));
    m_localPortLabel->setText(tr("Local Port:"));
    m_localPortSpin->setSpecialValueText(tr("Auto"));
    m_maxConnectionsLabel->setText(tr("Max Connections:"));
}

} // namespace ComAssistant
