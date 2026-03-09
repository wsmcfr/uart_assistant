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

namespace ComAssistant {

TabbedReceiveWidget::TabbedReceiveWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

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

    m_clearBtn = new QPushButton(tr("清空"));
    m_clearBtn->setFixedWidth(60);
    connect(m_clearBtn, &QPushButton::clicked, this, &TabbedReceiveWidget::onClearClicked);

    optionsLayout->addWidget(m_timestampCheck);
    optionsLayout->addWidget(m_autoScrollCheck);
    optionsLayout->addWidget(m_hexDisplayCheck);
    optionsLayout->addSpacing(8);
    optionsLayout->addWidget(m_highlightCheck);
    optionsLayout->addWidget(m_highlightSettingsBtn);
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
    m_mainTextEdit = new QTextEdit;
    m_mainTextEdit->setReadOnly(true);
    m_mainTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_mainTextEdit->setFont(QFont("Consolas", 10));
    m_mainTextEdit->document()->setMaximumBlockCount(m_maxLines);
    m_mainTextEdit->setPlaceholderText(tr("接收到的数据将在这里显示..."));

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
    m_hexTable = new QTableWidget;
    m_hexTable->setColumnCount(17);  // 16 hex + 1 ASCII

    QStringList headers;
    for (int i = 0; i < 16; ++i) {
        headers << QString("%1").arg(i, 2, 16, QChar('0')).toUpper();
    }
    headers << "ASCII";
    m_hexTable->setHorizontalHeaderLabels(headers);

    m_hexTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_hexTable->horizontalHeader()->setDefaultSectionSize(28);
    m_hexTable->horizontalHeader()->setStretchLastSection(true);
    m_hexTable->verticalHeader()->setDefaultSectionSize(22);
    m_hexTable->setFont(QFont("Consolas", 9));
    m_hexTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_hexTable->setSelectionMode(QAbstractItemView::ContiguousSelection);

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
    filterLayout->addWidget(m_filterResult);

    m_tabWidget->addTab(filterWidget, tr("过滤"));
}

void TabbedReceiveWidget::appendData(const QByteArray& data)
{
    m_rawData.append(data);

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

    updateTerminalDisplay();
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
    const int maxLines = 1000;
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
        m_mainTextEdit->moveCursor(QTextCursor::End);
        m_mainTextEdit->insertPlainText(text);
    }

    // 记录历史
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    m_lineHistory.append(lines);

    // 智能滚屏：仅在未暂停时滚动到底部
    if (m_autoScrollEnabled && !m_smartScrollPaused) {
        m_mainTextEdit->verticalScrollBar()->setValue(
            m_mainTextEdit->verticalScrollBar()->maximum());
    }

    // 实时应用高亮
    if (m_highlightEnabled && !m_highlightRules.isEmpty()) {
        applyHighlight();
    }
}

void TabbedReceiveWidget::appendToHexView(const QByteArray& data)
{
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);

        // 需要新行
        if (m_hexColIndex == 0) {
            m_hexTable->setRowCount(m_hexRowCount + 1);
            QString rowHeader = QString("%1").arg(m_hexRowCount * 16, 8, 16, QChar('0')).toUpper();
            m_hexTable->setVerticalHeaderItem(m_hexRowCount, new QTableWidgetItem(rowHeader));
            // 初始化 ASCII 列
            m_hexTable->setItem(m_hexRowCount, 16, new QTableWidgetItem(""));
        }

        // 设置十六进制值
        QString hexStr = QString("%1").arg(byte, 2, 16, QChar('0')).toUpper();
        m_hexTable->setItem(m_hexRowCount, m_hexColIndex, new QTableWidgetItem(hexStr));

        // 更新 ASCII 列
        QTableWidgetItem* asciiItem = m_hexTable->item(m_hexRowCount, 16);
        if (asciiItem) {
            QString ascii = asciiItem->text();
            ascii += (byte >= 32 && byte < 127) ? QChar(byte) : '.';
            asciiItem->setText(ascii);
        }

        m_hexColIndex++;
        if (m_hexColIndex >= 16) {
            m_hexColIndex = 0;
            m_hexRowCount++;
        }
    }

    if (m_autoScrollEnabled) {
        m_hexTable->scrollToBottom();
    }
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
    // 清空当前显示
    m_mainTextEdit->clear();

    // 根据当前HEX显示设置重新显示数据
    if (m_rawData.isEmpty()) {
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
}

void TabbedReceiveWidget::clear()
{
    m_rawData.clear();
    m_utf8Buffer.clear();  // 清空 UTF-8 缓冲区
    m_lineHistory.clear();
    m_needTimestamp = true;  // 重置时间戳状态

    m_mainTextEdit->clear();

    m_hexTable->setRowCount(0);
    m_hexRowCount = 0;
    m_hexColIndex = 0;

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
    }
}

void TabbedReceiveWidget::setMaxLines(int maxLines)
{
    m_maxLines = qBound(100, maxLines, 100000);
    if (m_mainTextEdit && m_mainTextEdit->document()) {
        m_mainTextEdit->document()->setMaximumBlockCount(m_maxLines);
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
                "QTextEdit {"
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
                "QTextEdit {"
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
                "QTextEdit {"
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
    if (m_highlightEnabled && !m_highlightRules.isEmpty()) {
        applyHighlight();
    }
}

void TabbedReceiveWidget::onHighlightToggled(bool checked)
{
    m_highlightEnabled = checked;
    applyHighlight();
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
    applyHighlight();
}

void TabbedReceiveWidget::addHighlightRule(const HighlightRule& rule)
{
    m_highlightRules.append(rule);
}

void TabbedReceiveWidget::removeHighlightRule(const QString& keyword)
{
    for (int i = 0; i < m_highlightRules.size(); ++i) {
        if (m_highlightRules[i].keyword == keyword) {
            m_highlightRules.removeAt(i);
            return;
        }
    }
}

void TabbedReceiveWidget::clearHighlightRules()
{
    m_highlightRules.clear();
}

void TabbedReceiveWidget::setHighlightEnabled(bool enabled)
{
    m_highlightEnabled = enabled;
    m_highlightCheck->setChecked(enabled);
    applyHighlight();
}

void TabbedReceiveWidget::applyHighlight()
{
    if (!m_highlightEnabled || m_highlightRules.isEmpty()) {
        // 移除所有高亮，恢复默认样式
        QTextCursor cursor(m_mainTextEdit->document());
        cursor.select(QTextCursor::Document);
        QTextCharFormat format;
        format.clearForeground();
        cursor.setCharFormat(format);
        return;
    }

    // 先清除所有高亮
    QTextCursor cursor(m_mainTextEdit->document());
    cursor.select(QTextCursor::Document);
    QTextCharFormat defaultFormat;
    defaultFormat.clearForeground();
    cursor.setCharFormat(defaultFormat);

    // 应用每条高亮规则
    for (const HighlightRule& rule : m_highlightRules) {
        if (!rule.enabled) continue;

        QTextCharFormat format;
        format.setForeground(rule.color);
        format.setFontWeight(QFont::Bold);  // 加粗使高亮更明显

        QString text = m_mainTextEdit->toPlainText();
        Qt::CaseSensitivity cs = rule.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;

        int pos = 0;
        while ((pos = text.indexOf(rule.keyword, pos, cs)) != -1) {
            QTextCursor highlightCursor(m_mainTextEdit->document());
            highlightCursor.setPosition(pos);
            highlightCursor.setPosition(pos + rule.keyword.length(), QTextCursor::KeepAnchor);
            highlightCursor.setCharFormat(format);
            pos += rule.keyword.length();
        }
    }
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
