/**
 * @file DataTableWidget.cpp
 * @brief 数据表格视图控件实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "DataTableWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QTextStream>
#include <QFile>
#include <QEvent>
#include <QMutexLocker>

namespace ComAssistant {

DataTableWidget::DataTableWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    // 批量更新定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(UPDATE_INTERVAL_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &DataTableWidget::updateDisplay);
    m_updateTimer->start();
}

DataTableWidget::~DataTableWidget()
{
    m_updateTimer->stop();
}

void DataTableWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);

    setupToolBar();
    mainLayout->addWidget(m_toolBar);

    setupTable();
    mainLayout->addWidget(m_tableView);

    // 状态栏
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("共 0 条记录"));
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    retranslateUi();
}

void DataTableWidget::setupToolBar()
{
    m_toolBar = new QToolBar(this);
    m_toolBar->setIconSize(QSize(16, 16));

    // 暂停按钮
    m_pauseAction = m_toolBar->addAction(tr("暂停"));
    m_pauseAction->setCheckable(true);
    m_pauseAction->setChecked(false);
    connect(m_pauseAction, &QAction::toggled, this, &DataTableWidget::onPauseToggled);

    m_toolBar->addSeparator();

    // 方向过滤
    QLabel* dirLabel = new QLabel(tr("方向:"));
    m_toolBar->addWidget(dirLabel);

    m_directionCombo = new QComboBox();
    m_directionCombo->addItem(tr("全部"), "ALL");
    m_directionCombo->addItem(tr("接收(RX)"), "RX");
    m_directionCombo->addItem(tr("发送(TX)"), "TX");
    m_directionCombo->setFixedWidth(100);
    connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataTableWidget::onDirectionFilterChanged);
    m_toolBar->addWidget(m_directionCombo);

    m_toolBar->addSeparator();

    // 搜索过滤
    QLabel* filterLabel = new QLabel(tr("搜索:"));
    m_toolBar->addWidget(filterLabel);

    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText(tr("输入关键字过滤..."));
    m_filterEdit->setFixedWidth(200);
    m_filterEdit->setClearButtonEnabled(true);
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &DataTableWidget::onFilterTextChanged);
    m_toolBar->addWidget(m_filterEdit);

    m_toolBar->addSeparator();

    // 自动滚动
    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    m_autoScrollCheck->setChecked(m_autoScroll);
    connect(m_autoScrollCheck, &QCheckBox::toggled,
            this, &DataTableWidget::onAutoScrollToggled);
    m_toolBar->addWidget(m_autoScrollCheck);

    m_toolBar->addSeparator();

    // 复制按钮
    m_copyAction = m_toolBar->addAction(tr("复制"));
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &DataTableWidget::onCopyClicked);

    // 导出按钮
    m_exportAction = m_toolBar->addAction(tr("导出CSV"));
    connect(m_exportAction, &QAction::triggered, this, &DataTableWidget::onExportClicked);

    // 清空按钮
    m_clearAction = m_toolBar->addAction(tr("清空"));
    connect(m_clearAction, &QAction::triggered, this, &DataTableWidget::onClearClicked);
}

void DataTableWidget::setupTable()
{
    m_tableView = new QTableView(this);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setSortingEnabled(true);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->verticalHeader()->setDefaultSectionSize(22);

    // 创建数据模型
    m_model = new QStandardItemModel(0, ColCount, this);
    m_model->setHorizontalHeaderLabels({
        tr("序号"),
        tr("时间"),
        tr("方向"),
        tr("HEX"),
        tr("ASCII"),
        tr("解析数值"),
        tr("协议"),
        tr("描述")
    });

    // 创建代理模型用于过滤和排序
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1);  // 所有列

    m_tableView->setModel(m_proxyModel);

    // 设置列宽
    m_tableView->setColumnWidth(ColIndex, 60);
    m_tableView->setColumnWidth(ColTimestamp, 120);
    m_tableView->setColumnWidth(ColDirection, 50);
    m_tableView->setColumnWidth(ColHex, 300);
    m_tableView->setColumnWidth(ColAscii, 200);
    m_tableView->setColumnWidth(ColValues, 150);
    m_tableView->setColumnWidth(ColProtocol, 80);

    // 双击信号
    connect(m_tableView, &QTableView::doubleClicked,
            this, &DataTableWidget::onTableDoubleClicked);
}

void DataTableWidget::retranslateUi()
{
    m_pauseAction->setText(m_paused ? tr("继续") : tr("暂停"));
    m_copyAction->setText(tr("复制"));
    m_exportAction->setText(tr("导出CSV"));
    m_clearAction->setText(tr("清空"));
    m_autoScrollCheck->setText(tr("自动滚动"));
    m_filterEdit->setPlaceholderText(tr("输入关键字过滤..."));
    updateStatusLabel();
}

void DataTableWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void DataTableWidget::addReceivedData(const QByteArray& data,
                                       const QString& protocol,
                                       const QVector<double>& parsedValues)
{
    if (m_paused || !m_showReceived) {
        return;
    }

    TableDataRecord record;
    record.index = ++m_recordIndex;
    record.timestamp = QDateTime::currentDateTime();
    record.direction = "RX";
    record.rawData = data;
    record.parsedValues = parsedValues;
    record.protocol = protocol;

    QMutexLocker locker(&m_mutex);
    m_pendingRecords.append(record);
    if (m_pendingRecords.size() > m_maxRecords) {
        m_pendingRecords.erase(m_pendingRecords.begin(),
                               m_pendingRecords.begin() + (m_pendingRecords.size() - m_maxRecords));
    }
}

void DataTableWidget::addSentData(const QByteArray& data, const QString& protocol)
{
    if (m_paused || !m_showSent) {
        return;
    }

    TableDataRecord record;
    record.index = ++m_recordIndex;
    record.timestamp = QDateTime::currentDateTime();
    record.direction = "TX";
    record.rawData = data;
    record.protocol = protocol;

    QMutexLocker locker(&m_mutex);
    m_pendingRecords.append(record);
    if (m_pendingRecords.size() > m_maxRecords) {
        m_pendingRecords.erase(m_pendingRecords.begin(),
                               m_pendingRecords.begin() + (m_pendingRecords.size() - m_maxRecords));
    }
}

void DataTableWidget::updateDisplay()
{
    QVector<TableDataRecord> recordsToAdd;
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingRecords.isEmpty()) {
            return;
        }
        recordsToAdd = m_pendingRecords;
        m_pendingRecords.clear();
    }

    // 批量添加记录，避免每条数据都触发表格视图和代理模型的中间刷新。
    addRecords(recordsToAdd);

    // 裁剪过多的记录
    trimRecords();

    // 更新状态
    updateStatusLabel();

    // 自动滚动
    if (m_autoScroll) {
        m_tableView->scrollToBottom();
    }
}

void DataTableWidget::addRecords(const QVector<TableDataRecord>& records)
{
    /*
     * 高频接收时 updateDisplay 一次可能拿到几十到几百条记录。
     * 逐条 appendRow 会反复触发代理过滤/排序、视图布局和 repaint；
     * 这里把 UI 更新窗口合并为一次，显示内容、排序能力和过滤结果保持不变。
     */
    if (records.isEmpty()) {
        return;
    }

    const bool sortingEnabled = m_tableView->isSortingEnabled();
    const int sortColumn = m_tableView->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = m_tableView->horizontalHeader()->sortIndicatorOrder();
    const bool dynamicSortFilter = m_proxyModel->dynamicSortFilter();

    m_tableView->setUpdatesEnabled(false);
    m_tableView->setSortingEnabled(false);
    m_proxyModel->setDynamicSortFilter(false);

    m_records.reserve(m_records.size() + records.size());
    for (const auto& record : records) {
        m_records.append(record);

        QList<QStandardItem*> items;

        // 序号
        auto* indexItem = new QStandardItem(QString::number(record.index));
        indexItem->setTextAlignment(Qt::AlignCenter);
        items.append(indexItem);

        // 时间戳
        auto* timeItem = new QStandardItem(record.timestamp.toString("HH:mm:ss.zzz"));
        timeItem->setTextAlignment(Qt::AlignCenter);
        items.append(timeItem);

        // 方向
        auto* dirItem = new QStandardItem(record.direction);
        dirItem->setTextAlignment(Qt::AlignCenter);
        if (record.direction == "RX") {
            dirItem->setForeground(QColor("#2196F3"));  // 蓝色
        } else {
            dirItem->setForeground(QColor("#4CAF50"));  // 绿色
        }
        items.append(dirItem);

        // HEX
        items.append(new QStandardItem(formatHexString(record.rawData)));

        // ASCII
        items.append(new QStandardItem(formatAsciiString(record.rawData)));

        // 解析数值
        items.append(new QStandardItem(formatParsedValues(record.parsedValues)));

        // 协议
        auto* protoItem = new QStandardItem(record.protocol);
        protoItem->setTextAlignment(Qt::AlignCenter);
        items.append(protoItem);

        // 描述
        items.append(new QStandardItem(record.description));

        m_model->appendRow(items);
    }

    m_proxyModel->setDynamicSortFilter(dynamicSortFilter);
    m_proxyModel->invalidate();
    m_tableView->setSortingEnabled(sortingEnabled);
    if (sortingEnabled && sortColumn >= 0) {
        m_tableView->sortByColumn(sortColumn, sortOrder);
    }
    m_tableView->setUpdatesEnabled(true);
}

