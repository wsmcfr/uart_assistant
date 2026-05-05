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
#include <QSizePolicy>

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
    /*
     * Qt 默认会把 Tab/Shift+Tab 当作焦点切换键，导致终端面板失焦、
     * 光标消失。本终端需要把 Tab 作为补全/发送按键处理，所以禁用
     * QWidget 的 Tab 焦点导航，让 keyPressEvent 能稳定收到 Tab。
     */
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

bool VT100Display::event(QEvent* event)
{
    /*
     * QWidget 会在 keyPressEvent 之前拦截 Tab/Backtab 做焦点切换。
     * 终端里 Tab 是有效输入键，必须在 event 层转交给 keyPressEvent。
     */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            keyPressEvent(keyEvent);
            return true;
        }
    }
    return QWidget::event(event);
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
            /*
             * 串口终端的屏幕只展示用户已经看见的本地输入和设备返回数据。
             * Enter 后保留当前输入行，但不再通过 appendSentData 额外回显
             * 一遍；如果设备没有回包，只停在下一行开头等待后续输入/回包。
             */
            // 发送数据（使用设置的换行符）
            QByteArray data = m_inputBuffer.toUtf8() + m_lineEnding.toUtf8();
            finishInputLineAfterSubmit();
            emit keyPressed(data);
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
        // Tab 先交给 TerminalModeWidget 做本地命令补全；没有匹配时才会发送到设备。
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

void VT100Display::replaceInputBuffer(const QString& text)
{
    /*
     * 本地补全需要把“当前正在输入的这一行”替换掉，而不是在末尾追加。
     * 这里先退回到本行起点并清到行尾，再重新绘制补全文本，避免旧字符残留。
     */
    if (m_inputBuffer == text) {
        return;
    }

    if (m_localEcho) {
        const int row = m_buffer->cursorRow();
        const int col = qMax(0, m_buffer->cursorCol() - m_inputBuffer.size());
        m_buffer->setCursorPosition(row, col);
        m_buffer->clearToEndOfLine();
        for (const QChar& ch : text) {
            m_buffer->putChar(ch);
        }
        update();
    }

    m_inputBuffer = text;
}

void VT100Display::showCompletionCandidates(const QStringList& candidates)
{
    /*
     * 多候选补全模拟 Linux 终端：保留用户已经输入的前缀，换到下一行打印
     * 候选，再开启一条新的输入行并恢复前缀。这里输出的是本地 UI 提示，
     * 不会通过 sendDataRequested 发给设备。
     */
    if (candidates.isEmpty()) {
        return;
    }

    const QString prefix = m_inputBuffer;
    m_buffer->carriageReturn();
    m_buffer->newLine();
    m_buffer->putString(candidates.join(QStringLiteral("  ")));
    m_buffer->carriageReturn();
    m_buffer->newLine();

    if (m_localEcho) {
        for (const QChar& ch : prefix) {
            m_buffer->putChar(ch);
        }
    }

    m_inputBuffer = prefix;
    m_cursorVisible = true;
    update();
}

void VT100Display::finishInputLineAfterSubmit()
{
    /*
     * 提交时保留用户已经输入并看到的内容，避免 Enter 看起来像“清屏”。
     * 内部输入缓冲必须清空，否则下一次输入/Tab 补全会错误地沿用旧命令。
     * 发送成功后的 appendSentData 不会写屏幕，因此不会出现本地内容重复显示。
     */
    m_buffer->carriageReturn();
    m_buffer->newLine();
    m_inputBuffer.clear();
    m_cursorVisible = true;
    update();
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
    /*
     * 终端模式的输入已经合并到黑色面板内部。默认开启本地回显，
     * 用户键入字符时才能像普通终端一样立即在光标位置看到输入内容。
     */
    m_display->setLocalEcho(true);
    if (m_echoAction) {
        /*
         * 工具栏创建时本地回显还未开启，这里把可见勾选状态同步到真实
         * 终端输入状态，避免用户看到“未勾选但实际可输入可见”的错觉。
         */
        m_echoAction->setChecked(m_display->localEcho());
    }

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

    // VT100终端显示区：输入、回显、Enter 发送都在同一个面板内完成。
    m_display = new VT100Display(m_termBuffer);
    mainLayout->addWidget(m_display, 1);

    // 连接显示组件的按键信号
    connect(m_display, &VT100Display::keyPressed, this, &TerminalModeWidget::onKeyPressed);
}

