/**
 * @file FrameModeWidget.cpp
 * @brief 帧模式组件实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "FrameModeWidget.h"
#include "utils/ChecksumUtils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <QPalette>
#include <QSizePolicy>

namespace ComAssistant {

FrameModeWidget::FrameModeWidget(QWidget* parent)
    : IModeWidget(parent)
{
    setupUi();
    setupToolBar();

    // 超时定时器
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &FrameModeWidget::onFrameTimeout);

    m_frameFlushTimer = new QTimer(this);
    m_frameFlushTimer->setSingleShot(true);
    connect(m_frameFlushTimer, &QTimer::timeout, this, &FrameModeWidget::flushPendingFrames);

    // 默认帧配置
    m_config.header = QByteArray::fromHex("AA");
    m_config.footer = QByteArray::fromHex("55");
    m_config.timeout = 100;
}

void FrameModeWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 主分割器
    m_splitter = new QSplitter(Qt::Horizontal);

    // 左侧：帧列表
    QWidget* leftWidget = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(5, 5, 5, 5);

    // 过滤器
    QHBoxLayout* filterLayout = new QHBoxLayout;
    m_filterLabel = new QLabel(tr("过滤:"));
    filterLayout->addWidget(m_filterLabel);
    m_filterEdit = new QLineEdit;
    m_filterEdit->setPlaceholderText(tr("输入关键字过滤帧..."));
    connect(m_filterEdit, &QLineEdit::textChanged, this, &FrameModeWidget::onFilterChanged);
    filterLayout->addWidget(m_filterEdit);
    leftLayout->addLayout(filterLayout);

    // 帧表格
    m_frameTable = new QTableWidget;
    m_frameTable->setColumnCount(5);
    m_frameTable->setHorizontalHeaderLabels({tr("#"), tr("时间"), tr("长度"), tr("状态"), tr("数据")});
    m_frameTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_frameTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_frameTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_frameTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_frameTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_frameTable->setColumnWidth(0, 60);
    m_frameTable->setColumnWidth(1, 150);
    m_frameTable->setColumnWidth(2, 80);
    m_frameTable->setColumnWidth(3, 90);
    m_frameTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_frameTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_frameTable->setAlternatingRowColors(true);
    m_frameTable->setFont(QFont("Consolas", 9));
    m_frameTable->setWordWrap(false);
    m_frameTable->horizontalHeader()->setMinimumSectionSize(42);
    connect(m_frameTable, &QTableWidget::cellClicked, this, &FrameModeWidget::onFrameSelected);
    leftLayout->addWidget(m_frameTable);

    // 统计信息
    m_statsLabel = new QLabel;
    m_statsLabel->setObjectName("statsLabel");
    leftLayout->addWidget(m_statsLabel);

    m_splitter->addWidget(leftWidget);

    // 右侧：帧详情
    QWidget* rightWidget = new QWidget;
    rightWidget->setMinimumWidth(280);
    rightWidget->setMaximumWidth(520);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(5, 5, 5, 5);

    m_detailLabel = new QLabel(tr("帧详情"));
    m_detailLabel->setStyleSheet("font-weight: bold;");
    rightLayout->addWidget(m_detailLabel);

    m_detailView = new QTextEdit;
    m_detailView->setReadOnly(true);
    m_detailView->setFont(QFont("Consolas", 10));
    rightLayout->addWidget(m_detailView);

    m_splitter->addWidget(rightWidget);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);

    mainLayout->addWidget(m_splitter);

    // 发送区：固定为一条紧凑操作栏，避免按钮在宽屏或窄屏下被纵向拉伸成大块。
    QWidget* sendWidget = new QWidget;
    sendWidget->setObjectName("frameSendWidget");
    sendWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sendWidget->setFixedHeight(58);
    QHBoxLayout* sendLayout = new QHBoxLayout(sendWidget);
    sendLayout->setContentsMargins(10, 8, 10, 8);
    sendLayout->setSpacing(8);

    m_sendLabel = new QLabel(tr("发送帧:"));
    m_sendLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    sendLayout->addWidget(m_sendLabel);
    m_sendEdit = new QLineEdit;
    m_sendEdit->setPlaceholderText(tr("输入十六进制数据，如: 01 02 03"));
    m_sendEdit->setFont(QFont("Consolas", 10));
    m_sendEdit->setMinimumWidth(220);
    m_sendEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sendLayout->addWidget(m_sendEdit, 1);

    m_sendBtn = new QPushButton(tr("发送"));
    m_sendBtn->setMinimumWidth(72);
    m_sendBtn->setFixedHeight(36);
    m_sendBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_sendBtn, &QPushButton::clicked, this, &FrameModeWidget::onSendFrame);
    sendLayout->addWidget(m_sendBtn);

    m_sendWithHeaderBtn = new QPushButton(tr("带帧头尾发送"));
    m_sendWithHeaderBtn->setMinimumWidth(132);
    m_sendWithHeaderBtn->setFixedHeight(36);
    m_sendWithHeaderBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_sendWithHeaderBtn, &QPushButton::clicked, [this]() {
        QString text = m_sendEdit->text().simplified().remove(' ');
        if (text.isEmpty()) return;

        QByteArray data = m_config.header + QByteArray::fromHex(text.toLatin1()) + m_config.footer;
        emit sendDataRequested(data);
    });
    sendLayout->addWidget(m_sendWithHeaderBtn);

    mainLayout->addWidget(sendWidget);
}

void FrameModeWidget::setupToolBar()
{
    m_toolBar = new QToolBar;
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // 帧头设置
    m_headerLabel = new QLabel(tr("帧头:"));
    m_headerLabel->setMinimumWidth(48);
    m_toolBar->addWidget(m_headerLabel);
    m_headerEdit = new QLineEdit("AA");
    m_headerEdit->setMinimumWidth(64);
    m_headerEdit->setMaximumWidth(90);
    m_headerEdit->setFont(QFont("Consolas", 9));
    connect(m_headerEdit, &QLineEdit::textChanged, this, &FrameModeWidget::onConfigChanged);
    m_toolBar->addWidget(m_headerEdit);

    // 帧尾设置
    m_footerLabel = new QLabel(tr("帧尾:"));
    m_footerLabel->setMinimumWidth(48);
    m_toolBar->addWidget(m_footerLabel);
    m_footerEdit = new QLineEdit("55");
    m_footerEdit->setMinimumWidth(64);
    m_footerEdit->setMaximumWidth(90);
    m_footerEdit->setFont(QFont("Consolas", 9));
    connect(m_footerEdit, &QLineEdit::textChanged, this, &FrameModeWidget::onConfigChanged);
    m_toolBar->addWidget(m_footerEdit);

    m_toolBar->addSeparator();

    // 校验方式
    m_checksumLabel = new QLabel(tr("校验:"));
    m_checksumLabel->setMinimumWidth(48);
    m_toolBar->addWidget(m_checksumLabel);
    m_checksumCombo = new QComboBox;
    m_checksumCombo->addItem(tr("无"), 0);
    m_checksumCombo->addItem("XOR", 1);
    m_checksumCombo->addItem("SUM", 2);
    m_checksumCombo->addItem("CRC16", 3);
    m_checksumCombo->setMinimumWidth(100);
    m_checksumCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_checksumCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        m_config.checksumType = index;
    });
    m_toolBar->addWidget(m_checksumCombo);

    m_toolBar->addSeparator();

    // 清空
    m_clearAction = m_toolBar->addAction(tr("清空"));
    connect(m_clearAction, &QAction::triggered, this, &FrameModeWidget::onClearFrames);

    // 导出
    m_exportAction = m_toolBar->addAction(tr("导出"));
    connect(m_exportAction, &QAction::triggered, this, &FrameModeWidget::onExportFrames);
}

void FrameModeWidget::appendReceivedData(const QByteArray& data)
{
    m_buffer.append(data);
    m_timeoutTimer->start(m_config.timeout);
    processBuffer();
}

void FrameModeWidget::appendSentData(const QByteArray& data)
{
    FrameData frame;
    frame.index = ++m_totalFrames;
    frame.timestamp = QDateTime::currentDateTime();
    frame.rawData = data;
    frame.valid = true;
    frame.parsedFields["direction"] = "TX";

    addFrame(frame);
}

void FrameModeWidget::processBuffer()
{
    while (true) {
        // 查找帧头
        int headerPos = m_buffer.indexOf(m_config.header);
        if (headerPos < 0) {
            m_buffer.clear();
            return;
        }

        // 丢弃帧头前的数据
        if (headerPos > 0) {
            m_buffer = m_buffer.mid(headerPos);
        }

        // 查找帧尾
        int footerPos = m_buffer.indexOf(m_config.footer, m_config.header.size());
        if (footerPos < 0) {
            // 帧尾未找到，等待更多数据
            return;
        }

        // 提取完整帧
        int frameEnd = footerPos + m_config.footer.size();
        QByteArray frameData = m_buffer.left(frameEnd);
        m_buffer = m_buffer.mid(frameEnd);

        // 创建帧数据
        FrameData frame;
        frame.index = ++m_totalFrames;
        frame.timestamp = QDateTime::currentDateTime();
        frame.rawData = frameData;
        frame.parsedFields["direction"] = "RX";

        // 验证帧
        QString error;
        frame.valid = validateFrame(frameData, error);
        if (!frame.valid) {
            frame.errorInfo = error;
            m_invalidFrames++;
        } else {
            m_validFrames++;
        }

        addFrame(frame);
        updateStatistics();
    }
}

void FrameModeWidget::onFrameTimeout()
{
    // 超时处理 - 如果缓冲区有数据但没有完整帧，作为无效帧处理
    if (!m_buffer.isEmpty()) {
        FrameData frame;
        frame.index = ++m_totalFrames;
        frame.timestamp = QDateTime::currentDateTime();
        frame.rawData = m_buffer;
        frame.valid = false;
        frame.errorInfo = tr("超时/不完整");
        frame.parsedFields["direction"] = "RX";

        addFrame(frame);
        m_invalidFrames++;
        m_buffer.clear();
        updateStatistics();
    }
}

bool FrameModeWidget::validateFrame(const QByteArray& data, QString& error)
{
    // 检查最小长度
    int minLen = m_config.header.size() + m_config.footer.size();
    if (data.size() < minLen) {
        error = tr("帧太短");
        return false;
    }

    // 检查帧头
    if (!data.startsWith(m_config.header)) {
        error = tr("帧头错误");
        return false;
    }

    // 检查帧尾
    if (!data.endsWith(m_config.footer)) {
        error = tr("帧尾错误");
        return false;
    }

    // 校验（如果启用）
    if (m_config.checksumType > 0) {
        // 简化实现：暂不验证校验和
    }

    return true;
}

void FrameModeWidget::addFrame(const FrameData& frame)
{
    /*
     * 记录先进入内存列表，再批量刷新到表格。这样导出和详情查看仍基于完整
     * FrameData，而 UI 不会因为高频 insertRow/scrollToBottom 被每帧打断。
     */
    m_frames.append(frame);
    m_pendingFrames.append(frame);
    trimFrameRecords();
    scheduleFrameFlush();
}

