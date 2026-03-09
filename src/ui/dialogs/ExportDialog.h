/**
 * @file ExportDialog.h
 * @brief 增强数据导出对话框
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_EXPORTDIALOG_H
#define COMASSISTANT_EXPORTDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QDateTimeEdit>
#include <QTabWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QEvent>

#include "core/export/DataExporter.h"

namespace ComAssistant {

/**
 * @brief 增强数据导出对话框
 */
class ExportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(QWidget* parent = nullptr);
    ~ExportDialog() override = default;

    /**
     * @brief 设置要导出的数据
     */
    void setRecords(const QVector<DataRecord>& records);

    /**
     * @brief 获取导出选项
     */
    ExportOptions exportOptions() const;

    /**
     * @brief 获取导出文件路径
     */
    QString exportPath() const;

private slots:
    void onFormatChanged(int index);
    void onBrowseClicked();
    void onExportClicked();
    void onPreviewClicked();
    void onFilterChanged();
    void onExportProgress(int current, int total);
    void onExportFinished(bool success, const QString& message);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupFormatTab();
    void setupContentTab();
    void setupFilterTab();
    void updatePreview();
    void updateStatistics();
    QString getFileFilter() const;
    QString getDefaultExtension() const;
    void retranslateUi();

private:
    // 数据
    QVector<DataRecord> m_records;
    DataExporter* m_exporter = nullptr;

    // 标签页
    QTabWidget* m_tabWidget = nullptr;

    // === 格式标签页 ===
    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QComboBox* m_encodingCombo = nullptr;
    QComboBox* m_lineEndingCombo = nullptr;

    // === 内容标签页 ===
    QCheckBox* m_includeTimestampCheck = nullptr;
    QCheckBox* m_includeDirectionCheck = nullptr;
    QCheckBox* m_includeSourceCheck = nullptr;
    QCheckBox* m_includeLineNumberCheck = nullptr;
    QCheckBox* m_hexFormatCheck = nullptr;
    QCheckBox* m_showPrintableCheck = nullptr;
    QSpinBox* m_hexBytesPerLineSpin = nullptr;
    QLineEdit* m_timestampFormatEdit = nullptr;

    // CSV选项
    QGroupBox* m_csvGroup = nullptr;
    QComboBox* m_csvSeparatorCombo = nullptr;
    QCheckBox* m_csvQuoteCheck = nullptr;

    // HTML选项
    QGroupBox* m_htmlGroup = nullptr;
    QLineEdit* m_htmlTitleEdit = nullptr;
    QComboBox* m_htmlThemeCombo = nullptr;

    // === 过滤标签页 ===
    QCheckBox* m_filterDirectionCheck = nullptr;
    QRadioButton* m_receiveOnlyRadio = nullptr;
    QRadioButton* m_sendOnlyRadio = nullptr;

    QCheckBox* m_filterTimeCheck = nullptr;
    QDateTimeEdit* m_startTimeEdit = nullptr;
    QDateTimeEdit* m_endTimeEdit = nullptr;

    QCheckBox* m_filterContentCheck = nullptr;
    QLineEdit* m_contentFilterEdit = nullptr;
    QCheckBox* m_invertFilterCheck = nullptr;

    // === 预览和统计 ===
    QTextEdit* m_previewEdit = nullptr;
    QLabel* m_totalRecordsLabel = nullptr;
    QLabel* m_filteredRecordsLabel = nullptr;
    QLabel* m_totalBytesLabel = nullptr;

    // === 按钮和进度 ===
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_previewBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_EXPORTDIALOG_H