void TerminalModeWidget::setupToolBar()
{
    m_toolBar = new QToolBar;
    m_toolBar->setObjectName("terminalToolBar");
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // 清屏
    m_clearScreenAction = m_toolBar->addAction(tr("清屏"));
    connect(m_clearScreenAction, &QAction::triggered, this, &TerminalModeWidget::onClearScreen);

    m_toolBar->addSeparator();

    // 换行模式：放在工具栏前段，保证中英文界面都能直接看到关键收发设置。
    m_newlineLabel = new QLabel(tr("换行:"));
    m_newlineLabel->setObjectName("terminalToolbarLabel");
    m_toolBar->addWidget(m_newlineLabel);

    m_newlineCombo = new QComboBox;
    m_newlineCombo->addItem("LF (\\n)", "LF");
    m_newlineCombo->addItem("CR (\\r)", "CR");
    m_newlineCombo->addItem("CRLF (\\r\\n)", "CRLF");
    m_newlineCombo->setObjectName("terminalToolbarCombo");
    m_newlineCombo->setMinimumWidth(120);
    m_newlineCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_newlineCombo, &QComboBox::currentTextChanged, [this]() {
        m_newlineMode = m_newlineCombo->currentData().toString();
        // 同步到VT100Display
        m_display->setLineEnding(getLineEnding());
    });
    m_toolBar->addWidget(m_newlineCombo);

    m_toolBar->addSeparator();

    // 主题选择：用户截图中此控件被右侧裁剪，所以提升到快捷键之前。
    m_themeLabel = new QLabel(tr("主题:"));
    m_themeLabel->setObjectName("terminalToolbarLabel");
    m_toolBar->addWidget(m_themeLabel);

    m_themeCombo = new QComboBox;
    for (const auto& theme : s_themes) {
        m_themeCombo->addItem(theme.name);
    }
    m_themeCombo->setObjectName("terminalToolbarCombo");
    m_themeCombo->setMinimumWidth(150);
    m_themeCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TerminalModeWidget::onThemeChanged);
    m_toolBar->addWidget(m_themeCombo);

    // 字体大小
    m_fontSizeLabel = new QLabel(tr("字号:"));
    m_fontSizeLabel->setObjectName("terminalToolbarLabel");
    m_toolBar->addWidget(m_fontSizeLabel);

    m_fontSizeSpinBox = new QSpinBox;
    m_fontSizeSpinBox->setRange(6, 72);
    m_fontSizeSpinBox->setValue(12);
    m_fontSizeSpinBox->setObjectName("terminalToolbarSpinBox");
    m_fontSizeSpinBox->setMinimumWidth(64);
    connect(m_fontSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TerminalModeWidget::onFontSizeChanged);
    m_toolBar->addWidget(m_fontSizeSpinBox);

    m_toolBar->addSeparator();

    // 本地回显
    m_echoAction = m_toolBar->addAction(tr("本地回显"));
    m_echoAction->setCheckable(true);
    m_echoAction->setChecked(m_display->localEcho());
    connect(m_echoAction, &QAction::toggled, this, &TerminalModeWidget::onToggleLocalEcho);

    // 时间戳
    m_timestampAction = m_toolBar->addAction(tr("时间戳"));
    m_timestampAction->setCheckable(true);
    connect(m_timestampAction, &QAction::toggled, this, &TerminalModeWidget::onToggleTimestamp);

    // 日志
    m_logAction = m_toolBar->addAction(tr("记录日志"));
    m_logAction->setCheckable(true);
    connect(m_logAction, &QAction::toggled, this, &TerminalModeWidget::onToggleLog);

    m_toolBar->addSeparator();

    // 快捷键按钮
    m_ctrlCAction = m_toolBar->addAction("Ctrl+C");
    m_ctrlCAction->setToolTip(tr("发送中断信号"));
    connect(m_ctrlCAction, &QAction::triggered, this, &TerminalModeWidget::onSendCtrlC);

    m_ctrlDAction = m_toolBar->addAction("Ctrl+D");
    m_ctrlDAction->setToolTip(tr("发送 EOF"));
    connect(m_ctrlDAction, &QAction::triggered, this, &TerminalModeWidget::onSendCtrlD);

    m_ctrlZAction = m_toolBar->addAction("Ctrl+Z");
    m_ctrlZAction->setToolTip(tr("发送挂起信号"));
    connect(m_ctrlZAction, &QAction::triggered, this, &TerminalModeWidget::onSendCtrlZ);

    m_toolBar->addSeparator();

    // 编辑命令列表
    m_editCmdsAction = m_toolBar->addAction(tr("编辑命令"));
    m_editCmdsAction->setToolTip(tr("编辑本地命令列表（用于Tab补全）"));
    connect(m_editCmdsAction, &QAction::triggered, this, &TerminalModeWidget::onEditCommands);
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
    /*
     * 终端模式不能在“发送成功”回调里把本地发送的数据写回屏幕。
     * 主窗口会在底层 write 成功后调用 appendSentData；如果这里再交给
     * ANSI 解析器处理，就会出现用户发送 help 后屏幕立即显示 help 的
     * 假回包。真实终端界面保留用户在面板内已经输入的命令，并只额外展示
     * 设备返回的数据；没有回包时不会凭空新增内容。
     */
    Q_UNUSED(data);
}

