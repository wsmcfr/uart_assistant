/**
 * @file ScriptEditorDialog.cpp
 * @brief 脚本编辑器对话框实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "ScriptEditorDialog.h"
#include "ScriptExecutionWorker.h"
#include "../syntax/LuaSyntaxHighlighter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QScrollBar>
#include <QTextBlock>
#include <QThread>

#include <utility>

namespace ComAssistant {
namespace {

/**
 * @brief 生成发送数据的输出预览文本。
 * @param data 即将发送的原始字节。
 * @return 适合显示在脚本输出区域的简短文本。
 *
 * 对可打印 ASCII 文本优先显示简化后的内容，便于用户确认 AT 指令等常见脚本；
 * 对二进制数据只显示字节数，避免控制字符破坏输出区域的可读性。
 */
QString sentPayloadPreview(const QByteArray& data)
{
    bool printable = true;
    for (char byte : data) {
        const uchar value = static_cast<uchar>(byte);
        if (value == '\r' || value == '\n' || value == '\t') {
            continue;
        }
        if (value < 32 || value > 126) {
            printable = false;
            break;
        }
    }

    if (printable) {
        const QString text = QString::fromUtf8(data).simplified();
        if (!text.isEmpty()) {
            return text;
        }
    }

    return QStringLiteral("%1 bytes").arg(data.size());
}

} // namespace

ScriptEditorDialog::ScriptEditorDialog(QWidget* parent)
    : QDialog(parent)
{
    qRegisterMetaType<ComAssistant::LuaSandboxResult>("ComAssistant::LuaSandboxResult");

    setWindowTitle(tr("脚本编辑器"));
    setMinimumSize(900, 600);
    resize(1000, 700);

    setupUi();
    loadScriptList();

    // 示例脚本
    QString defaultScript = R"(-- 串口助手 Lua 沙箱示例
-- 可用 API:
--   print(...)            - 输出日志
--   serial.isOpen()       - 查询当前脚本发送通道是否可用
--   serial.send(data)     - 发送字符串或原始字节
--   serial.sendHex(hex)   - 发送十六进制文本
--   hexToBytes(hex)       - 十六进制转字节
--   bytesToHex(data)      - 字节转十六进制文本
--   crc16(data) / crc32(data)

print("脚本加载完成")

if serial.isOpen() then
    serial.send("AT\r\n")
    serial.sendHex("AA 55 01 02 03")
end

local request = hexToBytes("01 03 00 00 00 02")
print("CRC16:", crc16(request))
)";
    m_codeEditor->setPlainText(defaultScript);
}

ScriptEditorDialog::~ScriptEditorDialog()
{
    /*
     * 对话框关闭时不能直接销毁仍在运行的 QThread。这里先请求 Lua hook 取消，
     * 再等待一个短窗口；绝大多数 Lua 循环会立即退出。若极端脚本仍未结束，
     * 则解除线程父对象并让既有 finished 连接完成自清理，避免 UI 对象被跨线程访问。
     */
    if (m_scriptThread) {
        requestWorkerCancellation();
        m_scriptThread->quit();
        if (m_scriptThread->wait(1000)) {
            cleanupWorkerThread();
        } else {
            if (m_scriptWorker) {
                m_scriptWorker->disconnect(this);
            }
            m_scriptThread->disconnect(this);
            m_scriptThread->setParent(nullptr);
            connect(m_scriptThread, &QThread::finished,
                    m_scriptThread, &QObject::deleteLater);
        }
    }
}

void ScriptEditorDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 工具栏
    QHBoxLayout* toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(4);

    m_newBtn = new QPushButton(tr("新建"));
    m_openBtn = new QPushButton(tr("打开"));
    m_saveBtn = new QPushButton(tr("保存"));
    m_runBtn = new QPushButton(tr("运行"));
    m_stopBtn = new QPushButton(tr("停止"));

    m_newBtn->setFixedWidth(60);
    m_openBtn->setFixedWidth(60);
    m_saveBtn->setFixedWidth(60);
    m_runBtn->setFixedWidth(60);
    m_stopBtn->setFixedWidth(60);

    m_runBtn->setObjectName("runScriptBtn");
    m_stopBtn->setObjectName("stopScriptBtn");
    m_stopBtn->setEnabled(false);

    connect(m_newBtn, &QPushButton::clicked, this, &ScriptEditorDialog::onNewScript);
    connect(m_openBtn, &QPushButton::clicked, this, &ScriptEditorDialog::onOpenScript);
    connect(m_saveBtn, &QPushButton::clicked, this, &ScriptEditorDialog::onSaveScript);
    connect(m_runBtn, &QPushButton::clicked, this, &ScriptEditorDialog::onRunScript);
    connect(m_stopBtn, &QPushButton::clicked, this, &ScriptEditorDialog::onStopScript);

    toolbarLayout->addWidget(m_newBtn);
    toolbarLayout->addWidget(m_openBtn);
    toolbarLayout->addWidget(m_saveBtn);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_runBtn);
    toolbarLayout->addWidget(m_stopBtn);
    toolbarLayout->addStretch();

    QLabel* helpLabel = new QLabel(tr("提示: Ctrl+Enter 运行脚本"));
    helpLabel->setObjectName("scriptHelpLabel");
    toolbarLayout->addWidget(helpLabel);

    mainLayout->addLayout(toolbarLayout);

    // 主分割区域
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧：脚本列表
    QWidget* leftPanel = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* listLabel = new QLabel(tr("脚本列表"));
    listLabel->setObjectName("scriptSectionLabel");
    leftLayout->addWidget(listLabel);

    m_scriptList = new QListWidget;
    m_scriptList->setMaximumWidth(200);
    connect(m_scriptList, &QListWidget::itemClicked,
            this, &ScriptEditorDialog::onScriptSelected);
    connect(m_scriptList, &QListWidget::itemDoubleClicked,
            this, &ScriptEditorDialog::onScriptDoubleClicked);
    leftLayout->addWidget(m_scriptList);

    m_mainSplitter->addWidget(leftPanel);

    // 中间：代码编辑器
    QWidget* centerPanel = new QWidget;
    QVBoxLayout* centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* editorLabel = new QLabel(tr("代码编辑"));
    editorLabel->setObjectName("scriptSectionLabel");
    centerLayout->addWidget(editorLabel);

    m_codeEditor = new QPlainTextEdit;
    m_codeEditor->setObjectName("codeEditor");
    m_codeEditor->setFont(QFont("Consolas", 11));
    m_codeEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_codeEditor->setTabStopDistance(QFontMetricsF(m_codeEditor->font()).horizontalAdvance(' ') * 4);

    // 语法高亮
    m_highlighter = new LuaSyntaxHighlighter(m_codeEditor->document());

    centerLayout->addWidget(m_codeEditor);
    m_mainSplitter->addWidget(centerPanel);

    // 右侧：输出区域
    QWidget* rightPanel = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* outputHeader = new QHBoxLayout;
    QLabel* outputLabel = new QLabel(tr("输出"));
    outputLabel->setObjectName("scriptSectionLabel");
    outputHeader->addWidget(outputLabel);

    QPushButton* clearOutputBtn = new QPushButton(tr("清空"));
    clearOutputBtn->setFixedWidth(50);
    connect(clearOutputBtn, &QPushButton::clicked, this, &ScriptEditorDialog::clearOutput);
    outputHeader->addStretch();
    outputHeader->addWidget(clearOutputBtn);

    rightLayout->addLayout(outputHeader);

    m_outputArea = new QTextEdit;
    m_outputArea->setObjectName("scriptOutputArea");
    m_outputArea->setReadOnly(true);
    m_outputArea->setFont(QFont("Consolas", 10));
    rightLayout->addWidget(m_outputArea);

    m_mainSplitter->addWidget(rightPanel);

    // 设置分割比例
    m_mainSplitter->setStretchFactor(0, 1);  // 脚本列表
    m_mainSplitter->setStretchFactor(1, 3);  // 代码编辑器
    m_mainSplitter->setStretchFactor(2, 2);  // 输出区域

    mainLayout->addWidget(m_mainSplitter);
}

QString ScriptEditorDialog::currentScript() const
{
    return m_codeEditor->toPlainText();
}

void ScriptEditorDialog::setScript(const QString& script)
{
    m_codeEditor->setPlainText(script);
}

