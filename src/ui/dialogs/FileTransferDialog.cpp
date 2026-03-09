/**
 * @file FileTransferDialog.cpp
 * @brief 文件传输对话框实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "FileTransferDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

namespace ComAssistant {

FileTransferDialog::FileTransferDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("文件传输"));
    resize(500, 400);
}

FileTransferDialog::~FileTransferDialog()
{
    if (m_transfer) {
        m_transfer->cancel();
        delete m_transfer;
    }
}

void FileTransferDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 传输方向
    auto* directionGroup = new QGroupBox(tr("传输方向"), this);
    auto* directionLayout = new QHBoxLayout(directionGroup);
    m_sendRadio = new QRadioButton(tr("发送文件"), this);
    m_receiveRadio = new QRadioButton(tr("接收文件"), this);
    m_sendRadio->setChecked(true);
    directionLayout->addWidget(m_sendRadio);
    directionLayout->addWidget(m_receiveRadio);
    directionLayout->addStretch();
    mainLayout->addWidget(directionGroup);

    // 协议选择
    auto* protocolLayout = new QHBoxLayout();
    protocolLayout->addWidget(new QLabel(tr("传输协议:"), this));
    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItem("XMODEM", (int)TransferProtocol::XModem);
    m_protocolCombo->addItem("XMODEM-CRC", (int)TransferProtocol::XModemCRC);
    m_protocolCombo->addItem("XMODEM-1K", (int)TransferProtocol::XModem1K);
    m_protocolCombo->addItem("YMODEM", (int)TransferProtocol::YModem);
    m_protocolCombo->setCurrentIndex(1);  // 默认XMODEM-CRC
    protocolLayout->addWidget(m_protocolCombo);
    protocolLayout->addStretch();
    mainLayout->addLayout(protocolLayout);

    // 文件选择
    auto* fileLayout = new QHBoxLayout();
    fileLayout->addWidget(new QLabel(tr("文件:"), this));
    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setPlaceholderText(tr("选择要发送或保存的文件路径"));
    m_browseBtn = new QPushButton(tr("浏览..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &FileTransferDialog::onBrowseClicked);
    fileLayout->addWidget(m_filePathEdit, 1);
    fileLayout->addWidget(m_browseBtn);
    mainLayout->addLayout(fileLayout);

    // 进度显示
    auto* progressGroup = new QGroupBox(tr("传输进度"), this);
    auto* progressLayout = new QGridLayout(progressGroup);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar, 0, 0, 1, 4);

    m_statusLabel = new QLabel(tr("就绪"), this);
    m_speedLabel = new QLabel(tr("速度: --"), this);
    m_packetLabel = new QLabel(tr("数据包: 0/0"), this);
    progressLayout->addWidget(m_statusLabel, 1, 0);
    progressLayout->addWidget(m_speedLabel, 1, 1);
    progressLayout->addWidget(m_packetLabel, 1, 2);

    mainLayout->addWidget(progressGroup);

    // 日志
    auto* logGroup = new QGroupBox(tr("传输日志"), this);
    auto* logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(120);
    logLayout->addWidget(m_logEdit);
    mainLayout->addWidget(logGroup);

    // 控制按钮
    auto* buttonLayout = new QHBoxLayout();
    m_startBtn = new QPushButton(tr("开始传输"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_cancelBtn->setEnabled(false);

    connect(m_startBtn, &QPushButton::clicked, this, &FileTransferDialog::onStartClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FileTransferDialog::onCancelClicked);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_startBtn);
    buttonLayout->addWidget(m_cancelBtn);
    mainLayout->addLayout(buttonLayout);

    // 连接方向切换信号
    connect(m_sendRadio, &QRadioButton::toggled, this, [this](bool checked) {
        Q_UNUSED(checked)
        updateUI();
    });
}

void FileTransferDialog::updateUI()
{
    bool isSend = m_sendRadio->isChecked();
    m_filePathEdit->setPlaceholderText(isSend ?
        tr("选择要发送的文件") : tr("选择保存位置"));
}

void FileTransferDialog::onBrowseClicked()
{
    QString path;
    if (m_sendRadio->isChecked()) {
        path = QFileDialog::getOpenFileName(this, tr("选择要发送的文件"), "",
            tr("所有文件 (*);;HEX文件 (*.hex);;BIN文件 (*.bin)"));
    } else {
        path = QFileDialog::getSaveFileName(this, tr("选择保存位置"), "",
            tr("所有文件 (*)"));
    }

    if (!path.isEmpty()) {
        m_filePathEdit->setText(path);
    }
}

void FileTransferDialog::onStartClicked()
{
    QString filePath = m_filePathEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择文件"));
        return;
    }

    // 创建传输对象
    if (m_transfer) {
        delete m_transfer;
    }

    TransferProtocol protocol = (TransferProtocol)m_protocolCombo->currentData().toInt();
    m_transfer = FileTransferFactory::create(protocol, this);

    if (!m_transfer) {
        QMessageBox::critical(this, tr("错误"), tr("创建传输对象失败"));
        return;
    }

    // 连接信号
    connect(m_transfer, &FileTransfer::sendData, this, &FileTransferDialog::sendData);
    connect(m_transfer, &FileTransfer::progressUpdated,
            this, &FileTransferDialog::onProgressUpdated);
    connect(m_transfer, &FileTransfer::transferCompleted,
            this, &FileTransferDialog::onTransferCompleted);

    // 开始传输
    bool started = false;
    if (m_sendRadio->isChecked()) {
        started = m_transfer->startSend(filePath);
    } else {
        started = m_transfer->startReceive(filePath);
    }

    if (started) {
        m_startBtn->setEnabled(false);
        m_cancelBtn->setEnabled(true);
        m_protocolCombo->setEnabled(false);
        m_sendRadio->setEnabled(false);
        m_receiveRadio->setEnabled(false);
        m_browseBtn->setEnabled(false);
        m_filePathEdit->setEnabled(false);

        appendLog(tr("传输开始..."));
    } else {
        QMessageBox::critical(this, tr("错误"), tr("启动传输失败"));
    }
}

void FileTransferDialog::onCancelClicked()
{
    if (m_transfer) {
        m_transfer->cancel();
        appendLog(tr("用户取消传输"));
    }
}

void FileTransferDialog::onProgressUpdated(const TransferProgress& progress)
{
    m_progressBar->setValue(static_cast<int>(progress.percentage()));

    QString statusText;
    switch (progress.state) {
    case TransferState::WaitingStart:
        statusText = tr("等待开始...");
        break;
    case TransferState::Transferring:
        statusText = tr("传输中...");
        break;
    case TransferState::Completing:
        statusText = tr("完成中...");
        break;
    default:
        statusText = tr("就绪");
        break;
    }
    m_statusLabel->setText(statusText);

    if (progress.speed > 0) {
        double speedKB = progress.speed / 1024.0;
        m_speedLabel->setText(tr("速度: %1 KB/s").arg(speedKB, 0, 'f', 2));
    }

    m_packetLabel->setText(tr("数据包: %1/%2")
        .arg(progress.currentPacket).arg(progress.totalPackets));

    if (progress.retryCount > 0) {
        appendLog(tr("重试 %1 次").arg(progress.retryCount));
    }
}

void FileTransferDialog::onTransferCompleted(bool success, const QString& message)
{
    m_startBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_protocolCombo->setEnabled(true);
    m_sendRadio->setEnabled(true);
    m_receiveRadio->setEnabled(true);
    m_browseBtn->setEnabled(true);
    m_filePathEdit->setEnabled(true);

    if (success) {
        m_progressBar->setValue(100);
        m_statusLabel->setText(tr("传输完成"));
        appendLog(tr("传输成功: %1").arg(message));
        QMessageBox::information(this, tr("成功"), message);
    } else {
        m_statusLabel->setText(tr("传输失败"));
        appendLog(tr("传输失败: %1").arg(message));
        QMessageBox::warning(this, tr("失败"), message);
    }
}

void FileTransferDialog::processReceivedData(const QByteArray& data)
{
    if (m_transfer) {
        m_transfer->processReceivedData(data);
    }
}

void FileTransferDialog::appendLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_logEdit->append(QString("[%1] %2").arg(timestamp, message));
}

void FileTransferDialog::setConnected(bool connected)
{
    m_connected = connected;
    m_startBtn->setEnabled(connected);
    if (!connected) {
        m_statusLabel->setText(tr("未连接"));
    } else {
        m_statusLabel->setText(tr("就绪"));
    }
}

void FileTransferDialog::setIAPMode(bool iapMode)
{
    m_iapMode = iapMode;
    if (iapMode) {
        setWindowTitle(tr("IAP固件升级"));
        m_sendRadio->setChecked(true);
        m_receiveRadio->setEnabled(false);
        m_filePathEdit->setPlaceholderText(tr("选择固件文件 (.hex/.bin)"));
    } else {
        setWindowTitle(tr("文件传输"));
        m_receiveRadio->setEnabled(true);
    }
}

void FileTransferDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void FileTransferDialog::retranslateUi()
{
    if (m_iapMode) {
        setWindowTitle(tr("IAP固件升级"));
        m_filePathEdit->setPlaceholderText(tr("选择固件文件 (.hex/.bin)"));
    } else {
        setWindowTitle(tr("文件传输"));
        m_filePathEdit->setPlaceholderText(tr("选择要发送或保存的文件路径"));
    }

    // 传输方向
    m_sendRadio->setText(tr("发送文件"));
    m_receiveRadio->setText(tr("接收文件"));

    // 按钮
    m_browseBtn->setText(tr("浏览..."));
    m_startBtn->setText(tr("开始传输"));
    m_cancelBtn->setText(tr("取消"));

    // 状态显示
    if (!m_connected) {
        m_statusLabel->setText(tr("未连接"));
    } else {
        m_statusLabel->setText(tr("就绪"));
    }
    m_speedLabel->setText(tr("速度: --"));
    m_packetLabel->setText(tr("数据包: 0/0"));
}

} // namespace ComAssistant
