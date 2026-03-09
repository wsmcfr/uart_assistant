/**
 * @file ScriptEditorDialog.cpp
 * @brief 脚本编辑器对话框实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "ScriptEditorDialog.h"
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
#include <QRegularExpression>

namespace ComAssistant {

ScriptEditorDialog::ScriptEditorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("脚本编辑器"));
    setMinimumSize(900, 600);
    resize(1000, 700);

    setupUi();
    loadScriptList();

    // 示例脚本
    QString defaultScript = R"(-- 串口助手Lua脚本示例
-- 可用API:
--   serial.send(data)     - 发送字符串
--   serial.sendHex(hex)   - 发送十六进制
--   utils.crc16(data)     - 计算CRC16
--   utils.sleep(ms)       - 延时毫秒
--   ui.log(msg)           - 输出日志

-- 示例: 发送AT指令
function sendAT()
    serial.send("AT\r\n")
    utils.sleep(100)
end

-- 示例: 循环发送
function loopSend(count, interval)
    for i = 1, count do
        serial.send("Hello " .. i .. "\r\n")
        utils.sleep(interval)
        ui.log("发送第 " .. i .. " 次")
    end
end

-- 运行示例
ui.log("脚本加载完成")
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

    m_runBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; }");
    m_stopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; }");
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
    helpLabel->setStyleSheet("color: #888;");
    toolbarLayout->addWidget(helpLabel);

    mainLayout->addLayout(toolbarLayout);

    // 主分割区域
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧：脚本列表
    QWidget* leftPanel = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* listLabel = new QLabel(tr("脚本列表"));
    listLabel->setStyleSheet("font-weight: bold;");
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
    editorLabel->setStyleSheet("font-weight: bold;");
    centerLayout->addWidget(editorLabel);

    m_codeEditor = new QPlainTextEdit;
    m_codeEditor->setFont(QFont("Consolas", 11));
    m_codeEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_codeEditor->setTabStopDistance(QFontMetricsF(m_codeEditor->font()).horizontalAdvance(' ') * 4);

    // 编辑器样式 - 暗色主题
    m_codeEditor->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #333;"
        "  selection-background-color: #264f78;"
        "}"
    );

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
    outputLabel->setStyleSheet("font-weight: bold;");
    outputHeader->addWidget(outputLabel);

    QPushButton* clearOutputBtn = new QPushButton(tr("清空"));
    clearOutputBtn->setFixedWidth(50);
    connect(clearOutputBtn, &QPushButton::clicked, this, &ScriptEditorDialog::clearOutput);
    outputHeader->addStretch();
    outputHeader->addWidget(clearOutputBtn);

    rightLayout->addLayout(outputHeader);

    m_outputArea = new QTextEdit;
    m_outputArea->setReadOnly(true);
    m_outputArea->setFont(QFont("Consolas", 10));
    m_outputArea->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1a1a1a;"
        "  color: #00ff00;"
        "  border: 1px solid #333;"
        "}"
    );
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
        stream.setEncoding(QStringConverter::Utf8);
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
        stream.setEncoding(QStringConverter::Utf8);
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

    // 执行脚本
    QString script = m_codeEditor->toPlainText();
    executeSimpleScript(script);

    m_isRunning = false;
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);

    appendOutput(tr("[系统] 脚本执行完成"), QColor(100, 149, 237));
}

void ScriptEditorDialog::onStopScript()
{
    m_isRunning = false;
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    appendOutput(tr("[系统] 脚本已停止"), QColor(255, 165, 0));
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
        stream.setEncoding(QStringConverter::Utf8);
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

void ScriptEditorDialog::executeSimpleScript(const QString& script)
{
    // 简单的脚本解释器（不依赖Lua库）
    // 仅支持基本的 ui.log() 调用演示

    QStringList lines = script.split('\n');

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();

        // 跳过注释和空行
        if (trimmed.isEmpty() || trimmed.startsWith("--")) {
            continue;
        }

        // 解析 ui.log("message")
        QRegularExpression logPattern(R"(ui\.log\s*\(\s*["'](.*)["']\s*\))");
        QRegularExpressionMatch match = logPattern.match(trimmed);
        if (match.hasMatch()) {
            QString msg = match.captured(1);
            appendOutput(msg, QColor(0, 255, 0));
        }

        // 解析 serial.send("data")
        QRegularExpression sendPattern(R"(serial\.send\s*\(\s*["'](.*)["']\s*\))");
        match = sendPattern.match(trimmed);
        if (match.hasMatch()) {
            QString data = match.captured(1);
            // 处理转义字符
            data.replace("\\r", "\r");
            data.replace("\\n", "\n");
            data.replace("\\t", "\t");
            emit sendData(data.toUtf8());
            appendOutput(tr("[发送] %1").arg(data.simplified()), QColor(33, 150, 243));
        }

        // 解析 serial.sendHex("AA BB CC")
        QRegularExpression sendHexPattern(R"(serial\.sendHex\s*\(\s*["'](.*)["']\s*\))");
        match = sendHexPattern.match(trimmed);
        if (match.hasMatch()) {
            QString hexStr = match.captured(1);
            QByteArray data;
            QStringList hexParts = hexStr.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
            for (const QString& hex : hexParts) {
                bool ok;
                int value = hex.toInt(&ok, 16);
                if (ok && value >= 0 && value <= 255) {
                    data.append(static_cast<char>(value));
                }
            }
            if (!data.isEmpty()) {
                emit sendData(data);
                appendOutput(tr("[发送HEX] %1").arg(hexStr), QColor(33, 150, 243));
            }
        }
    }
}

} // namespace ComAssistant
