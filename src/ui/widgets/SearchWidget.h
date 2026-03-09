/**
 * @file SearchWidget.h
 * @brief 数据搜索组件
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_SEARCHWIDGET_H
#define COMASSISTANT_SEARCHWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QCompleter>
#include <QStringListModel>

namespace ComAssistant {

/**
 * @brief 搜索结果信息
 */
struct SearchResult {
    int position = -1;      ///< 匹配位置
    int length = 0;         ///< 匹配长度
    int lineNumber = -1;    ///< 行号
    QString matchedText;    ///< 匹配的文本
};

/**
 * @brief 数据搜索组件
 *
 * 提供文本搜索功能，支持正则表达式、大小写敏感等选项
 */
class SearchWidget : public QWidget {
    Q_OBJECT

public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget() override = default;

    /**
     * @brief 设置目标文本编辑器
     */
    void setTargetTextEdit(QTextEdit* textEdit);
    void setTargetPlainTextEdit(QPlainTextEdit* textEdit);

    /**
     * @brief 设置搜索文本
     */
    void setSearchText(const QString& text);

    /**
     * @brief 获取搜索文本
     */
    QString searchText() const;

    /**
     * @brief 获取所有匹配结果
     */
    QVector<SearchResult> findAll(const QString& text) const;

    /**
     * @brief 高亮所有匹配
     */
    void highlightAll();

    /**
     * @brief 清除高亮
     */
    void clearHighlight();

    /**
     * @brief 获取匹配数量
     */
    int matchCount() const { return m_matchCount; }

    /**
     * @brief 获取当前匹配索引
     */
    int currentMatch() const { return m_currentMatch; }

signals:
    /**
     * @brief 搜索完成
     */
    void searchCompleted(int matchCount);

    /**
     * @brief 关闭请求
     */
    void closeRequested();

    /**
     * @brief 当前匹配变化
     */
    void currentMatchChanged(int current, int total);

public slots:
    /**
     * @brief 查找下一个
     */
    void findNext();

    /**
     * @brief 查找上一个
     */
    void findPrevious();

    /**
     * @brief 执行搜索
     */
    void performSearch();

    /**
     * @brief 显示并聚焦
     */
    void showAndFocus();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onSearchTextChanged(const QString& text);
    void onCaseSensitiveToggled(bool checked);
    void onRegexToggled(bool checked);
    void onWholeWordToggled(bool checked);

private:
    void setupUi();
    void updateMatchInfo();
    void addToHistory(const QString& text);
    void scrollToMatch(int position);
    QString getTargetText() const;
    void selectText(int start, int length);
    void retranslateUi();

private:
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_findNextBtn = nullptr;
    QPushButton* m_findPrevBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QCheckBox* m_caseSensitiveCheck = nullptr;
    QCheckBox* m_regexCheck = nullptr;
    QCheckBox* m_wholeWordCheck = nullptr;
    QLabel* m_matchInfoLabel = nullptr;

    QTextEdit* m_targetTextEdit = nullptr;
    QPlainTextEdit* m_targetPlainTextEdit = nullptr;

    QVector<SearchResult> m_results;
    int m_currentMatch = -1;
    int m_matchCount = 0;

    QStringList m_searchHistory;
    QCompleter* m_completer = nullptr;
    QStringListModel* m_historyModel = nullptr;

    static const int MAX_HISTORY_SIZE = 20;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SEARCHWIDGET_H
