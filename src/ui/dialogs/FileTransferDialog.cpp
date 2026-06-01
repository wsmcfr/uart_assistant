/**
 * @file FileTransferDialog.cpp
 * @brief 文件传输对话框实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "FileTransferDialog.h"
#include "utils/ChecksumUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QDateTime>
#include <QtGlobal>

namespace ComAssistant {

FileTransferDialog::FileTransferDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("文件传输"));
    resize(720, 620);
}

FileTransferDialog::~FileTransferDialog()
{
    if (m_transfer) {
        /*
         * 只有正在进行或等待中的传输需要主动取消。若传输已经完成，
         * 析构时再调用 cancel() 可能向设备额外发送 CAN/取消语义数据。
         */
        const TransferState state = m_transfer->state();
        if (state == TransferState::WaitingStart ||
            state == TransferState::Transferring ||
            state == TransferState::Completing) {
            m_transfer->cancel();
        }
        delete m_transfer;
    }
}

void FileTransferDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // 模式选择：三种文件发送形态共享同一个对话框，避免用户在多个入口之间来回找。
    auto* modeLayout = new QHBoxLayout();
    modeLayout->addWidget(new QLabel(tr("发送模式:"), this));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("裸流分块"), static_cast<int>(DialogTransferMode::RawStream));
    m_modeCombo->addItem(tr("XMODEM/YMODEM"), static_cast<int>(DialogTransferMode::StandardProtocol));
    m_modeCombo->addItem(tr("自定义 OTA"), static_cast<int>(DialogTransferMode::CustomOta));
    m_modeCombo->setCurrentIndex(0);
    modeLayout->addWidget(m_modeCombo);
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);

    // 传输方向
    m_directionGroup = new QGroupBox(tr("传输方向"), this);
    auto* directionLayout = new QHBoxLayout(m_directionGroup);
    m_sendRadio = new QRadioButton(tr("发送文件"), this);
    m_receiveRadio = new QRadioButton(tr("接收文件"), this);
    m_sendRadio->setChecked(true);
    directionLayout->addWidget(m_sendRadio);
    directionLayout->addWidget(m_receiveRadio);
    directionLayout->addStretch();
    mainLayout->addWidget(m_directionGroup);

    // XMODEM/YMODEM 参数区：继续复用已有标准协议实现。
    m_standardOptionsWidget = new QWidget(this);
    auto* protocolLayout = new QHBoxLayout(m_standardOptionsWidget);
    protocolLayout->setContentsMargins(0, 0, 0, 0);
    m_protocolLabel = new QLabel(tr("传输协议:"), this);
    protocolLayout->addWidget(m_protocolLabel);
    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->addItem("XMODEM", (int)TransferProtocol::XModem);
    m_protocolCombo->addItem("XMODEM-CRC", (int)TransferProtocol::XModemCRC);
    m_protocolCombo->addItem("XMODEM-1K", (int)TransferProtocol::XModem1K);
    m_protocolCombo->addItem("YMODEM", (int)TransferProtocol::YModem);
    m_protocolCombo->setCurrentIndex(1);  // 默认XMODEM-CRC
    protocolLayout->addWidget(m_protocolCombo);
    protocolLayout->addStretch();

    // 裸流参数区：块大小和间隔直接决定串口吞吐与 MCU 处理压力。
    m_rawOptionsWidget = new QWidget(this);
    auto* rawLayout = new QGridLayout(m_rawOptionsWidget);
    rawLayout->setContentsMargins(0, 0, 0, 0);
    m_rawBlockSizeSpin = new QSpinBox(this);
    m_rawBlockSizeSpin->setRange(1, 4096);
    m_rawBlockSizeSpin->setValue(256);
    m_rawBlockSizeSpin->setSuffix(tr(" B"));
    m_rawIntervalSpin = new QSpinBox(this);
    m_rawIntervalSpin->setRange(0, 60000);
    m_rawIntervalSpin->setValue(10);
    m_rawIntervalSpin->setSuffix(tr(" ms"));
    rawLayout->addWidget(new QLabel(tr("每包字节:"), this), 0, 0);
    rawLayout->addWidget(m_rawBlockSizeSpin, 0, 1);
    rawLayout->addWidget(new QLabel(tr("包间隔:"), this), 0, 2);
    rawLayout->addWidget(m_rawIntervalSpin, 0, 3);
    rawLayout->setColumnStretch(4, 1);

    // OTA 参数区：提供通用可配置格式，适配简单自定义 Bootloader。
    m_otaOptionsWidget = new QWidget(this);
    auto* otaLayout = new QGridLayout(m_otaOptionsWidget);
    otaLayout->setContentsMargins(0, 0, 0, 0);
    m_otaMagicEdit = new QLineEdit("OTA1", this);
    m_otaMagicEdit->setMaxLength(4);
    m_otaBlockSizeSpin = new QSpinBox(this);
    m_otaBlockSizeSpin->setRange(1, 4096);
    m_otaBlockSizeSpin->setValue(256);
    m_otaBlockSizeSpin->setSuffix(tr(" B"));
    m_otaIntervalSpin = new QSpinBox(this);
    m_otaIntervalSpin->setRange(0, 60000);
    m_otaIntervalSpin->setValue(10);
    m_otaIntervalSpin->setSuffix(tr(" ms"));
    m_otaWaitAckCheck = new QCheckBox(tr("等待 ACK"), this);
    m_otaAckEdit = new QLineEdit("ACK", this);
    m_otaTimeoutSpin = new QSpinBox(this);
    m_otaTimeoutSpin->setRange(1, 60000);
    m_otaTimeoutSpin->setValue(1000);
    m_otaTimeoutSpin->setSuffix(tr(" ms"));
    m_otaRetrySpin = new QSpinBox(this);
    m_otaRetrySpin->setRange(0, 100);
    m_otaRetrySpin->setValue(3);
    otaLayout->addWidget(new QLabel(tr("Magic:"), this), 0, 0);
    otaLayout->addWidget(m_otaMagicEdit, 0, 1);
    otaLayout->addWidget(new QLabel(tr("每包字节:"), this), 0, 2);
    otaLayout->addWidget(m_otaBlockSizeSpin, 0, 3);
    otaLayout->addWidget(new QLabel(tr("包间隔:"), this), 1, 0);
    otaLayout->addWidget(m_otaIntervalSpin, 1, 1);
    otaLayout->addWidget(m_otaWaitAckCheck, 1, 2);
    otaLayout->addWidget(m_otaAckEdit, 1, 3);
    otaLayout->addWidget(new QLabel(tr("ACK超时:"), this), 2, 0);
    otaLayout->addWidget(m_otaTimeoutSpin, 2, 1);
    otaLayout->addWidget(new QLabel(tr("重试次数:"), this), 2, 2);
    otaLayout->addWidget(m_otaRetrySpin, 2, 3);
    otaLayout->setColumnStretch(4, 1);

    m_optionsStack = new QStackedWidget(this);
    m_optionsStack->addWidget(m_rawOptionsWidget);
    m_optionsStack->addWidget(m_standardOptionsWidget);
    m_optionsStack->addWidget(m_otaOptionsWidget);
    mainLayout->addWidget(m_optionsStack);

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

    // 文件信息：选择文件后立即展示，发送前让用户确认大小、CRC 与预计耗时。
    m_fileInfoGroup = new QGroupBox(tr("文件信息"), this);
    auto* fileInfoLayout = new QGridLayout(m_fileInfoGroup);
    m_fileNameValueLabel = new QLabel("--", this);
    m_fileSizeValueLabel = new QLabel("--", this);
    m_crc32ValueLabel = new QLabel("--", this);
    m_blockCountValueLabel = new QLabel("--", this);
    m_estimatedValueLabel = new QLabel("--", this);
    fileInfoLayout->addWidget(new QLabel(tr("文件名:"), this), 0, 0);
    fileInfoLayout->addWidget(m_fileNameValueLabel, 0, 1);
    fileInfoLayout->addWidget(new QLabel(tr("大小:"), this), 0, 2);
    fileInfoLayout->addWidget(m_fileSizeValueLabel, 0, 3);
    fileInfoLayout->addWidget(new QLabel(tr("CRC32:"), this), 1, 0);
    fileInfoLayout->addWidget(m_crc32ValueLabel, 1, 1);
    fileInfoLayout->addWidget(new QLabel(tr("分块数:"), this), 1, 2);
    fileInfoLayout->addWidget(m_blockCountValueLabel, 1, 3);
    fileInfoLayout->addWidget(new QLabel(tr("预计耗时:"), this), 2, 0);
    fileInfoLayout->addWidget(m_estimatedValueLabel, 2, 1, 1, 3);
    mainLayout->addWidget(m_fileInfoGroup);

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
    m_pauseBtn = new QPushButton(tr("暂停"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_pauseBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);

    connect(m_startBtn, &QPushButton::clicked, this, &FileTransferDialog::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &FileTransferDialog::onPauseClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &FileTransferDialog::onCancelClicked);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_startBtn);
    buttonLayout->addWidget(m_pauseBtn);
    buttonLayout->addWidget(m_cancelBtn);
    mainLayout->addLayout(buttonLayout);

    // 连接方向切换信号
    connect(m_sendRadio, &QRadioButton::toggled, this, [this](bool checked) {
        Q_UNUSED(checked)
        updateUI();
    });
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FileTransferDialog::onModeChanged);
    connect(m_filePathEdit, &QLineEdit::textChanged,
            this, &FileTransferDialog::onFilePathChanged);
    connect(m_rawBlockSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateEstimatedInfo(); });
    connect(m_rawIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateEstimatedInfo(); });
    connect(m_otaBlockSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateEstimatedInfo(); });
    connect(m_otaIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { updateEstimatedInfo(); });

    updateUI();
    resetFileInfo();
}

