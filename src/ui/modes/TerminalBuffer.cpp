/**
 * @file TerminalBuffer.cpp
 * @brief VT100终端缓冲区实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "TerminalBuffer.h"
#include <algorithm>

namespace ComAssistant {

TerminalBuffer::TerminalBuffer(int cols, int rows, QObject* parent)
    : QObject(parent)
    , m_cols(cols)
    , m_rows(rows)
    , m_scrollBottom(rows - 1)
{
    // 初始化屏幕缓冲区
    m_screen.resize(rows);
    for (int r = 0; r < rows; ++r) {
        m_screen[r].resize(cols);
    }
}

void TerminalBuffer::resize(int cols, int rows)
{
    if (cols == m_cols && rows == m_rows) return;

    // 创建新的屏幕缓冲区
    QVector<QVector<TerminalCell>> newScreen(rows);
    for (int r = 0; r < rows; ++r) {
        newScreen[r].resize(cols);
        // 复制旧内容
        if (r < m_rows) {
            int copyLen = qMin(cols, m_cols);
            for (int c = 0; c < copyLen; ++c) {
                newScreen[r][c] = m_screen[r][c];
            }
        }
    }

    m_screen = newScreen;
    m_cols = cols;
    m_rows = rows;
    m_scrollBottom = rows - 1;

    // 确保光标在边界内
    ensureCursorInBounds();

    emit screenUpdated();
}

void TerminalBuffer::setCursorPosition(int row, int col)
{
    m_cursorRow = row;
    m_cursorCol = col;
    ensureCursorInBounds();
    emit cursorMoved(m_cursorRow, m_cursorCol);
}

void TerminalBuffer::moveCursor(int deltaRow, int deltaCol)
{
    setCursorPosition(m_cursorRow + deltaRow, m_cursorCol + deltaCol);
}

void TerminalBuffer::setScrollRegion(int top, int bottom)
{
    m_scrollTop = qBound(0, top, m_rows - 1);
    m_scrollBottom = qBound(m_scrollTop, bottom, m_rows - 1);
}

void TerminalBuffer::putChar(QChar ch)
{
    if (m_cursorCol >= m_cols) {
        // 自动换行
        carriageReturn();
        newLine();
    }

    m_screen[m_cursorRow][m_cursorCol].character = ch;
    m_screen[m_cursorRow][m_cursorCol].attr = m_currentAttr;
    m_cursorCol++;

    emit screenUpdated();
}

void TerminalBuffer::putString(const QString& str)
{
    for (const QChar& ch : str) {
        putChar(ch);
    }
}

TerminalCell& TerminalBuffer::cellAt(int row, int col)
{
    static TerminalCell dummy;
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
        return dummy;
    }
    return m_screen[row][col];
}

const TerminalCell& TerminalBuffer::cellAt(int row, int col) const
{
    static TerminalCell dummy;
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
        return dummy;
    }
    return m_screen[row][col];
}

void TerminalBuffer::resetAttribute()
{
    m_currentAttr = CharAttribute();
}

void TerminalBuffer::clearScreen()
{
    for (int r = 0; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            m_screen[r][c].character = ' ';
            m_screen[r][c].attr = m_currentAttr;
        }
    }
    emit screenUpdated();
}

void TerminalBuffer::clearToEndOfScreen()
{
    // 清除光标位置到行末
    clearToEndOfLine();
    // 清除后续所有行
    for (int r = m_cursorRow + 1; r < m_rows; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            m_screen[r][c].character = ' ';
            m_screen[r][c].attr = m_currentAttr;
        }
    }
    emit screenUpdated();
}

void TerminalBuffer::clearToStartOfScreen()
{
    // 清除光标位置到行首
    clearToStartOfLine();
    // 清除前面所有行
    for (int r = 0; r < m_cursorRow; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            m_screen[r][c].character = ' ';
            m_screen[r][c].attr = m_currentAttr;
        }
    }
    emit screenUpdated();
}

void TerminalBuffer::clearLine()
{
    for (int c = 0; c < m_cols; ++c) {
        m_screen[m_cursorRow][c].character = ' ';
        m_screen[m_cursorRow][c].attr = m_currentAttr;
    }
    emit screenUpdated();
}

void TerminalBuffer::clearToEndOfLine()
{
    for (int c = m_cursorCol; c < m_cols; ++c) {
        m_screen[m_cursorRow][c].character = ' ';
        m_screen[m_cursorRow][c].attr = m_currentAttr;
    }
    emit screenUpdated();
}

void TerminalBuffer::clearToStartOfLine()
{
    for (int c = 0; c <= m_cursorCol; ++c) {
        m_screen[m_cursorRow][c].character = ' ';
        m_screen[m_cursorRow][c].attr = m_currentAttr;
    }
    emit screenUpdated();
}

void TerminalBuffer::scrollUp(int lines)
{
    if (lines <= 0) return;

    // 将顶行保存到历史
    for (int i = 0; i < lines && m_scrollTop + i <= m_scrollBottom; ++i) {
        if (m_scrollTop == 0) {
            addLineToHistory(m_screen[0]);
        }
    }

    // 在滚动区域内滚动
    for (int r = m_scrollTop; r <= m_scrollBottom - lines; ++r) {
        m_screen[r] = m_screen[r + lines];
    }

    // 清空底部新行
    for (int r = m_scrollBottom - lines + 1; r <= m_scrollBottom; ++r) {
        if (r >= 0 && r < m_rows) {
            for (int c = 0; c < m_cols; ++c) {
                m_screen[r][c].character = ' ';
                m_screen[r][c].attr = m_currentAttr;
            }
        }
    }

    emit screenUpdated();
}

void TerminalBuffer::scrollDown(int lines)
{
    if (lines <= 0) return;

    // 在滚动区域内滚动
    for (int r = m_scrollBottom; r >= m_scrollTop + lines; --r) {
        m_screen[r] = m_screen[r - lines];
    }

    // 清空顶部新行
    for (int r = m_scrollTop; r < m_scrollTop + lines; ++r) {
        if (r >= 0 && r < m_rows) {
            for (int c = 0; c < m_cols; ++c) {
                m_screen[r][c].character = ' ';
                m_screen[r][c].attr = m_currentAttr;
            }
        }
    }

    emit screenUpdated();
}

void TerminalBuffer::insertLines(int count)
{
    if (m_cursorRow < m_scrollTop || m_cursorRow > m_scrollBottom) return;

    // 从当前行开始，向下移动内容
    for (int r = m_scrollBottom; r >= m_cursorRow + count; --r) {
        m_screen[r] = m_screen[r - count];
    }

    // 清空插入的新行
    for (int r = m_cursorRow; r < m_cursorRow + count && r <= m_scrollBottom; ++r) {
        for (int c = 0; c < m_cols; ++c) {
            m_screen[r][c].character = ' ';
            m_screen[r][c].attr = m_currentAttr;
        }
    }

    emit screenUpdated();
}

void TerminalBuffer::deleteLines(int count)
{
    if (m_cursorRow < m_scrollTop || m_cursorRow > m_scrollBottom) return;

    // 从当前行开始，向上移动内容
    for (int r = m_cursorRow; r <= m_scrollBottom - count; ++r) {
        m_screen[r] = m_screen[r + count];
    }

    // 清空底部腾出的行
    for (int r = m_scrollBottom - count + 1; r <= m_scrollBottom; ++r) {
        if (r >= 0 && r < m_rows) {
            for (int c = 0; c < m_cols; ++c) {
                m_screen[r][c].character = ' ';
                m_screen[r][c].attr = m_currentAttr;
            }
        }
    }

    emit screenUpdated();
}

void TerminalBuffer::insertChars(int count)
{
    // 向右移动字符
    for (int c = m_cols - 1; c >= m_cursorCol + count; --c) {
        m_screen[m_cursorRow][c] = m_screen[m_cursorRow][c - count];
    }

    // 清空插入位置
    for (int c = m_cursorCol; c < m_cursorCol + count && c < m_cols; ++c) {
        m_screen[m_cursorRow][c].character = ' ';
        m_screen[m_cursorRow][c].attr = m_currentAttr;
    }

    emit screenUpdated();
}

void TerminalBuffer::deleteChars(int count)
{
    // 向左移动字符
    for (int c = m_cursorCol; c < m_cols - count; ++c) {
        m_screen[m_cursorRow][c] = m_screen[m_cursorRow][c + count];
    }

    // 清空右侧腾出的位置
    for (int c = m_cols - count; c < m_cols; ++c) {
        if (c >= 0) {
            m_screen[m_cursorRow][c].character = ' ';
            m_screen[m_cursorRow][c].attr = m_currentAttr;
        }
    }

    emit screenUpdated();
}

void TerminalBuffer::newLine()
{
    m_cursorRow++;
    if (m_cursorRow > m_scrollBottom) {
        m_cursorRow = m_scrollBottom;
        scrollUp(1);
    }
    emit cursorMoved(m_cursorRow, m_cursorCol);
}

void TerminalBuffer::carriageReturn()
{
    m_cursorCol = 0;
    emit cursorMoved(m_cursorRow, m_cursorCol);
}

void TerminalBuffer::tab()
{
    // 移到下一个8的倍数位置
    int nextTab = ((m_cursorCol / 8) + 1) * 8;
    m_cursorCol = qMin(nextTab, m_cols - 1);
    emit cursorMoved(m_cursorRow, m_cursorCol);
}

void TerminalBuffer::backspace()
{
    if (m_cursorCol > 0) {
        m_cursorCol--;
        emit cursorMoved(m_cursorRow, m_cursorCol);
    }
}

void TerminalBuffer::saveCursor()
{
    m_savedCursorRow = m_cursorRow;
    m_savedCursorCol = m_cursorCol;
    m_savedAttr = m_currentAttr;
}

void TerminalBuffer::restoreCursor()
{
    m_cursorRow = m_savedCursorRow;
    m_cursorCol = m_savedCursorCol;
    m_currentAttr = m_savedAttr;
    ensureCursorInBounds();
    emit cursorMoved(m_cursorRow, m_cursorCol);
}

const QVector<TerminalCell>& TerminalBuffer::historyLine(int index) const
{
    static QVector<TerminalCell> empty;
    if (index < 0 || index >= m_history.size()) {
        return empty;
    }
    return m_history[index];
}

void TerminalBuffer::ensureCursorInBounds()
{
    m_cursorRow = qBound(0, m_cursorRow, m_rows - 1);
    m_cursorCol = qBound(0, m_cursorCol, m_cols - 1);
}

void TerminalBuffer::addLineToHistory(const QVector<TerminalCell>& line)
{
    m_history.append(line);
    while (m_history.size() > m_maxHistoryLines) {
        m_history.removeFirst();
    }
}

} // namespace ComAssistant
