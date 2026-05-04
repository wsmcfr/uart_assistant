/**
 * @file TerminalModeWidget.cpp
 * @brief 终端模式组件实现 - 完整VT100终端仿真
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "TerminalModeWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QAction>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTextStream>
#include <QDateTime>
#include <QFileDialog>
#include <QApplication>
#include <QPainter>
#include <QFontMetrics>
#include <QClipboard>
#include <QMessageBox>
#include <QSettings>
#include <QInputDialog>

namespace ComAssistant {

// ==================== 预定义主题 ====================

static QVector<TerminalTheme> createThemes()
{
    QVector<TerminalTheme> themes;

    // 经典绿色
    {
        TerminalTheme theme;
        theme.name = "Classic Green";
        theme.foreground = QColor(0x00, 0xFF, 0x00);
        theme.background = QColor(0x0A, 0x0A, 0x0A);
        theme.cursorColor = QColor(0x00, 0xFF, 0x00);
        theme.selectionBg = QColor(0x00, 0x88, 0x00);
        theme.selectionFg = QColor(0xFF, 0xFF, 0xFF);
        // 标准16色
        theme.palette[0] = QColor(0x00, 0x00, 0x00);
        theme.palette[1] = QColor(0xCD, 0x00, 0x00);
        theme.palette[2] = QColor(0x00, 0xCD, 0x00);
        theme.palette[3] = QColor(0xCD, 0xCD, 0x00);
        theme.palette[4] = QColor(0x00, 0x00, 0xEE);
        theme.palette[5] = QColor(0xCD, 0x00, 0xCD);
        theme.palette[6] = QColor(0x00, 0xCD, 0xCD);
        theme.palette[7] = QColor(0xE5, 0xE5, 0xE5);
        theme.palette[8] = QColor(0x7F, 0x7F, 0x7F);
        theme.palette[9] = QColor(0xFF, 0x00, 0x00);
        theme.palette[10] = QColor(0x00, 0xFF, 0x00);
        theme.palette[11] = QColor(0xFF, 0xFF, 0x00);
        theme.palette[12] = QColor(0x5C, 0x5C, 0xFF);
        theme.palette[13] = QColor(0xFF, 0x00, 0xFF);
        theme.palette[14] = QColor(0x00, 0xFF, 0xFF);
        theme.palette[15] = QColor(0xFF, 0xFF, 0xFF);
        themes.append(theme);
    }

    // Monokai
    {
        TerminalTheme theme;
        theme.name = "Monokai";
        theme.foreground = QColor(0xF8, 0xF8, 0xF2);
        theme.background = QColor(0x27, 0x28, 0x22);
        theme.cursorColor = QColor(0xF8, 0xF8, 0xF0);
        theme.selectionBg = QColor(0x49, 0x48, 0x3E);
        theme.selectionFg = QColor(0xF8, 0xF8, 0xF2);
        theme.palette[0] = QColor(0x27, 0x28, 0x22);
        theme.palette[1] = QColor(0xF9, 0x26, 0x72);
        theme.palette[2] = QColor(0xA6, 0xE2, 0x2E);
        theme.palette[3] = QColor(0xF4, 0xBF, 0x75);
        theme.palette[4] = QColor(0x66, 0xD9, 0xEF);
        theme.palette[5] = QColor(0xAE, 0x81, 0xFF);
        theme.palette[6] = QColor(0xA1, 0xEF, 0xE4);
        theme.palette[7] = QColor(0xF8, 0xF8, 0xF2);
        theme.palette[8] = QColor(0x75, 0x71, 0x5E);
        theme.palette[9] = QColor(0xF9, 0x26, 0x72);
        theme.palette[10] = QColor(0xA6, 0xE2, 0x2E);
        theme.palette[11] = QColor(0xF4, 0xBF, 0x75);
        theme.palette[12] = QColor(0x66, 0xD9, 0xEF);
        theme.palette[13] = QColor(0xAE, 0x81, 0xFF);
        theme.palette[14] = QColor(0xA1, 0xEF, 0xE4);
        theme.palette[15] = QColor(0xF9, 0xF8, 0xF5);
        themes.append(theme);
    }

    // Dracula
    {
        TerminalTheme theme;
        theme.name = "Dracula";
        theme.foreground = QColor(0xF8, 0xF8, 0xF2);
        theme.background = QColor(0x28, 0x2A, 0x36);
        theme.cursorColor = QColor(0xF8, 0xF8, 0xF2);
        theme.selectionBg = QColor(0x44, 0x47, 0x5A);
        theme.selectionFg = QColor(0xF8, 0xF8, 0xF2);
        theme.palette[0] = QColor(0x21, 0x22, 0x2C);
        theme.palette[1] = QColor(0xFF, 0x55, 0x55);
        theme.palette[2] = QColor(0x50, 0xFA, 0x7B);
        theme.palette[3] = QColor(0xF1, 0xFA, 0x8C);
        theme.palette[4] = QColor(0xBD, 0x93, 0xF9);
        theme.palette[5] = QColor(0xFF, 0x79, 0xC6);
        theme.palette[6] = QColor(0x8B, 0xE9, 0xFD);
        theme.palette[7] = QColor(0xF8, 0xF8, 0xF2);
        theme.palette[8] = QColor(0x62, 0x72, 0xA4);
        theme.palette[9] = QColor(0xFF, 0x6E, 0x6E);
        theme.palette[10] = QColor(0x69, 0xFF, 0x94);
        theme.palette[11] = QColor(0xFF, 0xFF, 0xA5);
        theme.palette[12] = QColor(0xD6, 0xAC, 0xFF);
        theme.palette[13] = QColor(0xFF, 0x92, 0xDF);
        theme.palette[14] = QColor(0xA4, 0xFF, 0xFF);
        theme.palette[15] = QColor(0xFF, 0xFF, 0xFF);
        themes.append(theme);
    }

    // Solarized Dark
    {
        TerminalTheme theme;
        theme.name = "Solarized Dark";
        theme.foreground = QColor(0x83, 0x94, 0x96);
        theme.background = QColor(0x00, 0x2B, 0x36);
        theme.cursorColor = QColor(0x93, 0xA1, 0xA1);
        theme.selectionBg = QColor(0x07, 0x36, 0x42);
        theme.selectionFg = QColor(0x93, 0xA1, 0xA1);
        theme.palette[0] = QColor(0x07, 0x36, 0x42);
        theme.palette[1] = QColor(0xDC, 0x32, 0x2F);
        theme.palette[2] = QColor(0x85, 0x99, 0x00);
        theme.palette[3] = QColor(0xB5, 0x89, 0x00);
        theme.palette[4] = QColor(0x26, 0x8B, 0xD2);
        theme.palette[5] = QColor(0xD3, 0x36, 0x82);
        theme.palette[6] = QColor(0x2A, 0xA1, 0x98);
        theme.palette[7] = QColor(0xEE, 0xE8, 0xD5);
        theme.palette[8] = QColor(0x00, 0x2B, 0x36);
        theme.palette[9] = QColor(0xCB, 0x4B, 0x16);
        theme.palette[10] = QColor(0x58, 0x6E, 0x75);
        theme.palette[11] = QColor(0x65, 0x7B, 0x83);
        theme.palette[12] = QColor(0x83, 0x94, 0x96);
        theme.palette[13] = QColor(0x6C, 0x71, 0xC4);
        theme.palette[14] = QColor(0x93, 0xA1, 0xA1);
        theme.palette[15] = QColor(0xFD, 0xF6, 0xE3);
        themes.append(theme);
    }

    // One Dark
    {
        TerminalTheme theme;
        theme.name = "One Dark";
        theme.foreground = QColor(0xAB, 0xB2, 0xBF);
        theme.background = QColor(0x28, 0x2C, 0x34);
        theme.cursorColor = QColor(0x52, 0x8B, 0xFF);
        theme.selectionBg = QColor(0x3E, 0x44, 0x51);
        theme.selectionFg = QColor(0xAB, 0xB2, 0xBF);
        theme.palette[0] = QColor(0x28, 0x2C, 0x34);
        theme.palette[1] = QColor(0xE0, 0x6C, 0x75);
        theme.palette[2] = QColor(0x98, 0xC3, 0x79);
        theme.palette[3] = QColor(0xE5, 0xC0, 0x7B);
        theme.palette[4] = QColor(0x61, 0xAF, 0xEF);
        theme.palette[5] = QColor(0xC6, 0x78, 0xDD);
        theme.palette[6] = QColor(0x56, 0xB6, 0xC2);
        theme.palette[7] = QColor(0xAB, 0xB2, 0xBF);
        theme.palette[8] = QColor(0x54, 0x5B, 0x6E);
        theme.palette[9] = QColor(0xE0, 0x6C, 0x75);
        theme.palette[10] = QColor(0x98, 0xC3, 0x79);
        theme.palette[11] = QColor(0xE5, 0xC0, 0x7B);
        theme.palette[12] = QColor(0x61, 0xAF, 0xEF);
        theme.palette[13] = QColor(0xC6, 0x78, 0xDD);
        theme.palette[14] = QColor(0x56, 0xB6, 0xC2);
        theme.palette[15] = QColor(0xFF, 0xFF, 0xFF);
        themes.append(theme);
    }

    // Ubuntu
    {
        TerminalTheme theme;
        theme.name = "Ubuntu";
        theme.foreground = QColor(0xEE, 0xEE, 0xEC);
        theme.background = QColor(0x30, 0x0A, 0x24);
        theme.cursorColor = QColor(0xEE, 0xEE, 0xEC);
        theme.selectionBg = QColor(0x59, 0x2B, 0x4A);
        theme.selectionFg = QColor(0xEE, 0xEE, 0xEC);
        theme.palette[0] = QColor(0x2E, 0x34, 0x36);
        theme.palette[1] = QColor(0xCC, 0x00, 0x00);
        theme.palette[2] = QColor(0x4E, 0x9A, 0x06);
        theme.palette[3] = QColor(0xC4, 0xA0, 0x00);
        theme.palette[4] = QColor(0x34, 0x65, 0xA4);
        theme.palette[5] = QColor(0x75, 0x50, 0x7B);
        theme.palette[6] = QColor(0x06, 0x98, 0x9A);
        theme.palette[7] = QColor(0xD3, 0xD7, 0xCF);
        theme.palette[8] = QColor(0x55, 0x57, 0x53);
        theme.palette[9] = QColor(0xEF, 0x29, 0x29);
        theme.palette[10] = QColor(0x8A, 0xE2, 0x34);
        theme.palette[11] = QColor(0xFC, 0xE9, 0x4F);
        theme.palette[12] = QColor(0x72, 0x9F, 0xCF);
        theme.palette[13] = QColor(0xAD, 0x7F, 0xA8);
        theme.palette[14] = QColor(0x34, 0xE2, 0xE2);
        theme.palette[15] = QColor(0xEE, 0xEE, 0xEC);
        themes.append(theme);
    }

    return themes;
}

static QVector<TerminalTheme> s_themes = createThemes();

// ==================== VT100Display ====================

VT100Display::VT100Display(TerminalBuffer* buffer, QWidget* parent)
    : QWidget(parent)
    , m_buffer(buffer)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_InputMethodEnabled);
    setMouseTracking(true);

    // 默认字体
    m_font = QFont("Consolas", 12);
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    calculateCellSize();

    // 默认主题
    m_theme = s_themes[0];

    // 光标闪烁定时器
    m_cursorTimer = new QTimer(this);
    m_cursorTimer->setInterval(530);
    connect(m_cursorTimer, &QTimer::timeout, this, &VT100Display::onCursorBlink);
    m_cursorTimer->start();

    // 连接缓冲区更新信号
    connect(m_buffer, &TerminalBuffer::screenUpdated, this, &VT100Display::onScreenUpdated);
}

void VT100Display::setTerminalFont(const QFont& font)
{
    m_font = font;
    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    calculateCellSize();
    updateTerminalSize();
    update();
}

void VT100Display::setFontSize(int size)
{
    m_font.setPointSize(qBound(6, size, 72));
    calculateCellSize();
    updateTerminalSize();
    update();
}

void VT100Display::setTheme(const TerminalTheme& theme)
{
    m_theme = theme;
    update();
}

void VT100Display::setCursorBlinking(bool enabled)
{
    m_cursorBlinking = enabled;
    if (enabled) {
        m_cursorTimer->start();
    } else {
        m_cursorTimer->stop();
        m_cursorVisible = true;
    }
    update();
}

void VT100Display::setCursorStyle(int style)
{
    m_cursorStyle = style;
    update();
}

void VT100Display::scrollToBottom()
{
    m_scrollOffset = 0;
    update();
}

void VT100Display::scrollBy(int lines)
{
    int maxScroll = m_buffer->historyLineCount();
    m_scrollOffset = qBound(0, m_scrollOffset + lines, maxScroll);
    update();
}

QString VT100Display::selectedText() const
{
    if (m_selectionStart == m_selectionEnd) return QString();

    QString result;
    QPoint start = m_selectionStart;
    QPoint end = m_selectionEnd;

    // 确保start在end之前
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x())) {
        std::swap(start, end);
    }

    for (int row = start.y(); row <= end.y(); ++row) {
        int colStart = (row == start.y()) ? start.x() : 0;
        int colEnd = (row == end.y()) ? end.x() : m_buffer->cols() - 1;

        for (int col = colStart; col <= colEnd; ++col) {
            result += m_buffer->cellAt(row, col).character;
        }

        if (row < end.y()) {
            result += '\n';
        }
    }

    return result;
}

void VT100Display::clearSelection()
{
    m_selectionStart = m_selectionEnd = QPoint(-1, -1);
    update();
}

void VT100Display::calculateCellSize()
{
    QFontMetrics fm(m_font);
    m_cellWidth = fm.horizontalAdvance('M');
    m_cellHeight = fm.height();
    m_descent = fm.descent();
}

void VT100Display::updateTerminalSize()
{
    if (m_cellWidth == 0 || m_cellHeight == 0) return;

    int cols = width() / m_cellWidth;
    int rows = height() / m_cellHeight;

    if (cols > 0 && rows > 0 && (cols != m_buffer->cols() || rows != m_buffer->rows())) {
        m_buffer->resize(cols, rows);
        emit sizeChanged(cols, rows);
    }
}

QPoint VT100Display::cellAt(const QPoint& pos) const
{
    int col = pos.x() / m_cellWidth;
    int row = pos.y() / m_cellHeight;
    return QPoint(qBound(0, col, m_buffer->cols() - 1),
                  qBound(0, row, m_buffer->rows() - 1));
}

void VT100Display::onCursorBlink()
{
    if (m_cursorBlinking && hasFocus()) {
        m_cursorVisible = !m_cursorVisible;
        update();
    }
}

void VT100Display::onScreenUpdated()
{
    // 有新数据时自动滚动到底部，确保显示最新内容
    m_scrollOffset = 0;
    update();
}

void VT100Display::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setFont(m_font);

    // 绘制背景
    painter.fillRect(rect(), m_theme.background);

    const auto& screen = m_buffer->screen();
    int rows = m_buffer->rows();
    int cols = m_buffer->cols();

    // 绘制每个单元格（考虑滚动偏移）
    for (int displayRow = 0; displayRow < rows; ++displayRow) {
        // 根据滚动偏移计算实际数据来源
        int histIndex = m_scrollOffset - (rows - displayRow);
        for (int col = 0; col < cols; ++col) {
            const TerminalCell* cellPtr = nullptr;
            if (histIndex < 0) {
                // 从当前屏幕缓冲区读取
                cellPtr = &screen[displayRow][col];
            } else {
                // 从历史缓冲区读取
                int histLine = m_buffer->historyLineCount() - 1 - histIndex;
                if (histLine >= 0 && histLine < m_buffer->historyLineCount()) {
                    const auto& histLineData = m_buffer->historyLine(histLine);
                    if (col < histLineData.size()) {
                        cellPtr = &histLineData[col];
                    }
                }
                if (!cellPtr) {
                    static const TerminalCell emptyCell;
                    cellPtr = &emptyCell;
                }
            }
            const TerminalCell& cell = *cellPtr;

            int x = col * m_cellWidth;
            int y = displayRow * m_cellHeight;

            // 检查是否在选择区域内
            bool selected = false;
            if (m_selectionStart != m_selectionEnd) {
                QPoint start = m_selectionStart;
                QPoint end = m_selectionEnd;
                if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x())) {
                    std::swap(start, end);
                }
                if ((displayRow > start.y() || (displayRow == start.y() && col >= start.x())) &&
                    (displayRow < end.y() || (displayRow == end.y() && col <= end.x()))) {
                    selected = true;
                }
            }

            // 确定颜色
            QColor fg = cell.attr.foreground;
            QColor bg = cell.attr.background;

            // 如果是默认颜色，使用主题颜色
            static const QColor defaultFg(0xAA, 0xAA, 0xAA);
            static const QColor defaultBg(0x00, 0x00, 0x00);

            if (fg == defaultFg) {
                fg = m_theme.foreground;
            }
            if (bg == defaultBg) {
                bg = m_theme.background;
            }

            if (cell.attr.reverse) {
                std::swap(fg, bg);
            }

            if (selected) {
                fg = m_theme.selectionFg;
                bg = m_theme.selectionBg;
            }

            // 绘制背景（如果不是默认背景）
            if (bg != m_theme.background || selected) {
                painter.fillRect(x, y, m_cellWidth, m_cellHeight, bg);
            }

            // 绘制字符
            if (cell.character != ' ' && !cell.attr.hidden) {
                QFont cellFont = m_font;
                if (cell.attr.bold) {
                    cellFont.setBold(true);
                }
                if (cell.attr.italic) {
                    cellFont.setItalic(true);
                }
                if (cell.attr.dim) {
                    fg = fg.darker(150);
                }

                painter.setFont(cellFont);
                painter.setPen(fg);
                painter.drawText(x, y + m_cellHeight - m_descent, QString(cell.character));

                // 下划线
                if (cell.attr.underline) {
                    painter.drawLine(x, y + m_cellHeight - 1, x + m_cellWidth, y + m_cellHeight - 1);
                }

                // 删除线
                if (cell.attr.strikethrough) {
                    int midY = y + m_cellHeight / 2;
                    painter.drawLine(x, midY, x + m_cellWidth, midY);
                }

                painter.setFont(m_font);
            }
        }
    }

    // 绘制光标
    if (m_buffer->isCursorVisible() && m_cursorVisible && m_scrollOffset == 0) {
        int cursorX = m_buffer->cursorCol() * m_cellWidth;
        int cursorY = m_buffer->cursorRow() * m_cellHeight;

        painter.setPen(Qt::NoPen);
        painter.setBrush(m_theme.cursorColor);

        switch (m_cursorStyle) {
        case 0:  // 块状光标
            painter.setOpacity(0.7);
            painter.drawRect(cursorX, cursorY, m_cellWidth, m_cellHeight);
            painter.setOpacity(1.0);
            break;
        case 1:  // 下划线光标
            painter.drawRect(cursorX, cursorY + m_cellHeight - 2, m_cellWidth, 2);
            break;
        case 2:  // 竖线光标
            painter.drawRect(cursorX, cursorY, 2, m_cellHeight);
            break;
        }
    }
}

void VT100Display::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateTerminalSize();
}

void VT100Display::keyPressEvent(QKeyEvent* event)
{
    int key = event->key();
    bool ctrl = event->modifiers() & Qt::ControlModifier;

    // Ctrl+C/D/Z 等控制字符直接发送
    if (ctrl) {
        QByteArray data = keyToBytes(event);
        if (!data.isEmpty()) {
            emit keyPressed(data);
            scrollToBottom();
        }
        return;
    }

    // 行编辑模式
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // 发送整行 + 换行符
        {
            // 本地显示换行
            if (m_localEcho) {
                m_buffer->newLine();
            }
            // 发送数据（使用设置的换行符）
            QByteArray data = m_inputBuffer.toUtf8() + m_lineEnding.toUtf8();
            emit keyPressed(data);
            m_inputBuffer.clear();
            scrollToBottom();
        }
        break;

    case Qt::Key_Backspace:
        // 删除最后一个字符
        if (!m_inputBuffer.isEmpty()) {
            m_inputBuffer.chop(1);
            if (m_localEcho) {
                m_buffer->backspace();
                // 清除光标位置的字符
                m_buffer->cellAt(m_buffer->cursorRow(), m_buffer->cursorCol()).character = ' ';
                update();
            }
        }
        break;

    case Qt::Key_Tab:
        // Tab键发送到设备
        emit keyPressed(QByteArray(1, '\t'));
        break;

    case Qt::Key_Up:
    case Qt::Key_Down:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
    case Qt::Key_Insert:
    case Qt::Key_Delete:
    case Qt::Key_F1:
    case Qt::Key_F2:
    case Qt::Key_F3:
    case Qt::Key_F4:
    case Qt::Key_F5:
    case Qt::Key_F6:
    case Qt::Key_F7:
    case Qt::Key_F8:
    case Qt::Key_F9:
    case Qt::Key_F10:
    case Qt::Key_F11:
    case Qt::Key_F12:
    case Qt::Key_Escape:
        // 功能键直接发送
        {
            QByteArray data = keyToBytes(event);
            if (!data.isEmpty()) {
                emit keyPressed(data);
            }
        }
        break;

    default:
        // 普通字符添加到缓冲区
        {
            QString text = event->text();
            if (!text.isEmpty() && text[0].isPrint()) {
                m_inputBuffer += text;
                // 本地回显时显示字符
                if (m_localEcho) {
                    for (const QChar& ch : text) {
                        m_buffer->putChar(ch);
                    }
                    update();
                }
            }
        }
        break;
    }
}

QByteArray VT100Display::keyToBytes(QKeyEvent* event) const
{
    QByteArray result;

    // 处理修饰键
    bool ctrl = event->modifiers() & Qt::ControlModifier;
    bool alt = event->modifiers() & Qt::AltModifier;
    bool shift = event->modifiers() & Qt::ShiftModifier;

    // Ctrl+字母 -> 控制字符
    if (ctrl && !alt) {
        int key = event->key();
        if (key >= Qt::Key_A && key <= Qt::Key_Z) {
            result.append(static_cast<char>(key - Qt::Key_A + 1));
            return result;
        }
        // 特殊组合
        switch (key) {
        case Qt::Key_Space:
        case Qt::Key_2:
            result.append('\x00');  // NUL
            return result;
        case Qt::Key_BracketLeft:
        case Qt::Key_3:
            result.append('\x1B');  // ESC
            return result;
        case Qt::Key_Backslash:
        case Qt::Key_4:
            result.append('\x1C');  // FS
            return result;
        case Qt::Key_BracketRight:
        case Qt::Key_5:
            result.append('\x1D');  // GS
            return result;
        case Qt::Key_6:
            result.append('\x1E');  // RS
            return result;
        case Qt::Key_Minus:
        case Qt::Key_7:
            result.append('\x1F');  // US
            return result;
        }
    }

    // 功能键和方向键 -> ANSI转义序列
    switch (event->key()) {
    case Qt::Key_Up:
        result = "\x1B[A";
        break;
    case Qt::Key_Down:
        result = "\x1B[B";
        break;
    case Qt::Key_Right:
        result = "\x1B[C";
        break;
    case Qt::Key_Left:
        result = "\x1B[D";
        break;
    case Qt::Key_Home:
        result = ctrl ? "\x1B[1;5H" : "\x1B[H";
        break;
    case Qt::Key_End:
        result = ctrl ? "\x1B[1;5F" : "\x1B[F";
        break;
    case Qt::Key_PageUp:
        result = "\x1B[5~";
        break;
    case Qt::Key_PageDown:
        result = "\x1B[6~";
        break;
    case Qt::Key_Insert:
        result = "\x1B[2~";
        break;
    case Qt::Key_Delete:
        result = "\x1B[3~";
        break;
    case Qt::Key_F1:
        result = "\x1BOP";
        break;
    case Qt::Key_F2:
        result = "\x1BOQ";
        break;
    case Qt::Key_F3:
        result = "\x1BOR";
        break;
    case Qt::Key_F4:
        result = "\x1BOS";
        break;
    case Qt::Key_F5:
        result = "\x1B[15~";
        break;
    case Qt::Key_F6:
        result = "\x1B[17~";
        break;
    case Qt::Key_F7:
        result = "\x1B[18~";
        break;
    case Qt::Key_F8:
        result = "\x1B[19~";
        break;
    case Qt::Key_F9:
        result = "\x1B[20~";
        break;
    case Qt::Key_F10:
        result = "\x1B[21~";
        break;
    case Qt::Key_F11:
        result = "\x1B[23~";
        break;
    case Qt::Key_F12:
        result = "\x1B[24~";
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        result = "\r";
        break;
    case Qt::Key_Tab:
        result = "\t";
        break;
    case Qt::Key_Backspace:
        result = "\x7F";
        break;
    case Qt::Key_Escape:
        result = "\x1B";
        break;
    default:
        // 普通字符
        QString text = event->text();
        if (!text.isEmpty()) {
            result = text.toUtf8();
        }
        break;
    }

    // Alt+key -> ESC + key
    if (alt && !result.isEmpty() && result[0] != '\x1B') {
        result.prepend('\x1B');
    }

    return result;
}

void VT100Display::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_selectionStart = cellAt(event->pos());
        m_selectionEnd = m_selectionStart;
        update();
    }
    setFocus();
}

void VT100Display::mouseMoveEvent(QMouseEvent* event)
{
    if (m_selecting) {
        m_selectionEnd = cellAt(event->pos());
        update();
    }
}

void VT100Display::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_selecting = false;

        // 如果有选中内容，复制到剪贴板
        QString text = selectedText();
        if (!text.isEmpty()) {
            QApplication::clipboard()->setText(text);
        }
    }
}

void VT100Display::wheelEvent(QWheelEvent* event)
{
    int delta = event->angleDelta().y();
    if (delta > 0) {
        scrollBy(3);  // 向上滚动
    } else if (delta < 0) {
        scrollBy(-3);  // 向下滚动
    }
}

void VT100Display::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    m_cursorVisible = true;
    update();
}

void VT100Display::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

// ==================== CommandLineEdit ====================

CommandLineEdit::CommandLineEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setPlaceholderText(tr("输入命令..."));
    setFont(QFont("Consolas", 11));
    setStyleSheet(R"(
        QLineEdit {
            background-color: #2a2a2a;
            color: #00ff00;
            border: 1px solid #404040;
            border-radius: 3px;
            padding: 5px 10px;
        }
        QLineEdit:focus {
            border-color: #00aa00;
        }
    )");

    connect(this, &QLineEdit::returnPressed, [this]() {
        QString cmd = text().trimmed();
        if (!cmd.isEmpty()) {
            addToHistory(cmd);
            emit commandSubmitted(cmd);
        }
        clear();
        m_historyIndex = -1;
    });
}

void CommandLineEdit::addToHistory(const QString& command)
{
    if (!m_history.isEmpty() && m_history.first() == command) {
        return;
    }

    m_history.prepend(command);
    if (m_history.size() > MAX_HISTORY) {
        m_history.removeLast();
    }
    m_historyIndex = -1;
}

void CommandLineEdit::clearHistory()
{
    m_history.clear();
    m_historyIndex = -1;
}

void CommandLineEdit::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        if (m_history.isEmpty()) break;
        if (m_historyIndex < 0) {
            m_currentInput = text();
            m_historyIndex = 0;
        } else if (m_historyIndex < m_history.size() - 1) {
            m_historyIndex++;
        }
        setText(m_history[m_historyIndex]);
        break;

    case Qt::Key_Down:
        if (m_historyIndex < 0) break;
        if (m_historyIndex > 0) {
            m_historyIndex--;
            setText(m_history[m_historyIndex]);
        } else {
            m_historyIndex = -1;
            setText(m_currentInput);
        }
        break;

    case Qt::Key_Tab:
        emit tabPressed();
        event->accept();
        return;

    case Qt::Key_Escape:
        clear();
        m_historyIndex = -1;
        break;

    default:
        QLineEdit::keyPressEvent(event);
        break;
    }
}

void CommandLineEdit::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QLineEdit::changeEvent(event);
}

void CommandLineEdit::retranslateUi()
{
    setPlaceholderText(tr("输入命令..."));
}

// ==================== TerminalModeWidget ====================

TerminalModeWidget::TerminalModeWidget(QWidget* parent)
    : IModeWidget(parent)
{
    // 创建核心组件
    m_termBuffer = new TerminalBuffer(80, 24, this);
    m_ansiParser = new AnsiParser(m_termBuffer, this);

    setupUi();
    setupToolBar();
    loadCommands();  // 加载命令列表

    // 初始化VT100Display设置
    m_display->setLineEnding(getLineEnding());

    // 连接信号
    connect(m_ansiParser, &AnsiParser::bell, this, [this]() {
        QApplication::beep();
    });
}

TerminalModeWidget::~TerminalModeWidget()
{
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
    }
}

void TerminalModeWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // VT100终端显示区 - 直接在此输入，像真正的终端
    m_display = new VT100Display(m_termBuffer);
    mainLayout->addWidget(m_display, 1);

    // 连接显示组件的按键信号
    connect(m_display, &VT100Display::keyPressed, this, &TerminalModeWidget::onKeyPressed);

    // 底部命令输入栏 - 可见，方便用户直接输入发送
    QWidget* inputBar = new QWidget;
    inputBar->setStyleSheet(R"(
        QWidget {
            background-color: #1e1e1e;
            border-top: 1px solid #333333;
        }
    )");
    QHBoxLayout* inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(4, 2, 4, 2);
    inputLayout->setSpacing(4);

    // 命令提示符标签
    QLabel* promptLabel = new QLabel(">>>");
    promptLabel->setStyleSheet("color: #00cc00; font-family: Consolas, monospace; font-size: 12pt; font-weight: bold;");

    // 命令输入框
    m_commandLine = new CommandLineEdit;
    m_commandLine->setStyleSheet(R"(
        QLineEdit {
            background-color: #2d2d2d;
            color: #cccccc;
            border: 1px solid #444444;
            border-radius: 3px;
            padding: 3px 6px;
            font-family: Consolas, monospace;
            font-size: 12pt;
        }
        QLineEdit:focus {
            border-color: #006600;
        }
    )");
    m_commandLine->setPlaceholderText(tr("输入命令..."));

    // 发送按钮
    QPushButton* sendBtn = new QPushButton(tr("发送"));
    sendBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #006600;
            color: white;
            border: none;
            border-radius: 3px;
            padding: 4px 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #008800;
        }
        QPushButton:pressed {
            background-color: #004400;
        }
        QPushButton:disabled {
            background-color: #444444;
            color: #888888;
        }
    )");

    inputLayout->addWidget(promptLabel);
    inputLayout->addWidget(m_commandLine, 1);
    inputLayout->addWidget(sendBtn);

    mainLayout->addWidget(inputBar);

    // 连接发送逻辑
    connect(m_commandLine, &QLineEdit::returnPressed, this, [this]() {
        sendCommand(m_commandLine->text());
        m_commandLine->clear();
    });
    connect(sendBtn, &QPushButton::clicked, this, [this]() {
        sendCommand(m_commandLine->text());
        m_commandLine->clear();
    });
    connect(m_commandLine, &CommandLineEdit::tabPressed, this, &TerminalModeWidget::onTabPressed);

    // 保存发送按钮引用，用于连接状态更新
    m_sendBtn = sendBtn;

    // 初始禁用（未连接时）
    m_commandLine->setEnabled(m_connected);
    m_sendBtn->setEnabled(m_connected);
}

void TerminalModeWidget::setupToolBar()
{
    m_toolBar = new QToolBar;
    m_toolBar->setStyleSheet(R"(
        QToolBar {
            background-color: #2a2a2a;
            border: none;
            spacing: 5px;
            padding: 3px;
        }
        QToolButton {
            background-color: transparent;
            color: #cccccc;
            border: none;
            padding: 5px 10px;
            border-radius: 3px;
        }
        QToolButton:hover {
            background-color: #404040;
        }
        QToolButton:pressed {
            background-color: #505050;
        }
        QToolButton:checked {
            background-color: #006600;
            color: white;
        }
    )");

    // 清屏
    QAction* clearAction = m_toolBar->addAction(tr("清屏"));
    connect(clearAction, &QAction::triggered, this, &TerminalModeWidget::onClearScreen);

    m_toolBar->addSeparator();

    // 本地回显
    m_echoAction = m_toolBar->addAction(tr("本地回显"));
    m_echoAction->setCheckable(true);
    connect(m_echoAction, &QAction::toggled, this, &TerminalModeWidget::onToggleLocalEcho);

    // 时间戳
    QAction* timestampAction = m_toolBar->addAction(tr("时间戳"));
    timestampAction->setCheckable(true);
    connect(timestampAction, &QAction::toggled, this, &TerminalModeWidget::onToggleTimestamp);

    // 日志
    QAction* logAction = m_toolBar->addAction(tr("记录日志"));
    logAction->setCheckable(true);
    connect(logAction, &QAction::toggled, this, &TerminalModeWidget::onToggleLog);

    m_toolBar->addSeparator();

    // 快捷键按钮
    QAction* ctrlCAction = m_toolBar->addAction("Ctrl+C");
    ctrlCAction->setToolTip(tr("发送中断信号"));
    connect(ctrlCAction, &QAction::triggered, this, &TerminalModeWidget::onSendCtrlC);

    QAction* ctrlDAction = m_toolBar->addAction("Ctrl+D");
    ctrlDAction->setToolTip(tr("发送 EOF"));
    connect(ctrlDAction, &QAction::triggered, this, &TerminalModeWidget::onSendCtrlD);

    QAction* ctrlZAction = m_toolBar->addAction("Ctrl+Z");
    ctrlZAction->setToolTip(tr("发送挂起信号"));
    connect(ctrlZAction, &QAction::triggered, this, &TerminalModeWidget::onSendCtrlZ);

    m_toolBar->addSeparator();

    // 换行模式
    m_newlineLabel = new QLabel(tr("换行:"));
    m_newlineLabel->setStyleSheet("color: #cccccc; margin-left: 5px;");
    m_toolBar->addWidget(m_newlineLabel);

    m_newlineCombo = new QComboBox;
    m_newlineCombo->addItem("LF (\\n)", "LF");
    m_newlineCombo->addItem("CR (\\r)", "CR");
    m_newlineCombo->addItem("CRLF (\\r\\n)", "CRLF");
    m_newlineCombo->setStyleSheet(R"(
        QComboBox {
            background-color: #404040;
            color: #cccccc;
            border: none;
            border-radius: 3px;
            padding: 3px 10px;
        }
    )");
    connect(m_newlineCombo, &QComboBox::currentTextChanged, [this]() {
        m_newlineMode = m_newlineCombo->currentData().toString();
        // 同步到VT100Display
        m_display->setLineEnding(getLineEnding());
    });
    m_toolBar->addWidget(m_newlineCombo);

    m_toolBar->addSeparator();

    // 主题选择
    m_themeLabel = new QLabel(tr("主题:"));
    m_themeLabel->setStyleSheet("color: #cccccc; margin-left: 5px;");
    m_toolBar->addWidget(m_themeLabel);

    m_themeCombo = new QComboBox;
    for (const auto& theme : s_themes) {
        m_themeCombo->addItem(theme.name);
    }
    m_themeCombo->setStyleSheet(R"(
        QComboBox {
            background-color: #404040;
            color: #cccccc;
            border: none;
            border-radius: 3px;
            padding: 3px 10px;
        }
    )");
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TerminalModeWidget::onThemeChanged);
    m_toolBar->addWidget(m_themeCombo);

    // 字体大小
    m_fontSizeLabel = new QLabel(tr("字号:"));
    m_fontSizeLabel->setStyleSheet("color: #cccccc; margin-left: 5px;");
    m_toolBar->addWidget(m_fontSizeLabel);

    m_fontSizeSpinBox = new QSpinBox;
    m_fontSizeSpinBox->setRange(6, 72);
    m_fontSizeSpinBox->setValue(12);
    m_fontSizeSpinBox->setStyleSheet(R"(
        QSpinBox {
            background-color: #404040;
            color: #cccccc;
            border: none;
            border-radius: 3px;
            padding: 3px 5px;
        }
    )");
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TerminalModeWidget::onFontSizeChanged);
    m_toolBar->addWidget(m_fontSizeSpinBox);

    m_toolBar->addSeparator();

    // 编辑命令列表
    QAction* editCmdsAction = m_toolBar->addAction(tr("编辑命令"));
    editCmdsAction->setToolTip(tr("编辑本地命令列表（用于Tab补全）"));
    connect(editCmdsAction, &QAction::triggered, this, &TerminalModeWidget::onEditCommands);
}

void TerminalModeWidget::appendReceivedData(const QByteArray& data)
{
    m_ansiParser->process(data);

    // 记录日志
    if (m_loggingEnabled && m_logFile && m_logFile->isOpen()) {
        QTextStream stream(m_logFile);
        if (m_timestampEnabled) {
            stream << "[" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "] ";
        }
        stream << QString::fromUtf8(data);
    }

    emit dataReceived(data);
}

void TerminalModeWidget::appendSentData(const QByteArray& data)
{
    if (m_display->localEcho()) {
        m_ansiParser->process(data);
    }
}

void TerminalModeWidget::setConnected(bool connected)
{
    IModeWidget::setConnected(connected);
    // 更新命令输入栏的启用状态
    if (m_commandLine) {
        m_commandLine->setEnabled(connected);
    }
    if (m_sendBtn) {
        m_sendBtn->setEnabled(connected);
    }
}

void TerminalModeWidget::clear()
{
    m_termBuffer->clearScreen();
    m_termBuffer->setCursorPosition(0, 0);
}

bool TerminalModeWidget::exportToFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    const auto& screen = m_termBuffer->screen();
    for (const auto& row : screen) {
        QString line;
        for (const auto& cell : row) {
            line += cell.character;
        }
        stream << line.trimmed() << "\n";
    }
    file.close();
    return true;
}

void TerminalModeWidget::onActivated()
{
    m_display->setFocus();
}

void TerminalModeWidget::onDeactivated()
{
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
}

void TerminalModeWidget::setLocalEcho(bool enabled)
{
    m_display->setLocalEcho(enabled);
    m_echoAction->setChecked(enabled);
}

bool TerminalModeWidget::localEcho() const
{
    return m_display->localEcho();
}

void TerminalModeWidget::setNewlineMode(const QString& mode)
{
    m_newlineMode = mode;
}

QVector<TerminalTheme> TerminalModeWidget::availableThemes()
{
    return s_themes;
}

void TerminalModeWidget::setTheme(const QString& themeName)
{
    for (int i = 0; i < s_themes.size(); ++i) {
        if (s_themes[i].name == themeName) {
            m_display->setTheme(s_themes[i]);
            m_themeCombo->setCurrentIndex(i);
            break;
        }
    }
}

void TerminalModeWidget::setFontSize(int size)
{
    m_display->setFontSize(size);
    m_fontSizeSpinBox->setValue(size);
}

int TerminalModeWidget::fontSize() const
{
    return m_display->fontSize();
}

void TerminalModeWidget::onCommandSubmitted(const QString& command)
{
    sendCommand(command);
}

void TerminalModeWidget::onClearScreen()
{
    clear();
}

void TerminalModeWidget::onToggleLocalEcho(bool checked)
{
    m_display->setLocalEcho(checked);
}

void TerminalModeWidget::onToggleTimestamp(bool checked)
{
    m_timestampEnabled = checked;
}

void TerminalModeWidget::onToggleLog(bool checked)
{
    m_loggingEnabled = checked;

    if (checked) {
        QString fileName = QFileDialog::getSaveFileName(this,
            tr("选择日志文件"),
            QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_terminal.log",
            tr("日志文件 (*.log *.txt)"));

        if (!fileName.isEmpty()) {
            m_logFile = new QFile(fileName);
            if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
                delete m_logFile;
                m_logFile = nullptr;
                m_loggingEnabled = false;
            }
        } else {
            m_loggingEnabled = false;
        }
    } else {
        if (m_logFile) {
            m_logFile->close();
            delete m_logFile;
            m_logFile = nullptr;
        }
    }
}

void TerminalModeWidget::onSendCtrlC()
{
    emit sendDataRequested(QByteArray(1, 0x03));
}

void TerminalModeWidget::onSendCtrlD()
{
    emit sendDataRequested(QByteArray(1, 0x04));
}

void TerminalModeWidget::onSendCtrlZ()
{
    emit sendDataRequested(QByteArray(1, 0x1A));
}

void TerminalModeWidget::onKeyPressed(const QByteArray& data)
{
    if (!m_connected) {
        return;
    }
    emit sendDataRequested(data);
}

void TerminalModeWidget::onThemeChanged(int index)
{
    if (index >= 0 && index < s_themes.size()) {
        m_display->setTheme(s_themes[index]);
    }
}

void TerminalModeWidget::onFontSizeChanged(int size)
{
    m_display->setFontSize(size);
}

void TerminalModeWidget::onTabPressed()
{
    // 首先尝试本地补全
    QString currentText = m_commandLine->text();
    if (!currentText.isEmpty() && !m_commands.isEmpty()) {
        // 检查是否正在进行补全
        if (m_completionIndex >= 0 && m_completionPrefix == currentText.left(m_completionPrefix.length())) {
            // 继续补全：循环到下一个匹配项
            performLocalCompletion();
            return;
        }

        // 开始新的补全
        m_completionPrefix = currentText;
        m_completionIndex = -1;
        performLocalCompletion();

        // 如果找到了匹配项，就不发送Tab到设备
        if (m_completionIndex >= 0) {
            return;
        }
    }

    // 没有本地匹配或命令列表为空，发送Tab到设备
    if (m_connected) {
        emit sendDataRequested(QByteArray(1, '\t'));
    }
}

void TerminalModeWidget::performLocalCompletion()
{
    if (m_completionPrefix.isEmpty() || m_commands.isEmpty()) {
        return;
    }

    // 收集所有匹配的命令
    QStringList matches;
    for (const QString& cmd : m_commands) {
        if (cmd.startsWith(m_completionPrefix, Qt::CaseInsensitive)) {
            matches.append(cmd);
        }
    }

    if (matches.isEmpty()) {
        m_completionIndex = -1;
        return;
    }

    // 移动到下一个匹配项
    m_completionIndex++;
    if (m_completionIndex >= matches.size()) {
        m_completionIndex = 0;
    }

    // 设置命令行文本
    m_commandLine->setText(matches[m_completionIndex]);
}

void TerminalModeWidget::onEditCommands()
{
    // 将当前命令列表转为文本
    QString currentCommands = m_commands.join("\n");

    bool ok;
    QString newCommands = QInputDialog::getMultiLineText(
        this,
        tr("编辑命令列表"),
        tr("输入命令（每行一个），用于Tab补全:"),
        currentCommands,
        &ok
    );

    if (ok) {
        // 解析新的命令列表
        m_commands.clear();
        QStringList lines = newCommands.split('\n', QString::SkipEmptyParts);
        for (const QString& line : lines) {
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                m_commands.append(trimmed);
            }
        }

        // 保存到配置
        saveCommands();

        // 重置补全状态
        m_completionIndex = -1;
        m_completionPrefix.clear();

        QMessageBox::information(this, tr("命令列表"),
            tr("已保存 %1 个命令").arg(m_commands.size()));
    }
}

void TerminalModeWidget::loadCommands()
{
    QSettings settings;
    settings.beginGroup("TerminalMode");
    int size = settings.beginReadArray("Commands");
    m_commands.clear();
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString cmd = settings.value("command").toString();
        if (!cmd.isEmpty()) {
            m_commands.append(cmd);
        }
    }
    settings.endArray();
    settings.endGroup();

    // 如果没有命令，添加一些常用默认命令
    if (m_commands.isEmpty()) {
        m_commands << "help" << "ls" << "cd" << "pwd" << "cat" << "echo"
                   << "reboot" << "reset" << "version" << "status"
                   << "clear" << "exit" << "quit";
    }
}

void TerminalModeWidget::saveCommands()
{
    QSettings settings;
    settings.beginGroup("TerminalMode");
    settings.beginWriteArray("Commands");
    for (int i = 0; i < m_commands.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("command", m_commands[i]);
    }
    settings.endArray();
    settings.endGroup();
}

void TerminalModeWidget::sendCommand(const QString& command)
{
    if (!m_connected) {
        emit statusMessage(tr("未连接"));
        return;
    }

    // 在终端显示区显示发送的命令（带换行）
    QString displayText = command + "\n";
    m_ansiParser->process(displayText.toUtf8());

    QByteArray data = command.toUtf8() + getLineEnding().toUtf8();
    emit sendDataRequested(data);
}

QString TerminalModeWidget::getLineEnding() const
{
    if (m_newlineMode == "CR") {
        return "\r";
    } else if (m_newlineMode == "CRLF") {
        return "\r\n";
    } else {
        return "\n";
    }
}

void TerminalModeWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    IModeWidget::changeEvent(event);
}

void TerminalModeWidget::retranslateUi()
{
    if (m_sendBtn) m_sendBtn->setText(tr("发送"));
    if (m_newlineLabel) m_newlineLabel->setText(tr("换行:"));
    if (m_themeLabel) m_themeLabel->setText(tr("主题:"));
    if (m_fontSizeLabel) m_fontSizeLabel->setText(tr("字号:"));
}

} // namespace ComAssistant
