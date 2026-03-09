/**
 * @file DebugModeWidget.cpp
 * @brief 调试模式组件实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "DebugModeWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QAction>
#include <QPalette>
#include <QScrollBar>

namespace ComAssistant {

DebugModeWidget::DebugModeWidget(QWidget* parent)
    : IModeWidget(parent)
{
    setupUi();
    setupToolBar();
}

void DebugModeWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 主分割器（上下）
    m_mainSplitter = new QSplitter(Qt::Vertical);

    // 上部：记录表格
    QWidget* topWidget = new QWidget;
    QVBoxLayout* topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(5, 5, 5, 5);

    // 过滤器
    QHBoxLayout* filterLayout = new QHBoxLayout;
    m_filterLabel = new QLabel(tr("过滤:"));
    filterLayout->addWidget(m_filterLabel);
    m_filterEdit = new QLineEdit;
    m_filterEdit->setPlaceholderText(tr("输入关键字过滤..."));
    connect(m_filterEdit, &QLineEdit::textChanged, this, &DebugModeWidget::onFilterChanged);
    filterLayout->addWidget(m_filterEdit);

    m_txCheck = new QCheckBox(tr("TX"));
    m_txCheck->setChecked(true);
    m_rxCheck = new QCheckBox(tr("RX"));
    m_rxCheck->setChecked(true);
    filterLayout->addWidget(m_txCheck);
    filterLayout->addWidget(m_rxCheck);

    topLayout->addLayout(filterLayout);

    // 记录表格
    m_recordTable = new QTableWidget;
    m_recordTable->setColumnCount(5);
    m_recordTable->setHorizontalHeaderLabels({tr("#"), tr("时间"), tr("△t"), tr("方向"), tr("数据")});
    m_recordTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_recordTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_recordTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_recordTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_recordTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_recordTable->setColumnWidth(0, 50);
    m_recordTable->setColumnWidth(1, 100);
    m_recordTable->setColumnWidth(2, 70);
    m_recordTable->setColumnWidth(3, 40);
    m_recordTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recordTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recordTable->setAlternatingRowColors(true);
    m_recordTable->setFont(QFont("Consolas", 9));
    connect(m_recordTable, &QTableWidget::cellClicked, this, &DebugModeWidget::onRecordSelected);
    topLayout->addWidget(m_recordTable);

    // 智能滚屏指示器
    m_smartScrollIndicator = new QLabel(tr("已暂停滚动 - 滚动到底部恢复"));
    m_smartScrollIndicator->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(255, 193, 7, 0.9);"
        "  color: #333;"
        "  padding: 6px 12px;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "}"
    );
    m_smartScrollIndicator->setAlignment(Qt::AlignCenter);
    m_smartScrollIndicator->hide();
    topLayout->addWidget(m_smartScrollIndicator);

    // 连接滚动条信号实现智能滚屏
    connect(m_recordTable->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &DebugModeWidget::onTableScrollValueChanged);

    m_mainSplitter->addWidget(topWidget);

    // 下部分割器（左右）
    m_bottomSplitter = new QSplitter(Qt::Horizontal);

    // 左下：详情视图
    QWidget* detailWidget = new QWidget;
    QVBoxLayout* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(5, 5, 5, 5);

    m_detailLabel = new QLabel(tr("数据详情"));
    m_detailLabel->setStyleSheet("font-weight: bold;");
    detailLayout->addWidget(m_detailLabel);

    m_detailView = new QTextEdit;
    m_detailView->setReadOnly(true);
    m_detailView->setFont(QFont("Consolas", 10));
    detailLayout->addWidget(m_detailView);

    m_bottomSplitter->addWidget(detailWidget);

    // 右下：监视变量
    QWidget* watchWidget = new QWidget;
    QVBoxLayout* watchLayout = new QVBoxLayout(watchWidget);
    watchLayout->setContentsMargins(5, 5, 5, 5);

    m_watchLabel = new QLabel(tr("监视变量"));
    m_watchLabel->setStyleSheet("font-weight: bold;");
    watchLayout->addWidget(m_watchLabel);

    m_watchTree = new QTreeWidget;
    m_watchTree->setHeaderLabels({tr("名称"), tr("值"), tr("类型")});
    m_watchTree->setColumnWidth(0, 100);
    m_watchTree->setColumnWidth(1, 100);
    m_watchTree->setAlternatingRowColors(true);
    watchLayout->addWidget(m_watchTree);

    // 添加一些示例监视变量
    QTreeWidgetItem* item1 = new QTreeWidgetItem({tr("帧计数"), "0", "int"});
    QTreeWidgetItem* item2 = new QTreeWidgetItem({tr("错误计数"), "0", "int"});
    QTreeWidgetItem* item3 = new QTreeWidgetItem({tr("最后命令"), "-", "string"});
    m_watchTree->addTopLevelItem(item1);
    m_watchTree->addTopLevelItem(item2);
    m_watchTree->addTopLevelItem(item3);

    m_bottomSplitter->addWidget(watchWidget);
    m_bottomSplitter->setStretchFactor(0, 2);
    m_bottomSplitter->setStretchFactor(1, 1);

    m_mainSplitter->addWidget(m_bottomSplitter);
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_mainSplitter);

    // 发送区
    QWidget* sendWidget = new QWidget;
    sendWidget->setObjectName("debugSendWidget");
    QHBoxLayout* sendLayout = new QHBoxLayout(sendWidget);
    sendLayout->setContentsMargins(10, 8, 10, 8);

    m_sendLabel = new QLabel(tr("发送:"));
    sendLayout->addWidget(m_sendLabel);

    m_sendEdit = new QLineEdit;
    m_sendEdit->setPlaceholderText(tr("输入数据，回车发送..."));
    m_sendEdit->setFont(QFont("Consolas", 10));
    connect(m_sendEdit, &QLineEdit::returnPressed, this, &DebugModeWidget::onSendClicked);
    sendLayout->addWidget(m_sendEdit);

    m_sendHexCheck = new QCheckBox(tr("HEX"));
    sendLayout->addWidget(m_sendHexCheck);

    m_sendBtn = new QPushButton(tr("发送"));
    connect(m_sendBtn, &QPushButton::clicked, this, &DebugModeWidget::onSendClicked);
    sendLayout->addWidget(m_sendBtn);

    mainLayout->addWidget(sendWidget);
}

void DebugModeWidget::setupToolBar()
{
    m_toolBar = new QToolBar;

    // 暂停/继续
    QAction* pauseAction = m_toolBar->addAction(tr("暂停"));
    pauseAction->setCheckable(true);
    connect(pauseAction, &QAction::triggered, this, &DebugModeWidget::onTogglePause);

    // 单步
    QAction* stepAction = m_toolBar->addAction(tr("步进"));
    stepAction->setEnabled(false);
    connect(stepAction, &QAction::triggered, this, &DebugModeWidget::onStepNext);

    m_toolBar->addSeparator();

    // 断点
    QAction* breakpointAction = m_toolBar->addAction(tr("添加断点"));
    connect(breakpointAction, &QAction::triggered, this, &DebugModeWidget::onAddBreakpoint);

    m_toolBar->addSeparator();

    // HEX显示
    QAction* hexAction = m_toolBar->addAction(tr("HEX"));
    hexAction->setCheckable(true);
    hexAction->setChecked(true);
    connect(hexAction, &QAction::triggered, this, &DebugModeWidget::onToggleHexDisplay);

    // 时间差
    QAction* timeDiffAction = m_toolBar->addAction(tr("时间差"));
    timeDiffAction->setCheckable(true);
    timeDiffAction->setChecked(true);
    connect(timeDiffAction, &QAction::triggered, this, &DebugModeWidget::onToggleTimeDiff);

    m_toolBar->addSeparator();

    // 清空
    QAction* clearAction = m_toolBar->addAction(tr("清空"));
    connect(clearAction, &QAction::triggered, this, &DebugModeWidget::onClearRecords);

    // 导出
    QAction* exportAction = m_toolBar->addAction(tr("导出"));
    connect(exportAction, &QAction::triggered, this, &DebugModeWidget::onExportRecords);

    m_toolBar->addSeparator();

    // 统计
    m_statsLabel = new QLabel(tr("TX: 0 | RX: 0"));
    m_statsLabel->setObjectName("debugStatsLabel");
    m_toolBar->addWidget(m_statsLabel);
}

void DebugModeWidget::appendReceivedData(const QByteArray& data)
{
    // 按行处理数据
    m_rxBuffer.append(data);

    int pos;
    while ((pos = m_rxBuffer.indexOf('\n')) >= 0) {
        QByteArray line = m_rxBuffer.left(pos);
        m_rxBuffer = m_rxBuffer.mid(pos + 1);

        if (line.endsWith('\r')) {
            line.chop(1);
        }

        if (line.isEmpty()) continue;

        DebugDataRecord record;
        record.index = ++m_recordIndex;
        record.timestamp = QDateTime::currentDateTime();
        record.isTx = false;
        record.data = line;
        record.isBreakpoint = checkBreakpoint(line);

        if (m_paused) {
            m_pendingRecords.append(record);
        } else {
            addRecord(record);
            m_totalRx++;
        }
    }

    // 更新统计
    if (m_statsLabel) {
        m_statsLabel->setText(QString("TX: %1 | RX: %2").arg(m_totalTx).arg(m_totalRx));
    }

    emit dataReceived(data);
}

void DebugModeWidget::appendSentData(const QByteArray& data)
{
    DebugDataRecord record;
    record.index = ++m_recordIndex;
    record.timestamp = QDateTime::currentDateTime();
    record.isTx = true;
    record.data = data;

    if (m_paused) {
        m_pendingRecords.append(record);
    } else {
        addRecord(record);
        m_totalTx++;
    }

    // 更新统计
    if (m_statsLabel) {
        m_statsLabel->setText(QString("TX: %1 | RX: %2").arg(m_totalTx).arg(m_totalRx));
    }
}

void DebugModeWidget::addRecord(const DebugDataRecord& record)
{
    m_records.append(record);

    int row = m_recordTable->rowCount();
    m_recordTable->insertRow(row);

    // 序号
    QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(record.index));
    if (record.isBreakpoint) {
        indexItem->setBackground(QColor(255, 200, 200));
        indexItem->setText("● " + QString::number(record.index));
    }
    m_recordTable->setItem(row, 0, indexItem);

    // 时间
    m_recordTable->setItem(row, 1, new QTableWidgetItem(record.timestamp.toString("hh:mm:ss.zzz")));

    // 时间差
    QString timeDiff = "-";
    if (m_showTimeDiff && m_lastTimestamp.isValid()) {
        timeDiff = formatTimeDiff(m_lastTimestamp, record.timestamp);
    }
    m_recordTable->setItem(row, 2, new QTableWidgetItem(timeDiff));
    m_lastTimestamp = record.timestamp;

    // 方向
    QTableWidgetItem* dirItem = new QTableWidgetItem(record.isTx ? "▶" : "◀");
    dirItem->setForeground(record.isTx ? QColor(33, 150, 243) : QColor(76, 175, 80));
    dirItem->setTextAlignment(Qt::AlignCenter);
    m_recordTable->setItem(row, 3, dirItem);

    // 数据
    QString dataStr = formatData(record.data, m_hexDisplay);
    m_recordTable->setItem(row, 4, new QTableWidgetItem(dataStr));

    // 智能滚屏：仅在未暂停时滚动到底部
    if (!m_paused && !m_smartScrollPaused) {
        m_recordTable->scrollToBottom();
    }

    // 如果命中断点，自动暂停
    if (record.isBreakpoint) {
        setPaused(true);
        emit statusMessage(tr("命中断点 #%1").arg(record.index));
    }
}

void DebugModeWidget::onRecordSelected(int row, int column)
{
    Q_UNUSED(column)
    if (row >= 0 && row < m_records.size()) {
        updateRecordDetail(m_records[row]);
    }
}

void DebugModeWidget::updateRecordDetail(const DebugDataRecord& record)
{
    // 检测当前是否为深色主题（通过背景亮度判断）
    QColor bgColor = m_detailView->palette().color(QPalette::Base);
    bool isDark = bgColor.lightness() < 128;

    // 根据主题选择颜色
    QString bgPreview = isDark ? "#1e1e1e" : "#f0f0f0";
    QString textColor = isDark ? "#e0e0e0" : "#333333";
    QString borderColor = isDark ? "#3d3d3d" : "#d0d0d0";

    QString html;
    html += QString("<div style='color:%1;'>").arg(textColor);
    html += QString("<h3>记录 #%1</h3>").arg(record.index);
    html += QString("<p><b>时间:</b> %1</p>").arg(record.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"));
    html += QString("<p><b>方向:</b> <span style='color:%1'>%2</span></p>")
        .arg(record.isTx ? "#2196F3" : "#4CAF50")
        .arg(record.isTx ? tr("发送 (TX)") : tr("接收 (RX)"));
    html += QString("<p><b>长度:</b> %1 字节</p>").arg(record.data.size());

    if (record.isBreakpoint) {
        html += "<p><b style='color:red'>● 断点</b></p>";
    }

    html += QString("<hr style='border-color:%1;'>").arg(borderColor);
    html += QString("<h4>HEX:</h4><pre style='background:%1;color:%2;padding:10px;border-radius:4px;border:1px solid %3;font-family:Consolas;'>")
        .arg(bgPreview).arg(textColor).arg(borderColor);
    QString hexStr;
    for (int i = 0; i < record.data.size(); ++i) {
        hexStr += QString("%1 ").arg(static_cast<unsigned char>(record.data[i]), 2, 16, QChar('0')).toUpper();
        if ((i + 1) % 16 == 0) hexStr += "\n";
    }
    html += hexStr + "</pre>";

    html += QString("<h4>ASCII:</h4><pre style='background:%1;color:%2;padding:10px;border-radius:4px;border:1px solid %3;font-family:Consolas;'>")
        .arg(bgPreview).arg(textColor).arg(borderColor);
    for (unsigned char byte : record.data) {
        html += (byte >= 32 && byte < 127) ? QChar(byte) : '.';
    }
    html += "</pre></div>";

    m_detailView->setHtml(html);
}

QString DebugModeWidget::formatData(const QByteArray& data, bool hex)
{
    if (hex) {
        QString result;
        for (unsigned char byte : data) {
            result += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
        }
        return result.trimmed();
    } else {
        return QString::fromUtf8(data);
    }
}

QString DebugModeWidget::formatTimeDiff(const QDateTime& prev, const QDateTime& current)
{
    qint64 diff = prev.msecsTo(current);
    if (diff < 1000) {
        return QString("+%1ms").arg(diff);
    } else if (diff < 60000) {
        return QString("+%1s").arg(diff / 1000.0, 0, 'f', 2);
    } else {
        return QString("+%1m").arg(diff / 60000.0, 0, 'f', 1);
    }
}

bool DebugModeWidget::checkBreakpoint(const QByteArray& data)
{
    for (const QByteArray& pattern : m_breakpoints) {
        if (data.contains(pattern)) {
            return true;
        }
    }
    return false;
}

void DebugModeWidget::onTogglePause()
{
    setPaused(!m_paused);
}

void DebugModeWidget::setPaused(bool paused)
{
    m_paused = paused;

    // 更新工具栏按钮状态
    QList<QAction*> actions = m_toolBar->actions();
    for (QAction* action : actions) {
        if (action->text() == tr("暂停")) {
            action->setChecked(paused);
            action->setText(paused ? tr("继续") : tr("暂停"));
        } else if (action->text() == tr("步进")) {
            action->setEnabled(paused);
        }
    }

    // 如果取消暂停，显示所有待处理记录
    if (!paused) {
        for (const DebugDataRecord& record : m_pendingRecords) {
            addRecord(record);
        }
        m_pendingRecords.clear();
    }
}

void DebugModeWidget::onStepNext()
{
    if (!m_pendingRecords.isEmpty()) {
        addRecord(m_pendingRecords.takeFirst());
    }
}

void DebugModeWidget::onToggleHexDisplay()
{
    m_hexDisplay = !m_hexDisplay;
    // 刷新显示
    for (int row = 0; row < m_recordTable->rowCount(); ++row) {
        if (row < m_records.size()) {
            QString dataStr = formatData(m_records[row].data, m_hexDisplay);
            m_recordTable->item(row, 4)->setText(dataStr);
        }
    }
}

void DebugModeWidget::onToggleTimeDiff()
{
    m_showTimeDiff = !m_showTimeDiff;
}

void DebugModeWidget::onAddBreakpoint()
{
    bool ok;
    QString pattern = QInputDialog::getText(this, tr("添加断点"),
        tr("输入断点模式 (HEX 或文本):"), QLineEdit::Normal, "", &ok);

    if (ok && !pattern.isEmpty()) {
        // 尝试作为 HEX 解析
        QString cleanPattern = pattern.simplified().remove(' ');
        QByteArray bytes;
        if (cleanPattern.length() % 2 == 0) {
            bytes = QByteArray::fromHex(cleanPattern.toLatin1());
            if (!bytes.isEmpty()) {
                m_breakpoints.append(bytes);
                emit statusMessage(tr("添加断点: %1").arg(bytes.toHex(' ').toUpper().constData()));
                return;
            }
        }
        // 作为文本处理
        m_breakpoints.append(pattern.toUtf8());
        emit statusMessage(tr("添加断点: %1").arg(pattern));
    }
}

void DebugModeWidget::onClearRecords()
{
    clear();
}

void DebugModeWidget::onExportRecords()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("导出调试记录"),
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_debug.log",
        tr("日志文件 (*.log);;CSV 文件 (*.csv)"));

    if (fileName.isEmpty()) return;
    exportToFile(fileName);
}

void DebugModeWidget::onFilterChanged(const QString& text)
{
    for (int row = 0; row < m_recordTable->rowCount(); ++row) {
        bool match = text.isEmpty();
        if (!match) {
            for (int col = 0; col < m_recordTable->columnCount(); ++col) {
                QTableWidgetItem* item = m_recordTable->item(row, col);
                if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
        }
        m_recordTable->setRowHidden(row, !match);
    }
}

void DebugModeWidget::clear()
{
    m_records.clear();
    m_pendingRecords.clear();
    m_recordTable->setRowCount(0);
    m_detailView->clear();
    m_rxBuffer.clear();
    m_txBuffer.clear();
    m_lastTimestamp = QDateTime();
    m_totalTx = 0;
    m_totalRx = 0;
    m_recordIndex = 0;

    // 重置智能滚屏状态
    m_smartScrollPaused = false;
    hideSmartScrollIndicator();

    if (m_statsLabel) {
        m_statsLabel->setText("TX: 0 | RX: 0");
    }
}

bool DebugModeWidget::exportToFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);

    if (fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        stream << "Index,Timestamp,Direction,Length,HexData,ASCII\n";
        for (const DebugDataRecord& record : m_records) {
            stream << record.index << ","
                   << record.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << ","
                   << (record.isTx ? "TX" : "RX") << ","
                   << record.data.size() << ","
                   << record.data.toHex(' ').toUpper() << ","
                   << QString::fromUtf8(record.data).replace(",", ";") << "\n";
        }
    } else {
        for (const DebugDataRecord& record : m_records) {
            stream << "[" << record.timestamp.toString("hh:mm:ss.zzz") << "] "
                   << (record.isTx ? "[TX]" : "[RX]") << " "
                   << record.data.toHex(' ').toUpper() << "\n";
        }
    }

    file.close();
    emit statusMessage(tr("已导出 %1 条记录").arg(m_records.size()));
    return true;
}

void DebugModeWidget::onActivated()
{
    // 激活时可以恢复状态
}

void DebugModeWidget::onDeactivated()
{
    // 停用时保存状态
}

void DebugModeWidget::onSendClicked()
{
    if (!m_sendEdit) return;

    QString text = m_sendEdit->text();
    if (text.isEmpty()) return;

    QByteArray data;
    if (m_sendHexCheck && m_sendHexCheck->isChecked()) {
        // HEX模式：移除空格后转换
        QString cleanHex = text.simplified().remove(' ');
        data = QByteArray::fromHex(cleanHex.toLatin1());
    } else {
        // 文本模式
        data = text.toUtf8();
    }

    if (!data.isEmpty()) {
        emit sendDataRequested(data);
        m_sendEdit->clear();
    }
}

void DebugModeWidget::addBreakpoint(const QByteArray& pattern)
{
    if (!m_breakpoints.contains(pattern)) {
        m_breakpoints.append(pattern);
    }
}

void DebugModeWidget::removeBreakpoint(const QByteArray& pattern)
{
    m_breakpoints.removeAll(pattern);
}

void DebugModeWidget::clearBreakpoints()
{
    m_breakpoints.clear();
}

void DebugModeWidget::setWatchVariable(const QString& name, const QVariant& value)
{
    for (int i = 0; i < m_watchTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_watchTree->topLevelItem(i);
        if (item->text(0) == name) {
            item->setText(1, value.toString());
            return;
        }
    }
    // 添加新变量
    QTreeWidgetItem* item = new QTreeWidgetItem({name, value.toString(), value.typeName()});
    m_watchTree->addTopLevelItem(item);
}

void DebugModeWidget::clearWatchVariables()
{
    m_watchTree->clear();
}

void DebugModeWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    IModeWidget::changeEvent(event);
}

void DebugModeWidget::retranslateUi()
{
    // 过滤区
    if (m_filterLabel) m_filterLabel->setText(tr("过滤:"));
    if (m_filterEdit) m_filterEdit->setPlaceholderText(tr("输入关键字过滤..."));
    if (m_txCheck) m_txCheck->setText(tr("TX"));
    if (m_rxCheck) m_rxCheck->setText(tr("RX"));

    // 表格头
    m_recordTable->setHorizontalHeaderLabels({tr("#"), tr("时间"), tr("△t"), tr("方向"), tr("数据")});

    // 详情区
    if (m_detailLabel) m_detailLabel->setText(tr("数据详情"));

    // 监视区
    if (m_watchLabel) m_watchLabel->setText(tr("监视变量"));
    m_watchTree->setHeaderLabels({tr("名称"), tr("值"), tr("类型")});

    // 发送区
    if (m_sendLabel) m_sendLabel->setText(tr("发送:"));
    if (m_sendEdit) m_sendEdit->setPlaceholderText(tr("输入数据，回车发送..."));
    if (m_sendHexCheck) m_sendHexCheck->setText(tr("HEX"));
    if (m_sendBtn) m_sendBtn->setText(tr("发送"));

    // 工具栏动作 - 通过遍历更新
    QList<QAction*> actions = m_toolBar->actions();
    for (QAction* action : actions) {
        QString objName = action->objectName();
        if (objName.isEmpty()) {
            // 没有对象名的action根据当前文本判断
            if (action->text() == "HEX") {
                // 保持不变
            }
        }
    }

    // 统计标签
    if (m_statsLabel) {
        m_statsLabel->setText(QString("TX: %1 | RX: %2").arg(m_totalTx).arg(m_totalRx));
    }

    // 更新智能滚屏指示器文本
    if (m_smartScrollIndicator && m_smartScrollPaused) {
        m_smartScrollIndicator->setText(tr("已暂停滚动 - 滚动到底部恢复"));
    }
}

void DebugModeWidget::onTableScrollValueChanged(int value)
{
    QScrollBar* scrollBar = m_recordTable->verticalScrollBar();
    int maxValue = scrollBar->maximum();

    // 判断是否在底部（容差10像素）
    bool atBottom = (value >= maxValue - 10) || (maxValue == 0);

    if (!atBottom && !m_smartScrollPaused) {
        // 用户向上滚动，暂停自动滚动
        m_smartScrollPaused = true;
        showSmartScrollIndicator(tr("已暂停滚动 - 滚动到底部恢复"));
    } else if (atBottom && m_smartScrollPaused) {
        // 滚动到底部，恢复自动滚动
        m_smartScrollPaused = false;
        hideSmartScrollIndicator();
    }
}

void DebugModeWidget::showSmartScrollIndicator(const QString& message)
{
    if (m_smartScrollIndicator) {
        m_smartScrollIndicator->setText(message);
        m_smartScrollIndicator->show();
    }
}

void DebugModeWidget::hideSmartScrollIndicator()
{
    if (m_smartScrollIndicator) {
        m_smartScrollIndicator->hide();
    }
}

} // namespace ComAssistant
