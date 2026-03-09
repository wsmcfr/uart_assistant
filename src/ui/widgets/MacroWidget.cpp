/**
 * @file MacroWidget.cpp
 * @brief 宏录制回放界面组件实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "MacroWidget.h"
#include "core/macro/MacroRecorder.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>

namespace ComAssistant {

MacroWidget::MacroWidget(QWidget* parent)
    : QWidget(parent)
{
    m_recorder = new MacroRecorder(this);
    m_player = new MacroPlayer(this);
    m_currentMacro = new MacroData();

    setupUi();

    // 连接录制器信号
    connect(m_recorder, &MacroRecorder::recordingStateChanged,
            this, &MacroWidget::onRecordingStateChanged);
    connect(m_recorder, &MacroRecorder::eventRecorded,
            this, &MacroWidget::onEventRecorded);

    // 连接播放器信号
    connect(m_player, &MacroPlayer::sendData,
            this, &MacroWidget::sendData);
    connect(m_player, &MacroPlayer::playbackStateChanged,
            this, &MacroWidget::onPlaybackStateChanged);
    connect(m_player, &MacroPlayer::progressUpdated,
            this, &MacroWidget::onProgressUpdated);
    connect(m_player, &MacroPlayer::playbackFinished,
            this, &MacroWidget::onPlaybackFinished);
    connect(m_player, &MacroPlayer::currentEventChanged,
            this, &MacroWidget::onCurrentEventChanged);

    // 连接宏管理器信号
    connect(MacroManager::instance(), &MacroManager::macroListChanged,
            this, &MacroWidget::onRefreshList);

    // 录制计时器
    m_recordTimer = new QTimer(this);
    connect(m_recordTimer, &QTimer::timeout, this, &MacroWidget::updateRecordingTime);

    loadMacroList();
    updateButtonStates();
}

MacroWidget::~MacroWidget()
{
    delete m_currentMacro;
}

void MacroWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // ===== 录制控件组 =====
    m_recordGroup = new QGroupBox(tr("录制"), this);
    auto* recordLayout = new QGridLayout(m_recordGroup);

    m_recordBtn = new QPushButton(tr("开始录制"), this);
    m_recordBtn->setIcon(QIcon(":/icons/record.png"));
    connect(m_recordBtn, &QPushButton::clicked, this, &MacroWidget::onRecordClicked);
    recordLayout->addWidget(m_recordBtn, 0, 0);

    m_stopRecordBtn = new QPushButton(tr("停止录制"), this);
    m_stopRecordBtn->setIcon(QIcon(":/icons/stop.png"));
    m_stopRecordBtn->setEnabled(false);
    connect(m_stopRecordBtn, &QPushButton::clicked, this, &MacroWidget::onStopRecordClicked);
    recordLayout->addWidget(m_stopRecordBtn, 0, 1);

    m_recordReceiveCheck = new QCheckBox(tr("录制接收数据"), this);
    recordLayout->addWidget(m_recordReceiveCheck, 1, 0, 1, 2);

    m_recordTimeLabel = new QLabel(tr("时长: 00:00"), this);
    recordLayout->addWidget(m_recordTimeLabel, 2, 0);

    m_eventCountLabel = new QLabel(tr("事件: 0"), this);
    recordLayout->addWidget(m_eventCountLabel, 2, 1);

    mainLayout->addWidget(m_recordGroup);

    // ===== 宏列表组 =====
    m_macroListGroup = new QGroupBox(tr("宏列表"), this);
    auto* listLayout = new QGridLayout(m_macroListGroup);

    m_macroCombo = new QComboBox(this);
    m_macroCombo->setMinimumWidth(150);
    connect(m_macroCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MacroWidget::onMacroSelected);
    listLayout->addWidget(m_macroCombo, 0, 0, 1, 4);

    m_refreshBtn = new QPushButton(tr("刷新"), this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MacroWidget::onRefreshList);
    listLayout->addWidget(m_refreshBtn, 1, 0);

    m_deleteBtn = new QPushButton(tr("删除"), this);
    connect(m_deleteBtn, &QPushButton::clicked, this, &MacroWidget::onDeleteClicked);
    listLayout->addWidget(m_deleteBtn, 1, 1);

    m_importBtn = new QPushButton(tr("导入"), this);
    connect(m_importBtn, &QPushButton::clicked, this, &MacroWidget::onImportClicked);
    listLayout->addWidget(m_importBtn, 1, 2);

    m_exportBtn = new QPushButton(tr("导出"), this);
    connect(m_exportBtn, &QPushButton::clicked, this, &MacroWidget::onExportClicked);
    listLayout->addWidget(m_exportBtn, 1, 3);

    mainLayout->addWidget(m_macroListGroup);

    // ===== 播放控件组 =====
    m_playGroup = new QGroupBox(tr("播放"), this);
    auto* playLayout = new QGridLayout(m_playGroup);

    m_playBtn = new QPushButton(tr("播放"), this);
    m_playBtn->setIcon(QIcon(":/icons/play.png"));
    connect(m_playBtn, &QPushButton::clicked, this, &MacroWidget::onPlayClicked);
    playLayout->addWidget(m_playBtn, 0, 0);

    m_pauseBtn = new QPushButton(tr("暂停"), this);
    m_pauseBtn->setIcon(QIcon(":/icons/pause.png"));
    m_pauseBtn->setEnabled(false);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MacroWidget::onPauseClicked);
    playLayout->addWidget(m_pauseBtn, 0, 1);

    m_stopPlayBtn = new QPushButton(tr("停止"), this);
    m_stopPlayBtn->setIcon(QIcon(":/icons/stop.png"));
    m_stopPlayBtn->setEnabled(false);
    connect(m_stopPlayBtn, &QPushButton::clicked, this, &MacroWidget::onStopPlayClicked);
    playLayout->addWidget(m_stopPlayBtn, 0, 2);

    m_loopCheck = new QCheckBox(tr("循环"), this);
    playLayout->addWidget(m_loopCheck, 1, 0);

    m_loopCountLabel = new QLabel(tr("次数:"), this);
    playLayout->addWidget(m_loopCountLabel, 1, 1);
    m_loopCountSpin = new QSpinBox(this);
    m_loopCountSpin->setRange(0, 9999);
    m_loopCountSpin->setValue(1);
    m_loopCountSpin->setSpecialValueText(tr("无限"));
    playLayout->addWidget(m_loopCountSpin, 1, 2);

    m_speedLabel = new QLabel(tr("速度:"), this);
    playLayout->addWidget(m_speedLabel, 2, 0);
    m_speedSpin = new QDoubleSpinBox(this);
    m_speedSpin->setRange(0.1, 10.0);
    m_speedSpin->setValue(1.0);
    m_speedSpin->setSingleStep(0.1);
    m_speedSpin->setSuffix("x");
    playLayout->addWidget(m_speedSpin, 2, 1, 1, 2);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    playLayout->addWidget(m_progressBar, 3, 0, 1, 3);

    m_statusLabel = new QLabel(tr("就绪"), this);
    playLayout->addWidget(m_statusLabel, 4, 0, 1, 3);

    mainLayout->addWidget(m_playGroup);

    // ===== 事件列表 =====
    m_eventList = new QListWidget(this);
    m_eventList->setMaximumHeight(150);
    mainLayout->addWidget(m_eventList);

    mainLayout->addStretch();
}

void MacroWidget::loadMacroList()
{
    m_macroCombo->clear();
    m_macroCombo->addItem(tr("-- 选择宏 --"), QString());

    QStringList macros = MacroManager::instance()->availableMacros();
    for (const auto& name : macros) {
        m_macroCombo->addItem(name, name);
    }
}

void MacroWidget::updateButtonStates()
{
    bool isRecording = m_recorder->isRecording();
    bool isPlaying = m_player->isPlaying();
    bool isPaused = m_player->isPaused();
    bool hasMacro = m_macroCombo->currentIndex() > 0;

    m_recordBtn->setEnabled(!isRecording && !isPlaying);
    m_stopRecordBtn->setEnabled(isRecording);
    m_recordReceiveCheck->setEnabled(!isRecording);

    m_macroCombo->setEnabled(!isRecording && !isPlaying);
    m_deleteBtn->setEnabled(hasMacro && !isPlaying);
    m_exportBtn->setEnabled(hasMacro);

    m_playBtn->setEnabled(hasMacro && !isRecording && !isPlaying);
    m_pauseBtn->setEnabled(isPlaying);
    m_pauseBtn->setText(isPaused ? tr("继续") : tr("暂停"));
    m_stopPlayBtn->setEnabled(isPlaying);

    m_loopCheck->setEnabled(!isPlaying);
    m_loopCountSpin->setEnabled(!isPlaying && m_loopCheck->isChecked());
    m_speedSpin->setEnabled(!isPlaying);
}

void MacroWidget::recordSendEvent(const QByteArray& data, bool isHex)
{
    m_recorder->recordSend(data, isHex);
}

void MacroWidget::recordReceiveEvent(const QByteArray& data)
{
    m_recorder->recordReceive(data);
}

void MacroWidget::onRecordClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("新建宏"),
        tr("请输入宏名称:"), QLineEdit::Normal, QString(), &ok);

    if (!ok) {
        return;
    }

    if (name.isEmpty()) {
        name = tr("宏_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    }

    m_recorder->setRecordReceive(m_recordReceiveCheck->isChecked());
    m_recorder->startRecording(name);

    m_eventList->clear();
    m_recordTimer->start(1000);
}

void MacroWidget::onStopRecordClicked()
{
    m_recordTimer->stop();

    MacroData macro = m_recorder->stopRecording();
    if (macro.events.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("没有录制到任何事件"));
        return;
    }

    // 保存宏
    if (MacroManager::instance()->saveMacro(macro)) {
        QMessageBox::information(this, tr("成功"),
            tr("宏 \"%1\" 已保存，共 %2 个事件")
            .arg(macro.name).arg(macro.events.size()));
        loadMacroList();

        // 选中新创建的宏
        int index = m_macroCombo->findData(macro.name);
        if (index >= 0) {
            m_macroCombo->setCurrentIndex(index);
        }
    } else {
        QMessageBox::warning(this, tr("错误"), tr("保存宏失败"));
    }
}

void MacroWidget::onPlayClicked()
{
    if (m_currentMacro->events.isEmpty()) {
        return;
    }

    m_player->setLoopEnabled(m_loopCheck->isChecked());
    m_player->setLoopCount(m_loopCountSpin->value());
    m_player->setSpeedMultiplier(m_speedSpin->value());

    m_player->loadMacro(*m_currentMacro);
    m_player->startPlayback();
}

void MacroWidget::onPauseClicked()
{
    if (m_player->isPaused()) {
        m_player->resumePlayback();
    } else {
        m_player->pausePlayback();
    }
}

void MacroWidget::onStopPlayClicked()
{
    m_player->stopPlayback();
}

void MacroWidget::onMacroSelected(int index)
{
    m_eventList->clear();
    m_currentMacro->events.clear();

    if (index <= 0) {
        updateButtonStates();
        return;
    }

    QString name = m_macroCombo->currentData().toString();
    if (MacroManager::instance()->loadMacro(name, *m_currentMacro)) {
        // 显示事件列表
        for (const auto& event : m_currentMacro->events) {
            m_eventList->addItem(formatEvent(event));
        }
        m_statusLabel->setText(tr("已加载: %1 个事件").arg(m_currentMacro->events.size()));
    } else {
        QMessageBox::warning(this, tr("错误"), tr("加载宏失败"));
    }

    updateButtonStates();
}

void MacroWidget::onDeleteClicked()
{
    QString name = m_macroCombo->currentData().toString();
    if (name.isEmpty()) {
        return;
    }

    int ret = QMessageBox::question(this, tr("确认删除"),
        tr("确定要删除宏 \"%1\" 吗?").arg(name));

    if (ret == QMessageBox::Yes) {
        MacroManager::instance()->deleteMacro(name);
        loadMacroList();
    }
}

void MacroWidget::onImportClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        tr("导入宏"), QString(), tr("宏文件 (*.macro);;所有文件 (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    if (MacroManager::instance()->importMacro(filePath)) {
        loadMacroList();
        QMessageBox::information(this, tr("成功"), tr("宏导入成功"));
    } else {
        QMessageBox::warning(this, tr("错误"), tr("导入宏失败"));
    }
}

void MacroWidget::onExportClicked()
{
    QString name = m_macroCombo->currentData().toString();
    if (name.isEmpty()) {
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this,
        tr("导出宏"), name + ".macro", tr("宏文件 (*.macro);;所有文件 (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    if (MacroManager::instance()->exportMacro(name, filePath)) {
        QMessageBox::information(this, tr("成功"), tr("宏导出成功"));
    } else {
        QMessageBox::warning(this, tr("错误"), tr("导出宏失败"));
    }
}

void MacroWidget::onRefreshList()
{
    loadMacroList();
}

void MacroWidget::onRecordingStateChanged(bool recording)
{
    updateButtonStates();
    if (recording) {
        m_recordTimeLabel->setText(tr("时长: 00:00"));
        m_eventCountLabel->setText(tr("事件: 0"));
    }
}

void MacroWidget::onEventRecorded(const MacroEvent& event)
{
    m_eventList->addItem(formatEvent(event));
    m_eventList->scrollToBottom();
    m_eventCountLabel->setText(tr("事件: %1").arg(m_recorder->eventCount()));
}

void MacroWidget::onPlaybackStateChanged(bool playing, bool paused)
{
    Q_UNUSED(playing)
    Q_UNUSED(paused)
    updateButtonStates();
}

void MacroWidget::onProgressUpdated(int current, int total, int loop)
{
    int percent = total > 0 ? (current * 100 / total) : 0;
    m_progressBar->setValue(percent);

    if (m_loopCheck->isChecked()) {
        m_statusLabel->setText(tr("播放中: %1/%2 (循环 %3)").arg(current).arg(total).arg(loop));
    } else {
        m_statusLabel->setText(tr("播放中: %1/%2").arg(current).arg(total));
    }
}

void MacroWidget::onPlaybackFinished()
{
    m_progressBar->setValue(100);
    m_statusLabel->setText(tr("播放完成"));
    updateButtonStates();
}

void MacroWidget::onCurrentEventChanged(const MacroEvent& event)
{
    Q_UNUSED(event)
    // 可以高亮当前事件
}

void MacroWidget::updateRecordingTime()
{
    qint64 duration = m_recorder->recordingDuration();
    int seconds = static_cast<int>(duration / 1000);
    int minutes = seconds / 60;
    seconds %= 60;
    m_recordTimeLabel->setText(tr("时长: %1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0')));
}

QString MacroWidget::formatEvent(const MacroEvent& event) const
{
    QString typeStr;
    QString dataStr;

    switch (event.type) {
    case MacroEventType::Send:
        typeStr = tr("[发送]");
        if (event.isHex) {
            dataStr = event.data.toHex(' ').toUpper();
        } else {
            dataStr = QString::fromUtf8(event.data);
        }
        break;

    case MacroEventType::Receive:
        typeStr = tr("[接收]");
        dataStr = event.data.toHex(' ').toUpper();
        break;

    case MacroEventType::Delay:
        typeStr = tr("[延时]");
        dataStr = tr("%1 ms").arg(event.delayMs);
        break;

    case MacroEventType::Comment:
        typeStr = tr("[注释]");
        dataStr = event.comment;
        break;
    }

    // 截断过长的数据
    if (dataStr.length() > 50) {
        dataStr = dataStr.left(47) + "...";
    }

    return QString("%1 %2").arg(typeStr, dataStr);
}

void MacroWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void MacroWidget::retranslateUi()
{
    // 录制组
    m_recordGroup->setTitle(tr("录制"));
    m_recordBtn->setText(tr("开始录制"));
    m_stopRecordBtn->setText(tr("停止录制"));
    m_recordReceiveCheck->setText(tr("录制接收数据"));

    // 宏列表组
    m_macroListGroup->setTitle(tr("宏列表"));
    m_refreshBtn->setText(tr("刷新"));
    m_deleteBtn->setText(tr("删除"));
    m_importBtn->setText(tr("导入"));
    m_exportBtn->setText(tr("导出"));

    // 更新宏下拉框第一项
    if (m_macroCombo->count() > 0) {
        m_macroCombo->setItemText(0, tr("-- 选择宏 --"));
    }

    // 播放组
    m_playGroup->setTitle(tr("播放"));
    m_playBtn->setText(tr("播放"));
    // m_pauseBtn在updateButtonStates中根据状态设置
    updateButtonStates();
    m_stopPlayBtn->setText(tr("停止"));
    m_loopCheck->setText(tr("循环"));
    m_loopCountLabel->setText(tr("次数:"));
    m_loopCountSpin->setSpecialValueText(tr("无限"));
    m_speedLabel->setText(tr("速度:"));
    m_statusLabel->setText(tr("就绪"));
}

} // namespace ComAssistant
