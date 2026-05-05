/**
 * @file ToolboxDialog.h
 * @brief 工具箱对话框
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_TOOLBOXDIALOG_H
#define COMASSISTANT_TOOLBOXDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QEvent>

namespace ComAssistant {

/**
 * @brief 工具箱对话框
 * 提供校验和计算、进制转换、编码转换等功能
 */
class ToolboxDialog : public QDialog {
    Q_OBJECT

public:
    explicit ToolboxDialog(QWidget* parent = nullptr);
    ~ToolboxDialog() override = default;

protected:
    void changeEvent(QEvent* event) override;

private slots:
    // 校验和标签页
    void onCalculateChecksum();
    void onClearChecksum();

    // 进制转换标签页
    void onConvertBase();
    void onSwapEndian();
    void onConvertFloat();

    // 编码转换标签页
    void onConvertEncoding();
    void onConvertBase64();
    void onConvertUrl();
    void onConvertEscape();

private:
    void setupUi();
    void setupChecksumTab();
    void setupConversionTab();
    void setupEncodingTab();
    void applyThemeAwareStyles();

    QTabWidget* m_tabWidget = nullptr;

    // ===== 校验和标签页 =====
    QWidget* m_checksumTab = nullptr;
    QTextEdit* m_checksumInput = nullptr;
    QComboBox* m_checksumTypeCombo = nullptr;
    QCheckBox* m_checksumHexInput = nullptr;
    QLabel* m_checksumResult = nullptr;

    // ===== 进制转换标签页 =====
    QWidget* m_conversionTab = nullptr;
    // 进制转换
    QLineEdit* m_baseInput = nullptr;
    QComboBox* m_baseFromCombo = nullptr;
    QComboBox* m_baseToCombo = nullptr;
    QLineEdit* m_baseOutput = nullptr;
    // 字节序
    QLineEdit* m_endianInput = nullptr;
    QComboBox* m_endianTypeCombo = nullptr;
    QLineEdit* m_endianOutput = nullptr;
    // IEEE754
    QLineEdit* m_floatHexInput = nullptr;
    QCheckBox* m_floatBigEndian = nullptr;
    QLineEdit* m_floatValueOutput = nullptr;
    QLineEdit* m_floatValueInput = nullptr;
    QLineEdit* m_floatHexOutput = nullptr;

    // ===== 编码转换标签页 =====
    QWidget* m_encodingTab = nullptr;
    // 字符编码
    QTextEdit* m_encodingInput = nullptr;
    QComboBox* m_encodingFromCombo = nullptr;
    QComboBox* m_encodingToCombo = nullptr;
    QTextEdit* m_encodingOutput = nullptr;
    // Base64
    QTextEdit* m_base64Input = nullptr;
    QTextEdit* m_base64Output = nullptr;
    // URL编码
    QLineEdit* m_urlInput = nullptr;
    QLineEdit* m_urlOutput = nullptr;
    // 转义
    QLineEdit* m_escapeInput = nullptr;
    QLineEdit* m_escapeOutput = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_TOOLBOXDIALOG_H
