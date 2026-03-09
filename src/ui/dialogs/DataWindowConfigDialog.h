/**
 * @file DataWindowConfigDialog.h
 * @brief 数据分窗配置对话框
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef COMASSISTANT_DATAWINDOWCONFIGDIALOG_H
#define COMASSISTANT_DATAWINDOWCONFIGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "../widgets/DataWindowManager.h"

namespace ComAssistant {

/**
 * @brief 数据分窗配置对话框
 */
class DataWindowConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DataWindowConfigDialog(DataWindowManager* manager, QWidget* parent = nullptr);
    ~DataWindowConfigDialog() override = default;

    /**
     * @brief 获取规则列表
     * @return 规则列表
     */
    QVector<WindowRule> rules() const { return m_rules; }

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onAddRuleClicked();
    void onEditRuleClicked();
    void onRemoveRuleClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onRuleSelectionChanged();
    void onAccepted();

private:
    void setupUi();
    void retranslateUi();
    void updateRuleTable();
    void editRule(int index);

    DataWindowManager* m_manager;
    QVector<WindowRule> m_rules;

    // UI元素
    QTableWidget* m_ruleTable = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_moveUpBtn = nullptr;
    QPushButton* m_moveDownBtn = nullptr;
};

/**
 * @brief 规则编辑对话框
 */
class RuleEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RuleEditDialog(QWidget* parent = nullptr);
    ~RuleEditDialog() override = default;

    void setRule(const WindowRule& rule);
    WindowRule rule() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();

    QLineEdit* m_windowNameEdit = nullptr;
    QLineEdit* m_patternEdit = nullptr;
    QCheckBox* m_regexCheck = nullptr;
    QCheckBox* m_caseSensitiveCheck = nullptr;
    QCheckBox* m_enabledCheck = nullptr;

    // 标签（用于翻译）
    QLabel* m_windowNameLabel = nullptr;
    QLabel* m_patternLabel = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_DATAWINDOWCONFIGDIALOG_H
