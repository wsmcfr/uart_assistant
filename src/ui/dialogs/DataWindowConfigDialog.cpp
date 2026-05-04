/**
 * @file DataWindowConfigDialog.cpp
 * @brief 数据分窗配置对话框实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "DataWindowConfigDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QEvent>

namespace ComAssistant {

// ==================== DataWindowConfigDialog ====================

DataWindowConfigDialog::DataWindowConfigDialog(DataWindowManager* manager, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
{
    m_rules = manager->rules();
    setupUi();
    setWindowTitle(tr("数据分窗配置"));
    resize(600, 400);
}

void DataWindowConfigDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 说明标签
    QLabel* descLabel = new QLabel(tr(
        "配置数据分窗规则：根据关键字或正则表达式将数据路由到不同的窗口。\n"
        "规则按顺序匹配，一条数据可以匹配多个规则。"
    ));
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    // 规则表格
    m_ruleTable = new QTableWidget;
    m_ruleTable->setColumnCount(5);
    m_ruleTable->setHorizontalHeaderLabels({tr("窗口名称"), tr("匹配模式"), tr("正则"), tr("区分大小写"), tr("启用")});
    m_ruleTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ruleTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_ruleTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_ruleTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_ruleTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_ruleTable->setColumnWidth(2, 50);
    m_ruleTable->setColumnWidth(3, 80);
    m_ruleTable->setColumnWidth(4, 50);
    m_ruleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ruleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ruleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_ruleTable, &QTableWidget::itemSelectionChanged, this, &DataWindowConfigDialog::onRuleSelectionChanged);
    connect(m_ruleTable, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem*) {
        onEditRuleClicked();
    });
    mainLayout->addWidget(m_ruleTable);

    // 按钮区域
    QHBoxLayout* btnLayout = new QHBoxLayout;

    m_addBtn = new QPushButton(tr("添加"));
    connect(m_addBtn, &QPushButton::clicked, this, &DataWindowConfigDialog::onAddRuleClicked);
    btnLayout->addWidget(m_addBtn);

    m_editBtn = new QPushButton(tr("编辑"));
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QPushButton::clicked, this, &DataWindowConfigDialog::onEditRuleClicked);
    btnLayout->addWidget(m_editBtn);

    m_removeBtn = new QPushButton(tr("删除"));
    m_removeBtn->setEnabled(false);
    connect(m_removeBtn, &QPushButton::clicked, this, &DataWindowConfigDialog::onRemoveRuleClicked);
    btnLayout->addWidget(m_removeBtn);

    btnLayout->addStretch();

    m_moveUpBtn = new QPushButton(tr("上移"));
    m_moveUpBtn->setEnabled(false);
    connect(m_moveUpBtn, &QPushButton::clicked, this, &DataWindowConfigDialog::onMoveUpClicked);
    btnLayout->addWidget(m_moveUpBtn);

    m_moveDownBtn = new QPushButton(tr("下移"));
    m_moveDownBtn->setEnabled(false);
    connect(m_moveDownBtn, &QPushButton::clicked, this, &DataWindowConfigDialog::onMoveDownClicked);
    btnLayout->addWidget(m_moveDownBtn);

    mainLayout->addLayout(btnLayout);

    // 对话框按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &DataWindowConfigDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    updateRuleTable();
}

void DataWindowConfigDialog::updateRuleTable()
{
    m_ruleTable->setRowCount(m_rules.size());

    for (int i = 0; i < m_rules.size(); ++i) {
        const WindowRule& rule = m_rules[i];

        m_ruleTable->setItem(i, 0, new QTableWidgetItem(rule.windowName));
        m_ruleTable->setItem(i, 1, new QTableWidgetItem(rule.pattern));
        m_ruleTable->setItem(i, 2, new QTableWidgetItem(rule.isRegex ? tr("是") : tr("否")));
        m_ruleTable->setItem(i, 3, new QTableWidgetItem(rule.caseSensitive ? tr("是") : tr("否")));
        m_ruleTable->setItem(i, 4, new QTableWidgetItem(rule.enabled ? tr("是") : tr("否")));

        // 设置居中对齐
        for (int col = 2; col <= 4; ++col) {
            m_ruleTable->item(i, col)->setTextAlignment(Qt::AlignCenter);
        }

        // 如果规则禁用，显示为灰色
        if (!rule.enabled) {
            for (int col = 0; col < 5; ++col) {
                m_ruleTable->item(i, col)->setForeground(Qt::gray);
            }
        }
    }
}

void DataWindowConfigDialog::onAddRuleClicked()
{
    RuleEditDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        WindowRule rule = dialog.rule();
        if (!rule.windowName.isEmpty() && !rule.pattern.isEmpty()) {
            m_rules.append(rule);
            updateRuleTable();
        }
    }
}

void DataWindowConfigDialog::onEditRuleClicked()
{
    int row = m_ruleTable->currentRow();
    if (row >= 0 && row < m_rules.size()) {
        editRule(row);
    }
}

void DataWindowConfigDialog::editRule(int index)
{
    if (index < 0 || index >= m_rules.size()) {
        return;
    }

    RuleEditDialog dialog(this);
    dialog.setRule(m_rules[index]);
    if (dialog.exec() == QDialog::Accepted) {
        WindowRule rule = dialog.rule();
        if (!rule.windowName.isEmpty() && !rule.pattern.isEmpty()) {
            m_rules[index] = rule;
            updateRuleTable();
        }
    }
}

void DataWindowConfigDialog::onRemoveRuleClicked()
{
    int row = m_ruleTable->currentRow();
    if (row >= 0 && row < m_rules.size()) {
        m_rules.removeAt(row);
        updateRuleTable();
    }
}

void DataWindowConfigDialog::onMoveUpClicked()
{
    int row = m_ruleTable->currentRow();
    if (row > 0 && row < m_rules.size()) {
        m_rules.insert(row - 1, m_rules.takeAt(row));
        updateRuleTable();
        m_ruleTable->selectRow(row - 1);
    }
}

void DataWindowConfigDialog::onMoveDownClicked()
{
    int row = m_ruleTable->currentRow();
    if (row >= 0 && row < m_rules.size() - 1) {
        m_rules.insert(row, m_rules.takeAt(row + 1));
        updateRuleTable();
        m_ruleTable->selectRow(row + 1);
    }
}

void DataWindowConfigDialog::onRuleSelectionChanged()
{
    int row = m_ruleTable->currentRow();
    bool hasSelection = row >= 0;

    m_editBtn->setEnabled(hasSelection);
    m_removeBtn->setEnabled(hasSelection);
    m_moveUpBtn->setEnabled(hasSelection && row > 0);
    m_moveDownBtn->setEnabled(hasSelection && row < m_rules.size() - 1);
}

void DataWindowConfigDialog::onAccepted()
{
    accept();
}

void DataWindowConfigDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void DataWindowConfigDialog::retranslateUi()
{
    setWindowTitle(tr("数据分窗配置"));
    m_ruleTable->setHorizontalHeaderLabels({tr("窗口名称"), tr("匹配模式"), tr("正则"), tr("区分大小写"), tr("启用")});
    if (m_addBtn) m_addBtn->setText(tr("添加"));
    if (m_editBtn) m_editBtn->setText(tr("编辑"));
    if (m_removeBtn) m_removeBtn->setText(tr("删除"));
    if (m_moveUpBtn) m_moveUpBtn->setText(tr("上移"));
    if (m_moveDownBtn) m_moveDownBtn->setText(tr("下移"));
    updateRuleTable();
}

// ==================== RuleEditDialog ====================

RuleEditDialog::RuleEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    setWindowTitle(tr("编辑规则"));
    resize(400, 200);
}

void RuleEditDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QFormLayout* formLayout = new QFormLayout;

    m_windowNameLabel = new QLabel(tr("窗口名称:"));
    m_windowNameEdit = new QLineEdit;
    m_windowNameEdit->setPlaceholderText(tr("例如: 错误日志"));
    formLayout->addRow(m_windowNameLabel, m_windowNameEdit);

    m_patternLabel = new QLabel(tr("匹配模式:"));
    m_patternEdit = new QLineEdit;
    m_patternEdit->setPlaceholderText(tr("例如: ERROR 或 ^\\[ERR\\]"));
    formLayout->addRow(m_patternLabel, m_patternEdit);

    m_regexCheck = new QCheckBox(tr("使用正则表达式"));
    formLayout->addRow("", m_regexCheck);

    m_caseSensitiveCheck = new QCheckBox(tr("区分大小写"));
    formLayout->addRow("", m_caseSensitiveCheck);

    m_enabledCheck = new QCheckBox(tr("启用此规则"));
    m_enabledCheck->setChecked(true);
    formLayout->addRow("", m_enabledCheck);

    mainLayout->addLayout(formLayout);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void RuleEditDialog::setRule(const WindowRule& rule)
{
    m_windowNameEdit->setText(rule.windowName);
    m_patternEdit->setText(rule.pattern);
    m_regexCheck->setChecked(rule.isRegex);
    m_caseSensitiveCheck->setChecked(rule.caseSensitive);
    m_enabledCheck->setChecked(rule.enabled);
}

WindowRule RuleEditDialog::rule() const
{
    WindowRule rule;
    rule.windowName = m_windowNameEdit->text().trimmed();
    rule.pattern = m_patternEdit->text();
    rule.isRegex = m_regexCheck->isChecked();
    rule.caseSensitive = m_caseSensitiveCheck->isChecked();
    rule.enabled = m_enabledCheck->isChecked();
    rule.regexCompiled = false;
    return rule;
}

void RuleEditDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void RuleEditDialog::retranslateUi()
{
    setWindowTitle(tr("编辑规则"));
    if (m_windowNameLabel) m_windowNameLabel->setText(tr("窗口名称:"));
    if (m_patternLabel) m_patternLabel->setText(tr("匹配模式:"));
    if (m_windowNameEdit) m_windowNameEdit->setPlaceholderText(tr("例如: 错误日志"));
    if (m_patternEdit) m_patternEdit->setPlaceholderText(tr("例如: ERROR 或 ^\\[ERR\\]"));
    if (m_regexCheck) m_regexCheck->setText(tr("使用正则表达式"));
    if (m_caseSensitiveCheck) m_caseSensitiveCheck->setText(tr("区分大小写"));
    if (m_enabledCheck) m_enabledCheck->setText(tr("启用此规则"));
}

} // namespace ComAssistant
