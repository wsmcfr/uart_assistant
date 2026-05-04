/**
 * @file TerminalBuffer.h
 * @brief VT100终端缓冲区 - 支持完整的光标控制和屏幕操作
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef TERMINALBUFFER_H
#define TERMINALBUFFER_H

#include <QVector>
#include <QColor>
#include <QString>
#include <QObject>

namespace ComAssistant {

/**
 * @brief 字符属性（颜色、样式）
 */
struct CharAttribute {
    QColor foreground = QColor(0xAA, 0xAA, 0xAA);  // 默认浅灰色
    QColor background = QColor(0x00, 0x00, 0x00);  // 默认黑色
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool blink = false;
    bool reverse = false;
    bool hidden = false;
    bool strikethrough = false;

    bool operator==(const CharAttribute& other) const {
        return foreground == other.foreground &&
               background == other.background &&
               bold == other.bold &&
               dim == other.dim &&
               italic == other.italic &&
               underline == other.underline &&
               blink == other.blink &&
               reverse == other.reverse &&
               hidden == other.hidden &&
               strikethrough == other.strikethrough;
    }
};

/**
 * @brief 单个字符单元格
 */
struct TerminalCell {
    QChar character = ' ';
    CharAttribute attr;
};

/**
 * @brief 终端缓冲区 - 2D字符矩阵
 */
class TerminalBuffer : public QObject {
    Q_OBJECT

public:
    explicit TerminalBuffer(int cols = 80, int rows = 24, QObject* parent = nullptr);

    // 尺寸
    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    void resize(int cols, int rows);

    // 光标位置
    int cursorRow() const { return m_cursorRow; }
    int cursorCol() const { return m_cursorCol; }
    void setCursorPosition(int row, int col);
    void moveCursor(int deltaRow, int deltaCol);
    void setCursorVisible(bool visible) { m_cursorVisible = visible; }
    bool isCursorVisible() const { return m_cursorVisible; }

    // 滚动区域
    void setScrollRegion(int top, int bottom);
    int scrollTop() const { return m_scrollTop; }
    int scrollBottom() const { return m_scrollBottom; }

    // 字符操作
    void putChar(QChar ch);
    void putString(const QString& str);
    TerminalCell& cellAt(int row, int col);
    const TerminalCell& cellAt(int row, int col) const;

    // 当前属性
    void setCurrentAttribute(const CharAttribute& attr) { m_currentAttr = attr; }
    CharAttribute currentAttribute() const { return m_currentAttr; }
    void resetAttribute();

    // 屏幕操作
    void clearScreen();
    void clearToEndOfScreen();
    void clearToStartOfScreen();
    void clearLine();
    void clearToEndOfLine();
    void clearToStartOfLine();

    // 滚动
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);

    // 插入/删除
    void insertLines(int count);
    void deleteLines(int count);
    void insertChars(int count);
    void deleteChars(int count);

    // 换行处理
    void newLine();
    void carriageReturn();
    void tab();
    void backspace();

    // 保存/恢复光标
    void saveCursor();
    void restoreCursor();

    // 批量更新（抑制中间信号，只在 endBatchUpdate 时触发一次）
    void beginBatchUpdate();
    void endBatchUpdate();
    bool isBatchUpdating() const { return m_batchUpdateCount > 0; }

    // 回滚缓冲区（历史）
    int historyLineCount() const { return m_history.size(); }
    const QVector<TerminalCell>& historyLine(int index) const;
    int maxHistoryLines() const { return m_maxHistoryLines; }
    void setMaxHistoryLines(int max) { m_maxHistoryLines = max; }

    // 获取整个屏幕内容（用于渲染）
    const QVector<QVector<TerminalCell>>& screen() const { return m_screen; }

signals:
    void screenUpdated();
    void cursorMoved(int row, int col);
    void bellRing();

private:
    void ensureCursorInBounds();
    void addLineToHistory(const QVector<TerminalCell>& line);
    void putCharInternal(QChar ch);  // 内部写入，不触发信号

    int m_cols;
    int m_rows;
    int m_cursorRow = 0;
    int m_cursorCol = 0;
    bool m_cursorVisible = true;

    // 滚动区域（0-based，包含边界）
    int m_scrollTop = 0;
    int m_scrollBottom;

    // 保存的光标位置
    int m_savedCursorRow = 0;
    int m_savedCursorCol = 0;
    CharAttribute m_savedAttr;

    // 当前字符属性
    CharAttribute m_currentAttr;

    // 屏幕缓冲区
    QVector<QVector<TerminalCell>> m_screen;

    // 批量更新计数器
    int m_batchUpdateCount = 0;
    bool m_screenDirty = false;  // 批量更新期间是否有脏数据

    // 回滚历史
    QVector<QVector<TerminalCell>> m_history;
    int m_maxHistoryLines = 10000;
};

} // namespace ComAssistant

#endif // TERMINALBUFFER_H
