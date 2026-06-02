/**
 * @file ScriptEditorDialog.h
 * @brief 脚本编辑器对话框
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_SCRIPTEDITORDIALOG_H
#define COMASSISTANT_SCRIPTEDITORDIALOG_H

#include <QDialog>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>

namespace ComAssistant {

class LuaSyntaxHighlighter;

/**
 * @brief 脚本编辑器对话框
 */
class ScriptEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit ScriptEditorDialog(QWidget* parent = nullptr);
    ~ScriptEditorDialog() override;

    QString currentScript() const;
    void setScript(const QString& script);

signals:
    void scriptOutput(const QString& text);
    void sendData(const QByteArray& data);

private slots:
    void onNewScript();
    void onOpenScript();
    void onSaveScript();
    void onSaveAsScript();
    void onRunScript();
    void onStopScript();
    void onScriptSelected(QListWidgetItem* item);
    void onScriptDoubleClicked(QListWidgetItem* item);
    void updateLineNumbers();

private:
    void setupUi();
    void loadScriptList();
    void saveScriptList();
    void appendOutput(const QString& text, const QColor& color = QColor());
    void clearOutput();
    QString scriptsDirectory() const;

    /**
     * @brief 通过 LuaSandbox 执行脚本文本。
     * @param script 用户在编辑器中编写的 Lua 脚本文本。
     * @return 脚本是否执行成功；失败时错误已经写入输出区域。
     *
     * 脚本编辑器不直接持有串口对象，而是把 serial.send/serial.sendHex
     * 转换成 sendData(QByteArray) 信号，继续复用主窗口现有发送队列。
     */
    bool executeSandboxScript(const QString& script);

private:
    // UI组件
    QSplitter* m_mainSplitter = nullptr;
    QListWidget* m_scriptList = nullptr;
    QPlainTextEdit* m_codeEditor = nullptr;
    QTextEdit* m_outputArea = nullptr;
    QPlainTextEdit* m_lineNumberArea = nullptr;

    // 工具栏按钮
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_openBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_runBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;

    // 语法高亮
    LuaSyntaxHighlighter* m_highlighter = nullptr;

    // 状态
    QString m_currentFilePath;
    bool m_isRunning = false;
    bool m_modified = false;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SCRIPTEDITORDIALOG_H