void DataTableWidget::trimRecords()
{
    /*
     * 裁剪旧记录时一次性删除，避免长时间抓包后逐行 removeFirst/removeRow
     * 造成 O(n²) 搬移和连续 model reset。
     */
    const int overflow = m_records.size() - m_maxRecords;
    if (overflow <= 0) {
        return;
    }

    const bool sortingEnabled = m_tableView->isSortingEnabled();
    const int sortColumn = m_tableView->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = m_tableView->horizontalHeader()->sortIndicatorOrder();
    const bool dynamicSortFilter = m_proxyModel->dynamicSortFilter();

    m_tableView->setUpdatesEnabled(false);
    m_tableView->setSortingEnabled(false);
    m_proxyModel->setDynamicSortFilter(false);

    m_records.erase(m_records.begin(), m_records.begin() + overflow);
    m_model->removeRows(0, qMin(overflow, m_model->rowCount()));

    m_proxyModel->setDynamicSortFilter(dynamicSortFilter);
    m_proxyModel->invalidate();
    m_tableView->setSortingEnabled(sortingEnabled);
    if (sortingEnabled && sortColumn >= 0) {
        m_tableView->sortByColumn(sortColumn, sortOrder);
    }
    m_tableView->setUpdatesEnabled(true);
}

void DataTableWidget::clearAll()
{
    QMutexLocker locker(&m_mutex);
    m_records.clear();
    m_pendingRecords.clear();
    m_model->removeRows(0, m_model->rowCount());
    m_recordIndex = 0;
    updateStatusLabel();
}