void FrameModeWidget::fillFrameRow(int row, const FrameData& frame)
{
    // 序号
    m_frameTable->setItem(row, 0, new QTableWidgetItem(QString::number(frame.index)));

    // 时间
    m_frameTable->setItem(row, 1, new QTableWidgetItem(frame.timestamp.toString("hh:mm:ss.zzz")));

    // 长度
    m_frameTable->setItem(row, 2, new QTableWidgetItem(QString::number(frame.rawData.size())));

    // 状态
    QTableWidgetItem* statusItem = new QTableWidgetItem(frame.valid ? "✓" : "✗");
    statusItem->setForeground(frame.valid ? QColor(46, 204, 113) : QColor(231, 76, 60));
    m_frameTable->setItem(row, 3, statusItem);

    // 数据（HEX）
    QString hexStr;
    for (unsigned char byte : frame.rawData) {
        hexStr += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
    }
    m_frameTable->setItem(row, 4, new QTableWidgetItem(hexStr.trimmed()));

    // 滚动由 flushPendingFrames 统一处理，避免批量追加时每行都触发布局。
}

void FrameModeWidget::trimFrameRecords()
{
    /*
     * 表格和记录列表都设置硬上限，避免长时间抓包后内存无限增长。
     * 删除旧行时保持 m_frames 与表格行号一致，详情选择仍可直接用 row 取记录。
     */
    const int overflow = m_frames.size() - m_maxFrameRecords;
    if (overflow <= 0) {
        return;
    }

    m_frames.erase(m_frames.begin(), m_frames.begin() + overflow);

    /*
     * 已显示旧行优先裁掉；只有表格行数不足时，才裁还没落表的 pending。
     * 这样保持 m_frames 与表格 row 的对应关系，详情选择不会错位。
     */
    const int tableTrim = m_frameTable ? qMin(overflow, m_frameTable->rowCount()) : 0;
    if (m_frameTable && tableTrim > 0) {
        /*
         * 通过底层模型批量删除旧行，避免逐行 removeRow 在大数据量下反复刷新。
         */
        m_frameTable->model()->removeRows(0, qMin(tableTrim, m_frameTable->rowCount()));
    }

    const int pendingTrim = qMin(overflow - tableTrim, m_pendingFrames.size());
    if (pendingTrim > 0) {
        m_pendingFrames.erase(m_pendingFrames.begin(), m_pendingFrames.begin() + pendingTrim);
    }
}