void ScriptEditorDialog::onNewScript()
{
    if (m_modified) {
        int ret = QMessageBox::question(this, tr("新建脚本"),
            tr("当前脚本已修改，是否保存？"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (ret == QMessageBox::Yes) {
            onSaveScript();
        } else if (ret == QMessageBox::Cancel) {
            return;
        }
    }

    m_codeEditor->clear();
    m_currentFilePath.clear();
    m_modified = false;
    setWindowTitle(tr("脚本编辑器 - 新建"));
}

void ScriptEditorDialog::onOpenScript()
{
    QString dir = scriptsDirectory();
    QString filePath = QFileDialog::getOpenFileName(this,
        tr("打开脚本"), dir, tr("Lua脚本 (*.lua);;所有文件 (*)"));

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        m_codeEditor->setPlainText(stream.readAll());
        file.close();

        m_currentFilePath = filePath;
        m_modified = false;
        setWindowTitle(tr("脚本编辑器 - %1").arg(QFileInfo(filePath).fileName()));
    } else {
        QMessageBox::warning(this, tr("错误"),
            tr("无法打开文件: %1").arg(file.errorString()));
    }
}

void ScriptEditorDialog::onSaveScript()
{
    if (m_currentFilePath.isEmpty()) {
        onSaveAsScript();
        return;
    }

    QFile file(m_currentFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << m_codeEditor->toPlainText();
        file.close();

        m_modified = false;
        appendOutput(tr("[系统] 脚本已保存: %1").arg(m_currentFilePath), QColor(100, 149, 237));
    } else {
        QMessageBox::warning(this, tr("错误"),
            tr("无法保存文件: %1").arg(file.errorString()));
    }
}

void ScriptEditorDialog::onSaveAsScript()
{
    QString dir = scriptsDirectory();
    QString filePath = QFileDialog::getSaveFileName(this,
        tr("保存脚本"), dir, tr("Lua脚本 (*.lua)"));

    if (filePath.isEmpty()) return;

    if (!filePath.endsWith(".lua", Qt::CaseInsensitive)) {
        filePath += ".lua";
    }

    m_currentFilePath = filePath;
    onSaveScript();
    loadScriptList();  // 刷新列表
}

void ScriptEditorDialog::onRunScript()
{
    /*
     * 运行按钮只在空闲状态响应，避免用户重复启动多个 worker。
     * 真正的脚本执行会由 startWorkerExecution() 投递到后台线程。
     */
    if (m_runState != ScriptRunState::Idle) {
        return;
    }

    appendOutput(tr("[系统] 开始执行脚本..."), QColor(100, 149, 237));
    startWorkerExecution(m_codeEditor->toPlainText());
}

void ScriptEditorDialog::onStopScript()
{
    /*
     * 停止按钮只在 Running 状态有效。进入 Cancelling 后禁用按钮，
     * 防止重复写取消标记和重复追加“正在请求停止”提示。
     */
    if (m_runState != ScriptRunState::Running) {
        return;
    }

    requestWorkerCancellation();
    appendOutput(tr("[系统] 正在请求停止脚本..."), QColor(255, 165, 0));
    setRunState(ScriptRunState::Cancelling);
}

void ScriptEditorDialog::onScriptSelected(QListWidgetItem* item)
{
    Q_UNUSED(item)
    // 单击选中，不做操作
}

void ScriptEditorDialog::onScriptDoubleClicked(QListWidgetItem* item)
{
    QString fileName = item->text();
    QString filePath = scriptsDirectory() + "/" + fileName;

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        m_codeEditor->setPlainText(stream.readAll());
        file.close();

        m_currentFilePath = filePath;
        m_modified = false;
        setWindowTitle(tr("脚本编辑器 - %1").arg(fileName));
    }
}

void ScriptEditorDialog::updateLineNumbers()
{
    // 行号更新（暂时简化实现）
}

void ScriptEditorDialog::loadScriptList()
{
    m_scriptList->clear();

    QString dir = scriptsDirectory();
    QDir scriptDir(dir);

    if (!scriptDir.exists()) {
        scriptDir.mkpath(".");
    }

    QStringList filters;
    filters << "*.lua";
    QStringList files = scriptDir.entryList(filters, QDir::Files, QDir::Name);

    for (const QString& file : files) {
        m_scriptList->addItem(file);
    }
}

void ScriptEditorDialog::saveScriptList()
{
    // 脚本列表自动保存在目录中
}

void ScriptEditorDialog::appendOutput(const QString& text, const QColor& color)
{
    QString html;
    if (color.isValid()) {
        html = QString("<span style=\"color: %1;\">%2</span><br>")
            .arg(color.name())
            .arg(text.toHtmlEscaped());
    } else {
        html = QString("<span>%1</span><br>").arg(text.toHtmlEscaped());
    }

    m_outputArea->moveCursor(QTextCursor::End);
    m_outputArea->insertHtml(html);
    m_outputArea->verticalScrollBar()->setValue(
        m_outputArea->verticalScrollBar()->maximum());
}

void ScriptEditorDialog::clearOutput()
{
    m_outputArea->clear();
}

QString ScriptEditorDialog::scriptsDirectory() const
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData + "/scripts";
}

