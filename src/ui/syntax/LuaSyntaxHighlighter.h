/**
 * @file LuaSyntaxHighlighter.h
 * @brief Lua语法高亮器
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_LUASYNTAXHIGHLIGHTER_H
#define COMASSISTANT_LUASYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

namespace ComAssistant {

/**
 * @brief Lua语法高亮器
 */
class LuaSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit LuaSyntaxHighlighter(QTextDocument* parent = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> m_highlightingRules;

    // 格式
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_builtinFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_operatorFormat;

    // 多行注释
    QRegularExpression m_commentStartExpression;
    QRegularExpression m_commentEndExpression;
};

} // namespace ComAssistant

#endif // COMASSISTANT_LUASYNTAXHIGHLIGHTER_H