void FrameModeWidget::scheduleFrameFlush()
{
    if (m_pendingFrames.size() >= m_frameFlushBatchSize) {
        flushPendingFrames();
        return;
    }

    if (m_frameFlushTimer && !m_frameFlushTimer->isActive()) {
        m_frameFlushTimer->start(m_frameFlushIntervalMs);
    }
}

void FrameModeWidget::flushPendingFrames()
{
    if (m_frameFlushTimer) {
        m_frameFlushTimer->stop();
    }
    if (!m_frameTable || m_pendingFrames.isEmpty()) {
        return;
    }

    m_frameTable->setUpdatesEnabled(false);
    const int count = qMin(m_pendingFrames.size(), m_frameFlushBatchSize);
    const int firstRow = m_frameTable->rowCount();
    m_frameTable->setRowCount(firstRow + count);
    for (int i = 0; i < count; ++i) {
        fillFrameRow(firstRow + i, m_pendingFrames.at(i));
    }
    if (count > 0) {
        m_pendingFrames.erase(m_pendingFrames.begin(), m_pendingFrames.begin() + count);
    }
    if (!m_pendingFrames.isEmpty()) {
        scheduleFrameFlush();
    }
    m_frameTable->scrollToBottom();
    m_frameTable->setUpdatesEnabled(true);
}

