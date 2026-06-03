/**
 * @file DataWindow.cpp
 * @brief 数据分窗窗口实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "DataWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

namespace ComAssistant {

DataWindow::DataWindow(const QString& name, QWidget* parent)
    : QWidget(parent)
    , m_name(name)
{
    setupUi();
    /*
     * 数据分窗是独立辅助窗口。用户点击窗口关闭按钮时，应真正销毁对象，
     * 让 QTextDocument 历史和滚动缓存立即释放，而不是只从管理器映射移除。
     */
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowTitle(tr("数据窗口 - %1").arg(name));
    resize(500, 400);
}

void DataWindow::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 工具栏
    QHBoxLayout* toolLayout = new QHBoxLayout;
    toolLayout->setSpacing(8);

    m_timestampCheck = new QCheckBox(tr("时间戳"));
    m_timestampCheck->setChecked(m_timestampEnabled);
    connect(m_timestampCheck, &QCheckBox::toggled, this, &DataWindow::onTimestampToggled);
    toolLayout->addWidget(m_timestampCheck);

    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    m_autoScrollCheck->setChecked(m_autoScrollEnabled);
    connect(m_autoScrollCheck, &QCheckBox::toggled, this, &DataWindow::onAutoScrollToggled);
    toolLayout->addWidget(m_autoScrollCheck);

    toolLayout->addStretch();

    m_statsLabel = new QLabel(tr("行数: 0"));
    toolLayout->addWidget(m_statsLabel);

    m_exportBtn = new QPushButton(tr("导出"));
    m_exportBtn->setFixedWidth(60);
    connect(m_exportBtn, &QPushButton::clicked, this, &DataWindow::onExportClicked);
    toolLayout->addWidget(m_exportBtn);

    m_clearBtn = new QPushButton(tr("清空"));
    m_clearBtn->setFixedWidth(60);
    connect(m_clearBtn, &QPushButton::clicked, this, &DataWindow::onClearClicked);
    toolLayout->addWidget(m_clearBtn);

    mainLayout->addLayout(toolLayout);

    // 文本显示区
    m_textEdit = new QTextEdit;
    m_textEdit->setReadOnly(true);
    m_textEdit->setLineWrapMode(QTextEdit::NoWrap);
    m_textEdit->setFont(QFont("Consolas", 10));
    m_textEdit->document()->setMaximumBlockCount(m_maxLines);
    m_textEdit->document()->setUndoRedoEnabled(false);
    m_textEdit->setPlaceholderText(tr("匹配的数据将在这里显示..."));

    // 连接滚动条信号实现智能滚屏
    connect(m_textEdit->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &DataWindow::onScrollBarValueChanged);

    mainLayout->addWidget(m_textEdit);

    // 智能滚屏指示器
    m_smartScrollIndicator = new QLabel(tr("已暂停滚动 - 滚动到底部恢复"));
    m_smartScrollIndicator->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(255, 193, 7, 0.9);"
        "  color: #333;"
        "  padding: 6px 12px;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "}"
    );
    m_smartScrollIndicator->setAlignment(Qt::AlignCenter);
    m_smartScrollIndicator->hide();
    mainLayout->addWidget(m_smartScrollIndicator);
}

void DataWindow::appendData(const QString& data)
{
    QString text;

    // 按行处理
    QStringList lines = data.split('\n');
    for (const QString& line : lines) {
        if (line.isEmpty()) continue;

        if (m_timestampEnabled && m_needTimestamp) {
            text += "[" + QDateTime::currentDateTime().toString("hh:mm:ss.zzz") + "] ";
            m_needTimestamp = false;
        }

        text += line;

        if (data.contains('\n')) {
            text += "\n";
            m_needTimestamp = true;
            m_lineCount++;
        }
    }

    if (!text.isEmpty()) {
        m_textEdit->moveCursor(QTextCursor::End);
        m_textEdit->insertPlainText(text);
        trimTextDocument();

        // 智能滚屏：仅在未暂停时滚动到底部
        if (m_autoScrollEnabled && !m_smartScrollPaused) {
            m_textEdit->verticalScrollBar()->setValue(
                m_textEdit->verticalScrollBar()->maximum());
        }
    }

    // 更新统计
    m_statsLabel->setText(tr("行数: %1").arg(m_lineCount));
}

void DataWindow::clear()
{
    m_textEdit->clear();
    /*
     * 分窗没有撤销需求。清空后主动清理 QTextDocument 的 undo 栈，
     * 避免已经删除的历史内容仍被文档内部缓存持有。
     */
    if (m_textEdit->document()) {
        m_textEdit->document()->clearUndoRedoStacks();
    }
    m_lineCount = 0;
    m_needTimestamp = true;
    m_smartScrollPaused = false;
    hideSmartScrollIndicator();
    m_statsLabel->setText(tr("行数: 0"));
}

