/**
 * @file ExportDialog.cpp
 * @brief 增强数据导出对话框实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "ExportDialog.h"
#include "utils/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDateTime>

namespace ComAssistant {

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    m_exporter = new DataExporter(this);

    connect(m_exporter, &DataExporter::exportProgress,
            this, &ExportDialog::onExportProgress);
    connect(m_exporter, &DataExporter::exportFinished,
            this, &ExportDialog::onExportFinished);

    setupUi();

    // 默认路径
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_pathEdit->setText(defaultPath + "/serial_export_" + timestamp + ".txt");

    setWindowTitle(tr("数据导出"));
    resize(600, 500);
}

void ExportDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 标签页
    m_tabWidget = new QTabWidget(this);
    setupFormatTab();
    setupContentTab();
    setupFilterTab();
    mainLayout->addWidget(m_tabWidget);

    // 预览区域
    auto* previewGroup = new QGroupBox(tr("预览"), this);
    auto* previewLayout = new QVBoxLayout(previewGroup);
    m_previewEdit = new QTextEdit(this);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(120);
    m_previewEdit->setFont(QFont("Consolas", 9));
    previewLayout->addWidget(m_previewEdit);
    mainLayout->addWidget(previewGroup);

    // 统计信息
    auto* statsLayout = new QHBoxLayout();
    m_totalRecordsLabel = new QLabel(tr("总记录: 0"), this);
    m_filteredRecordsLabel = new QLabel(tr("过滤后: 0"), this);
    m_totalBytesLabel = new QLabel(tr("数据量: 0 bytes"), this);
    statsLayout->addWidget(m_totalRecordsLabel);
    statsLayout->addWidget(m_filteredRecordsLabel);
    statsLayout->addWidget(m_totalBytesLabel);
    statsLayout->addStretch();
    mainLayout->addLayout(statsLayout);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // 按钮
    auto* buttonLayout = new QHBoxLayout();
    m_previewBtn = new QPushButton(tr("更新预览"), this);
    connect(m_previewBtn, &QPushButton::clicked, this, &ExportDialog::onPreviewClicked);
    buttonLayout->addWidget(m_previewBtn);

    buttonLayout->addStretch();

    m_exportBtn = new QPushButton(tr("导出"), this);
    m_exportBtn->setDefault(true);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportDialog::onExportClicked);
    buttonLayout->addWidget(m_exportBtn);

    m_cancelBtn = new QPushButton(tr("取消"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(buttonLayout);
}

void ExportDialog::setupFormatTab()
{
    auto* tab = new QWidget();
    auto* layout = new QFormLayout(tab);

    // 格式选择
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("纯文本 (.txt)"), static_cast<int>(ExportFormat::PlainText));
    m_formatCombo->addItem(tr("CSV表格 (.csv)"), static_cast<int>(ExportFormat::Csv));
    m_formatCombo->addItem(tr("HTML网页 (.html)"), static_cast<int>(ExportFormat::Html));
    m_formatCombo->addItem(tr("JSON (.json)"), static_cast<int>(ExportFormat::Json));
    m_formatCombo->addItem(tr("XML (.xml)"), static_cast<int>(ExportFormat::Xml));
    m_formatCombo->addItem(tr("二进制 (.bin)"), static_cast<int>(ExportFormat::Binary));
    m_formatCombo->addItem(tr("十六进制转储 (.hex)"), static_cast<int>(ExportFormat::HexDump));
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::onFormatChanged);
    layout->addRow(tr("导出格式:"), m_formatCombo);

    // 文件路径
    auto* pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(tr("浏览..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowseClicked);
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(m_browseBtn);
    layout->addRow(tr("保存路径:"), pathLayout);

    // 编码
    m_encodingCombo = new QComboBox(this);
    m_encodingCombo->addItems({"UTF-8", "GBK", "GB2312", "UTF-16", "ISO-8859-1"});
    layout->addRow(tr("文件编码:"), m_encodingCombo);

    // 换行符
    m_lineEndingCombo = new QComboBox(this);
    m_lineEndingCombo->addItem(tr("Windows (CRLF)"), "\r\n");
    m_lineEndingCombo->addItem(tr("Unix (LF)"), "\n");
    m_lineEndingCombo->addItem(tr("Mac (CR)"), "\r");
    layout->addRow(tr("换行符:"), m_lineEndingCombo);

    m_tabWidget->addTab(tab, tr("格式"));
}

void ExportDialog::setupContentTab()
{
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);

    // 包含内容选项
    auto* includeGroup = new QGroupBox(tr("包含内容"), this);
    auto* includeLayout = new QGridLayout(includeGroup);

    m_includeTimestampCheck = new QCheckBox(tr("时间戳"), this);
    m_includeTimestampCheck->setChecked(true);
    includeLayout->addWidget(m_includeTimestampCheck, 0, 0);

    m_includeDirectionCheck = new QCheckBox(tr("方向 (TX/RX)"), this);
    m_includeDirectionCheck->setChecked(true);
    includeLayout->addWidget(m_includeDirectionCheck, 0, 1);

    m_includeSourceCheck = new QCheckBox(tr("数据来源"), this);
    includeLayout->addWidget(m_includeSourceCheck, 1, 0);

    m_includeLineNumberCheck = new QCheckBox(tr("行号"), this);
    includeLayout->addWidget(m_includeLineNumberCheck, 1, 1);

    layout->addWidget(includeGroup);

    // 显示格式选项
    auto* formatGroup = new QGroupBox(tr("显示格式"), this);
    auto* formatLayout = new QFormLayout(formatGroup);

    m_hexFormatCheck = new QCheckBox(tr("十六进制显示数据"), this);
    formatLayout->addRow(m_hexFormatCheck);

    m_showPrintableCheck = new QCheckBox(tr("显示可打印字符"), this);
    formatLayout->addRow(m_showPrintableCheck);

    m_hexBytesPerLineSpin = new QSpinBox(this);
    m_hexBytesPerLineSpin->setRange(8, 64);
    m_hexBytesPerLineSpin->setValue(16);
    formatLayout->addRow(tr("每行字节数:"), m_hexBytesPerLineSpin);

    m_timestampFormatEdit = new QLineEdit("yyyy-MM-dd hh:mm:ss.zzz", this);
    formatLayout->addRow(tr("时间格式:"), m_timestampFormatEdit);

    layout->addWidget(formatGroup);

    // CSV选项
    m_csvGroup = new QGroupBox(tr("CSV选项"), this);
    auto* csvLayout = new QFormLayout(m_csvGroup);

    m_csvSeparatorCombo = new QComboBox(this);
    m_csvSeparatorCombo->addItem(tr("逗号 (,)"), ",");
    m_csvSeparatorCombo->addItem(tr("分号 (;)"), ";");
    m_csvSeparatorCombo->addItem(tr("制表符 (Tab)"), "\t");
    csvLayout->addRow(tr("分隔符:"), m_csvSeparatorCombo);

    m_csvQuoteCheck = new QCheckBox(tr("字符串加引号"), this);
    m_csvQuoteCheck->setChecked(true);
    csvLayout->addRow(m_csvQuoteCheck);

    layout->addWidget(m_csvGroup);

    // HTML选项
    m_htmlGroup = new QGroupBox(tr("HTML选项"), this);
    auto* htmlLayout = new QFormLayout(m_htmlGroup);

    m_htmlTitleEdit = new QLineEdit(tr("串口数据导出"), this);
    htmlLayout->addRow(tr("页面标题:"), m_htmlTitleEdit);

    m_htmlThemeCombo = new QComboBox(this);
    m_htmlThemeCombo->addItem(tr("浅色主题"), "light");
    m_htmlThemeCombo->addItem(tr("深色主题"), "dark");
    htmlLayout->addRow(tr("主题:"), m_htmlThemeCombo);

    layout->addWidget(m_htmlGroup);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr("内容"));

    // 初始隐藏特定格式选项
    m_csvGroup->setVisible(false);
    m_htmlGroup->setVisible(false);
}

void ExportDialog::setupFilterTab()
{
    auto* tab = new QWidget();
    auto* layout = new QVBoxLayout(tab);

    // 方向过滤
    auto* dirGroup = new QGroupBox(tr("方向过滤"), this);
    auto* dirLayout = new QVBoxLayout(dirGroup);

    m_filterDirectionCheck = new QCheckBox(tr("启用方向过滤"), this);
    connect(m_filterDirectionCheck, &QCheckBox::toggled, this, &ExportDialog::onFilterChanged);
    dirLayout->addWidget(m_filterDirectionCheck);

    m_receiveOnlyRadio = new QRadioButton(tr("仅接收数据"), this);
    m_receiveOnlyRadio->setChecked(true);
    m_receiveOnlyRadio->setEnabled(false);
    dirLayout->addWidget(m_receiveOnlyRadio);

    m_sendOnlyRadio = new QRadioButton(tr("仅发送数据"), this);
    m_sendOnlyRadio->setEnabled(false);
    dirLayout->addWidget(m_sendOnlyRadio);

    connect(m_filterDirectionCheck, &QCheckBox::toggled, m_receiveOnlyRadio, &QWidget::setEnabled);
    connect(m_filterDirectionCheck, &QCheckBox::toggled, m_sendOnlyRadio, &QWidget::setEnabled);

    layout->addWidget(dirGroup);

    // 时间过滤
    auto* timeGroup = new QGroupBox(tr("时间过滤"), this);
    auto* timeLayout = new QFormLayout(timeGroup);

    m_filterTimeCheck = new QCheckBox(tr("启用时间过滤"), this);
    connect(m_filterTimeCheck, &QCheckBox::toggled, this, &ExportDialog::onFilterChanged);
    timeLayout->addRow(m_filterTimeCheck);

    m_startTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(-3600), this);
    m_startTimeEdit->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    m_startTimeEdit->setEnabled(false);
    timeLayout->addRow(tr("开始时间:"), m_startTimeEdit);

    m_endTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), this);
    m_endTimeEdit->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    m_endTimeEdit->setEnabled(false);
    timeLayout->addRow(tr("结束时间:"), m_endTimeEdit);

    connect(m_filterTimeCheck, &QCheckBox::toggled, m_startTimeEdit, &QWidget::setEnabled);
    connect(m_filterTimeCheck, &QCheckBox::toggled, m_endTimeEdit, &QWidget::setEnabled);

    layout->addWidget(timeGroup);

    // 内容过滤
    auto* contentGroup = new QGroupBox(tr("内容过滤"), this);
    auto* contentLayout = new QFormLayout(contentGroup);

    m_filterContentCheck = new QCheckBox(tr("启用内容过滤"), this);
    connect(m_filterContentCheck, &QCheckBox::toggled, this, &ExportDialog::onFilterChanged);
    contentLayout->addRow(m_filterContentCheck);

    m_contentFilterEdit = new QLineEdit(this);
    m_contentFilterEdit->setPlaceholderText(tr("正则表达式"));
    m_contentFilterEdit->setEnabled(false);
    contentLayout->addRow(tr("过滤规则:"), m_contentFilterEdit);

    m_invertFilterCheck = new QCheckBox(tr("反转过滤 (排除匹配项)"), this);
    m_invertFilterCheck->setEnabled(false);
    contentLayout->addRow(m_invertFilterCheck);

    connect(m_filterContentCheck, &QCheckBox::toggled, m_contentFilterEdit, &QWidget::setEnabled);
    connect(m_filterContentCheck, &QCheckBox::toggled, m_invertFilterCheck, &QWidget::setEnabled);

    layout->addWidget(contentGroup);

    layout->addStretch();

    m_tabWidget->addTab(tab, tr("过滤"));
}

void ExportDialog::setRecords(const QVector<DataRecord>& records)
{
    m_records = records;
    m_exporter->clearRecords();
    m_exporter->addRecords(records);

    updateStatistics();

    bool hasData = !records.isEmpty();
    m_exportBtn->setEnabled(hasData);
    m_previewBtn->setEnabled(hasData);

    if (hasData) {
        updatePreview();
    }
}

ExportOptions ExportDialog::exportOptions() const
{
    ExportOptions options;

    // 格式
    options.format = static_cast<ExportFormat>(m_formatCombo->currentData().toInt());
    options.encoding = m_encodingCombo->currentText();
    options.lineSeparator = m_lineEndingCombo->currentData().toString();

    // 内容
    options.includeTimestamp = m_includeTimestampCheck->isChecked();
    options.includeDirection = m_includeDirectionCheck->isChecked();
    options.includeSource = m_includeSourceCheck->isChecked();
    options.includeLineNumber = m_includeLineNumberCheck->isChecked();

    // 显示格式
    options.hexFormat = m_hexFormatCheck->isChecked();
    options.showPrintable = m_showPrintableCheck->isChecked();
    options.hexBytesPerLine = m_hexBytesPerLineSpin->value();
    options.timestampFormat = m_timestampFormatEdit->text();

    // CSV
    options.csvSeparator = m_csvSeparatorCombo->currentData().toString();
    options.csvQuoteStrings = m_csvQuoteCheck->isChecked();

    // HTML
    options.htmlTitle = m_htmlTitleEdit->text();
    options.htmlTheme = m_htmlThemeCombo->currentData().toString();

    // 过滤
    options.filterByDirection = m_filterDirectionCheck->isChecked();
    options.receiveOnly = m_receiveOnlyRadio->isChecked();

    options.filterByTime = m_filterTimeCheck->isChecked();
    options.startTime = m_startTimeEdit->dateTime();
    options.endTime = m_endTimeEdit->dateTime();

    options.filterByContent = m_filterContentCheck->isChecked();
    options.contentFilter = m_contentFilterEdit->text();
    options.invertContentFilter = m_invertFilterCheck->isChecked();

    return options;
}

QString ExportDialog::exportPath() const
{
    return m_pathEdit->text();
}

void ExportDialog::onFormatChanged(int index)
{
    Q_UNUSED(index)

    ExportFormat format = static_cast<ExportFormat>(m_formatCombo->currentData().toInt());

    // 更新文件扩展名
    QString path = m_pathEdit->text();
    if (!path.isEmpty()) {
        int lastDot = path.lastIndexOf('.');
        int lastSlash = qMax(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastDot > lastSlash) {
            path = path.left(lastDot);
        }
        path += getDefaultExtension();
        m_pathEdit->setText(path);
    }

    // 显示/隐藏特定格式选项
    m_csvGroup->setVisible(format == ExportFormat::Csv);
    m_htmlGroup->setVisible(format == ExportFormat::Html);

    updatePreview();
}

void ExportDialog::onBrowseClicked()
{
    QString filter = getFileFilter();
    QString path = QFileDialog::getSaveFileName(this, tr("选择导出位置"),
                                                 m_pathEdit->text(), filter);
    if (!path.isEmpty()) {
        m_pathEdit->setText(path);
    }
}

void ExportDialog::onExportClicked()
{
    if (m_pathEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请选择导出路径"));
        return;
    }

    if (m_records.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("没有数据可导出"));
        return;
    }

    m_exporter->setOptions(exportOptions());
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_exportBtn->setEnabled(false);

    if (m_exporter->exportToFile(m_pathEdit->text())) {
        ExportStatistics stats = m_exporter->statistics();
        LOG_INFO(QString("Data exported: %1 records to %2")
            .arg(stats.exportedRecords).arg(m_pathEdit->text()));
    }
}

void ExportDialog::onPreviewClicked()
{
    updatePreview();
}

void ExportDialog::onFilterChanged()
{
    updateStatistics();
    updatePreview();
}

void ExportDialog::onExportProgress(int current, int total)
{
    int percent = total > 0 ? (current * 100 / total) : 0;
    m_progressBar->setValue(percent);
}

void ExportDialog::onExportFinished(bool success, const QString& message)
{
    m_progressBar->setVisible(false);
    m_exportBtn->setEnabled(true);

    if (success) {
        ExportStatistics stats = m_exporter->statistics();
        QMessageBox::information(this, tr("导出完成"),
            tr("成功导出 %1 条记录到:\n%2\n\n文件大小: %3 字节\n耗时: %4")
            .arg(stats.exportedRecords)
            .arg(m_pathEdit->text())
            .arg(stats.fileSizeBytes)
            .arg(stats.duration));
        accept();
    } else {
        QMessageBox::critical(this, tr("导出失败"), message);
    }
}

void ExportDialog::updatePreview()
{
    if (m_records.isEmpty()) {
        m_previewEdit->setPlainText(tr("(无数据)"));
        return;
    }

    // 使用前10条记录预览
    DataExporter previewExporter;
    QVector<DataRecord> previewRecords = m_records.mid(0, 10);
    previewExporter.addRecords(previewRecords);
    previewExporter.setOptions(exportOptions());

    QString preview = previewExporter.exportToString();

    // 截断过长的预览
    if (preview.length() > 2000) {
        preview = preview.left(2000) + "\n...";
    }

    if (m_records.size() > 10) {
        preview += tr("\n\n--- 还有 %1 条记录未显示 ---").arg(m_records.size() - 10);
    }

    m_previewEdit->setPlainText(preview);
}

void ExportDialog::updateStatistics()
{
    m_totalRecordsLabel->setText(tr("总记录: %1").arg(m_records.size()));

    // 计算过滤后的记录数
    m_exporter->clearRecords();
    m_exporter->addRecords(m_records);
    m_exporter->setOptions(exportOptions());

    // 获取过滤后数量 (通过导出到字符串并计算)
    // 这里简化处理，实际应该有更好的方法
    qint64 totalBytes = 0;
    for (const auto& record : m_records) {
        totalBytes += record.data.size();
    }

    m_filteredRecordsLabel->setText(tr("过滤后: %1").arg(m_records.size()));
    m_totalBytesLabel->setText(tr("数据量: %1 bytes").arg(totalBytes));
}

QString ExportDialog::getFileFilter() const
{
    return DataExporter::allFormatsFilter();
}

QString ExportDialog::getDefaultExtension() const
{
    ExportFormat format = static_cast<ExportFormat>(m_formatCombo->currentData().toInt());
    return DataExporter::formatExtension(format);
}

void ExportDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void ExportDialog::retranslateUi()
{
    setWindowTitle(tr("数据导出"));

    // 标签页标题
    m_tabWidget->setTabText(0, tr("格式"));
    m_tabWidget->setTabText(1, tr("内容"));
    m_tabWidget->setTabText(2, tr("过滤"));

    // 按钮
    m_previewBtn->setText(tr("更新预览"));
    m_exportBtn->setText(tr("导出"));
    m_cancelBtn->setText(tr("取消"));
    m_browseBtn->setText(tr("浏览..."));

    // 格式选项
    m_formatCombo->setItemText(0, tr("纯文本 (.txt)"));
    m_formatCombo->setItemText(1, tr("CSV表格 (.csv)"));
    m_formatCombo->setItemText(2, tr("HTML网页 (.html)"));
    m_formatCombo->setItemText(3, tr("JSON (.json)"));
    m_formatCombo->setItemText(4, tr("XML (.xml)"));
    m_formatCombo->setItemText(5, tr("二进制 (.bin)"));
    m_formatCombo->setItemText(6, tr("十六进制转储 (.hex)"));

    // 换行符
    m_lineEndingCombo->setItemText(0, tr("Windows (CRLF)"));
    m_lineEndingCombo->setItemText(1, tr("Unix (LF)"));
    m_lineEndingCombo->setItemText(2, tr("Mac (CR)"));

    // 更新统计
    updateStatistics();
}

} // namespace ComAssistant
