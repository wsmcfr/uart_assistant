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

signals:
    void keyPressed(const QByteArray& data);
    void sizeChanged(int cols, int rows);

protected:
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
 * @brief 命令输入框（带历史记录）
 */
class CommandLineEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit CommandLineEdit(QWidget* parent = nullptr);

    void addToHistory(const QString& command);
    void clearHistory();
    QStringList history() const { return m_history; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

signals:
    void commandSubmitted(const QString& command);
    void tabPressed();  // Tab键信号

private:
    void retranslateUi();

    QStringList m_history;
    int m_historyIndex = -1;
    QString m_currentInput;
    static const int MAX_HISTORY = 100;
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
    void onCommandSubmitted(const QString& command);
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
    void onTabPressed();
    void onEditCommands();
    void performLocalCompletion();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupToolBar();
    void retranslateUi();
    void sendCommand(const QString& command);
    QString getLineEnding() const;
    void loadCommands();
    void saveCommands();

    // 核心组件
    TerminalBuffer* m_termBuffer;
    AnsiParser* m_ansiParser;
    VT100Display* m_display;

    CommandLineEdit* m_commandLine;
    QToolBar* m_toolBar;

    // UI元素
    QPushButton* m_sendBtn = nullptr;
    QLabel* m_newlineLabel = nullptr;
    QComboBox* m_newlineCombo = nullptr;
    QLabel* m_themeLabel = nullptr;
    QComboBox* m_themeCombo = nullptr;
    QLabel* m_fontSizeLabel = nullptr;
    QSpinBox* m_fontSizeSpinBox = nullptr;
    QAction* m_echoAction = nullptr;

    QString m_newlineMode = "LF";
    bool m_timestampEnabled = false;
    bool m_loggingEnabled = false;
    QFile* m_logFile = nullptr;

    // 本地命令补全
    QStringList m_commands;  // 命令列表
    int m_completionIndex = -1;  // 当前补全索引
    QString m_completionPrefix;  // 补全前缀
};

} // namespace ComAssistant

#endif // TERMINALMODEWIDGET_H