void FrameModeWidget::onFrameSelected(int row, int column)
{
    Q_UNUSED(column)
    if (row >= 0 && row < m_frames.size()) {
        updateFrameDetail(m_frames[row]);
    }
}

void FrameModeWidget::updateFrameDetail(const FrameData& frame)
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
    html += QString("<h3>帧 #%1</h3>").arg(frame.index);
    html += QString("<p><b>时间:</b> %1</p>").arg(frame.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz"));
    html += QString("<p><b>方向:</b> %1</p>").arg(frame.parsedFields.value("direction", "RX").toString());
    html += QString("<p><b>长度:</b> %1 字节</p>").arg(frame.rawData.size());
    html += QString("<p><b>状态:</b> <span style='color:%1'>%2</span></p>")
        .arg(frame.valid ? "#2ecc71" : "#e74c3c")
        .arg(frame.valid ? tr("有效") : frame.errorInfo);

    html += QString("<hr style='border-color:%1;'>").arg(borderColor);
    html += QString("<h4>原始数据 (HEX):</h4><pre style='background:%1;color:%2;padding:10px;border-radius:4px;border:1px solid %3;'>")
        .arg(bgPreview).arg(textColor).arg(borderColor);
    QString hexStr;
    for (int i = 0; i < frame.rawData.size(); ++i) {
        hexStr += QString("%1 ").arg(static_cast<unsigned char>(frame.rawData[i]), 2, 16, QChar('0')).toUpper();
        if ((i + 1) % 16 == 0) hexStr += "\n";
    }
    html += hexStr + "</pre>";

    html += QString("<h4>ASCII:</h4><pre style='background:%1;color:%2;padding:10px;border-radius:4px;border:1px solid %3;'>")
        .arg(bgPreview).arg(textColor).arg(borderColor);
    for (unsigned char byte : frame.rawData) {
        html += (byte >= 32 && byte < 127) ? QChar(byte) : '.';
    }
    html += "</pre></div>";

    m_detailView->setHtml(html);
}

void FrameModeWidget::updateStatistics()
{
    if (m_statsLabel) {
        m_statsLabel->setText(QString(tr("总帧: %1 | 有效: %2 | 无效: %3"))
            .arg(m_totalFrames).arg(m_validFrames).arg(m_invalidFrames));
    }
}

void FrameModeWidget::onConfigChanged()
{
    QString header = m_headerEdit->text().simplified().remove(' ');
    QString footer = m_footerEdit->text().simplified().remove(' ');

    m_config.header = QByteArray::fromHex(header.toLatin1());
    m_config.footer = QByteArray::fromHex(footer.toLatin1());
}

void FrameModeWidget::onClearFrames()
{
    clear();
}

