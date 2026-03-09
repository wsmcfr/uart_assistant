/**
 * @file SearchWidget.cpp
 * @brief 数据搜索组件实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "SearchWidget.h"
#include <QApplication>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QScrollBar>

namespace ComAssistant {

SearchWidget::SearchWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void SearchWidget::setupUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 2, 5, 2);
    layout->setSpacing(5);

    // 搜索输入框
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索..."));
    m_searchEdit->setMinimumWidth(200);
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchWidget::onSearchTextChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &SearchWidget::findNext);

    // 搜索历史自动补全
    m_historyModel = new QStringListModel(this);
    m_completer = new QCompleter(m_historyModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_searchEdit->setCompleter(m_completer);

    // 查找按钮
    m_findPrevBtn = new QPushButton(tr("上一个"), this);
    m_findPrevBtn->setToolTip(tr("查找上一个匹配 (Shift+F3)"));
    m_findPrevBtn->setFixedWidth(60);
    connect(m_findPrevBtn, &QPushButton::clicked, this, &SearchWidget::findPrevious);

    m_findNextBtn = new QPushButton(tr("下一个"), this);
    m_findNextBtn->setToolTip(tr("查找下一个匹配 (F3)"));
    m_findNextBtn->setFixedWidth(60);
    connect(m_findNextBtn, &QPushButton::clicked, this, &SearchWidget::findNext);

    // 选项复选框
    m_caseSensitiveCheck = new QCheckBox(tr("区分大小写"), this);
    m_caseSensitiveCheck->setToolTip(tr("区分大小写匹配"));
    connect(m_caseSensitiveCheck, &QCheckBox::toggled, this, &SearchWidget::onCaseSensitiveToggled);

    m_wholeWordCheck = new QCheckBox(tr("全词匹配"), this);
    m_wholeWordCheck->setToolTip(tr("只匹配完整单词"));
    connect(m_wholeWordCheck, &QCheckBox::toggled, this, &SearchWidget::onWholeWordToggled);

    m_regexCheck = new QCheckBox(tr("正则"), this);
    m_regexCheck->setToolTip(tr("使用正则表达式"));
    connect(m_regexCheck, &QCheckBox::toggled, this, &SearchWidget::onRegexToggled);

    // 匹配信息
    m_matchInfoLabel = new QLabel(this);
    m_matchInfoLabel->setMinimumWidth(80);

    // 关闭按钮
    m_closeBtn = new QPushButton("×", this);
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setToolTip(tr("关闭搜索栏 (Esc)"));
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        clearHighlight();
        hide();
        emit closeRequested();
    });

    // 布局
    layout->addWidget(m_searchEdit);
    layout->addWidget(m_findPrevBtn);
    layout->addWidget(m_findNextBtn);
    layout->addWidget(m_caseSensitiveCheck);
    layout->addWidget(m_wholeWordCheck);
    layout->addWidget(m_regexCheck);
    layout->addWidget(m_matchInfoLabel);
    layout->addStretch();
    layout->addWidget(m_closeBtn);

    // 安装事件过滤器
    m_searchEdit->installEventFilter(this);
}

void SearchWidget::setTargetTextEdit(QTextEdit* textEdit)
{
    m_targetTextEdit = textEdit;
    m_targetPlainTextEdit = nullptr;
}

void SearchWidget::setTargetPlainTextEdit(QPlainTextEdit* textEdit)
{
    m_targetPlainTextEdit = textEdit;
    m_targetTextEdit = nullptr;
}

void SearchWidget::setSearchText(const QString& text)
{
    m_searchEdit->setText(text);
    performSearch();
}

QString SearchWidget::searchText() const
{
    return m_searchEdit->text();
}

void SearchWidget::showAndFocus()
{
    show();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();
}

QString SearchWidget::getTargetText() const
{
    if (m_targetTextEdit) {
        return m_targetTextEdit->toPlainText();
    } else if (m_targetPlainTextEdit) {
        return m_targetPlainTextEdit->toPlainText();
    }
    return QString();
}

void SearchWidget::selectText(int start, int length)
{
    if (m_targetTextEdit) {
        QTextCursor cursor = m_targetTextEdit->textCursor();
        cursor.setPosition(start);
        cursor.setPosition(start + length, QTextCursor::KeepAnchor);
        m_targetTextEdit->setTextCursor(cursor);
        m_targetTextEdit->ensureCursorVisible();
    } else if (m_targetPlainTextEdit) {
        QTextCursor cursor = m_targetPlainTextEdit->textCursor();
        cursor.setPosition(start);
        cursor.setPosition(start + length, QTextCursor::KeepAnchor);
        m_targetPlainTextEdit->setTextCursor(cursor);
        m_targetPlainTextEdit->ensureCursorVisible();
    }
}

QVector<SearchResult> SearchWidget::findAll(const QString& text) const
{
    QVector<SearchResult> results;
    QString content = getTargetText();

    if (content.isEmpty() || text.isEmpty()) {
        return results;
    }

    QRegularExpression regex;

    if (m_regexCheck->isChecked()) {
        regex.setPattern(text);
    } else {
        QString pattern = QRegularExpression::escape(text);
        if (m_wholeWordCheck->isChecked()) {
            pattern = "\\b" + pattern + "\\b";
        }
        regex.setPattern(pattern);
    }

    if (!m_caseSensitiveCheck->isChecked()) {
        regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    }

    if (!regex.isValid()) {
        return results;
    }

    QRegularExpressionMatchIterator it = regex.globalMatch(content);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        SearchResult result;
        result.position = match.capturedStart();
        result.length = match.capturedLength();
        result.matchedText = match.captured();

        // 计算行号
        result.lineNumber = content.left(result.position).count('\n') + 1;

        results.append(result);
    }

    return results;
}

void SearchWidget::performSearch()
{
    QString text = m_searchEdit->text();

    clearHighlight();
    m_results.clear();
    m_currentMatch = -1;
    m_matchCount = 0;

    if (text.isEmpty()) {
        updateMatchInfo();
        return;
    }

    m_results = findAll(text);
    m_matchCount = m_results.size();

    if (m_matchCount > 0) {
        m_currentMatch = 0;
        highlightAll();
        scrollToMatch(m_results[0].position);
        selectText(m_results[0].position, m_results[0].length);
    }

    updateMatchInfo();
    addToHistory(text);

    emit searchCompleted(m_matchCount);
}

void SearchWidget::findNext()
{
    if (m_results.isEmpty()) {
        performSearch();
        return;
    }

    if (m_currentMatch < m_results.size() - 1) {
        m_currentMatch++;
    } else {
        m_currentMatch = 0;  // 循环到开头
    }

    const SearchResult& result = m_results[m_currentMatch];
    scrollToMatch(result.position);
    selectText(result.position, result.length);
    updateMatchInfo();

    emit currentMatchChanged(m_currentMatch + 1, m_matchCount);
}

void SearchWidget::findPrevious()
{
    if (m_results.isEmpty()) {
        performSearch();
        return;
    }

    if (m_currentMatch > 0) {
        m_currentMatch--;
    } else {
        m_currentMatch = m_results.size() - 1;  // 循环到末尾
    }

    const SearchResult& result = m_results[m_currentMatch];
    scrollToMatch(result.position);
    selectText(result.position, result.length);
    updateMatchInfo();

    emit currentMatchChanged(m_currentMatch + 1, m_matchCount);
}

void SearchWidget::highlightAll()
{
    if (m_results.isEmpty()) return;

    // 使用 QTextEdit 的额外选择来高亮所有匹配
    if (m_targetTextEdit) {
        QList<QTextEdit::ExtraSelection> selections;
        QTextCharFormat format;
        format.setBackground(QColor(255, 255, 0, 100));  // 淡黄色背景

        for (const SearchResult& result : m_results) {
            QTextEdit::ExtraSelection selection;
            selection.format = format;
            QTextCursor cursor = m_targetTextEdit->textCursor();
            cursor.setPosition(result.position);
            cursor.setPosition(result.position + result.length, QTextCursor::KeepAnchor);
            selection.cursor = cursor;
            selections.append(selection);
        }

        m_targetTextEdit->setExtraSelections(selections);
    } else if (m_targetPlainTextEdit) {
        QList<QTextEdit::ExtraSelection> selections;
        QTextCharFormat format;
        format.setBackground(QColor(255, 255, 0, 100));

        for (const SearchResult& result : m_results) {
            QTextEdit::ExtraSelection selection;
            selection.format = format;
            QTextCursor cursor = m_targetPlainTextEdit->textCursor();
            cursor.setPosition(result.position);
            cursor.setPosition(result.position + result.length, QTextCursor::KeepAnchor);
            selection.cursor = cursor;
            selections.append(selection);
        }

        m_targetPlainTextEdit->setExtraSelections(selections);
    }
}

void SearchWidget::clearHighlight()
{
    if (m_targetTextEdit) {
        m_targetTextEdit->setExtraSelections({});
    } else if (m_targetPlainTextEdit) {
        m_targetPlainTextEdit->setExtraSelections({});
    }
}

void SearchWidget::scrollToMatch(int position)
{
    if (m_targetTextEdit) {
        QTextCursor cursor = m_targetTextEdit->textCursor();
        cursor.setPosition(position);
        m_targetTextEdit->setTextCursor(cursor);
        m_targetTextEdit->ensureCursorVisible();
    } else if (m_targetPlainTextEdit) {
        QTextCursor cursor = m_targetPlainTextEdit->textCursor();
        cursor.setPosition(position);
        m_targetPlainTextEdit->setTextCursor(cursor);
        m_targetPlainTextEdit->ensureCursorVisible();
    }
}

void SearchWidget::updateMatchInfo()
{
    if (m_matchCount == 0) {
        if (m_searchEdit->text().isEmpty()) {
            m_matchInfoLabel->setText("");
        } else {
            m_matchInfoLabel->setText(tr("无匹配"));
            m_matchInfoLabel->setStyleSheet("color: red;");
        }
    } else {
        m_matchInfoLabel->setText(tr("%1/%2").arg(m_currentMatch + 1).arg(m_matchCount));
        m_matchInfoLabel->setStyleSheet("");
    }
}

void SearchWidget::addToHistory(const QString& text)
{
    if (text.isEmpty()) return;

    m_searchHistory.removeAll(text);
    m_searchHistory.prepend(text);

    while (m_searchHistory.size() > MAX_HISTORY_SIZE) {
        m_searchHistory.removeLast();
    }

    m_historyModel->setStringList(m_searchHistory);
}

void SearchWidget::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text)
    // 实时搜索（可选）
    // performSearch();
}

void SearchWidget::onCaseSensitiveToggled(bool checked)
{
    Q_UNUSED(checked)
    if (!m_searchEdit->text().isEmpty()) {
        performSearch();
    }
}

void SearchWidget::onRegexToggled(bool checked)
{
    Q_UNUSED(checked)
    if (!m_searchEdit->text().isEmpty()) {
        performSearch();
    }
}

void SearchWidget::onWholeWordToggled(bool checked)
{
    Q_UNUSED(checked)
    if (!m_searchEdit->text().isEmpty()) {
        performSearch();
    }
}

void SearchWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        clearHighlight();
        hide();
        emit closeRequested();
        return;
    }

    if (event->key() == Qt::Key_F3) {
        if (event->modifiers() & Qt::ShiftModifier) {
            findPrevious();
        } else {
            findNext();
        }
        return;
    }

    QWidget::keyPressEvent(event);
}

bool SearchWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            clearHighlight();
            hide();
            emit closeRequested();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SearchWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SearchWidget::retranslateUi()
{
    m_searchEdit->setPlaceholderText(tr("搜索..."));
    m_findPrevBtn->setText(tr("上一个"));
    m_findPrevBtn->setToolTip(tr("查找上一个匹配 (Shift+F3)"));
    m_findNextBtn->setText(tr("下一个"));
    m_findNextBtn->setToolTip(tr("查找下一个匹配 (F3)"));
    m_caseSensitiveCheck->setText(tr("区分大小写"));
    m_caseSensitiveCheck->setToolTip(tr("区分大小写匹配"));
    m_wholeWordCheck->setText(tr("全词匹配"));
    m_wholeWordCheck->setToolTip(tr("只匹配完整单词"));
    m_regexCheck->setText(tr("正则"));
    m_regexCheck->setToolTip(tr("使用正则表达式"));
    m_closeBtn->setToolTip(tr("关闭搜索栏 (Esc)"));
    updateMatchInfo();
}

} // namespace ComAssistant
