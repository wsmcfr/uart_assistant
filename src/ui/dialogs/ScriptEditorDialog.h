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

#include <atomic>
#include <memory>

class QThread;

namespace ComAssistant {

struct LuaSandboxResult;
class LuaSyntaxHighlighter;
class ScriptExecutionWorker;

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
    /**
     * @brief 脚本执行状态。
     *
     * Idle 表示可运行新脚本；Running 表示后台脚本正在执行；
     * Cancelling 表示用户已请求停止，等待 Lua hook 返回取消结果。
     */
    enum class ScriptRunState
    {
        Idle,
        Running,
        Cancelling
    };

    void setupUi();
    void loadScriptList();
    void saveScriptList();
    void appendOutput(const QString& text, const QColor& color = QColor());
    void clearOutput();
    QString scriptsDirectory() const;

    /**
     * @brief 启动后台 worker 执行脚本。
     * @param script 用户在编辑器中编写的 Lua 脚本文本。
     *
     * 该函数只在 UI 线程创建 QThread 和 worker，并建立 queued signal 连接。
     * 真正的 LuaSandbox 执行发生在 worker 所在线程。
     */
    void startWorkerExecution(const QString& script);

    /**
     * @brief 请求后台脚本取消。
     *
     * 取消通过原子标记交给 Lua hook 轮询处理；这里不强杀线程，
     * 避免 Lua state 或 Qt 信号处于半释放状态。
     */
    void requestWorkerCancellation();

    /**
     * @brief 处理 worker 请求发送的数据。
     * @param data 脚本产生的原始发送字节。
     *
     * 该槽在 UI 线程执行，负责追加输出预览并继续发射 sendData 信号。
     */
    void handleWorkerSendRequested(const QByteArray& data);

    /**
     * @brief 处理 worker 返回的脚本执行结果。
     * @param result LuaSandbox 的结构化执行结果。
     *
     * 该槽在 UI 线程执行，负责渲染输出、显示错误或取消状态，并恢复按钮状态。
     */
    void handleWorkerFinished(const LuaSandboxResult& result);

    /**
     * @brief 清理当前 worker 线程对象。
     *
     * finished 信号回到 UI 线程后调用该函数，统一断开指针和状态，
     * 避免 worker 生命周期散落在多个槽函数中。
     */
    void cleanupWorkerThread();

    /**
     * @brief 设置脚本运行状态并同步按钮可用性。
     * @param state 目标运行状态。
     */
    void setRunState(ScriptRunState state);

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
    QThread* m_scriptThread = nullptr;                    ///< 当前脚本执行线程，空表示没有后台任务
    ScriptExecutionWorker* m_scriptWorker = nullptr;      ///< 当前脚本 worker，生命周期归线程清理流程管理
    std::shared_ptr<std::atomic_bool> m_cancelRequested;  ///< 停止按钮写入、Lua hook 读取的取消标记
    ScriptRunState m_runState = ScriptRunState::Idle;     ///< 当前脚本运行状态，用于按钮和重复运行防护
};

} // namespace ComAssistant

#endif // COMASSISTANT_SCRIPTEDITORDIALOG_H
