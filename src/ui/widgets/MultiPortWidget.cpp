/**
 * @file MultiPortWidget.cpp
 * @brief 多串口管理界面组件实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "MultiPortWidget.h"
#include "core/communication/MultiPortManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFormLayout>

namespace ComAssistant {

MultiPortWidget::MultiPortWidget(QWidget* parent)
    : QWidget(parent)
    , m_manager(MultiPortManager::instance())
{
    setupUi();

    // 连接管理器信号
    connect(m_manager, &MultiPortManager::portCreated,
            this, &MultiPortWidget::onPortCreated);
    connect(m_manager, &MultiPortManager::portRemoved,
            this, &MultiPortWidget::onPortRemoved);
    connect(m_manager, &MultiPortManager::portOpened,
            this, &MultiPortWidget::onPortOpened);
    connect(m_manager, &MultiPortManager::portClosed,
            this, &MultiPortWidget::onPortClosed);
    connect(m_manager, &MultiPortManager::dataReceived,
            this, &MultiPortWidget::onDataReceived);
    connect(m_manager, &MultiPortManager::statisticsUpdated,
            this, &MultiPortWidget::onStatisticsUpdated);
    connect(m_manager, &MultiPortManager::portError,
            this, &MultiPortWidget::onPortError);

    // 定时更新状态
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MultiPortWidget::updatePortsStatus);
    m_statusTimer->start(1000);

    refreshAvailablePorts();
    updateTable();
}

void MultiPortWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // ===== 添加串口区域 =====
    m_addGroup = new QGroupBox(tr("添加串口"), this);
    auto* addLayout = new QHBoxLayout(m_addGroup);

    m_availablePortsCombo = new QComboBox(this);
    m_availablePortsCombo->setMinimumWidth(100);
    addLayout->addWidget(m_availablePortsCombo);

    m_refreshBtn = new QPushButton(tr("刷新"), this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MultiPortWidget::refreshAvailablePorts);
    addLayout->addWidget(m_refreshBtn);

    m_addBtn = new QPushButton(tr("添加"), this);
    connect(m_addBtn, &QPushButton::clicked, this, &MultiPortWidget::onAddPortClicked);
    addLayout->addWidget(m_addBtn);

    mainLayout->addWidget(m_addGroup);

    // ===== 串口列表 =====
    m_portTable = new QTableWidget(this);
    m_portTable->setColumnCount(ColCount);
    m_portTable->setHorizontalHeaderLabels({
        tr("别名"), tr("端口"), tr("波特率"), tr("状态"), tr("TX"), tr("RX")
    });
    m_portTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_portTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_portTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_portTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_portTable->horizontalHeader()->setStretchLastSection(true);
    m_portTable->verticalHeader()->setVisible(false);

    connect(m_portTable, &QTableWidget::itemSelectionChanged,
            this, &MultiPortWidget::onTableSelectionChanged);
    connect(m_portTable, &QTableWidget::customContextMenuRequested,
            this, &MultiPortWidget::onTableContextMenu);

    mainLayout->addWidget(m_portTable, 1);

    // ===== 操作按钮 =====
    auto* buttonLayout = new QHBoxLayout();

    m_openCloseBtn = new QPushButton(tr("打开"), this);
    connect(m_openCloseBtn, &QPushButton::clicked, this, &MultiPortWidget::onOpenCloseClicked);
    buttonLayout->addWidget(m_openCloseBtn);

    m_configBtn = new QPushButton(tr("配置"), this);
    connect(m_configBtn, &QPushButton::clicked, this, &MultiPortWidget::onConfigClicked);
    buttonLayout->addWidget(m_configBtn);

    m_removeBtn = new QPushButton(tr("移除"), this);
    connect(m_removeBtn, &QPushButton::clicked, this, &MultiPortWidget::onRemovePortClicked);
    buttonLayout->addWidget(m_removeBtn);

    buttonLayout->addStretch();

    m_sendToAllBtn = new QPushButton(tr("发送到全部"), this);
    connect(m_sendToAllBtn, &QPushButton::clicked, this, &MultiPortWidget::onSendToAllClicked);
    buttonLayout->addWidget(m_sendToAllBtn);

    mainLayout->addLayout(buttonLayout);

    // ===== 状态栏 =====
    m_statusLabel = new QLabel(tr("已打开: 0 个串口"), this);
    mainLayout->addWidget(m_statusLabel);

    // 初始状态
    m_openCloseBtn->setEnabled(false);
    m_configBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);
}

void MultiPortWidget::refreshAvailablePorts()
{
    m_availablePortsCombo->clear();

    auto ports = m_manager->availablePorts();
    for (const auto& port : ports) {
        QString text = QString("%1 (%2)").arg(port.portName(), port.description());
        m_availablePortsCombo->addItem(text, port.portName());
    }

    if (m_availablePortsCombo->count() == 0) {
        m_availablePortsCombo->addItem(tr("无可用串口"), QString());
    }
}

QString MultiPortWidget::selectedInstanceId() const
{
    int row = m_portTable->currentRow();
    if (row < 0) {
        return QString();
    }
    return m_portTable->item(row, ColPort)->data(Qt::UserRole).toString();
}

void MultiPortWidget::updateTable()
{
    m_portTable->setRowCount(0);

    QStringList ids = m_manager->instanceIds();
    for (const auto& id : ids) {
        const PortInstance* instance = m_manager->getInstance(id);
        if (!instance) continue;

        int row = m_portTable->rowCount();
        m_portTable->insertRow(row);

        auto* aliasItem = new QTableWidgetItem(instance->alias);
        m_portTable->setItem(row, ColAlias, aliasItem);

        auto* portItem = new QTableWidgetItem(instance->portName);
        portItem->setData(Qt::UserRole, id);  // 存储实例ID
        m_portTable->setItem(row, ColPort, portItem);

        auto* baudItem = new QTableWidgetItem(QString::number(instance->baudRate));
        m_portTable->setItem(row, ColBaudRate, baudItem);

        auto* statusItem = new QTableWidgetItem(instance->isOpen ? tr("已打开") : tr("已关闭"));
        statusItem->setForeground(instance->isOpen ? Qt::darkGreen : Qt::darkRed);
        m_portTable->setItem(row, ColStatus, statusItem);

        auto* txItem = new QTableWidgetItem(QString::number(instance->txBytes));
        m_portTable->setItem(row, ColTx, txItem);

        auto* rxItem = new QTableWidgetItem(QString::number(instance->rxBytes));
        m_portTable->setItem(row, ColRx, rxItem);
    }

    m_portTable->resizeColumnsToContents();
    updatePortsStatus();
}

int MultiPortWidget::findRowByInstanceId(const QString& instanceId) const
{
    for (int row = 0; row < m_portTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_portTable->item(row, ColPort);
        if (item && item->data(Qt::UserRole).toString() == instanceId) {
            return row;
        }
    }
    return -1;
}

void MultiPortWidget::updateRowStatus(int row, const QString& instanceId)
{
    const PortInstance* instance = m_manager->getInstance(instanceId);
    if (!instance || row < 0) return;

    if (m_portTable->item(row, ColStatus)) {
        m_portTable->item(row, ColStatus)->setText(instance->isOpen ? tr("已打开") : tr("已关闭"));
        m_portTable->item(row, ColStatus)->setForeground(instance->isOpen ? Qt::darkGreen : Qt::darkRed);
    }

    if (m_portTable->item(row, ColTx)) {
        m_portTable->item(row, ColTx)->setText(QString::number(instance->txBytes));
    }

    if (m_portTable->item(row, ColRx)) {
        m_portTable->item(row, ColRx)->setText(QString::number(instance->rxBytes));
    }

    if (m_portTable->item(row, ColAlias)) {
        m_portTable->item(row, ColAlias)->setText(instance->alias);
    }

    if (m_portTable->item(row, ColBaudRate)) {
        m_portTable->item(row, ColBaudRate)->setText(QString::number(instance->baudRate));
    }
}

void MultiPortWidget::onAddPortClicked()
{
    QString portName = m_availablePortsCombo->currentData().toString();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择有效的串口"));
        return;
    }

    QString id = m_manager->createPort(portName);
    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("创建串口失败，可能已被占用"));
    }
}

void MultiPortWidget::onRemovePortClicked()
{
    QString id = selectedInstanceId();
    if (id.isEmpty()) {
        return;
    }

    const PortInstance* instance = m_manager->getInstance(id);
    if (!instance) return;

    int ret = QMessageBox::question(this, tr("确认"),
        tr("确定要移除串口 %1 吗？").arg(instance->portName));

    if (ret == QMessageBox::Yes) {
        m_manager->removePort(id);
    }
}

void MultiPortWidget::onOpenCloseClicked()
{
    QString id = selectedInstanceId();
    if (id.isEmpty()) {
        return;
    }

    const PortInstance* instance = m_manager->getInstance(id);
    if (!instance) return;

    if (instance->isOpen) {
        m_manager->closePort(id);
    } else {
        if (!m_manager->openPort(id)) {
            QMessageBox::warning(this, tr("错误"), tr("打开串口失败"));
        }
    }
}

void MultiPortWidget::onConfigClicked()
{
    QString id = selectedInstanceId();
    if (id.isEmpty()) {
        return;
    }

    PortConfigDialog dialog(id, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_manager->setPortConfig(id,
            dialog.baudRate(),
            static_cast<QSerialPort::DataBits>(dialog.dataBits()),
            static_cast<QSerialPort::Parity>(dialog.parity()),
            static_cast<QSerialPort::StopBits>(dialog.stopBits()),
            static_cast<QSerialPort::FlowControl>(dialog.flowControl()));

        m_manager->setAlias(id, dialog.alias());

        int row = findRowByInstanceId(id);
        if (row >= 0) {
            updateRowStatus(row, id);
        }
    }
}

void MultiPortWidget::onSendToAllClicked()
{
    emit sendToActivePort(QByteArray());  // 由外部处理获取发送内容
}

void MultiPortWidget::onPortCreated(const QString& instanceId)
{
    Q_UNUSED(instanceId)
    updateTable();
}

void MultiPortWidget::onPortRemoved(const QString& instanceId)
{
    Q_UNUSED(instanceId)
    updateTable();
}

void MultiPortWidget::onPortOpened(const QString& instanceId)
{
    int row = findRowByInstanceId(instanceId);
    if (row >= 0) {
        updateRowStatus(row, instanceId);
    }
    updatePortsStatus();
    onTableSelectionChanged();  // 更新按钮状态
}

void MultiPortWidget::onPortClosed(const QString& instanceId)
{
    int row = findRowByInstanceId(instanceId);
    if (row >= 0) {
        updateRowStatus(row, instanceId);
    }
    updatePortsStatus();
    onTableSelectionChanged();
}

void MultiPortWidget::onDataReceived(const QString& instanceId, const QByteArray& data)
{
    emit showDataRequest(instanceId, data, true);
}

void MultiPortWidget::onStatisticsUpdated(const QString& instanceId)
{
    int row = findRowByInstanceId(instanceId);
    if (row >= 0) {
        updateRowStatus(row, instanceId);
    }
}

void MultiPortWidget::onPortError(const QString& instanceId, const QString& errorString)
{
    const PortInstance* instance = m_manager->getInstance(instanceId);
    QString portName = instance ? instance->portName : instanceId;
    QMessageBox::warning(this, tr("串口错误"),
        tr("串口 %1 发生错误:\n%2").arg(portName, errorString));
}

void MultiPortWidget::onTableSelectionChanged()
{
    QString id = selectedInstanceId();
    bool hasSelection = !id.isEmpty();

    m_removeBtn->setEnabled(hasSelection);
    m_configBtn->setEnabled(hasSelection);

    if (hasSelection) {
        const PortInstance* instance = m_manager->getInstance(id);
        if (instance) {
            m_openCloseBtn->setEnabled(true);
            m_openCloseBtn->setText(instance->isOpen ? tr("关闭") : tr("打开"));
            emit selectedPortChanged(id);
        }
    } else {
        m_openCloseBtn->setEnabled(false);
        m_openCloseBtn->setText(tr("打开"));
    }
}

void MultiPortWidget::onTableContextMenu(const QPoint& pos)
{
    QTableWidgetItem* item = m_portTable->itemAt(pos);
    if (!item) return;

    int row = item->row();
    QString id = m_portTable->item(row, ColPort)->data(Qt::UserRole).toString();
    const PortInstance* instance = m_manager->getInstance(id);
    if (!instance) return;

    QMenu menu(this);

    QAction* openCloseAction = menu.addAction(
        instance->isOpen ? tr("关闭") : tr("打开"));
    connect(openCloseAction, &QAction::triggered, this, [this, id, instance]() {
        if (instance->isOpen) {
            m_manager->closePort(id);
        } else {
            m_manager->openPort(id);
        }
    });

    menu.addSeparator();

    QAction* configAction = menu.addAction(tr("配置..."));
    connect(configAction, &QAction::triggered, this, [this, row]() {
        m_portTable->selectRow(row);
        onConfigClicked();
    });

    QAction* resetStatsAction = menu.addAction(tr("重置统计"));
    connect(resetStatsAction, &QAction::triggered, this, [this, id]() {
        m_manager->resetStatistics(id);
    });

    menu.addSeparator();

    QAction* removeAction = menu.addAction(tr("移除"));
    connect(removeAction, &QAction::triggered, this, [this, row]() {
        m_portTable->selectRow(row);
        onRemovePortClicked();
    });

    menu.exec(m_portTable->viewport()->mapToGlobal(pos));
}

void MultiPortWidget::updatePortsStatus()
{
    int openCount = m_manager->openPortCount();
    int totalCount = m_manager->instanceIds().size();
    m_statusLabel->setText(tr("已打开: %1/%2 个串口").arg(openCount).arg(totalCount));
}

void MultiPortWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void MultiPortWidget::retranslateUi()
{
    m_addGroup->setTitle(tr("添加串口"));
    m_refreshBtn->setText(tr("刷新"));
    m_addBtn->setText(tr("添加"));

    m_portTable->setHorizontalHeaderLabels({
        tr("别名"), tr("端口"), tr("波特率"), tr("状态"), tr("TX"), tr("RX")
    });

    m_configBtn->setText(tr("配置"));
    m_removeBtn->setText(tr("移除"));
    m_sendToAllBtn->setText(tr("发送到全部"));

    // 更新打开/关闭按钮文本
    onTableSelectionChanged();
    updatePortsStatus();
}

// ============== PortConfigDialog ==============

PortConfigDialog::PortConfigDialog(const QString& instanceId, QWidget* parent)
    : QDialog(parent)
    , m_instanceId(instanceId)
{
    setWindowTitle(tr("串口配置"));
    setupUi();
    loadCurrentConfig();
}

void PortConfigDialog::setupUi()
{
    auto* layout = new QFormLayout(this);

    m_aliasEdit = new QLineEdit(this);
    m_aliasLabel = new QLabel(tr("别名:"), this);
    layout->addRow(m_aliasLabel, m_aliasEdit);

    m_baudRateCombo = new QComboBox(this);
    m_baudRateCombo->addItems({"9600", "19200", "38400", "57600", "115200",
                              "230400", "460800", "921600", "1500000"});
    m_baudRateCombo->setEditable(true);
    m_baudRateLabel = new QLabel(tr("波特率:"), this);
    layout->addRow(m_baudRateLabel, m_baudRateCombo);

    m_dataBitsCombo = new QComboBox(this);
    m_dataBitsCombo->addItem("5", QSerialPort::Data5);
    m_dataBitsCombo->addItem("6", QSerialPort::Data6);
    m_dataBitsCombo->addItem("7", QSerialPort::Data7);
    m_dataBitsCombo->addItem("8", QSerialPort::Data8);
    m_dataBitsCombo->setCurrentIndex(3);
    m_dataBitsLabel = new QLabel(tr("数据位:"), this);
    layout->addRow(m_dataBitsLabel, m_dataBitsCombo);

    m_parityCombo = new QComboBox(this);
    m_parityCombo->addItem(tr("无"), QSerialPort::NoParity);
    m_parityCombo->addItem(tr("奇校验"), QSerialPort::OddParity);
    m_parityCombo->addItem(tr("偶校验"), QSerialPort::EvenParity);
    m_parityCombo->addItem(tr("标记"), QSerialPort::MarkParity);
    m_parityCombo->addItem(tr("空格"), QSerialPort::SpaceParity);
    m_parityLabel = new QLabel(tr("校验位:"), this);
    layout->addRow(m_parityLabel, m_parityCombo);

    m_stopBitsCombo = new QComboBox(this);
    m_stopBitsCombo->addItem("1", QSerialPort::OneStop);
    m_stopBitsCombo->addItem("1.5", QSerialPort::OneAndHalfStop);
    m_stopBitsCombo->addItem("2", QSerialPort::TwoStop);
    m_stopBitsLabel = new QLabel(tr("停止位:"), this);
    layout->addRow(m_stopBitsLabel, m_stopBitsCombo);

    m_flowControlCombo = new QComboBox(this);
    m_flowControlCombo->addItem(tr("无"), QSerialPort::NoFlowControl);
    m_flowControlCombo->addItem(tr("硬件 (RTS/CTS)"), QSerialPort::HardwareControl);
    m_flowControlCombo->addItem(tr("软件 (XON/XOFF)"), QSerialPort::SoftwareControl);
    m_flowControlLabel = new QLabel(tr("流控制:"), this);
    layout->addRow(m_flowControlLabel, m_flowControlCombo);

    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttonBox);
}

void PortConfigDialog::loadCurrentConfig()
{
    const PortInstance* instance = MultiPortManager::instance()->getInstance(m_instanceId);
    if (!instance) return;

    m_aliasEdit->setText(instance->alias);
    m_baudRateCombo->setCurrentText(QString::number(instance->baudRate));

    int idx = m_dataBitsCombo->findData(instance->dataBits);
    if (idx >= 0) m_dataBitsCombo->setCurrentIndex(idx);

    idx = m_parityCombo->findData(instance->parity);
    if (idx >= 0) m_parityCombo->setCurrentIndex(idx);

    idx = m_stopBitsCombo->findData(instance->stopBits);
    if (idx >= 0) m_stopBitsCombo->setCurrentIndex(idx);

    idx = m_flowControlCombo->findData(instance->flowControl);
    if (idx >= 0) m_flowControlCombo->setCurrentIndex(idx);
}

qint32 PortConfigDialog::baudRate() const
{
    return m_baudRateCombo->currentText().toInt();
}

int PortConfigDialog::dataBits() const
{
    return m_dataBitsCombo->currentData().toInt();
}

int PortConfigDialog::parity() const
{
    return m_parityCombo->currentData().toInt();
}

int PortConfigDialog::stopBits() const
{
    return m_stopBitsCombo->currentData().toInt();
}

int PortConfigDialog::flowControl() const
{
    return m_flowControlCombo->currentData().toInt();
}

QString PortConfigDialog::alias() const
{
    return m_aliasEdit->text();
}

void PortConfigDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void PortConfigDialog::retranslateUi()
{
    setWindowTitle(tr("串口配置"));
    m_aliasLabel->setText(tr("别名:"));
    m_baudRateLabel->setText(tr("波特率:"));
    m_dataBitsLabel->setText(tr("数据位:"));
    m_parityLabel->setText(tr("校验位:"));
    m_stopBitsLabel->setText(tr("停止位:"));
    m_flowControlLabel->setText(tr("流控制:"));

    // 更新校验位下拉框选项
    m_parityCombo->setItemText(0, tr("无"));
    m_parityCombo->setItemText(1, tr("奇校验"));
    m_parityCombo->setItemText(2, tr("偶校验"));
    m_parityCombo->setItemText(3, tr("标记"));
    m_parityCombo->setItemText(4, tr("空格"));

    // 更新流控制下拉框选项
    m_flowControlCombo->setItemText(0, tr("无"));
    m_flowControlCombo->setItemText(1, tr("硬件 (RTS/CTS)"));
    m_flowControlCombo->setItemText(2, tr("软件 (XON/XOFF)"));
}

} // namespace ComAssistant
