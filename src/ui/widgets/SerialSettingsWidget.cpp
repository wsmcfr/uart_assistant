/**
 * @file SerialSettingsWidget.cpp
 * @brief 串口设置组件实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "SerialSettingsWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSerialPortInfo>
#include <QDebug>

namespace ComAssistant {

SerialSettingsWidget::SerialSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    m_autoRefreshTimer = new QTimer(this);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &SerialSettingsWidget::onAutoRefresh);

    refreshPorts();
}

void SerialSettingsWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 串口选择
    QGroupBox* portGroup = new QGroupBox(tr("Port Settings"));
    QFormLayout* formLayout = new QFormLayout(portGroup);

    // 端口
    QHBoxLayout* portLayout = new QHBoxLayout;
    m_portCombo = new QComboBox;
    m_portCombo->setMinimumWidth(150);
    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setFixedWidth(60);
    portLayout->addWidget(m_portCombo, 1);
    portLayout->addWidget(m_refreshBtn);
    formLayout->addRow(tr("Port:"), portLayout);

    // 波特率
    m_baudRateCombo = new QComboBox;
    m_baudRateCombo->setEditable(true);
    populateBaudRates();
    formLayout->addRow(tr("Baud Rate:"), m_baudRateCombo);

    // 数据位
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItem("5", static_cast<int>(DataBits::Five));
    m_dataBitsCombo->addItem("6", static_cast<int>(DataBits::Six));
    m_dataBitsCombo->addItem("7", static_cast<int>(DataBits::Seven));
    m_dataBitsCombo->addItem("8", static_cast<int>(DataBits::Eight));
    m_dataBitsCombo->setCurrentIndex(3);  // 默认8位
    formLayout->addRow(tr("Data Bits:"), m_dataBitsCombo);

    // 停止位
    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItem("1", static_cast<int>(StopBits::One));
    m_stopBitsCombo->addItem("1.5", static_cast<int>(StopBits::OnePointFive));
    m_stopBitsCombo->addItem("2", static_cast<int>(StopBits::Two));
    formLayout->addRow(tr("Stop Bits:"), m_stopBitsCombo);

    // 校验位
    m_parityCombo = new QComboBox;
    m_parityCombo->addItem(tr("None"), static_cast<int>(Parity::None));
    m_parityCombo->addItem(tr("Even"), static_cast<int>(Parity::Even));
    m_parityCombo->addItem(tr("Odd"), static_cast<int>(Parity::Odd));
    m_parityCombo->addItem(tr("Space"), static_cast<int>(Parity::Space));
    m_parityCombo->addItem(tr("Mark"), static_cast<int>(Parity::Mark));
    formLayout->addRow(tr("Parity:"), m_parityCombo);

    // 流控制
    m_flowControlCombo = new QComboBox;
    m_flowControlCombo->addItem(tr("None"), static_cast<int>(FlowControl::None));
    m_flowControlCombo->addItem(tr("Hardware (RTS/CTS)"), static_cast<int>(FlowControl::Hardware));
    m_flowControlCombo->addItem(tr("Software (XON/XOFF)"), static_cast<int>(FlowControl::Software));
    formLayout->addRow(tr("Flow Control:"), m_flowControlCombo);

    mainLayout->addWidget(portGroup);

    // 自动刷新选项
    m_autoRefreshCheck = new QCheckBox(tr("Auto refresh port list"));
    mainLayout->addWidget(m_autoRefreshCheck);

    mainLayout->addStretch();

    // 连接信号
    connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SerialSettingsWidget::onPortChanged);
    connect(m_baudRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SerialSettingsWidget::onBaudRateChanged);
    connect(m_dataBitsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SerialSettingsWidget::onDataBitsChanged);
    connect(m_stopBitsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SerialSettingsWidget::onStopBitsChanged);
    connect(m_parityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SerialSettingsWidget::onParityChanged);
    connect(m_flowControlCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SerialSettingsWidget::onFlowControlChanged);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SerialSettingsWidget::onRefreshClicked);
    connect(m_autoRefreshCheck, &QCheckBox::toggled, [this](bool checked) {
        if (checked) {
            m_autoRefreshTimer->start(2000);
        } else {
            m_autoRefreshTimer->stop();
        }
    });
}

void SerialSettingsWidget::populateBaudRates()
{
    QList<int> baudRates = {
        1200, 2400, 4800, 9600, 14400, 19200, 38400,
        57600, 115200, 230400, 460800, 921600, 1000000,
        2000000, 3000000
    };

    for (int rate : baudRates) {
        m_baudRateCombo->addItem(QString::number(rate), rate);
    }

    // 默认选择 115200
    int index = m_baudRateCombo->findData(115200);
    if (index >= 0) {
        m_baudRateCombo->setCurrentIndex(index);
    }
}

void SerialSettingsWidget::setConfig(const SerialConfig& config)
{
    m_config = config;

    // 设置端口
    int portIndex = m_portCombo->findText(config.portName);
    if (portIndex >= 0) {
        m_portCombo->setCurrentIndex(portIndex);
    }

    // 设置波特率
    int baudIndex = m_baudRateCombo->findData(config.baudRate);
    if (baudIndex >= 0) {
        m_baudRateCombo->setCurrentIndex(baudIndex);
    } else {
        m_baudRateCombo->setEditText(QString::number(config.baudRate));
    }

    // 设置数据位
    int dataIndex = m_dataBitsCombo->findData(static_cast<int>(config.dataBits));
    if (dataIndex >= 0) {
        m_dataBitsCombo->setCurrentIndex(dataIndex);
    }

    // 设置停止位
    int stopIndex = m_stopBitsCombo->findData(static_cast<int>(config.stopBits));
    if (stopIndex >= 0) {
        m_stopBitsCombo->setCurrentIndex(stopIndex);
    }

    // 设置校验位
    int parityIndex = m_parityCombo->findData(static_cast<int>(config.parity));
    if (parityIndex >= 0) {
        m_parityCombo->setCurrentIndex(parityIndex);
    }

    // 设置流控制
    int flowIndex = m_flowControlCombo->findData(static_cast<int>(config.flowControl));
    if (flowIndex >= 0) {
        m_flowControlCombo->setCurrentIndex(flowIndex);
    }
}

SerialConfig SerialSettingsWidget::config() const
{
    return m_config;
}

void SerialSettingsWidget::refreshPorts()
{
    QString currentPort = m_portCombo->currentText();

    m_portCombo->blockSignals(true);
    m_portCombo->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    qDebug() << "Found" << ports.size() << "serial ports:";
    for (const auto& port : ports) {
        QString displayText = QString("%1 - %2").arg(port.portName(), port.description());
        qDebug() << "  -" << displayText;
        m_portCombo->addItem(displayText, port.portName());
    }

    // 恢复之前选择的端口
    if (!currentPort.isEmpty()) {
        for (int i = 0; i < m_portCombo->count(); ++i) {
            if (m_portCombo->itemData(i).toString() == currentPort ||
                m_portCombo->itemText(i).startsWith(currentPort)) {
                m_portCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    m_portCombo->blockSignals(false);

    if (m_portCombo->count() > 0 && m_portCombo->currentIndex() >= 0) {
        m_config.portName = m_portCombo->currentData().toString();
    }
}

void SerialSettingsWidget::onPortChanged(int index)
{
    if (index >= 0) {
        m_config.portName = m_portCombo->itemData(index).toString();
        emit configChanged(m_config);
    }
}

void SerialSettingsWidget::onBaudRateChanged(int index)
{
    if (index >= 0) {
        bool ok;
        int baudRate = m_baudRateCombo->currentText().toInt(&ok);
        if (ok) {
            m_config.baudRate = baudRate;
            emit configChanged(m_config);
        }
    }
}

void SerialSettingsWidget::onDataBitsChanged(int index)
{
    if (index >= 0) {
        m_config.dataBits = static_cast<DataBits>(m_dataBitsCombo->itemData(index).toInt());
        emit configChanged(m_config);
    }
}

void SerialSettingsWidget::onStopBitsChanged(int index)
{
    if (index >= 0) {
        m_config.stopBits = static_cast<StopBits>(m_stopBitsCombo->itemData(index).toInt());
        emit configChanged(m_config);
    }
}

void SerialSettingsWidget::onParityChanged(int index)
{
    if (index >= 0) {
        m_config.parity = static_cast<Parity>(m_parityCombo->itemData(index).toInt());
        emit configChanged(m_config);
    }
}

void SerialSettingsWidget::onFlowControlChanged(int index)
{
    if (index >= 0) {
        m_config.flowControl = static_cast<FlowControl>(m_flowControlCombo->itemData(index).toInt());
        emit configChanged(m_config);
    }
}

void SerialSettingsWidget::onRefreshClicked()
{
    refreshPorts();
}

void SerialSettingsWidget::onAutoRefresh()
{
    refreshPorts();
}

void SerialSettingsWidget::updateConfig()
{
    emit configChanged(m_config);
}

void SerialSettingsWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SerialSettingsWidget::retranslateUi()
{
    m_refreshBtn->setText(tr("Refresh"));
    m_autoRefreshCheck->setText(tr("Auto refresh port list"));

    // 校验位下拉框
    m_parityCombo->setItemText(0, tr("None"));
    m_parityCombo->setItemText(1, tr("Even"));
    m_parityCombo->setItemText(2, tr("Odd"));
    m_parityCombo->setItemText(3, tr("Space"));
    m_parityCombo->setItemText(4, tr("Mark"));

    // 流控制下拉框
    m_flowControlCombo->setItemText(0, tr("None"));
    m_flowControlCombo->setItemText(1, tr("Hardware (RTS/CTS)"));
    m_flowControlCombo->setItemText(2, tr("Software (XON/XOFF)"));
}

} // namespace ComAssistant