void DataTableWidget::setMaxRecords(int maxRecords)
{
    m_maxRecords = qMax(100, maxRecords);
    trimRecords();
}

void DataTableWidget::setShowReceived(bool show)
{
    m_showReceived = show;
}

void DataTableWidget::setShowSent(bool show)
{
    m_showSent = show;
}

void DataTableWidget::setAutoScroll(bool enabled)
{
    m_autoScroll = enabled;
    m_autoScrollCheck->setChecked(enabled);
}

void DataTableWidget::setPaused(bool paused)
{
    m_paused = paused;
    m_pauseAction->setChecked(paused);
    m_pauseAction->setText(paused ? tr("继续") : tr("暂停"));
}

void DataTableWidget::setFilterText(const QString& text)
{
    m_filterEdit->setText(text);
}

QString DataTableWidget::formatHexString(const QByteArray& data)
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) {
            result += ' ';
        }
        result += QString("%1").arg(static_cast<unsigned char>(data.at(i)), 2, 16, QChar('0')).toUpper();
    }
    return result;
}

QString DataTableWidget::formatAsciiString(const QByteArray& data)
{
    QString result;
    for (char c : data) {
        if (c >= 32 && c < 127) {
            result += c;
        } else if (c == '\r') {
            result += "\\r";
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\t') {
            result += "\\t";
        } else {
            result += '.';
        }
    }
    return result;
}

