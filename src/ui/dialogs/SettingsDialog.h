/**
 * @file SettingsDialog.h
 * @brief 设置对话框
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_SETTINGSDIALOG_H
#define COMASSISTANT_SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QPushButton>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QLabel>

namespace ComAssistant {

/**
 * @brief 设置对话框
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

signals:
    void settingsApplied();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onApplyClicked();
    void onOkClicked();
    void onCancelClicked();
    void onResetClicked();

private:
    void setupUi();
    void setupGeneralTab();
    void setupSerialTab();
    void setupDisplayTab();
    void retranslateUi();

    void loadSettings();
    void saveSettings();
    void resetToDefaults();

private:
    QTabWidget* m_tabWidget = nullptr;

    // 常规设置
    QWidget* m_generalTab = nullptr;
    QGroupBox* m_autoSaveGroup = nullptr;
    QFormLayout* m_autoSaveLayout = nullptr;
    QGroupBox* m_windowGroup = nullptr;
    QGroupBox* m_langGroup = nullptr;
    QFormLayout* m_langLayout = nullptr;
    QCheckBox* m_autoSaveCheck = nullptr;
    QSpinBox* m_autoSaveIntervalSpin = nullptr;
    QCheckBox* m_checkRecoveryCheck = nullptr;
    QCheckBox* m_rememberWindowCheck = nullptr;
    QComboBox* m_languageCombo = nullptr;

    // 串口设置
    QWidget* m_serialTab = nullptr;
    QGroupBox* m_defaultSerialGroup = nullptr;
    QFormLayout* m_defaultSerialLayout = nullptr;
    QGroupBox* m_reconnectGroup = nullptr;
    QFormLayout* m_reconnectLayout = nullptr;
    QComboBox* m_defaultBaudCombo = nullptr;
    QComboBox* m_defaultDataBitsCombo = nullptr;
    QComboBox* m_defaultParityCombo = nullptr;
    QComboBox* m_defaultStopBitsCombo = nullptr;
    QCheckBox* m_autoReconnectCheck = nullptr;
    QSpinBox* m_reconnectIntervalSpin = nullptr;

    // 显示设置
    QWidget* m_displayTab = nullptr;
    QGroupBox* m_fontGroup = nullptr;
    QFormLayout* m_fontLayout = nullptr;
    QGroupBox* m_displayGroup = nullptr;
    QFormLayout* m_displayLayout = nullptr;
    QFontComboBox* m_fontCombo = nullptr;
    QSpinBox* m_fontSizeSpin = nullptr;
    QSpinBox* m_maxLinesSpin = nullptr;
    QCheckBox* m_showTimestampCheck = nullptr;
    QCheckBox* m_autoScrollCheck = nullptr;
    QComboBox* m_newlineCombo = nullptr;

    // 按钮
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_applyBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SETTINGSDIALOG_H