void TerminalModeWidget::setConnected(bool connected)
{
    IModeWidget::setConnected(connected);
    if (m_display) {
        m_display->setEnabled(connected);
        if (connected) {
            m_display->setFocus();
        }
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
    if (m_echoAction && m_echoAction->isChecked() != enabled) {
        m_echoAction->setChecked(enabled);
    }
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
    /*
     * Tab 在终端面板中优先作为本地命令补全使用。只有没有匹配项时，
     * performLocalCompletion 会返回 false，此时再把 Tab 发给设备。
     * 这是纯本地编辑行为，即使当前未连接设备也应当可用，避免用户以为
     * “编辑命令”保存后没有生效。
     */
    if (data == QByteArray(1, '\t')) {
        if (performLocalCompletion()) {
            return;
        }
    }

    if (data.endsWith(getLineEnding().toUtf8())) {
        /*
         * Enter 提交后下一次输入已经是新的命令行。补全轮转状态如果继续保留，
         * 第二次输入同一前缀再按 Tab 会从上一次匹配项之后开始，表现为 ch
         * 不再补成 chmod，而是跳到 chown 等后续命令。
         */
        resetCompletionState();
    }

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

bool TerminalModeWidget::performLocalCompletion()
{
    const QString currentText = m_display ? m_display->inputBuffer() : QString();
    if (currentText.isEmpty() || m_commands.isEmpty()) {
        return false;
    }

    const QStringList matches = matchingCommands(currentText);

    if (matches.isEmpty()) {
        return false;
    }

    if (matches.size() == 1) {
        /*
         * 单一候选才直接替换输入行，保持普通终端补全的高效率。
         */
        m_display->replaceInputBuffer(matches.first());
        return true;
    }

    /*
     * 多候选时只列出匹配项，不擅自替换为第一个命令。用户继续输入更多
     * 字符后再按 Tab，会以新的前缀重新过滤候选。
     */
    m_display->showCompletionCandidates(matches);
    return true;
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

QStringList TerminalModeWidget::matchingCommands(const QString& prefix) const
{
    /*
     * 按命令列表原始顺序收集候选，避免 UI 提示顺序和“编辑命令”中的顺序不一致。
     */
    QStringList matches;
    if (prefix.isEmpty()) {
        return matches;
    }

    for (const QString& cmd : m_commands) {
        if (cmd.startsWith(prefix, Qt::CaseInsensitive)) {
            matches.append(cmd);
        }
    }
    return matches;
}

void TerminalModeWidget::resetCompletionState()
{
    /*
     * 预留状态重置入口。当前 Linux 风格补全每次都根据实时输入前缀重新匹配，
     * 不再维护跨 Tab 的轮转索引；保留该函数便于 Enter 等流程表达语义。
     */
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
    if (m_newlineLabel) m_newlineLabel->setText(tr("换行:"));
    if (m_themeLabel) m_themeLabel->setText(tr("主题:"));
    if (m_fontSizeLabel) m_fontSizeLabel->setText(tr("字号:"));

    // 工具栏按钮翻译
    if (m_clearScreenAction) m_clearScreenAction->setText(tr("清屏"));
    if (m_echoAction) m_echoAction->setText(tr("本地回显"));
    if (m_timestampAction) m_timestampAction->setText(tr("时间戳"));
    if (m_logAction) m_logAction->setText(tr("记录日志"));
    if (m_editCmdsAction) {
        m_editCmdsAction->setText(tr("编辑命令"));
        m_editCmdsAction->setToolTip(tr("编辑本地命令列表（用于Tab补全）"));
    }
    if (m_ctrlCAction) m_ctrlCAction->setToolTip(tr("发送中断信号"));
    if (m_ctrlDAction) m_ctrlDAction->setToolTip(tr("发送 EOF"));
    if (m_ctrlZAction) m_ctrlZAction->setToolTip(tr("发送挂起信号"));
}

} // namespace ComAssistant