void FileTransferDialog::updateUI()
{
    /*
     * 裸流和 OTA 第一版只做发送，不做接收；标准协议保留接收模式，
     * 以兼容已有 XMODEM/YMODEM 使用方式。
     */
    const DialogTransferMode mode = currentMode();
    const bool standardMode = (mode == DialogTransferMode::StandardProtocol);
    const bool forceSendOnly = !standardMode || m_iapMode;

    if (forceSendOnly) {
        m_sendRadio->setChecked(true);
    }

    m_receiveRadio->setEnabled(!forceSendOnly);
    m_directionGroup->setVisible(standardMode || m_iapMode);
    m_optionsStack->setCurrentIndex(m_modeCombo->currentIndex());

    const bool isSend = m_sendRadio->isChecked();
    m_filePathEdit->setPlaceholderText(isSend ?
        tr("选择要发送的文件") : tr("选择保存位置"));
    updateEstimatedInfo();
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
    /*
     * 文件发送会持续占用当前串口，启动前先检查连接状态和文件路径，
     * 避免用户误以为已经发送但实际没有任何底层通信对象。
     */
    if (!m_connected) {
        QMessageBox::warning(this, tr("警告"), tr("请先打开当前连接后再发送文件。"));
        return;
    }

    QString filePath = m_filePathEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请先选择文件"));
        return;
    }

    // 创建传输对象
    if (m_transfer) {
        delete m_transfer;
    }

    const DialogTransferMode mode = currentMode();
    if (mode == DialogTransferMode::RawStream) {
        auto* rawTransfer = new RawFileTransfer(this);
        RawTransferOptions options;
        options.blockSize = m_rawBlockSizeSpin->value();
        options.intervalMs = m_rawIntervalSpin->value();
        rawTransfer->setOptions(options);
        m_transfer = rawTransfer;
    } else if (mode == DialogTransferMode::CustomOta) {
        auto* otaTransfer = new OtaFileTransfer(this);
        OtaTransferOptions options;
        options.magic = m_otaMagicEdit->text();
        options.blockSize = m_otaBlockSizeSpin->value();
        options.intervalMs = m_otaIntervalSpin->value();
        options.waitAck = m_otaWaitAckCheck->isChecked();
        options.ackToken = m_otaAckEdit->text().toUtf8();
        options.timeoutMs = m_otaTimeoutSpin->value();
        options.maxRetries = m_otaRetrySpin->value();
        otaTransfer->setOptions(options);
        m_transfer = otaTransfer;
    } else {
        TransferProtocol protocol = (TransferProtocol)m_protocolCombo->currentData().toInt();
        m_transfer = FileTransferFactory::create(protocol, this);
    }

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
    if (m_sendRadio->isChecked() || mode != DialogTransferMode::StandardProtocol) {
        started = m_transfer->startSend(filePath);
    } else {
        started = m_transfer->startReceive(filePath);
    }

    if (started) {
        m_transferPaused = false;
        setTransferControlsEnabled(false);
        m_startBtn->setEnabled(false);
        m_pauseBtn->setEnabled(mode != DialogTransferMode::StandardProtocol);
        m_pauseBtn->setText(tr("暂停"));
        m_cancelBtn->setEnabled(true);

        appendLog(tr("传输开始..."));
    } else {
        QMessageBox::critical(this, tr("错误"), tr("启动传输失败"));
    }
}