void FrameModeWidget::onExportFrames()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("导出帧数据"),
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_frames.csv",
        tr("CSV 文件 (*.csv);;文本文件 (*.txt)"));

    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法创建文件"));
        return;
    }

    QTextStream stream(&file);
    stream << "Index,Timestamp,Length,Status,HexData\n";

    for (const FrameData& frame : m_frames) {
        QString hexStr;
        for (unsigned char byte : frame.rawData) {
            hexStr += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
        }
        stream << frame.index << ","
               << frame.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << ","
               << frame.rawData.size() << ","
               << (frame.valid ? "Valid" : "Invalid") << ","
               << hexStr.trimmed() << "\n";
    }

    file.close();
    emit statusMessage(tr("已导出 %1 帧").arg(m_frames.size()));
}

void FrameModeWidget::onFilterChanged(const QString& text)
{
    for (int row = 0; row < m_frameTable->rowCount(); ++row) {
        bool match = text.isEmpty();
        if (!match) {
            for (int col = 0; col < m_frameTable->columnCount(); ++col) {
                QTableWidgetItem* item = m_frameTable->item(row, col);
                if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
        }
        m_frameTable->setRowHidden(row, !match);
    }
}

void FrameModeWidget::onSendFrame()
{
    QString text = m_sendEdit->text().simplified().remove(' ');
    if (text.isEmpty()) return;

    QByteArray data = QByteArray::fromHex(text.toLatin1());
    emit sendDataRequested(data);
}

void FrameModeWidget::clear()
{
    if (m_frameFlushTimer) {
        m_frameFlushTimer->stop();
    }
    m_pendingFrames.clear();
    m_frames.clear();
    m_frameTable->setRowCount(0);
    m_detailView->clear();
    m_buffer.clear();
    m_totalFrames = 0;
    m_validFrames = 0;
    m_invalidFrames = 0;
    updateStatistics();
}

bool FrameModeWidget::exportToFile(const QString& fileName)
{
    flushPendingFrames();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    for (const FrameData& frame : m_frames) {
        stream << "[" << frame.timestamp.toString("hh:mm:ss.zzz") << "] ";
        stream << (frame.valid ? "[OK]" : "[ERR]") << " ";
        for (unsigned char byte : frame.rawData) {
            stream << QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
        }
        stream << "\n";
    }

    file.close();
    return true;
}

void FrameModeWidget::onActivated()
{
    // 激活时可以恢复状态
}

void FrameModeWidget::onDeactivated()
{
    m_timeoutTimer->stop();
}

void FrameModeWidget::setFrameConfig(const ModeFrameConfig& config)
{
    m_config = config;
    m_headerEdit->setText(config.header.toHex().toUpper());
    m_footerEdit->setText(config.footer.toHex().toUpper());
}

void FrameModeWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    IModeWidget::changeEvent(event);
}

void FrameModeWidget::retranslateUi()
{
    // 过滤区
    if (m_filterLabel) m_filterLabel->setText(tr("过滤:"));
    if (m_filterEdit) m_filterEdit->setPlaceholderText(tr("输入关键字过滤帧..."));

    // 表格头
    m_frameTable->setHorizontalHeaderLabels({tr("#"), tr("时间"), tr("长度"), tr("状态"), tr("数据")});

    // 详情区
    if (m_detailLabel) m_detailLabel->setText(tr("帧详情"));

    // 发送区
    if (m_sendLabel) m_sendLabel->setText(tr("发送帧:"));
    if (m_sendEdit) m_sendEdit->setPlaceholderText(tr("输入十六进制数据，如: 01 02 03"));
    if (m_sendBtn) m_sendBtn->setText(tr("发送"));
    if (m_sendWithHeaderBtn) m_sendWithHeaderBtn->setText(tr("带帧头尾发送"));

    // 工具栏标签
    if (m_headerLabel) m_headerLabel->setText(tr("帧头:"));
    if (m_footerLabel) m_footerLabel->setText(tr("帧尾:"));
    if (m_checksumLabel) m_checksumLabel->setText(tr("校验:"));
    if (m_checksumCombo) {
        m_checksumCombo->setItemText(0, tr("无"));
    }

    // 工具栏动作
    if (m_clearAction) m_clearAction->setText(tr("清空"));
    if (m_exportAction) m_exportAction->setText(tr("导出"));

    // 更新统计
    updateStatistics();
}

} // namespace ComAssistant
