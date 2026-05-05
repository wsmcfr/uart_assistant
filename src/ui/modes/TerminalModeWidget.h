/**
 * @file TerminalModeWidget.h
 * @brief 终端模式组件 - 完整VT100终端仿真
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef TERMINALMODEWIDGET_H
#define TERMINALMODEWIDGET_H

#include "IModeWidget.h"
#include "TerminalBuffer.h"
#include "AnsiParser.h"

#include <QWidget>
#include <QLineEdit>
#include <QStringList>
#include <QColor>
#include <QToolBar>
#include <QTimer>
#include <QFile>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QFont>
#include <QScrollBar>

namespace ComAssistant {

/**
 * @brief 终端配色主题
 */
struct TerminalTheme {
    QString name;
    QColor foreground;
    QColor background;
    QColor cursorColor;
    QColor selectionBg;
    QColor selectionFg;
    // 16色调色板
    QColor palette[16];
};

/**
 * @brief VT100终端显示组件
 */
class VT100Display : public QWidget {
    Q_OBJECT

public:
    explicit VT100Display(TerminalBuffer* buffer, QWidget* parent = nullptr);

    // 字体设置
    void setTerminalFont(const QFont& font);
    QFont terminalFont() const { return m_font; }
    void setFontSize(int size);
    int fontSize() const { return m_font.pointSize(); }

    // 主题设置
    void setTheme(const TerminalTheme& theme);
    TerminalTheme theme() const { return m_theme; }

    // 光标设置
    void setCursorBlinking(bool enabled);
    bool isCursorBlinking() const { return m_cursorBlinking; }
    void setCursorStyle(int style);  // 0: 块, 1: 下划线, 2: 竖线

    // 滚动
    void scrollToBottom();
    void scrollBy(int lines);

    // 获取选中文本
    QString selectedText() const;
    void clearSelection();

    // 本地回显
    void setLocalEcho(bool enabled) { m_localEcho = enabled; }
    bool localEcho() const { return m_localEcho; }

    // 换行符设置
    void setLineEnding(const QString& ending) { m_lineEnding = ending; }
    QString lineEnding() const { return m_lineEnding; }

    /**
     * @brief 获取当前正在面板内输入的命令行文本。
     * @return 尚未按 Enter 发送的输入缓冲内容。
     */
    QString inputBuffer() const { return m_inputBuffer; }

    /**
     * @brief 用补全文本替换当前正在输入的命令行。
     * @param text 新的命令行内容，通常来自编辑命令列表。
     */
    void replaceInputBuffer(const QString& text);

    /**
     * @brief 在当前输入行下方列出 Tab 补全候选项，并恢复输入前缀。
     * @param candidates 按命令列表顺序匹配到的候选命令。
     *
     * 多候选补全采用 Linux 终端式交互：不擅自替换用户当前输入，而是在
     * 下一行打印候选列表，再回到新的输入行显示原前缀，方便用户继续输入。
     */
    void showCompletionCandidates(const QStringList& candidates);

    /**
     * @brief 结束面板内当前正在编辑的本地输入行。
     *
     * 按 Enter 发送时调用。用户已经在终端面板里看到的输入内容需要保留，
     * 但发送成功回调不能再额外回显一遍；这里仅清空内部输入缓冲，并把
     * 光标移动到下一行开头，等待设备真实返回内容。
     */
    void finishInputLineAfterSubmit();

signals:
    void keyPressed(const QByteArray& data);
    void sizeChanged(int cols, int rows);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    void onCursorBlink();
    void onScreenUpdated();

private:
    void calculateCellSize();
    void updateTerminalSize();
    QPoint cellAt(const QPoint& pos) const;
    QByteArray keyToBytes(QKeyEvent* event) const;

    TerminalBuffer* m_buffer;
    QFont m_font;
    int m_cellWidth = 0;
    int m_cellHeight = 0;
    int m_descent = 0;

    // 主题
    TerminalTheme m_theme;

    // 光标
    QTimer* m_cursorTimer;
    bool m_cursorVisible = true;
    bool m_cursorBlinking = true;
    int m_cursorStyle = 0;  // 0: 块

    // 滚动
    int m_scrollOffset = 0;  // 向上滚动的行数

    // 选择
    bool m_selecting = false;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;

    // 本地回显
    bool m_localEcho = false;

    // 行编辑模式输入缓冲
    QString m_inputBuffer;

    // 换行符
    QString m_lineEnding = "\r\n";
};

/**
 * @brief 终端模式组件
 */
class TerminalModeWidget : public IModeWidget {
    Q_OBJECT

public:
    explicit TerminalModeWidget(QWidget* parent = nullptr);
    ~TerminalModeWidget() override;

    // IModeWidget 接口实现
    QString modeName() const override { return tr("终端"); }
    QString modeIcon() const override { return "terminal"; }
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;
    bool exportToFile(const QString& fileName) override;
    void onActivated() override;
    void onDeactivated() override;
    QWidget* modeToolBar() override { return m_toolBar; }
    void setConnected(bool connected) override;

    // 终端设置
    void setLocalEcho(bool enabled);
    bool localEcho() const;
    void setNewlineMode(const QString& mode);
    QString newlineMode() const { return m_newlineMode; }

    // 主题
    static QVector<TerminalTheme> availableThemes();
    void setTheme(const QString& themeName);

    // 字体
    void setFontSize(int size);
    int fontSize() const;

private slots:
    void onClearScreen();
    void onToggleLocalEcho(bool checked);
    void onToggleTimestamp(bool checked);
    void onToggleLog(bool checked);
    void onSendCtrlC();
    void onSendCtrlD();
    void onSendCtrlZ();
    void onKeyPressed(const QByteArray& data);
    void onThemeChanged(int index);
    void onFontSizeChanged(int size);
    void onEditCommands();
    bool performLocalCompletion();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupToolBar();
    void retranslateUi();
    QString getLineEnding() const;
    void loadCommands();
    void saveCommands();
    QStringList matchingCommands(const QString& prefix) const;
    void resetCompletionState();

    // 核心组件
    TerminalBuffer* m_termBuffer;
    AnsiParser* m_ansiParser;
    VT100Display* m_display;

    QToolBar* m_toolBar;

    // UI元素
    QLabel* m_newlineLabel = nullptr;
    QComboBox* m_newlineCombo = nullptr;
    QLabel* m_themeLabel = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QLabel* m_fontSizeLabel = nullptr;
    QSpinBox* m_fontSizeSpinBox = nullptr;
    QAction* m_echoAction = nullptr;
    QAction* m_clearScreenAction = nullptr;
    QAction* m_timestampAction = nullptr;
    QAction* m_logAction = nullptr;
    QAction* m_editCmdsAction = nullptr;
    QAction* m_ctrlCAction = nullptr;
    QAction* m_ctrlDAction = nullptr;
    QAction* m_ctrlZAction = nullptr;

    QString m_newlineMode = "LF";
    bool m_timestampEnabled = false;
    bool m_loggingEnabled = false;
    QFile* m_logFile = nullptr;

    // 本地命令补全
    QStringList m_commands;  // 命令列表
};

} // namespace ComAssistant

#endif // TERMINALMODEWIDGET_H
