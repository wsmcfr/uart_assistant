/**
 * @file TabbedReceiveWidget.cpp
 * @brief 多标签接收区组件实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "TabbedReceiveWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QScrollBar>
#include <QFile>
#include <QTextStream>
#include <QLabel>
#include <QTabBar>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QColorDialog>
#include <QMessageBox>
#include <QEvent>
#include <QAbstractTableModel>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QElapsedTimer>

namespace ComAssistant {

class ReceiveHighlightHighlighter final : public QSyntaxHighlighter {
public:
    explicit ReceiveHighlightHighlighter(QTextDocument* parent)
        : QSyntaxHighlighter(parent)
    {
    }

    void setRules(const QVector<HighlightRule>& rules)
    {
        m_rules.clear();
        m_rules.reserve(rules.size());

        for (const HighlightRule& rule : rules) {
            if (!rule.enabled || rule.keyword.isEmpty()) {
                continue;
            }

            CompiledRule compiled;
            compiled.format.setForeground(rule.color);
            compiled.format.setFontWeight(QFont::Bold);

            QRegularExpression regex(QRegularExpression::escape(rule.keyword));
            if (!rule.caseSensitive) {
                regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
            }
            compiled.regex = regex;

            m_rules.append(compiled);
        }
    }

    void setEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

protected:
    void highlightBlock(const QString& text) override
    {
        if (!m_enabled || m_rules.isEmpty()) {
            return;
        }

        for (const CompiledRule& rule : m_rules) {
            QRegularExpressionMatchIterator it = rule.regex.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                if (match.hasMatch()) {
                    setFormat(match.capturedStart(), match.capturedLength(), rule.format);
                }
            }
        }
    }

private:
    struct CompiledRule {
        QRegularExpression regex;
        QTextCharFormat format;
    };

    QVector<CompiledRule> m_rules;
    bool m_enabled = true;
};

class ReceiveHexModel final : public QAbstractTableModel {
public:
    explicit ReceiveHexModel(QObject* parent = nullptr)
        : QAbstractTableModel(parent)
    {
        setMaxBytes(8 * 1024 * 1024);
    }

    void setMaxBytes(int maxBytes)
    {
        maxBytes = qMax(16, maxBytes);
        if (m_maxBytes == maxBytes) {
            return;
        }

        beginResetModel();
        QByteArray newBuffer;
        newBuffer.resize(maxBytes);

        const int copyBytes = qMin(m_size, maxBytes);
        for (int i = 0; i < copyBytes; ++i) {
            newBuffer[i] = static_cast<char>(byteAt(m_size - copyBytes + i));
        }

        m_buffer = newBuffer;
        m_maxBytes = maxBytes;
        m_start = 0;
        m_size = copyBytes;
        endResetModel();
    }

    void clear()
    {
        beginResetModel();
        if (m_buffer.size() != m_maxBytes) {
            m_buffer.resize(m_maxBytes);
        }
        m_buffer.fill(0);
        m_start = 0;
        m_size = 0;
        m_totalBytes = 0;
        endResetModel();
    }

    void appendData(const QByteArray& data)
    {
        if (data.isEmpty() || m_maxBytes <= 0) {
            return;
        }

        if (m_buffer.size() != m_maxBytes) {
            m_buffer.resize(m_maxBytes);
        }

        const int incoming = data.size();
        m_totalBytes += static_cast<quint64>(incoming);

        if (incoming >= m_maxBytes) {
            beginResetModel();
            const int offset = incoming - m_maxBytes;
            for (int i = 0; i < m_maxBytes; ++i) {
                m_buffer[i] = data[offset + i];
            }
            m_start = 0;
            m_size = m_maxBytes;
            endResetModel();
            return;
        }

        const int oldSize = m_size;
        const int oldRowCount = rowCount();
        const int newSize = qMin(m_maxBytes, m_size + incoming);
        const int newRowCount = rowsForBytes(newSize);
        const bool overflow = (oldSize + incoming > m_maxBytes);

        if (newRowCount > oldRowCount) {
            beginInsertRows(QModelIndex(), oldRowCount, newRowCount - 1);
        }

        for (int i = 0; i < incoming; ++i) {
            writeByte(static_cast<quint8>(data[i]));
        }

        if (newRowCount > oldRowCount) {
            endInsertRows();
        }

        if (newRowCount == 0) {
            return;
        }

        if (overflow || oldSize >= m_maxBytes) {
            emit dataChanged(index(0, 0), index(newRowCount - 1, 16));
        } else {
            const int firstRow = qMax(0, oldRowCount - 1);
            emit dataChanged(index(firstRow, 0), index(newRowCount - 1, 16));
        }
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid()) {
            return 0;
        }
        return rowsForBytes(m_size);
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 17;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid()) {
            return {};
        }

        if (role == Qt::TextAlignmentRole) {
            if (index.column() == 16) {
                return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
            }
            return QVariant::fromValue(Qt::AlignCenter);
        }

        if (role != Qt::DisplayRole) {
            return {};
        }

        const int row = index.row();
        const int col = index.column();
        const int logicalIndex = row * 16 + col;

        if (col < 16) {
            if (logicalIndex >= m_size) {
                return {};
            }
            const quint8 value = byteAt(logicalIndex);
            return QString("%1").arg(value, 2, 16, QChar('0')).toUpper();
        }

        const int rowStart = row * 16;
        if (rowStart >= m_size) {
            return {};
        }

        QString ascii;
        ascii.reserve(16);
        for (int i = 0; i < 16; ++i) {
            const int idx = rowStart + i;
            if (idx >= m_size) {
                break;
            }
            const quint8 value = byteAt(idx);
            ascii.append((value >= 32 && value < 127) ? QChar(value) : QChar('.'));
        }
        return ascii;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (role != Qt::DisplayRole) {
            return {};
        }

        if (orientation == Qt::Horizontal) {
            if (section < 16) {
                return QString("%1").arg(section, 2, 16, QChar('0')).toUpper();
            }
            return QStringLiteral("ASCII");
        }

        const quint64 baseOffset = (m_totalBytes > static_cast<quint64>(m_size))
            ? (m_totalBytes - static_cast<quint64>(m_size))
            : 0;
        const quint64 rowOffset = baseOffset + static_cast<quint64>(section) * 16;
        return QString("%1").arg(rowOffset, 8, 16, QChar('0')).toUpper();
    }

private:
    int rowsForBytes(int bytes) const
    {
        return (bytes + 15) / 16;
    }

    quint8 byteAt(int logicalIndex) const
    {
        if (logicalIndex < 0 || logicalIndex >= m_size || m_maxBytes <= 0) {
            return 0;
        }
        const int pos = (m_start + logicalIndex) % m_maxBytes;
        return static_cast<quint8>(m_buffer[pos]);
    }

    void writeByte(quint8 value)
    {
        if (m_maxBytes <= 0) {
            return;
        }

        if (m_buffer.size() != m_maxBytes) {
            m_buffer.resize(m_maxBytes);
        }

        if (m_size < m_maxBytes) {
            const int pos = (m_start + m_size) % m_maxBytes;
            m_buffer[pos] = static_cast<char>(value);
            ++m_size;
            return;
        }

        m_buffer[m_start] = static_cast<char>(value);
        m_start = (m_start + 1) % m_maxBytes;
    }

    QByteArray m_buffer;
    int m_maxBytes = 0;
    int m_start = 0;
    int m_size = 0;
    quint64 m_totalBytes = 0;
};

TabbedReceiveWidget::TabbedReceiveWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    m_highlightTimer = new QTimer(this);
    m_highlightTimer->setSingleShot(true);
    connect(m_highlightTimer, &QTimer::timeout, this, &TabbedReceiveWidget::applyHighlight);

    m_terminalRefreshTimer = new QTimer(this);
    m_terminalRefreshTimer->setSingleShot(true);
    connect(m_terminalRefreshTimer, &QTimer::timeout, this, &TabbedReceiveWidget::updateTerminalDisplay);

    m_perfTimer = new QTimer(this);
    connect(m_perfTimer, &QTimer::timeout, this, &TabbedReceiveWidget::updatePerformanceStats);
    m_perfTimer->start(1000);

    // 添加默认高亮规则 - 只高亮文字颜色
    addHighlightRule({"OK", QColor(46, 204, 113), false, true});       // 绿色
    addHighlightRule({"SUCCESS", QColor(46, 204, 113), false, true});  // 绿色
    addHighlightRule({"ERROR", QColor(231, 76, 60), false, true});     // 红色
    addHighlightRule({"FAIL", QColor(231, 76, 60), false, true});      // 红色
    addHighlightRule({"WARN", QColor(241, 196, 15), false, true});     // 黄色
}

void TabbedReceiveWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // 选项栏
    QHBoxLayout* optionsLayout = new QHBoxLayout;
    optionsLayout->setContentsMargins(4, 4, 4, 4);
    optionsLayout->setSpacing(12);

    m_timestampCheck = new QCheckBox(tr("时间戳"));
    m_timestampCheck->setChecked(m_timestampEnabled);
    connect(m_timestampCheck, &QCheckBox::toggled,
            this, &TabbedReceiveWidget::onTimestampToggled);

    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    m_autoScrollCheck->setChecked(m_autoScrollEnabled);
    connect(m_autoScrollCheck, &QCheckBox::toggled,
            this, &TabbedReceiveWidget::onAutoScrollToggled);

    m_hexDisplayCheck = new QCheckBox(tr("HEX显示"));
    m_hexDisplayCheck->setChecked(m_hexDisplayEnabled);
    connect(m_hexDisplayCheck, &QCheckBox::toggled,
            this, &TabbedReceiveWidget::onHexDisplayToggled);

    m_highlightCheck = new QCheckBox(tr("高亮"));
    m_highlightCheck->setChecked(m_highlightEnabled);
    connect(m_highlightCheck, &QCheckBox::toggled,
            this, &TabbedReceiveWidget::onHighlightToggled);

    m_highlightSettingsBtn = new QPushButton(tr("设置"));
    m_highlightSettingsBtn->setFixedWidth(60);
    m_highlightSettingsBtn->setToolTip(tr("高亮规则设置"));
    connect(m_highlightSettingsBtn, &QPushButton::clicked,
            this, &TabbedReceiveWidget::onHighlightSettingsClicked);

    m_perfLabel = new QLabel(tr("性能: 接收 0 B/s | 渲染 0 ms"));
    m_perfLabel->setStyleSheet("color: #666;");
    m_perfLabel->setMinimumWidth(220);

    m_clearBtn = new QPushButton(tr("清空"));
    m_clearBtn->setFixedWidth(60);
    connect(m_clearBtn, &QPushButton::clicked, this, &TabbedReceiveWidget::onClearClicked);

    optionsLayout->addWidget(m_timestampCheck);
    optionsLayout->addWidget(m_autoScrollCheck);
    optionsLayout->addWidget(m_hexDisplayCheck);
    optionsLayout->addSpacing(8);
    optionsLayout->addWidget(m_highlightCheck);
    optionsLayout->addWidget(m_highlightSettingsBtn);
    optionsLayout->addWidget(m_perfLabel);
    optionsLayout->addStretch();
    optionsLayout->addWidget(m_clearBtn);

    mainLayout->addLayout(optionsLayout);

    // 标签页
    m_tabWidget = new QTabWidget;
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);

    setupMainTab();
    setupHexTab();
    setupFilterTab();

    // 主标签页不可关闭（移除左右两侧的关闭按钮）
    m_tabWidget->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    m_tabWidget->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested,
            this, &TabbedReceiveWidget::onTabCloseRequested);

    mainLayout->addWidget(m_tabWidget);
}

void TabbedReceiveWidget::setupOptionsBar()
{
    // 移除此函数内容，已整合到 setupUi
}

void TabbedReceiveWidget::setupMainTab()
{
    m_mainTextEdit = new QPlainTextEdit;
    m_mainTextEdit->setReadOnly(true);
    m_mainTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_mainTextEdit->setFont(QFont("Consolas", 10));
    m_mainTextEdit->document()->setMaximumBlockCount(m_maxLines);
    m_mainTextEdit->document()->setUndoRedoEnabled(false);
    m_mainTextEdit->setPlaceholderText(tr("接收到的数据将在这里显示..."));

    if (!m_highlighter) {
        m_highlighter = new ReceiveHighlightHighlighter(m_mainTextEdit->document());
        m_highlighter->setEnabled(m_highlightEnabled);
        m_highlighter->setRules(m_highlightRules);
    }

    // 创建一个包含文本编辑器和智能滚屏指示器的容器
    QWidget* textContainer = new QWidget;
    QVBoxLayout* containerLayout = new QVBoxLayout(textContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);
    containerLayout->addWidget(m_mainTextEdit);

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
    containerLayout->addWidget(m_smartScrollIndicator);

    // 连接滚动条信号实现智能滚屏
    connect(m_mainTextEdit->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &TabbedReceiveWidget::onScrollBarValueChanged);

    m_tabWidget->addTab(textContainer, tr("文本"));
}

void TabbedReceiveWidget::setupHexTab()
{
    m_hexTable = new QTableView;
    m_hexModel = new ReceiveHexModel(m_hexTable);
    m_hexModel->setMaxBytes(m_maxRawDataBytes);

    m_hexTable->setModel(m_hexModel);
    m_hexTable->setWordWrap(false);
    m_hexTable->setUniformRowHeights(true);
    m_hexTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_hexTable->setSelectionMode(QAbstractItemView::ContiguousSelection);
    m_hexTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_hexTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_hexTable->horizontalHeader()->setDefaultSectionSize(28);
    m_hexTable->horizontalHeader()->setStretchLastSection(true);
    m_hexTable->verticalHeader()->setDefaultSectionSize(22);
    m_hexTable->setFont(QFont("Consolas", 9));

    m_tabWidget->addTab(m_hexTable, tr("十六进制"));
}

void TabbedReceiveWidget::setupFilterTab()
{
    QWidget* filterWidget = new QWidget;
    QVBoxLayout* filterLayout = new QVBoxLayout(filterWidget);
    filterLayout->setContentsMargins(4, 4, 4, 4);

    // 过滤输入
    QHBoxLayout* inputLayout = new QHBoxLayout;
    inputLayout->addWidget(new QLabel(tr("过滤:")));
    m_filterInput = new QLineEdit;
    m_filterInput->setPlaceholderText(tr("输入关键字进行过滤..."));
    connect(m_filterInput, &QLineEdit::textChanged,
            this, &TabbedReceiveWidget::onFilterTextChanged);
    inputLayout->addWidget(m_filterInput);
    filterLayout->addLayout(inputLayout);

    // 过滤结果
    m_filterResult = new QTextEdit;
    m_filterResult->setReadOnly(true);
    m_filterResult->setFont(QFont("Consolas", 10));
    m_filterResult->document()->setUndoRedoEnabled(false);
    filterLayout->addWidget(m_filterResult);

    m_tabWidget->addTab(filterWidget, tr("过滤"));
}

void TabbedReceiveWidget::appendData(const QByteArray& data)
{
    QElapsedTimer perfTimer;
    perfTimer.start();

    m_rawData.append(data);
    trimRawDataBuffer();

    // 根据显示模式处理数据
    switch (m_displayMode) {
        case ReceiveDisplayMode::Serial:
            appendSerialMode(data);
            break;
        case ReceiveDisplayMode::Terminal:
            appendTerminalMode(data);
            break;
        case ReceiveDisplayMode::Frame:
            appendFrameMode(data);
            break;
        case ReceiveDisplayMode::Debug:
            appendDebugMode(data, false);
            break;
    }

    appendToHexView(data);
    emit dataReceived(data);

    if (!data.isEmpty()) {
        m_perfRxBytes += data.size();
    }
    m_perfRenderCostSumMs += static_cast<double>(perfTimer.nsecsElapsed()) / 1000000.0;
    m_perfRenderSamples += 1;
}

void TabbedReceiveWidget::appendSentData(const QByteArray& data)
{
    // 仅在调试模式下显示发送数据
    if (m_displayMode == ReceiveDisplayMode::Debug) {
        appendDebugMode(data, true);
    }
}

// ==================== 串口模式 ====================
void TabbedReceiveWidget::appendSerialMode(const QByteArray& data)
{
    appendToMainView(data);
}

// ==================== 终端模式 ====================
void TabbedReceiveWidget::appendTerminalMode(const QByteArray& data)
{
    // 处理终端数据，支持ANSI转义序列和控制字符
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);

        // 处理ANSI转义序列
        if (m_inAnsiEscape) {
            m_ansiBuffer.append(byte);
            // 检查转义序列是否结束 (以字母结尾)
            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')) {
                processAnsiEscape(m_ansiBuffer);
                m_ansiBuffer.clear();
                m_inAnsiEscape = false;
            }
            continue;
        }

        // 检查ESC字符开始转义序列
        if (byte == 0x1B) {
            m_inAnsiEscape = true;
            m_ansiBuffer.clear();
            m_ansiBuffer.append(byte);
            continue;
        }

        // 处理控制字符
        switch (byte) {
            case '\r':  // 回车
                terminalCarriageReturn();
                break;
            case '\n':  // 换行
                terminalNewLine();
                break;
            case '\b':  // 退格
            case 0x7F:  // DEL
                terminalBackspace();
                break;
            case '\t':  // 制表符
                terminalTab();
                break;
            case '\a':  // 响铃 - 忽略
                break;
            default:
                if (byte >= 32) {  // 可打印字符
                    terminalWriteChar(QChar(byte));
                }
                break;
        }
    }

    scheduleTerminalDisplayUpdate();
}

void TabbedReceiveWidget::processAnsiEscape(const QByteArray& seq)
{
    // 解析ANSI转义序列
    if (seq.size() < 2) return;

    // CSI序列: ESC [
    if (seq.size() >= 3 && seq[1] == '[') {
        QString params = QString::fromLatin1(seq.mid(2, seq.size() - 3));
        char cmd = seq[seq.size() - 1];

        QStringList paramList = params.split(';');

        switch (cmd) {
            case 'm':  // SGR - 设置图形
                for (const QString& p : paramList) {
                    int code = p.isEmpty() ? 0 : p.toInt();
                    terminalSetColor(code);
                }
                break;
            case 'H':  // 光标位置
            case 'f':
                // 简化处理，暂不实现
                break;
            case 'J':  // 清屏
                if (params == "2") {
                    m_terminalLines.clear();
                    m_terminalLines.append("");
                    m_terminalCursorX = 0;
                    m_terminalCursorY = 0;
                }
                break;
            case 'K':  // 清行
                terminalClearLine();
                break;
            case 'A':  // 光标上移
            case 'B':  // 光标下移
            case 'C':  // 光标右移
            case 'D':  // 光标左移
                // 简化处理
                break;
        }
    }
}

void TabbedReceiveWidget::terminalWriteChar(QChar ch)
{
    // 确保当前行存在
    while (m_terminalCursorY >= m_terminalLines.size()) {
        m_terminalLines.append("");
    }

    QString& currentLine = m_terminalLines[m_terminalCursorY];

    // 在光标位置插入或覆盖字符
    if (m_terminalCursorX >= currentLine.length()) {
        currentLine.append(QString(m_terminalCursorX - currentLine.length(), ' '));
        currentLine.append(ch);
    } else {
        currentLine[m_terminalCursorX] = ch;
    }

    m_terminalCursorX++;
}

void TabbedReceiveWidget::terminalNewLine()
{
    m_terminalCursorY++;
    m_terminalCursorX = 0;

    // 确保行存在
    while (m_terminalCursorY >= m_terminalLines.size()) {
        m_terminalLines.append("");
    }

    // 限制最大行数
    const int maxLines = qMax(200, qMin(m_terminalMaxLines, m_maxLines));
    while (m_terminalLines.size() > maxLines) {
        m_terminalLines.removeFirst();
        m_terminalCursorY--;
    }
}

void TabbedReceiveWidget::terminalCarriageReturn()
{
    m_terminalCursorX = 0;
}

void TabbedReceiveWidget::terminalBackspace()
{
    if (m_terminalCursorX > 0) {
        m_terminalCursorX--;
        // 删除字符
        if (m_terminalCursorY < m_terminalLines.size()) {
            QString& line = m_terminalLines[m_terminalCursorY];
            if (m_terminalCursorX < line.length()) {
                line.remove(m_terminalCursorX, 1);
            }
        }
    }
}

void TabbedReceiveWidget::terminalTab()
{
    // 移动到下一个8字符对齐位置
    m_terminalCursorX = ((m_terminalCursorX / 8) + 1) * 8;
}

void TabbedReceiveWidget::terminalClearLine()
{
    if (m_terminalCursorY < m_terminalLines.size()) {
        m_terminalLines[m_terminalCursorY].truncate(m_terminalCursorX);
    }
}

void TabbedReceiveWidget::terminalSetColor(int code)
{
    // 处理ANSI颜色代码
    switch (code) {
        case 0:  // 重置
            terminalResetFormat();
            break;
        case 1:  // 粗体
            m_terminalBold = true;
            break;
        case 30: m_terminalFgColor = QColor(0, 0, 0); break;       // 黑
        case 31: m_terminalFgColor = QColor(205, 49, 49); break;   // 红
        case 32: m_terminalFgColor = QColor(13, 188, 121); break;  // 绿
        case 33: m_terminalFgColor = QColor(229, 229, 16); break;  // 黄
        case 34: m_terminalFgColor = QColor(36, 114, 200); break;  // 蓝
        case 35: m_terminalFgColor = QColor(188, 63, 188); break;  // 品红
        case 36: m_terminalFgColor = QColor(17, 168, 205); break;  // 青
        case 37: m_terminalFgColor = QColor(229, 229, 229); break; // 白
        case 39: m_terminalFgColor = QColor(0, 255, 0); break;     // 默认（绿）
    }
}

void TabbedReceiveWidget::terminalResetFormat()
{
    m_terminalFgColor = QColor(0, 255, 0);  // 默认绿色
    m_terminalBgColor = QColor(26, 26, 26); // 默认黑色
    m_terminalBold = false;
}

void TabbedReceiveWidget::updateTerminalDisplay()
{
    const int maxLines = qMax(200, qMin(m_terminalMaxLines, m_maxLines));
    while (m_terminalLines.size() > maxLines) {
        m_terminalLines.removeFirst();
        if (m_terminalCursorY > 0) {
            --m_terminalCursorY;
        }
    }

    // 将终端行缓冲更新到显示
    QString text = m_terminalLines.join('\n');
    m_mainTextEdit->setPlainText(text);

    // 智能滚屏：仅在未暂停时滚动到底部
    if (m_autoScrollEnabled && !m_smartScrollPaused) {
        m_mainTextEdit->verticalScrollBar()->setValue(
            m_mainTextEdit->verticalScrollBar()->maximum());
    }
}

// ==================== 帧模式 ====================
void TabbedReceiveWidget::appendFrameMode(const QByteArray& data)
{
    // 将数据添加到帧缓冲
    m_frameBuffer.append(data);

    // 按换行符分割帧
    int pos;
    while ((pos = m_frameBuffer.indexOf('\n')) >= 0) {
        QByteArray frame = m_frameBuffer.left(pos);
        m_frameBuffer.remove(0, pos + 1);

        // 移除可能的 \r
        if (frame.endsWith('\r')) {
            frame.chop(1);
        }

        if (frame.isEmpty()) continue;

        m_frameCounter++;

        // 格式化帧显示：[序号] [时间] [长度] 内容
        QString frameText = QString("[%1] [%2] [%3B] %4\n")
            .arg(m_frameCounter, 4, 10, QChar('0'))
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
            .arg(frame.size(), 4)
            .arg(QString::fromUtf8(frame));

        m_mainTextEdit->moveCursor(QTextCursor::End);
        m_mainTextEdit->insertPlainText(frameText);

        m_lineHistory.append(frameText);
        trimLineHistory();
    }

    // 智能滚屏：仅在未暂停时滚动到底部
    if (m_autoScrollEnabled && !m_smartScrollPaused) {
        m_mainTextEdit->verticalScrollBar()->setValue(
            m_mainTextEdit->verticalScrollBar()->maximum());
    }
}

// ==================== 调试模式 ====================
void TabbedReceiveWidget::appendDebugMode(const QByteArray& data, bool isSent)
{
    // 使用行缓冲机制，只在遇到换行符时输出完整行
    QByteArray& buffer = isSent ? m_debugTxBuffer : m_debugRxBuffer;
    QDateTime& timestamp = isSent ? m_debugTxTimestamp : m_debugRxTimestamp;

    // 如果缓冲区为空，记录起始时间戳
    if (buffer.isEmpty()) {
        timestamp = QDateTime::currentDateTime();
    }

    // 将数据添加到缓冲区
    buffer.append(data);

    // 处理完整的行
    int pos;
    while ((pos = buffer.indexOf('\n')) >= 0) {
        QByteArray line = buffer.left(pos);
        buffer.remove(0, pos + 1);

        // 移除可能的 \r
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        // 格式化输出
        QString prefix;
        QString style;

        if (isSent) {
            prefix = QString("[%1] [TX] ").arg(timestamp.toString("hh:mm:ss.zzz"));
            style = "color: #2196F3;";  // 蓝色表示发送
        } else {
            prefix = QString("[%1] [RX] ").arg(timestamp.toString("hh:mm:ss.zzz"));
            style = "color: #4CAF50;";  // 绿色表示接收
        }

        // 显示数据
        QString text;
        if (m_hexDisplayEnabled) {
            // HEX格式
            for (unsigned char byte : line) {
                text += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
            }
        } else {
            text = QString::fromUtf8(line);
        }

        // 使用HTML格式插入带颜色的文本
        QString html = QString("<span style=\"%1\">%2%3</span><br>")
            .arg(style)
            .arg(prefix.toHtmlEscaped())
            .arg(text.toHtmlEscaped());

        m_mainTextEdit->moveCursor(QTextCursor::End);
        m_mainTextEdit->insertHtml(html);

        m_lineHistory.append(prefix + text);
        trimLineHistory();

        // 更新时间戳为下一行
        timestamp = QDateTime::currentDateTime();
    }

    // 智能滚屏：仅在未暂停时滚动到底部
    if (m_autoScrollEnabled && !m_smartScrollPaused) {
        m_mainTextEdit->verticalScrollBar()->setValue(
            m_mainTextEdit->verticalScrollBar()->maximum());
    }
}

void TabbedReceiveWidget::appendToMainView(const QByteArray& data)
{
    QString text;

    // 根据当前HEX显示设置转换数据
    if (m_hexDisplayEnabled) {
        // HEX显示模式 - 直接处理原始字节
        for (int i = 0; i < data.size(); ++i) {
            unsigned char byte = static_cast<unsigned char>(data[i]);

            // 在行首添加时间戳
            if (m_timestampEnabled && m_needTimestamp) {
                text += formatTimestamp() + " ";
                m_needTimestamp = false;
            }

            text += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();

            // 遇到换行符(0x0A)时添加实际换行，并标记下一行需要时间戳
            if (byte == 0x0A) {
                text += "\n";
                m_needTimestamp = true;
            }
        }
    } else {
        // 文本显示模式 - 使用缓冲区处理 UTF-8 多字节字符
        m_utf8Buffer.append(data);

        // 查找最后一个完整的 UTF-8 字符位置
        int validLen = 0;
        int i = 0;
        while (i < m_utf8Buffer.size()) {
            unsigned char byte = static_cast<unsigned char>(m_utf8Buffer[i]);

            int charLen = 1;
            if ((byte & 0x80) == 0) {
                // ASCII 字符 (0xxxxxxx)
                charLen = 1;
            } else if ((byte & 0xE0) == 0xC0) {
                // 2字节 UTF-8 (110xxxxx)
                charLen = 2;
            } else if ((byte & 0xF0) == 0xE0) {
                // 3字节 UTF-8 (1110xxxx) - 中文通常是这种
                charLen = 3;
            } else if ((byte & 0xF8) == 0xF0) {
                // 4字节 UTF-8 (11110xxx)
                charLen = 4;
            } else {
                // 无效或续字节，跳过
                i++;
                validLen = i;
                continue;
            }

            // 检查是否有足够的字节
            if (i + charLen <= m_utf8Buffer.size()) {
                validLen = i + charLen;
                i += charLen;
            } else {
                // 不完整的字符，停止处理
                break;
            }
        }

        // 提取完整的部分进行解码
        if (validLen > 0) {
            QByteArray completeData = m_utf8Buffer.left(validLen);
            m_utf8Buffer = m_utf8Buffer.mid(validLen);  // 保留不完整的部分

            QString decoded = QString::fromUtf8(completeData);

            for (int j = 0; j < decoded.size(); ++j) {
                QChar ch = decoded[j];

                // 跳过回车符，只处理换行符
                if (ch == '\r') {
                    continue;
                }

                // 在行首添加时间戳
                if (m_timestampEnabled && m_needTimestamp) {
                    text += formatTimestamp() + " ";
                    m_needTimestamp = false;
                }

                text += ch;

                // 遇到换行符时标记下一行需要时间戳
                if (ch == '\n') {
                    m_needTimestamp = true;
                }
            }
        }
    }

    if (!text.isEmpty()) {
        m_mainTextEdit->setUpdatesEnabled(false);
        m_mainTextEdit->moveCursor(QTextCursor::End);
        m_mainTextEdit->insertPlainText(text);

        // 记录历史（限长）
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            m_lineHistory.append(lines);
            trimLineHistory();
        }
        // 智能滚屏：仅在未暂停时滚动到底部
        if (m_autoScrollEnabled && !m_smartScrollPaused) {
            m_mainTextEdit->verticalScrollBar()->setValue(
                m_mainTextEdit->verticalScrollBar()->maximum());
        }
        m_mainTextEdit->setUpdatesEnabled(true);
    }

}

void TabbedReceiveWidget::appendToHexView(const QByteArray& data)
{
    if (data.isEmpty() || !m_hexTable || !m_hexModel) {
        return;
    }

    m_hexTable->setUpdatesEnabled(false);
    m_hexModel->appendData(data);
    if (m_autoScrollEnabled) {
        m_hexTable->scrollToBottom();
    }
    m_hexTable->setUpdatesEnabled(true);
}

void TabbedReceiveWidget::updateFilterView()
{
    QString filter = m_filterInput->text();
    if (filter.isEmpty()) {
        m_filterResult->clear();
        return;
    }

    QString result;
    for (const QString& line : m_lineHistory) {
        if (line.contains(filter, Qt::CaseInsensitive)) {
            result += line + "\n";
        }
    }

    m_filterResult->setText(result);
}

void TabbedReceiveWidget::refreshMainView()
{
    if (!m_mainTextEdit) {
        return;
    }

    m_mainTextEdit->setUpdatesEnabled(false);
    // 清空当前显示
    m_mainTextEdit->clear();

    // 根据当前HEX显示设置重新显示数据
    if (m_rawData.isEmpty()) {
        m_mainTextEdit->setUpdatesEnabled(true);
        return;
    }

    QString text;
    if (m_hexDisplayEnabled) {
        // HEX显示模式
        for (int i = 0; i < m_rawData.size(); ++i) {
            unsigned char byte = static_cast<unsigned char>(m_rawData[i]);
            text += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();

            // 遇到换行符(0x0A)时添加实际换行
            if (byte == 0x0A) {
                text += "\n";
            }
        }
    } else {
        // 文本显示模式
        text = QString::fromUtf8(m_rawData);
    }

    m_mainTextEdit->setPlainText(text);

    // 智能滚屏：仅在未暂停时滚动到底部
    if (m_autoScrollEnabled && !m_smartScrollPaused) {
        m_mainTextEdit->verticalScrollBar()->setValue(
            m_mainTextEdit->verticalScrollBar()->maximum());
    }
    m_mainTextEdit->setUpdatesEnabled(true);

}

void TabbedReceiveWidget::clear()
{
    m_rawData.clear();
    m_utf8Buffer.clear();  // 清空 UTF-8 缓冲区
    m_lineHistory.clear();
    m_needTimestamp = true;  // 重置时间戳状态

    m_mainTextEdit->clear();
    if (m_highlightTimer) {
        m_highlightTimer->stop();
    }
    if (m_terminalRefreshTimer) {
        m_terminalRefreshTimer->stop();
    }

    if (m_hexModel) {
        m_hexModel->clear();
    }

    m_filterResult->clear();

    // 清空调试模式缓冲区
    m_debugRxBuffer.clear();
    m_debugTxBuffer.clear();

    // 清空帧模式缓冲区
    m_frameBuffer.clear();
    m_frameCounter = 0;

    // 重置智能滚屏状态
    m_smartScrollPaused = false;
    hideSmartScrollIndicator();

    // 重置终端模式状态
    m_terminalLines.clear();
    m_terminalLines.append("");
    m_terminalCursorX = 0;
    m_terminalCursorY = 0;
    m_ansiBuffer.clear();
    m_inAnsiEscape = false;

    m_perfRxBytes = 0;
    m_perfRenderCostSumMs = 0.0;
    m_perfRenderSamples = 0;
    if (m_perfLabel) {
        m_perfLabel->setText(tr("性能: 接收 0 B/s | 渲染 0 ms"));
    }
}

void TabbedReceiveWidget::exportToFile(const QString& fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << m_mainTextEdit->toPlainText();
        file.close();
    }
}

void TabbedReceiveWidget::showSearchBar()
{
    // 切换到过滤标签页，该标签页已有搜索功能
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        if (m_tabWidget->tabText(i).contains(tr("过滤")) ||
            m_tabWidget->tabText(i).contains("Filter")) {
            m_tabWidget->setCurrentIndex(i);
            if (m_filterInput) {
                m_filterInput->setFocus();
                m_filterInput->selectAll();
            }
            break;
        }
    }
}

void TabbedReceiveWidget::hideSearchBar()
{
    // 切换回主标签页
    m_tabWidget->setCurrentIndex(0);
}

void TabbedReceiveWidget::setTimestampEnabled(bool enabled)
{
    m_timestampEnabled = enabled;
    if (enabled) {
        // 立即让后续接收数据在新行添加时间戳，避免切换后“看起来没生效”
        m_needTimestamp = true;
    }
    m_timestampCheck->setChecked(enabled);
}

bool TabbedReceiveWidget::isTimestampEnabled() const
{
    return m_timestampEnabled;
}

void TabbedReceiveWidget::setAutoScrollEnabled(bool enabled)
{
    m_autoScrollEnabled = enabled;
    m_autoScrollCheck->setChecked(enabled);
}

bool TabbedReceiveWidget::isAutoScrollEnabled() const
{
    return m_autoScrollEnabled;
}

void TabbedReceiveWidget::setHexDisplayEnabled(bool enabled)
{
    m_hexDisplayEnabled = enabled;
    m_hexDisplayCheck->setChecked(enabled);
}

bool TabbedReceiveWidget::isHexDisplayEnabled() const
{
    return m_hexDisplayEnabled;
}

void TabbedReceiveWidget::setDisplayFont(const QFont& font)
{
    if (m_mainTextEdit) {
        m_mainTextEdit->setFont(font);
    }
    if (m_filterResult) {
        m_filterResult->setFont(font);
    }
    if (m_hexTable) {
        QFont hexFont = font;
        hexFont.setPointSize(qMax(8, font.pointSize() - 1));
        m_hexTable->setFont(hexFont);
        if (m_hexTable->horizontalHeader()) {
            m_hexTable->horizontalHeader()->setFont(hexFont);
        }
        if (m_hexTable->verticalHeader()) {
            m_hexTable->verticalHeader()->setFont(hexFont);
        }
    }
}

void TabbedReceiveWidget::setMaxLines(int maxLines)
{
    m_maxLines = qBound(100, maxLines, 100000);
    if (m_mainTextEdit && m_mainTextEdit->document()) {
        m_mainTextEdit->document()->setMaximumBlockCount(m_maxLines);
    }
    trimLineHistory();

    const int terminalLimit = qMax(200, qMin(m_terminalMaxLines, m_maxLines));
    while (m_terminalLines.size() > terminalLimit) {
        m_terminalLines.removeFirst();
        if (m_terminalCursorY > 0) {
            --m_terminalCursorY;
        }
    }
}

void TabbedReceiveWidget::setHexBufferBytes(int bytes)
{
    const int clampedBytes = qMax(1024 * 1024, qMin(bytes, 512 * 1024 * 1024));
    if (m_maxRawDataBytes == clampedBytes) {
        return;
    }

    m_maxRawDataBytes = clampedBytes;
    trimRawDataBuffer();
    if (m_hexModel) {
        m_hexModel->setMaxBytes(m_maxRawDataBytes);
    }
}

void TabbedReceiveWidget::setDisplayMode(int mode)
{
    if (static_cast<int>(m_displayMode) == mode) return;

    m_displayMode = static_cast<ReceiveDisplayMode>(mode);

    // 清空当前显示，准备切换模式
    m_mainTextEdit->clear();
    m_terminalLines.clear();
    m_terminalCursorX = 0;
    m_terminalCursorY = 0;
    m_frameCounter = 0;
    m_frameBuffer.clear();
    m_ansiBuffer.clear();
    m_inAnsiEscape = false;
    m_debugRxBuffer.clear();
    m_debugTxBuffer.clear();

    // 根据显示模式调整UI外观
    switch (m_displayMode) {
        case ReceiveDisplayMode::Serial:
            // 串口模式 - 默认外观
            m_mainTextEdit->setStyleSheet("");
            m_mainTextEdit->setFont(QFont("Consolas", 10));
            m_timestampEnabled = false;
            if (m_timestampCheck) m_timestampCheck->setChecked(false);
            m_tabWidget->setTabText(0, tr("文本"));
            break;

        case ReceiveDisplayMode::Terminal:
            // 终端模式 - 黑底绿字，仿真终端风格
            m_mainTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "  background-color: #1a1a1a;"
                "  color: #00ff00;"
                "  border: none;"
                "}"
            );
            m_mainTextEdit->setFont(QFont("Consolas", 11));
            m_timestampEnabled = false;
            if (m_timestampCheck) m_timestampCheck->setChecked(false);
            m_tabWidget->setTabText(0, tr("终端"));
            // 初始化终端行
            m_terminalLines.append("");
            break;

        case ReceiveDisplayMode::Frame:
            // 帧模式 - 带边框，每帧清晰分隔
            m_mainTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "  background-color: #f5f5f5;"
                "  color: #333333;"
                "  border: 1px solid #cccccc;"
                "}"
            );
            m_mainTextEdit->setFont(QFont("Consolas", 10));
            m_timestampEnabled = true;
            if (m_timestampCheck) m_timestampCheck->setChecked(true);
            m_tabWidget->setTabText(0, tr("帧数据"));
            break;

        case ReceiveDisplayMode::Debug:
            // 调试模式 - 带时间戳和方向
            m_mainTextEdit->setStyleSheet(
                "QPlainTextEdit {"
                "  background-color: #fffef0;"
                "  color: #333333;"
                "  border: 1px solid #e0e0e0;"
                "}"
            );
            m_mainTextEdit->setFont(QFont("Consolas", 10));
            m_timestampEnabled = true;
            if (m_timestampCheck) m_timestampCheck->setChecked(true);
            m_tabWidget->setTabText(0, tr("调试"));
            break;
    }

    m_needTimestamp = true;
}

void TabbedReceiveWidget::onTabCloseRequested(int index)
{
    // 主标签页不可关闭
    if (index == 0) return;

    m_tabWidget->removeTab(index);
}

void TabbedReceiveWidget::onFilterTextChanged(const QString& text)
{
    Q_UNUSED(text)
    updateFilterView();
}

void TabbedReceiveWidget::onClearClicked()
{
    clear();
}

void TabbedReceiveWidget::onTimestampToggled(bool checked)
{
    m_timestampEnabled = checked;
    if (checked) {
        m_needTimestamp = true;
    }
}

void TabbedReceiveWidget::onAutoScrollToggled(bool checked)
{
    m_autoScrollEnabled = checked;
    // 启用自动滚动时，清除智能滚屏暂停状态
    if (checked) {
        m_smartScrollPaused = false;
        hideSmartScrollIndicator();
    }
}

void TabbedReceiveWidget::onScrollBarValueChanged(int value)
{
    // 仅在自动滚动启用时才进行智能滚屏检测
    if (!m_autoScrollEnabled) {
        return;
    }

    QScrollBar* scrollBar = m_mainTextEdit->verticalScrollBar();
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

void TabbedReceiveWidget::showSmartScrollIndicator(const QString& message)
{
    if (m_smartScrollIndicator) {
        m_smartScrollIndicator->setText(message);
        m_smartScrollIndicator->show();
    }
}

void TabbedReceiveWidget::hideSmartScrollIndicator()
{
    if (m_smartScrollIndicator) {
        m_smartScrollIndicator->hide();
    }
}

void TabbedReceiveWidget::checkScrollPosition()
{
    if (!m_autoScrollEnabled) {
        return;
    }

    QScrollBar* scrollBar = m_mainTextEdit->verticalScrollBar();
    int maxValue = scrollBar->maximum();
    int currentValue = scrollBar->value();

    bool atBottom = (currentValue >= maxValue - 10) || (maxValue == 0);
    if (atBottom && m_smartScrollPaused) {
        m_smartScrollPaused = false;
        hideSmartScrollIndicator();
    }
}

void TabbedReceiveWidget::onHexDisplayToggled(bool checked)
{
    m_hexDisplayEnabled = checked;
    refreshMainView();
}

void TabbedReceiveWidget::onHighlightToggled(bool checked)
{
    m_highlightEnabled = checked;
    scheduleHighlightUpdate();
}

void TabbedReceiveWidget::onHighlightSettingsClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("高亮规则设置"));
    dialog.setMinimumSize(400, 350);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    // 规则列表
    QListWidget* ruleList = new QListWidget;
    for (const HighlightRule& rule : m_highlightRules) {
        QListWidgetItem* item = new QListWidgetItem(rule.keyword);
        item->setForeground(rule.color);
        item->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
        ruleList->addItem(item);
    }
    layout->addWidget(ruleList);

    // 按钮区域
    QHBoxLayout* btnLayout = new QHBoxLayout;

    QPushButton* addBtn = new QPushButton(tr("添加"));
    QPushButton* editBtn = new QPushButton(tr("编辑"));
    QPushButton* removeBtn = new QPushButton(tr("删除"));

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(editBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);

    // 添加/编辑规则对话框
    auto showAddEditDialog = [this, &dialog](HighlightRule* existingRule = nullptr) -> bool {
        QDialog editDialog(&dialog);
        editDialog.setWindowTitle(existingRule ? tr("编辑规则") : tr("添加规则"));
        editDialog.setMinimumWidth(300);

        QFormLayout* form = new QFormLayout(&editDialog);

        QLineEdit* keywordEdit = new QLineEdit(existingRule ? existingRule->keyword : "");
        keywordEdit->setPlaceholderText(tr("例如: ERROR"));
        form->addRow(tr("关键词:"), keywordEdit);

        // 颜色选择
        QPushButton* colorBtn = new QPushButton;
        QColor currentColor = existingRule ? existingRule->color : QColor(231, 76, 60);
        colorBtn->setStyleSheet(QString("background-color: %1; color: white; font-weight: bold;").arg(currentColor.name()));
        colorBtn->setText(currentColor.name());
        connect(colorBtn, &QPushButton::clicked, [&currentColor, colorBtn]() {
            QColor color = QColorDialog::getColor(currentColor);
            if (color.isValid()) {
                currentColor = color;
                colorBtn->setStyleSheet(QString("background-color: %1; color: white; font-weight: bold;").arg(color.name()));
                colorBtn->setText(color.name());
            }
        });
        form->addRow(tr("颜色:"), colorBtn);

        QCheckBox* caseCheck = new QCheckBox(tr("区分大小写"));
        caseCheck->setChecked(existingRule ? existingRule->caseSensitive : false);
        form->addRow(caseCheck);

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &editDialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &editDialog, &QDialog::reject);

        if (editDialog.exec() == QDialog::Accepted) {
            QString keyword = keywordEdit->text().trimmed();
            if (keyword.isEmpty()) {
                QMessageBox::warning(&dialog, tr("错误"), tr("关键词不能为空"));
                return false;
            }

            if (existingRule) {
                existingRule->keyword = keyword;
                existingRule->color = currentColor;
                existingRule->caseSensitive = caseCheck->isChecked();
            } else {
                HighlightRule newRule;
                newRule.keyword = keyword;
                newRule.color = currentColor;
                newRule.caseSensitive = caseCheck->isChecked();
                newRule.enabled = true;
                addHighlightRule(newRule);
            }
            return true;
        }
        return false;
    };

    // 添加按钮
    connect(addBtn, &QPushButton::clicked, [&ruleList, &showAddEditDialog, this]() {
        if (showAddEditDialog()) {
            const HighlightRule& rule = m_highlightRules.last();
            QListWidgetItem* item = new QListWidgetItem(rule.keyword);
            item->setForeground(rule.color);
            item->setCheckState(Qt::Checked);
            ruleList->addItem(item);
        }
    });

    // 编辑按钮
    connect(editBtn, &QPushButton::clicked, [&ruleList, &showAddEditDialog, this]() {
        int row = ruleList->currentRow();
        if (row >= 0 && row < m_highlightRules.size()) {
            if (showAddEditDialog(&m_highlightRules[row])) {
                QListWidgetItem* item = ruleList->item(row);
                const HighlightRule& rule = m_highlightRules[row];
                item->setText(rule.keyword);
                item->setForeground(rule.color);
            }
        }
    });

    // 删除按钮
    connect(removeBtn, &QPushButton::clicked, [&ruleList, this]() {
        int row = ruleList->currentRow();
        if (row >= 0 && row < m_highlightRules.size()) {
            m_highlightRules.removeAt(row);
            delete ruleList->takeItem(row);
        }
    });

    // 复选框状态变化
    connect(ruleList, &QListWidget::itemChanged, [this, ruleList](QListWidgetItem* item) {
        int row = ruleList->row(item);
        if (row >= 0 && row < m_highlightRules.size()) {
            m_highlightRules[row].enabled = (item->checkState() == Qt::Checked);
        }
    });

    QDialogButtonBox* dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(dialogButtons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(dialogButtons);

    dialog.exec();
    scheduleHighlightUpdate();
}

void TabbedReceiveWidget::addHighlightRule(const HighlightRule& rule)
{
    m_highlightRules.append(rule);
    scheduleHighlightUpdate();
}

void TabbedReceiveWidget::removeHighlightRule(const QString& keyword)
{
    for (int i = 0; i < m_highlightRules.size(); ++i) {
        if (m_highlightRules[i].keyword == keyword) {
            m_highlightRules.removeAt(i);
            scheduleHighlightUpdate();
            return;
        }
    }
}

void TabbedReceiveWidget::clearHighlightRules()
{
    m_highlightRules.clear();
    scheduleHighlightUpdate();
}

void TabbedReceiveWidget::setHighlightEnabled(bool enabled)
{
    m_highlightEnabled = enabled;
    m_highlightCheck->setChecked(enabled);
    scheduleHighlightUpdate();
}

void TabbedReceiveWidget::scheduleHighlightUpdate()
{
    if (!m_mainTextEdit) {
        return;
    }

    if (!m_highlightEnabled || m_highlightRules.isEmpty()) {
        if (m_highlightTimer) {
            m_highlightTimer->stop();
        }
        applyHighlight();
        return;
    }

    if (!m_highlightTimer) {
        applyHighlight();
        return;
    }

    if (!m_highlightTimer->isActive()) {
        m_highlightTimer->start(120);
    }
}

void TabbedReceiveWidget::scheduleTerminalDisplayUpdate()
{
    if (!m_terminalRefreshTimer) {
        updateTerminalDisplay();
        return;
    }

    if (!m_terminalRefreshTimer->isActive()) {
        m_terminalRefreshTimer->start(40);
    }
}

void TabbedReceiveWidget::trimLineHistory()
{
    if (m_lineHistory.size() > m_maxLines) {
        m_lineHistory = m_lineHistory.mid(m_lineHistory.size() - m_maxLines);
    }
}

void TabbedReceiveWidget::trimRawDataBuffer()
{
    if (m_rawData.size() > m_maxRawDataBytes) {
        m_rawData = m_rawData.right(m_maxRawDataBytes);
    }
}

void TabbedReceiveWidget::updatePerformanceStats()
{
    if (!m_perfLabel) {
        return;
    }

    const qint64 bytes = m_perfRxBytes;
    m_perfRxBytes = 0;

    QString rateText;
    const double bytesPerSec = static_cast<double>(bytes);
    if (bytesPerSec >= 1024.0 * 1024.0) {
        rateText = QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 2) + " MB/s";
    } else if (bytesPerSec >= 1024.0) {
        rateText = QString::number(bytesPerSec / 1024.0, 'f', 1) + " KB/s";
    } else {
        rateText = QString::number(bytesPerSec, 'f', 0) + " B/s";
    }

    double renderMs = 0.0;
    if (m_perfRenderSamples > 0) {
        renderMs = m_perfRenderCostSumMs / static_cast<double>(m_perfRenderSamples);
    }
    m_perfRenderCostSumMs = 0.0;
    m_perfRenderSamples = 0;

    m_perfLabel->setText(tr("性能: 接收 %1 | 渲染 %2 ms")
                             .arg(rateText)
                             .arg(renderMs, 0, 'f', 2));
}

void TabbedReceiveWidget::applyHighlight()
{
    if (!m_highlighter) {
        return;
    }

    m_highlighter->setEnabled(m_highlightEnabled);
    m_highlighter->setRules(m_highlightRules);
    m_highlighter->rehighlight();
}

QString TabbedReceiveWidget::formatTimestamp() const
{
    return "[" + QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + "]";
}

QString TabbedReceiveWidget::bytesToHexString(const QByteArray& data) const
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        result += QString("%1 ").arg(
            static_cast<unsigned char>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return result;
}

void TabbedReceiveWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void TabbedReceiveWidget::retranslateUi()
{
    // 更新选项栏文字
    if (m_timestampCheck) {
        m_timestampCheck->setText(tr("时间戳"));
    }
    if (m_autoScrollCheck) {
        m_autoScrollCheck->setText(tr("自动滚动"));
    }
    if (m_hexDisplayCheck) {
        m_hexDisplayCheck->setText(tr("HEX显示"));
    }
    if (m_highlightCheck) {
        m_highlightCheck->setText(tr("高亮"));
    }
    if (m_highlightSettingsBtn) {
        m_highlightSettingsBtn->setText(tr("设置"));
        m_highlightSettingsBtn->setToolTip(tr("高亮规则设置"));
    }
    if (m_clearBtn) {
        m_clearBtn->setText(tr("清空"));
    }
    if (m_perfLabel) {
        m_perfLabel->setText(tr("性能: 接收 0 B/s | 渲染 0 ms"));
    }

    // 更新标签页标题
    if (m_tabWidget) {
        m_tabWidget->setTabText(0, tr("文本"));
        m_tabWidget->setTabText(1, tr("十六进制"));
        m_tabWidget->setTabText(2, tr("过滤"));
    }

    // 更新占位文本
    if (m_mainTextEdit) {
        m_mainTextEdit->setPlaceholderText(tr("接收到的数据将在这里显示..."));
    }
    if (m_filterInput) {
        m_filterInput->setPlaceholderText(tr("输入关键字进行过滤..."));
    }

    // 更新智能滚屏指示器文本
    if (m_smartScrollIndicator && m_smartScrollPaused) {
        m_smartScrollIndicator->setText(tr("已暂停滚动 - 滚动到底部恢复"));
    }
}

} // namespace ComAssistant
