/**
 * @file ScriptEditorDialog.cpp
 * @brief 脚本编辑器对话框实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "ScriptEditorDialog.h"
#include "core/script/LuaSandbox.h"
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
    if (m_isRunning) return;

    m_isRunning = true;
    m_runBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    appendOutput(tr("[系统] 开始执行脚本..."), QColor(100, 149, 237));

    // 脚本执行仍在当前版本同步完成，后续阶段再迁移到后台 worker 和强取消。
    QString script = m_codeEditor->toPlainText();
    const bool success = executeSandboxScript(script);

    m_isRunning = false;
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);

    if (success) {
        appendOutput(tr("[系统] 脚本执行完成"), QColor(100, 149, 237));
    }
}

void ScriptEditorDialog::onStopScript()
{
    m_isRunning = false;
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    appendOutput(tr("[系统] 当前版本会在脚本超时保护触发后停止，后台取消将在后续版本提供"),
                 QColor(255, 165, 0));
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

bool ScriptEditorDialog::executeSandboxScript(const QString& script)
{
    LuaSandboxOptions options;
    options.timeoutMs = 3000;
    options.memoryLimitKb = 2048;
    options.maxOutputLines = 500;
    options.allowCommunicationApi = true;

    /*
     * serial.send/serial.sendHex 只负责发起发送请求。这里拒绝空数据，
     * 是为了避免脚本误调用产生难以观察的空发送项，并让 LuaSandbox 返回明确错误。
     */
    options.sendCallback = [this](const QByteArray& bytes) {
        if (bytes.isEmpty()) {
            return false;
        }

        emit sendData(bytes);
        appendOutput(tr("[发送] %1").arg(sentPayloadPreview(bytes)), QColor(33, 150, 243));
        return true;
    };

    /*
     * 4.6 只接入脚本编辑器到既有发送链路，暂不把主窗口连接状态传入对话框。
     * 因此当通信 API 已显式启用时，脚本侧 isOpen() 返回 true。
     */
    options.isOpenCallback = []() {
        return true;
    };

    LuaSandbox sandbox;
    const LuaSandboxResult result = sandbox.execute(script, options);

    for (const QString& line : result.outputLines) {
        appendOutput(line, QColor(0, 180, 0));
    }

    if (!result.success) {
        appendOutput(tr("[错误] %1").arg(result.errorMessage), QColor(220, 20, 60));
        return false;
    }

    return true;
}

} // namespace ComAssistant