void ScriptEditorDialog::startWorkerExecution(const QString& script)
{
    /*
     * 启动前清掉已结束的旧任务指针。若旧线程仍在运行，cleanupWorkerThread()
     * 会保守返回；正常 UI 状态下这里不会发生并发启动。
     */
    cleanupWorkerThread();

    m_cancelRequested = std::make_shared<std::atomic_bool>(false);
    const std::weak_ptr<std::atomic_bool> cancelFlag = m_cancelRequested;

    /*
     * worker 持有的是 weak_ptr：如果对话框先析构，回调不会延长取消标记生命周期，
     * 只会把脚本视为已请求取消，避免悬空访问 UI 对象。
     */
    auto interruptCallback = [cancelFlag]() {
        const std::shared_ptr<std::atomic_bool> flag = cancelFlag.lock();
        return !flag || flag->load();
    };

    m_scriptThread = new QThread(this);
    m_scriptWorker = new ScriptExecutionWorker(script, interruptCallback);
    m_scriptWorker->moveToThread(m_scriptThread);
    QThread* thread = m_scriptThread;
    ScriptExecutionWorker* worker = m_scriptWorker;

    connect(m_scriptThread, &QThread::started,
            m_scriptWorker, &ScriptExecutionWorker::run);
    connect(m_scriptWorker, &ScriptExecutionWorker::sendRequested,
            this, &ScriptEditorDialog::handleWorkerSendRequested,
            Qt::QueuedConnection);
    connect(m_scriptWorker, &ScriptExecutionWorker::finished,
            this, &ScriptEditorDialog::handleWorkerFinished,
            Qt::QueuedConnection);
    connect(m_scriptWorker, &ScriptExecutionWorker::finished,
            m_scriptThread, &QThread::quit);
    connect(m_scriptThread, &QThread::finished,
            this, [this, thread, worker]() {
                /*
                 * 线程真正退出后再清空成员指针。这里校验捕获的 thread/worker 是否仍是当前任务，
                 * 避免用户快速再次运行脚本时，旧线程收尾误清理新任务状态。
                 */
                if (m_scriptThread == thread) {
                    m_scriptThread = nullptr;
                }
                if (m_scriptWorker == worker) {
                    m_scriptWorker = nullptr;
                }
                if (!m_scriptThread) {
                    m_cancelRequested.reset();
                }
            });
    connect(m_scriptThread, &QThread::finished,
            m_scriptWorker, &QObject::deleteLater);
    connect(m_scriptThread, &QThread::finished,
            m_scriptThread, &QObject::deleteLater);

    setRunState(ScriptRunState::Running);
    m_scriptThread->start();
}

void ScriptEditorDialog::requestWorkerCancellation()
{
    /*
     * 原子标记是 UI 线程和 Lua worker 线程之间唯一共享状态。
     * store(true) 后 Lua hook 会在下一次指令计数检查时返回 interrupted。
     */
    if (m_cancelRequested) {
        m_cancelRequested->store(true);
    }
}

void ScriptEditorDialog::handleWorkerSendRequested(const QByteArray& data)
{
    /*
     * 该槽通过 queued connection 在 UI 线程执行，所以可以安全更新输出区域。
     * 真正发送仍交给 MainWindow 既有 sendData 连接处理。
     */
    emit sendData(data);
    appendOutput(tr("[发送] %1").arg(sentPayloadPreview(data)), QColor(33, 150, 243));
}

void ScriptEditorDialog::handleWorkerFinished(const LuaSandboxResult& result)
{
    /*
     * worker 的结构化结果统一在这里转成用户可见输出。
     * 取消、成功、普通错误互斥展示，避免停止脚本时再显示 Lua 错误文本。
     */
    for (const QString& line : result.outputLines) {
        appendOutput(line, QColor(0, 180, 0));
    }

    if (result.interrupted) {
        appendOutput(tr("[系统] 脚本已取消"), QColor(255, 165, 0));
    } else if (result.success) {
        appendOutput(tr("[系统] 脚本执行完成"), QColor(100, 149, 237));
    } else {
        appendOutput(tr("[错误] %1").arg(result.errorMessage), QColor(220, 20, 60));
    }

    setRunState(ScriptRunState::Idle);
}

void ScriptEditorDialog::cleanupWorkerThread()
{
    /*
     * 正在运行的线程不能被 UI 线程直接清空指针或销毁；等待线程 finished
     * 连接负责最终清理。这里只处理已结束或未创建线程的空闲情况。
     */
    if (m_scriptThread && m_scriptThread->isRunning()) {
        return;
    }

    m_scriptThread = nullptr;
    m_scriptWorker = nullptr;
    m_cancelRequested.reset();
}

void ScriptEditorDialog::setRunState(ScriptRunState state)
{
    /*
     * 按状态集中控制按钮可用性，避免 onRunScript/onStopScript/finished
     * 分散修改 UI 后出现运行按钮和停止按钮不同步。
     */
    m_runState = state;
    m_isRunning = state != ScriptRunState::Idle;

    if (m_runBtn) {
        m_runBtn->setEnabled(state == ScriptRunState::Idle);
    }

    if (m_stopBtn) {
        m_stopBtn->setEnabled(state == ScriptRunState::Running);
    }
}

} // namespace ComAssistant