QString DataTableWidget::formatParsedValues(const QVector<double>& values)
{
    if (values.isEmpty()) {
        return QString();
    }

    QStringList parts;
    for (double v : values) {
        parts.append(QString::number(v, 'f', 3));
    }
    return parts.join(", ");
}

void DataTableWidget::updateStatusLabel()
{
    m_statusLabel->setText(tr("共 %1 条记录").arg(m_records.size()));
    emit recordCountChanged(m_records.size());
}

void DataTableWidget::onFilterTextChanged(const QString& text)
{
    m_proxyModel->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
}

void DataTableWidget::onDirectionFilterChanged(int index)
{
    QString filter = m_directionCombo->itemData(index).toString();
    if (filter == "ALL") {
        m_showReceived = true;
        m_showSent = true;
    } else if (filter == "RX") {
        m_showReceived = true;
        m_showSent = false;
    } else if (filter == "TX") {
        m_showReceived = false;
        m_showSent = true;
    }
}

void DataTableWidget::onClearClicked()
{
    clearAll();
}

void DataTableWidget::onExportClicked()
{
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("导出CSV"),
        QString(),
        tr("CSV文件 (*.csv)")
    );

    if (filename.isEmpty()) {
        return;
    }

    if (exportToCsv(filename)) {
        QMessageBox::information(this, tr("导出成功"),
                                  tr("数据已导出到: %1").arg(filename));
    } else {
        QMessageBox::warning(this, tr("导出失败"),
                              tr("无法写入文件: %1").arg(filename));
    }
}

void DataTableWidget::onCopyClicked()
{
    copySelected();
}

void DataTableWidget::onAutoScrollToggled(bool checked)
{
    m_autoScroll = checked;
}

void DataTableWidget::onPauseToggled(bool checked)
{
    m_paused = checked;
    m_pauseAction->setText(checked ? tr("继续") : tr("暂停"));
}

void DataTableWidget::onTableDoubleClicked(const QModelIndex& index)
{
    int sourceRow = m_proxyModel->mapToSource(index).row();
    if (sourceRow >= 0 && sourceRow < m_records.size()) {
        emit recordDoubleClicked(m_records[sourceRow]);
    }
}

void DataTableWidget::copySelected()
{
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    QString text;
    // 添加表头
    QStringList headers;
    for (int col = 0; col < m_model->columnCount(); ++col) {
        headers.append(m_model->headerData(col, Qt::Horizontal).toString());
    }
    text += headers.join('\t') + '\n';

    // 添加数据行
    for (const auto& proxyIndex : selected) {
        QStringList row;
        for (int col = 0; col < m_model->columnCount(); ++col) {
            QModelIndex cellIndex = m_proxyModel->index(proxyIndex.row(), col);
            row.append(m_proxyModel->data(cellIndex).toString());
        }
        text += row.join('\t') + '\n';
    }

    QApplication::clipboard()->setText(text);
}

QVector<TableDataRecord> DataTableWidget::selectedRecords() const
{
    QVector<TableDataRecord> result;
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    for (const auto& proxyIndex : selected) {
        int sourceRow = m_proxyModel->mapToSource(proxyIndex).row();
        if (sourceRow >= 0 && sourceRow < m_records.size()) {
            result.append(m_records[sourceRow]);
        }
    }
    return result;
}

bool DataTableWidget::exportToCsv(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    // 写入BOM以便Excel正确识别UTF-8
    stream << "\xEF\xBB\xBF";

    // 写入表头
    QStringList headers;
    for (int col = 0; col < m_model->columnCount(); ++col) {
        headers.append(m_model->headerData(col, Qt::Horizontal).toString());
    }
    stream << headers.join(',') << '\n';

    // 写入数据
    for (int row = 0; row < m_model->rowCount(); ++row) {
        QStringList values;
        for (int col = 0; col < m_model->columnCount(); ++col) {
            QString value = m_model->data(m_model->index(row, col)).toString();
            // CSV转义：如果包含逗号、引号或换行，需要用引号包围
            if (value.contains(',') || value.contains('"') || value.contains('\n')) {
                value = '"' + value.replace("\"", "\"\"") + '"';
            }
            values.append(value);
        }
        stream << values.join(',') << '\n';
    }

    file.close();
    return true;
}

} // namespace ComAssistant
