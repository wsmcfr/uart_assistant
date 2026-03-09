/**
 * @file SettingsDialog.cpp
 * @brief 设置对话框实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "SettingsDialog.h"
#include "core/config/ConfigManager.h"
#include "core/session/SessionManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QEvent>
#include <QSettings>

namespace ComAssistant {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi()
{
    setWindowTitle(tr("设置"));
    setMinimumSize(500, 450);
    resize(550, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 标签页
    m_tabWidget = new QTabWidget;
    setupGeneralTab();
    setupSerialTab();
    setupDisplayTab();
    mainLayout->addWidget(m_tabWidget);

    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    m_resetBtn = new QPushButton(tr("恢复默认"));
    m_okBtn = new QPushButton(tr("确定"));
    m_cancelBtn = new QPushButton(tr("取消"));
    m_applyBtn = new QPushButton(tr("应用"));

    m_okBtn->setDefault(true);

    buttonLayout->addWidget(m_resetBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okBtn);
    buttonLayout->addWidget(m_cancelBtn);
    buttonLayout->addWidget(m_applyBtn);

    mainLayout->addLayout(buttonLayout);

    // 连接信号
    connect(m_okBtn, &QPushButton::clicked, this, &SettingsDialog::onOkClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    connect(m_applyBtn, &QPushButton::clicked, this, &SettingsDialog::onApplyClicked);
    connect(m_resetBtn, &QPushButton::clicked, this, &SettingsDialog::onResetClicked);
}

void SettingsDialog::setupGeneralTab()
{
    m_generalTab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_generalTab);

    // 自动保存组
    m_autoSaveGroup = new QGroupBox(tr("自动保存"));
    m_autoSaveLayout = new QFormLayout(m_autoSaveGroup);

    m_autoSaveCheck = new QCheckBox(tr("启用自动保存"));
    m_autoSaveLayout->addRow(m_autoSaveCheck);

    m_autoSaveIntervalSpin = new QSpinBox;
    m_autoSaveIntervalSpin->setRange(10, 600);
    m_autoSaveIntervalSpin->setSuffix(tr(" 秒"));
    m_autoSaveIntervalSpin->setValue(30);
    m_autoSaveLayout->addRow(tr("保存间隔:"), m_autoSaveIntervalSpin);

    m_checkRecoveryCheck = new QCheckBox(tr("启动时检查恢复数据"));
    m_autoSaveLayout->addRow(m_checkRecoveryCheck);

    layout->addWidget(m_autoSaveGroup);

    // 窗口组
    m_windowGroup = new QGroupBox(tr("窗口"));
    QFormLayout* windowLayout = new QFormLayout(m_windowGroup);

    m_rememberWindowCheck = new QCheckBox(tr("记住窗口位置和大小"));
    windowLayout->addRow(m_rememberWindowCheck);

    layout->addWidget(m_windowGroup);

    // 语言组
    m_langGroup = new QGroupBox(tr("语言"));
    m_langLayout = new QFormLayout(m_langGroup);

    m_languageCombo = new QComboBox;
    m_languageCombo->addItem(tr("简体中文"), "zh_CN");
    m_languageCombo->addItem(tr("English"), "en_US");
    m_langLayout->addRow(tr("界面语言:"), m_languageCombo);

    layout->addWidget(m_langGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_generalTab, tr("常规"));
}

void SettingsDialog::setupSerialTab()
{
    m_serialTab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_serialTab);

    // 默认参数组
    m_defaultSerialGroup = new QGroupBox(tr("默认串口参数"));
    m_defaultSerialLayout = new QFormLayout(m_defaultSerialGroup);

    m_defaultBaudCombo = new QComboBox;
    m_defaultBaudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_defaultBaudCombo->setCurrentText("115200");
    m_defaultSerialLayout->addRow(tr("波特率:"), m_defaultBaudCombo);

    m_defaultDataBitsCombo = new QComboBox;
    m_defaultDataBitsCombo->addItems({"5", "6", "7", "8"});
    m_defaultDataBitsCombo->setCurrentText("8");
    m_defaultSerialLayout->addRow(tr("数据位:"), m_defaultDataBitsCombo);

    m_defaultParityCombo = new QComboBox;
    m_defaultParityCombo->addItem(tr("无"), static_cast<int>(Parity::None));
    m_defaultParityCombo->addItem(tr("奇校验"), static_cast<int>(Parity::Odd));
    m_defaultParityCombo->addItem(tr("偶校验"), static_cast<int>(Parity::Even));
    m_defaultSerialLayout->addRow(tr("校验位:"), m_defaultParityCombo);

    m_defaultStopBitsCombo = new QComboBox;
    m_defaultStopBitsCombo->addItem("1", static_cast<int>(StopBits::One));
    m_defaultStopBitsCombo->addItem("1.5", static_cast<int>(StopBits::OnePointFive));
    m_defaultStopBitsCombo->addItem("2", static_cast<int>(StopBits::Two));
    m_defaultSerialLayout->addRow(tr("停止位:"), m_defaultStopBitsCombo);

    layout->addWidget(m_defaultSerialGroup);

    // 重连组
    m_reconnectGroup = new QGroupBox(tr("自动重连"));
    m_reconnectLayout = new QFormLayout(m_reconnectGroup);

    m_autoReconnectCheck = new QCheckBox(tr("断开时自动重连"));
    m_reconnectLayout->addRow(m_autoReconnectCheck);

    m_reconnectIntervalSpin = new QSpinBox;
    m_reconnectIntervalSpin->setRange(1, 60);
    m_reconnectIntervalSpin->setSuffix(tr(" 秒"));
    m_reconnectIntervalSpin->setValue(3);
    m_reconnectLayout->addRow(tr("重连间隔:"), m_reconnectIntervalSpin);

    layout->addWidget(m_reconnectGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_serialTab, tr("串口"));
}

void SettingsDialog::setupDisplayTab()
{
    m_displayTab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_displayTab);

    // 字体组
    m_fontGroup = new QGroupBox(tr("字体"));
    m_fontLayout = new QFormLayout(m_fontGroup);

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setCurrentFont(QFont("Consolas"));
    m_fontLayout->addRow(tr("字体:"), m_fontCombo);

    m_fontSizeSpin = new QSpinBox;
    m_fontSizeSpin->setRange(8, 24);
    m_fontSizeSpin->setValue(10);
    m_fontSizeSpin->setSuffix(tr(" pt"));
    m_fontLayout->addRow(tr("字号:"), m_fontSizeSpin);

    layout->addWidget(m_fontGroup);

    // 显示组
    m_displayGroup = new QGroupBox(tr("显示"));
    m_displayLayout = new QFormLayout(m_displayGroup);

    m_maxLinesSpin = new QSpinBox;
    m_maxLinesSpin->setRange(100, 100000);
    m_maxLinesSpin->setSingleStep(1000);
    m_maxLinesSpin->setValue(10000);
    m_maxLinesSpin->setSuffix(tr(" 行"));
    m_displayLayout->addRow(tr("最大行数:"), m_maxLinesSpin);

    m_showTimestampCheck = new QCheckBox(tr("默认显示时间戳"));
    m_displayLayout->addRow(m_showTimestampCheck);

    m_autoScrollCheck = new QCheckBox(tr("默认自动滚动"));
    m_autoScrollCheck->setChecked(true);
    m_displayLayout->addRow(m_autoScrollCheck);

    m_newlineCombo = new QComboBox;
    m_newlineCombo->addItem("\\r\\n (CRLF)", "\r\n");
    m_newlineCombo->addItem("\\n (LF)", "\n");
    m_newlineCombo->addItem("\\r (CR)", "\r");
    m_displayLayout->addRow(tr("换行符:"), m_newlineCombo);

    layout->addWidget(m_displayGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_displayTab, tr("显示"));
}

void SettingsDialog::loadSettings()
{
    QSettings settings;
    auto* configMgr = ConfigManager::instance();

    // 常规设置
    m_autoSaveCheck->setChecked(settings.value("General/AutoSave", true).toBool());
    m_autoSaveIntervalSpin->setValue(settings.value("General/AutoSaveInterval", 30).toInt());
    m_checkRecoveryCheck->setChecked(settings.value("General/CheckRecovery", true).toBool());
    m_rememberWindowCheck->setChecked(settings.value("General/RememberWindow", true).toBool());

    QString lang = configMgr->language();
    if (lang.isEmpty()) {
        lang = settings.value("General/Language", "zh_CN").toString();
    }
    int langIdx = m_languageCombo->findData(lang);
    if (langIdx >= 0) {
        m_languageCombo->setCurrentIndex(langIdx);
    }

    // 串口设置
    SerialConfig serialConfig = configMgr->serialConfig();
    if (settings.contains("Serial/DefaultBaud")) {
        serialConfig.baudRate = settings.value("Serial/DefaultBaud", serialConfig.baudRate).toInt();
    }
    if (settings.contains("Serial/DefaultDataBits")) {
        serialConfig.dataBits = static_cast<DataBits>(
            settings.value("Serial/DefaultDataBits", static_cast<int>(serialConfig.dataBits)).toInt());
    }
    if (settings.contains("Serial/DefaultParity")) {
        const int legacyParity = settings.value("Serial/DefaultParity", 0).toInt();
        if (legacyParity == 2) {
            serialConfig.parity = Parity::Odd;
        } else if (legacyParity == 3) {
            serialConfig.parity = Parity::Even;
        } else {
            serialConfig.parity = static_cast<Parity>(legacyParity);
        }
    }
    if (settings.contains("Serial/DefaultStopBits")) {
        const int legacyStopBits = settings.value("Serial/DefaultStopBits", 1).toInt();
        if (legacyStopBits == 1) {
            serialConfig.stopBits = StopBits::One;
        } else if (legacyStopBits == 3) {
            serialConfig.stopBits = StopBits::OnePointFive;
        } else if (legacyStopBits == 2) {
            serialConfig.stopBits = StopBits::Two;
        } else {
            serialConfig.stopBits = static_cast<StopBits>(legacyStopBits);
        }
    }

    m_defaultBaudCombo->setCurrentText(QString::number(serialConfig.baudRate));
    m_defaultDataBitsCombo->setCurrentText(QString::number(static_cast<int>(serialConfig.dataBits)));

    const int parityIdx = m_defaultParityCombo->findData(static_cast<int>(serialConfig.parity));
    if (parityIdx >= 0) {
        m_defaultParityCombo->setCurrentIndex(parityIdx);
    }

    const int stopIdx = m_defaultStopBitsCombo->findData(static_cast<int>(serialConfig.stopBits));
    if (stopIdx >= 0) {
        m_defaultStopBitsCombo->setCurrentIndex(stopIdx);
    }

    m_autoReconnectCheck->setChecked(settings.value("Serial/AutoReconnect", false).toBool());
    m_reconnectIntervalSpin->setValue(settings.value("Serial/ReconnectInterval", 3).toInt());

    // 显示设置
    m_fontCombo->setCurrentFont(QFont(settings.value("Display/Font", "Consolas").toString()));
    m_fontSizeSpin->setValue(settings.value("Display/FontSize", 10).toInt());
    m_maxLinesSpin->setValue(settings.value("Display/MaxLines", 10000).toInt());
    m_showTimestampCheck->setChecked(settings.value("Display/ShowTimestamp", false).toBool());
    m_autoScrollCheck->setChecked(settings.value("Display/AutoScroll", true).toBool());

    QString newline = settings.value("Display/Newline", "\r\n").toString();
    int newlineIdx = m_newlineCombo->findData(newline);
    if (newlineIdx >= 0) m_newlineCombo->setCurrentIndex(newlineIdx);
}

void SettingsDialog::saveSettings()
{
    QSettings settings;
    auto* configMgr = ConfigManager::instance();

    // 常规设置
    settings.setValue("General/AutoSave", m_autoSaveCheck->isChecked());
    settings.setValue("General/AutoSaveInterval", m_autoSaveIntervalSpin->value());
    settings.setValue("General/CheckRecovery", m_checkRecoveryCheck->isChecked());
    settings.setValue("General/RememberWindow", m_rememberWindowCheck->isChecked());
    const QString selectedLanguage = m_languageCombo->currentData().toString();
    settings.setValue("General/Language", selectedLanguage);
    configMgr->setLanguage(selectedLanguage);

    // 串口设置
    SerialConfig serialConfig = configMgr->serialConfig();
    serialConfig.baudRate = m_defaultBaudCombo->currentText().toInt();
    serialConfig.dataBits = static_cast<DataBits>(m_defaultDataBitsCombo->currentText().toInt());
    serialConfig.parity = static_cast<Parity>(m_defaultParityCombo->currentData().toInt());
    serialConfig.stopBits = static_cast<StopBits>(m_defaultStopBitsCombo->currentData().toInt());
    configMgr->setSerialConfig(serialConfig);
    settings.setValue("Serial/DefaultBaud", serialConfig.baudRate);
    settings.setValue("Serial/DefaultDataBits", static_cast<int>(serialConfig.dataBits));
    settings.setValue("Serial/DefaultParity", static_cast<int>(serialConfig.parity));
    settings.setValue("Serial/DefaultStopBits", static_cast<int>(serialConfig.stopBits));
    settings.setValue("Serial/AutoReconnect", m_autoReconnectCheck->isChecked());
    settings.setValue("Serial/ReconnectInterval", m_reconnectIntervalSpin->value());

    // 显示设置
    settings.setValue("Display/Font", m_fontCombo->currentFont().family());
    settings.setValue("Display/FontSize", m_fontSizeSpin->value());
    settings.setValue("Display/MaxLines", m_maxLinesSpin->value());
    settings.setValue("Display/ShowTimestamp", m_showTimestampCheck->isChecked());
    settings.setValue("Display/AutoScroll", m_autoScrollCheck->isChecked());
    settings.setValue("Display/Newline", m_newlineCombo->currentData().toString());

    // 更新 SessionManager 的自动保存设置
    SessionManager::instance()->setAutoSaveEnabled(m_autoSaveCheck->isChecked());
    SessionManager::instance()->setAutoSaveInterval(m_autoSaveIntervalSpin->value());

    // 立即落盘，避免“点击应用后重启失效”
    configMgr->saveConfig();
    settings.sync();
}

void SettingsDialog::resetToDefaults()
{
    // 常规
    m_autoSaveCheck->setChecked(true);
    m_autoSaveIntervalSpin->setValue(30);
    m_checkRecoveryCheck->setChecked(true);
    m_rememberWindowCheck->setChecked(true);
    m_languageCombo->setCurrentIndex(0);

    // 串口
    m_defaultBaudCombo->setCurrentText("115200");
    m_defaultDataBitsCombo->setCurrentText("8");
    m_defaultParityCombo->setCurrentIndex(0);
    m_defaultStopBitsCombo->setCurrentIndex(0);
    m_autoReconnectCheck->setChecked(false);
    m_reconnectIntervalSpin->setValue(3);

    // 显示
    m_fontCombo->setCurrentFont(QFont("Consolas"));
    m_fontSizeSpin->setValue(10);
    m_maxLinesSpin->setValue(10000);
    m_showTimestampCheck->setChecked(false);
    m_autoScrollCheck->setChecked(true);
    m_newlineCombo->setCurrentIndex(0);
}

void SettingsDialog::onOkClicked()
{
    saveSettings();
    emit settingsApplied();
    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}

void SettingsDialog::onApplyClicked()
{
    saveSettings();
    emit settingsApplied();
}

void SettingsDialog::onResetClicked()
{
    resetToDefaults();
}

void SettingsDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void SettingsDialog::retranslateUi()
{
    setWindowTitle(tr("设置"));

    if (m_tabWidget) {
        m_tabWidget->setTabText(0, tr("常规"));
        m_tabWidget->setTabText(1, tr("串口"));
        m_tabWidget->setTabText(2, tr("显示"));
    }

    if (m_autoSaveGroup) {
        m_autoSaveGroup->setTitle(tr("自动保存"));
    }
    if (m_autoSaveCheck) {
        m_autoSaveCheck->setText(tr("启用自动保存"));
    }
    if (m_autoSaveIntervalSpin) {
        m_autoSaveIntervalSpin->setSuffix(tr(" 秒"));
    }
    if (m_autoSaveLayout && m_autoSaveIntervalSpin) {
        if (QWidget* label = m_autoSaveLayout->labelForField(m_autoSaveIntervalSpin)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("保存间隔:"));
            }
        }
    }
    if (m_checkRecoveryCheck) {
        m_checkRecoveryCheck->setText(tr("启动时检查恢复数据"));
    }

    if (m_windowGroup) {
        m_windowGroup->setTitle(tr("窗口"));
    }
    if (m_rememberWindowCheck) {
        m_rememberWindowCheck->setText(tr("记住窗口位置和大小"));
    }

    if (m_langGroup) {
        m_langGroup->setTitle(tr("语言"));
    }
    if (m_languageCombo) {
        m_languageCombo->setItemText(0, tr("简体中文"));
        m_languageCombo->setItemText(1, tr("English"));
    }
    if (m_langLayout && m_languageCombo) {
        if (QWidget* label = m_langLayout->labelForField(m_languageCombo)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("界面语言:"));
            }
        }
    }

    if (m_defaultSerialGroup) {
        m_defaultSerialGroup->setTitle(tr("默认串口参数"));
    }
    if (m_defaultParityCombo) {
        m_defaultParityCombo->setItemText(0, tr("无"));
        m_defaultParityCombo->setItemText(1, tr("奇校验"));
        m_defaultParityCombo->setItemText(2, tr("偶校验"));
    }
    if (m_defaultSerialLayout) {
        if (QWidget* label = m_defaultSerialLayout->labelForField(m_defaultBaudCombo)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("波特率:"));
            }
        }
        if (QWidget* label = m_defaultSerialLayout->labelForField(m_defaultDataBitsCombo)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("数据位:"));
            }
        }
        if (QWidget* label = m_defaultSerialLayout->labelForField(m_defaultParityCombo)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("校验位:"));
            }
        }
        if (QWidget* label = m_defaultSerialLayout->labelForField(m_defaultStopBitsCombo)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("停止位:"));
            }
        }
    }

    if (m_reconnectGroup) {
        m_reconnectGroup->setTitle(tr("自动重连"));
    }
    if (m_autoReconnectCheck) {
        m_autoReconnectCheck->setText(tr("断开时自动重连"));
    }
    if (m_reconnectIntervalSpin) {
        m_reconnectIntervalSpin->setSuffix(tr(" 秒"));
    }
    if (m_reconnectLayout && m_reconnectIntervalSpin) {
        if (QWidget* label = m_reconnectLayout->labelForField(m_reconnectIntervalSpin)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("重连间隔:"));
            }
        }
    }

    if (m_fontGroup) {
        m_fontGroup->setTitle(tr("字体"));
    }
    if (m_fontSizeSpin) {
        m_fontSizeSpin->setSuffix(tr(" pt"));
    }
    if (m_fontLayout) {
        if (QWidget* label = m_fontLayout->labelForField(m_fontCombo)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("字体:"));
            }
        }
        if (QWidget* label = m_fontLayout->labelForField(m_fontSizeSpin)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("字号:"));
            }
        }
    }

    if (m_displayGroup) {
        m_displayGroup->setTitle(tr("显示"));
    }
    if (m_maxLinesSpin) {
        m_maxLinesSpin->setSuffix(tr(" 行"));
    }
    if (m_showTimestampCheck) {
        m_showTimestampCheck->setText(tr("默认显示时间戳"));
    }
    if (m_autoScrollCheck) {
        m_autoScrollCheck->setText(tr("默认自动滚动"));
    }
    if (m_displayLayout) {
        if (QWidget* label = m_displayLayout->labelForField(m_maxLinesSpin)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("最大行数:"));
            }
        }
        if (QWidget* label = m_displayLayout->labelForField(m_newlineCombo)) {
            if (auto* textLabel = qobject_cast<QLabel*>(label)) {
                textLabel->setText(tr("换行符:"));
            }
        }
    }

    if (m_okBtn) {
        m_okBtn->setText(tr("确定"));
    }
    if (m_cancelBtn) {
        m_cancelBtn->setText(tr("取消"));
    }
    if (m_applyBtn) {
        m_applyBtn->setText(tr("应用"));
    }
    if (m_resetBtn) {
        m_resetBtn->setText(tr("恢复默认"));
    }
}

} // namespace ComAssistant