void DataWindow::setTimestampEnabled(bool enabled)
{
    m_timestampEnabled = enabled;
    m_timestampCheck->setChecked(enabled);
}

void DataWindow::setAutoScrollEnabled(bool enabled)
{
    m_autoScrollEnabled = enabled;
    m_autoScrollCheck->setChecked(enabled);
    if (enabled) {
        m_smartScrollPaused = false;
        hideSmartScrollIndicator();
    }
}

bool DataWindow::exportToFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << m_textEdit->toPlainText();
    file.close();
    return true;
}

void DataWindow::trimTextDocument()
{
    if (!m_textEdit) {
        return;
    }

    QTextDocument* document = m_textEdit->document();
    if (!document) {
        return;
    }

    /*
     * maximumBlockCount 已经负责“很多行”的场景；这里补上“单行极长”的
     * 场景。QTextDocument::characterCount() 通常包含一个隐式结尾字符，
     * 因此允许保留值略小于上限，避免边界处反复裁剪一个字符。
     */
    const int charCount = document->characterCount();
    if (charCount <= m_maxTextChars) {
        return;
    }

    const int removeChars = charCount - m_maxTextChars;
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    cursor.setPosition(0);
    cursor.setPosition(removeChars, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();

    /*
     * 裁剪掉最早文本后清理撤销栈，防止 QTextDocument 为已经删除的历史
     * 继续保留撤销数据，削弱本次内存优化效果。
     */
    document->clearUndoRedoStacks();
}

void DataWindow::closeEvent(QCloseEvent* event)
{
    emit windowClosed(m_name);
    event->accept();
}

void DataWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void DataWindow::onClearClicked()
{
    clear();
}

void DataWindow::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出数据"),
        QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + "_" + m_name + ".txt",
        tr("文本文件 (*.txt);;所有文件 (*)")
    );

    if (!fileName.isEmpty()) {
        exportToFile(fileName);
    }
}

void DataWindow::onTimestampToggled(bool checked)
{
    m_timestampEnabled = checked;
}

void DataWindow::onAutoScrollToggled(bool checked)
{
    m_autoScrollEnabled = checked;
    if (checked) {
        m_smartScrollPaused = false;
        hideSmartScrollIndicator();
    }
}

void DataWindow::onScrollBarValueChanged(int value)
{
    if (!m_autoScrollEnabled) {
        return;
    }

    QScrollBar* scrollBar = m_textEdit->verticalScrollBar();
    int maxValue = scrollBar->maximum();

    // 判断是否在底部（容差10像素）
    bool atBottom = (value >= maxValue - 10) || (maxValue == 0);

    if (!atBottom && !m_smartScrollPaused) {
        // 用户向上滚动，暂停自动滚动
        m_smartScrollPaused = true;
        showSmartScrollIndicator(tr("已暂停滚动 - 滚动到底部恢复"));
    } else if (atBottom && m_smartScrollPaused) {
        // 滚动到底部，恢复自动滚动
        m_smartScrollPaused = false;
        hideSmartScrollIndicator();
    }
}

void DataWindow::showSmartScrollIndicator(const QString& message)
{
    if (m_smartScrollIndicator) {
        m_smartScrollIndicator->setText(message);
        m_smartScrollIndicator->show();
    }
}

void DataWindow::hideSmartScrollIndicator()
{
    if (m_smartScrollIndicator) {
        m_smartScrollIndicator->hide();
    }
}

void DataWindow::retranslateUi()
{
    setWindowTitle(tr("数据窗口 - %1").arg(m_name));
    if (m_timestampCheck) m_timestampCheck->setText(tr("时间戳"));
    if (m_autoScrollCheck) m_autoScrollCheck->setText(tr("自动滚动"));
    if (m_exportBtn) m_exportBtn->setText(tr("导出"));
    if (m_clearBtn) m_clearBtn->setText(tr("清空"));
    if (m_statsLabel) m_statsLabel->setText(tr("行数: %1").arg(m_lineCount));
    if (m_textEdit) m_textEdit->setPlaceholderText(tr("匹配的数据将在这里显示..."));
    if (m_smartScrollIndicator && m_smartScrollPaused) {
        m_smartScrollIndicator->setText(tr("已暂停滚动 - 滚动到底部恢复"));
    }
}

} // namespace ComAssistant
