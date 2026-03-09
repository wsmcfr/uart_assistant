/**
 * @file LuaSyntaxHighlighter.cpp
 * @brief Lua语法高亮器实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "LuaSyntaxHighlighter.h"

namespace ComAssistant {

LuaSyntaxHighlighter::LuaSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    // 关键字格式 - 蓝色加粗
    m_keywordFormat.setForeground(QColor(86, 156, 214));
    m_keywordFormat.setFontWeight(QFont::Bold);

    // Lua关键字
    QStringList keywordPatterns = {
        "\\band\\b", "\\bbreak\\b", "\\bdo\\b", "\\belse\\b",
        "\\belseif\\b", "\\bend\\b", "\\bfalse\\b", "\\bfor\\b",
        "\\bfunction\\b", "\\bgoto\\b", "\\bif\\b", "\\bin\\b",
        "\\blocal\\b", "\\bnil\\b", "\\bnot\\b", "\\bor\\b",
        "\\brepeat\\b", "\\breturn\\b", "\\bthen\\b", "\\btrue\\b",
        "\\buntil\\b", "\\bwhile\\b"
    };

    for (const QString& pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_keywordFormat;
        m_highlightingRules.append(rule);
    }

    // 内置函数格式 - 紫色
    m_builtinFormat.setForeground(QColor(197, 134, 192));

    QStringList builtinPatterns = {
        "\\bprint\\b", "\\btype\\b", "\\btostring\\b", "\\btonumber\\b",
        "\\bipairs\\b", "\\bpairs\\b", "\\bnext\\b", "\\bselect\\b",
        "\\bsetmetatable\\b", "\\bgetmetatable\\b", "\\brawget\\b",
        "\\brawset\\b", "\\brawequal\\b", "\\brawlen\\b",
        "\\bcollectgarbage\\b", "\\bdofile\\b", "\\berror\\b",
        "\\bloadfile\\b", "\\bload\\b", "\\bpcall\\b", "\\bxpcall\\b",
        "\\bassert\\b", "\\brequire\\b",
        // 串口助手API
        "\\bserial\\b", "\\butils\\b", "\\bui\\b", "\\bfile\\b"
    };

    for (const QString& pattern : builtinPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = m_builtinFormat;
        m_highlightingRules.append(rule);
    }

    // 函数定义格式 - 黄色
    m_functionFormat.setForeground(QColor(220, 220, 170));
    rule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()");
    rule.format = m_functionFormat;
    m_highlightingRules.append(rule);

    // 数字格式 - 浅绿色
    m_numberFormat.setForeground(QColor(181, 206, 168));
    rule.pattern = QRegularExpression("\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?\\b");
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);

    // 十六进制数字
    rule.pattern = QRegularExpression("\\b0[xX][0-9a-fA-F]+\\b");
    rule.format = m_numberFormat;
    m_highlightingRules.append(rule);

    // 字符串格式 - 橙色
    m_stringFormat.setForeground(QColor(206, 145, 120));

    // 双引号字符串
    rule.pattern = QRegularExpression("\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\"");
    rule.format = m_stringFormat;
    m_highlightingRules.append(rule);

    // 单引号字符串
    rule.pattern = QRegularExpression("'[^'\\\\]*(\\\\.[^'\\\\]*)*'");
    rule.format = m_stringFormat;
    m_highlightingRules.append(rule);

    // 操作符格式 - 白色
    m_operatorFormat.setForeground(QColor(212, 212, 212));
    rule.pattern = QRegularExpression("[\\+\\-\\*/%\\^#=<>~\\.,;:\\[\\]\\(\\)\\{\\}]");
    rule.format = m_operatorFormat;
    m_highlightingRules.append(rule);

    // 单行注释 - 绿色
    m_commentFormat.setForeground(QColor(106, 153, 85));
    rule.pattern = QRegularExpression("--[^\\[].*$");
    rule.format = m_commentFormat;
    m_highlightingRules.append(rule);

    // 多行注释边界
    m_commentStartExpression = QRegularExpression("--\\[\\[");
    m_commentEndExpression = QRegularExpression("\\]\\]");
}

void LuaSyntaxHighlighter::highlightBlock(const QString& text)
{
    // 应用单行规则
    for (const HighlightingRule& rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // 处理多行注释
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        QRegularExpressionMatch startMatch = m_commentStartExpression.match(text);
        startIndex = startMatch.hasMatch() ? startMatch.capturedStart() : -1;
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch endMatch = m_commentEndExpression.match(text, startIndex);
        int endIndex = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
        int commentLength;

        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + endMatch.capturedLength();
        }

        setFormat(startIndex, commentLength, m_commentFormat);

        QRegularExpressionMatch nextStartMatch = m_commentStartExpression.match(text, startIndex + commentLength);
        startIndex = nextStartMatch.hasMatch() ? nextStartMatch.capturedStart() : -1;
    }
}

} // namespace ComAssistant