void FileTransferDialog::onPauseClicked()
{
    /*
     * 暂停/继续只对定时分块类协议有效；XMODEM/YMODEM 由对端 ACK 驱动，
     * 中途暂停容易破坏握手，因此标准协议不启用这个按钮。
     */
    if (!m_transfer) {
        return;
    }

    if (auto* rawTransfer = qobject_cast<RawFileTransfer*>(m_transfer)) {
        if (m_transferPaused) {
            rawTransfer->resume();
            m_transferPaused = false;
            m_pauseBtn->setText(tr("暂停"));
            appendLog(tr("继续裸流发送"));
        } else {
            rawTransfer->pause();
            m_transferPaused = true;
            m_pauseBtn->setText(tr("继续"));
            appendLog(tr("暂停裸流发送"));
        }
    } else if (auto* otaTransfer = qobject_cast<OtaFileTransfer*>(m_transfer)) {
        if (m_transferPaused) {
            otaTransfer->resume();
            m_transferPaused = false;
            m_pauseBtn->setText(tr("暂停"));
            appendLog(tr("继续 OTA 发送"));
        } else {
            otaTransfer->pause();
            m_transferPaused = true;
            m_pauseBtn->setText(tr("继续"));
            appendLog(tr("暂停 OTA 发送"));
        }
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
    m_pauseBtn->setEnabled(false);
    m_pauseBtn->setText(tr("暂停"));
    m_cancelBtn->setEnabled(false);
    m_transferPaused = false;
    setTransferControlsEnabled(true);
    updateUI();

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

void FileTransferDialog::onModeChanged(int index)
{
    Q_UNUSED(index)
    updateUI();
    updateFileInfo();
}

void FileTransferDialog::onFilePathChanged(const QString& path)
{
    Q_UNUSED(path)
    updateFileInfo();
}

FileTransferDialog::DialogTransferMode FileTransferDialog::currentMode() const
{
    if (!m_modeCombo) {
        return DialogTransferMode::RawStream;
    }
    return static_cast<DialogTransferMode>(m_modeCombo->currentData().toInt());
}

int FileTransferDialog::currentBlockSize() const
{
    /*
     * 标准协议的块大小由协议决定：XMODEM/XMODEM-CRC 为 128，
     * XMODEM-1K/YMODEM 为 1024。裸流和 OTA 则使用用户配置。
     */
    const DialogTransferMode mode = currentMode();
    if (mode == DialogTransferMode::RawStream) {
        return m_rawBlockSizeSpin ? m_rawBlockSizeSpin->value() : 256;
    }
    if (mode == DialogTransferMode::CustomOta) {
        return m_otaBlockSizeSpin ? m_otaBlockSizeSpin->value() : 256;
    }

    const TransferProtocol protocol =
        static_cast<TransferProtocol>(m_protocolCombo->currentData().toInt());
    return (protocol == TransferProtocol::XModem1K || protocol == TransferProtocol::YModem)
        ? 1024
        : 128;
}

int FileTransferDialog::currentIntervalMs() const
{
    const DialogTransferMode mode = currentMode();
    if (mode == DialogTransferMode::RawStream) {
        return m_rawIntervalSpin ? m_rawIntervalSpin->value() : 0;
    }
    if (mode == DialogTransferMode::CustomOta) {
        return m_otaIntervalSpin ? m_otaIntervalSpin->value() : 0;
    }
    return 0;
}

void FileTransferDialog::updateFileInfo()
{
    /*
     * 接收模式没有本地源文件可计算大小和 CRC，因此只在发送模式下展示
     * 文件元信息。读取失败不弹窗，避免用户输入路径过程中频繁打断。
     */
    const QString path = m_filePathEdit->text().trimmed();
    if (path.isEmpty() || (currentMode() == DialogTransferMode::StandardProtocol && !m_sendRadio->isChecked())) {
        resetFileInfo();
        return;
    }

    const QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        resetFileInfo();
        return;
    }

    m_selectedFileSize = fileInfo.size();
    m_fileNameValueLabel->setText(fileInfo.fileName());
    m_fileSizeValueLabel->setText(tr("%1 B").arg(m_selectedFileSize));

    QString errorMessage;
    if (calculateSelectedFileCrc32(m_selectedFileCrc32, errorMessage)) {
        m_crc32ValueLabel->setText(QString("0x%1")
            .arg(m_selectedFileCrc32, 8, 16, QChar('0')).toUpper());
    } else {
        m_crc32ValueLabel->setText(tr("计算失败"));
        appendLog(tr("CRC32计算失败: %1").arg(errorMessage));
    }

    updateEstimatedInfo();
}

void FileTransferDialog::resetFileInfo()
{
    m_selectedFileSize = 0;
    m_selectedFileCrc32 = 0;
    if (m_fileNameValueLabel) m_fileNameValueLabel->setText("--");
    if (m_fileSizeValueLabel) m_fileSizeValueLabel->setText("--");
    if (m_crc32ValueLabel) m_crc32ValueLabel->setText("--");
    if (m_blockCountValueLabel) m_blockCountValueLabel->setText("--");
    if (m_estimatedValueLabel) m_estimatedValueLabel->setText("--");
}

void FileTransferDialog::updateEstimatedInfo()
{
    if (m_selectedFileSize <= 0 || !m_blockCountValueLabel || !m_estimatedValueLabel) {
        return;
    }

    const int blockSize = qMax(1, currentBlockSize());
    const int blockCount = static_cast<int>((m_selectedFileSize + blockSize - 1) / blockSize);
    const int intervalMs = currentIntervalMs();
    const qint64 estimatedMs = static_cast<qint64>(qMax(0, blockCount - 1)) * intervalMs;

    m_blockCountValueLabel->setText(QString::number(blockCount));
    if (estimatedMs <= 0) {
        m_estimatedValueLabel->setText(tr("由串口速率和设备处理速度决定"));
    } else {
        m_estimatedValueLabel->setText(tr("约 %1 秒（仅按包间隔估算）")
            .arg(estimatedMs / 1000.0, 0, 'f', 2));
    }
}

bool FileTransferDialog::calculateSelectedFileCrc32(quint32& crc32, QString& errorMessage) const
{
    /*
     * 对话框层计算 CRC32 只用于预览，真正 OTA 发送器启动时还会重新
     * 流式计算一次，避免用户在预览后替换文件导致元信息过期。
     */
    QFile file(m_filePathEdit->text().trimmed());
    if (!file.open(QIODevice::ReadOnly)) {
        errorMessage = file.errorString();
        return false;
    }

    quint32 runningCrc = 0xFFFFFFFF;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(64 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            errorMessage = file.errorString();
            return false;
        }

        for (char ch : chunk) {
            runningCrc ^= static_cast<quint8>(ch);
            for (int bit = 0; bit < 8; ++bit) {
                if (runningCrc & 1U) {
                    runningCrc = (runningCrc >> 1) ^ 0xEDB88320U;
                } else {
                    runningCrc >>= 1;
                }
            }
        }
    }

    crc32 = runningCrc ^ 0xFFFFFFFFU;
    return true;
}

void FileTransferDialog::setTransferControlsEnabled(bool enabled)
{
    if (m_modeCombo) m_modeCombo->setEnabled(enabled);
    if (m_protocolCombo) m_protocolCombo->setEnabled(enabled);
    if (m_sendRadio) m_sendRadio->setEnabled(enabled);
    if (m_receiveRadio) m_receiveRadio->setEnabled(enabled);
    if (m_browseBtn) m_browseBtn->setEnabled(enabled);
    if (m_filePathEdit) m_filePathEdit->setEnabled(enabled);
    if (m_rawOptionsWidget) m_rawOptionsWidget->setEnabled(enabled);
    if (m_otaOptionsWidget) m_otaOptionsWidget->setEnabled(enabled);
    if (m_standardOptionsWidget) m_standardOptionsWidget->setEnabled(enabled);
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
        if (m_modeCombo) {
            const int otaIndex = m_modeCombo->findData(static_cast<int>(DialogTransferMode::CustomOta));
            if (otaIndex >= 0) {
                m_modeCombo->setCurrentIndex(otaIndex);
            }
        }
        m_sendRadio->setChecked(true);
        m_receiveRadio->setEnabled(false);
        m_filePathEdit->setPlaceholderText(tr("选择固件文件 (.hex/.bin)"));
    } else {
        setWindowTitle(tr("文件传输"));
        m_receiveRadio->setEnabled(true);
    }
    updateUI();
}

void FileTransferDialog::notifyLocalSendResult(bool success, const QString& errorMessage)
{
    /*
     * 文件传输对象只知道自己发出了一个协议包，不知道主窗口发送队列是否
     * 接受。该桥接函数由 MainWindow 在 onSendData() 后回调，保证 Raw/OTA
     * 不会早于本地发送结果读取下一块。
     */
    if (m_transfer) {
        m_transfer->notifyLocalSendResult(success, errorMessage);
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
