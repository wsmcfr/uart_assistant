/**
 * @file RecoveryDialog.cpp
 * @brief 崩溃恢复对话框实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "RecoveryDialog.h"
#include "core/utils/Logger.h"

#include <QEvent>
#include <QStyle>
#include <QApplication>

namespace ComAssistant {

RecoveryDialog::RecoveryDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
    retranslateUi();
}

void RecoveryDialog::setupUi()
{
    setMinimumSize(450, 300);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // 图标和标题
    QHBoxLayout* headerLayout = new QHBoxLayout;
    m_iconLabel = new QLabel;
    m_iconLabel->setPixmap(style()->standardPixmap(QStyle::SP_MessageBoxWarning).scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_iconLabel->setFixedSize(48, 48);

    QVBoxLayout* titleLayout = new QVBoxLayout;
    m_titleLabel = new QLabel;
    m_titleLabel->setObjectName("recoveryTitleLabel");
    m_messageLabel = new QLabel;
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setObjectName("recoveryMessageLabel");
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_messageLabel);
    titleLayout->addStretch();

    headerLayout->addWidget(m_iconLabel);
    headerLayout->addSpacing(15);
    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // 详情区域
    m_detailsEdit = new QTextEdit;
    m_detailsEdit->setReadOnly(true);
    m_detailsEdit->setMaximumHeight(120);
    m_detailsEdit->setObjectName("recoveryDetailsEdit");
    mainLayout->addWidget(m_detailsEdit);

    // 不再提示选项
    m_dontAskCheck = new QCheckBox;
    mainLayout->addWidget(m_dontAskCheck);

    mainLayout->addStretch();

    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout;

    m_recoverBtn = new QPushButton;
    m_recoverBtn->setDefault(true);
    m_recoverBtn->setObjectName("recoverBtn");
    connect(m_recoverBtn, &QPushButton::clicked, this, &RecoveryDialog::onRecoverClicked);

    m_discardBtn = new QPushButton;
    m_discardBtn->setObjectName("discardBtn");
    connect(m_discardBtn, &QPushButton::clicked, this, &RecoveryDialog::onDiscardClicked);

    m_laterBtn = new QPushButton;
    connect(m_laterBtn, &QPushButton::clicked, this, &RecoveryDialog::onLaterClicked);

    buttonLayout->addWidget(m_laterBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_discardBtn);
    buttonLayout->addWidget(m_recoverBtn);

    mainLayout->addLayout(buttonLayout);
}

void RecoveryDialog::retranslateUi()
{
    setWindowTitle(tr("会话恢复"));
    m_titleLabel->setText(tr("检测到未保存的会话数据"));
    m_messageLabel->setText(tr("程序上次异常退出，是否恢复之前的会话？"));

    m_recoverBtn->setText(tr("恢复会话"));
    m_discardBtn->setText(tr("丢弃数据"));
    m_laterBtn->setText(tr("稍后决定"));

    m_dontAskCheck->setText(tr("不再提示（下次自动丢弃）"));

    // 更新详情
    updateDetails();
}

void RecoveryDialog::setRecoveryInfo(const RecoveryInfo& info)
{
    m_info = info;
    updateDetails();
}

void RecoveryDialog::updateDetails()
{
    QString details;

    if (m_info.crashTime.isValid()) {
        details += tr("崩溃时间: %1\n").arg(m_info.crashTime.toString("yyyy-MM-dd hh:mm:ss"));
    }

    if (!m_info.portName.isEmpty()) {
        details += tr("串口: %1\n").arg(m_info.portName);
    }

    if (m_info.recordCount > 0) {
        details += tr("记录数量: %1 条\n").arg(m_info.recordCount);
    }

    if (m_info.dataSize > 0) {
        QString sizeStr;
        if (m_info.dataSize < 1024) {
            sizeStr = QString("%1 B").arg(m_info.dataSize);
        } else if (m_info.dataSize < 1024 * 1024) {
            sizeStr = QString("%1 KB").arg(m_info.dataSize / 1024.0, 0, 'f', 2);
        } else {
            sizeStr = QString("%1 MB").arg(m_info.dataSize / (1024.0 * 1024.0), 0, 'f', 2);
        }
        details += tr("数据大小: %1\n").arg(sizeStr);
    }

    if (!m_info.sessionFile.isEmpty()) {
        details += tr("会话文件: %1\n").arg(m_info.sessionFile);
    }

    if (!m_info.lastError.isEmpty()) {
        details += tr("错误信息: %1\n").arg(m_info.lastError);
    }

    if (details.isEmpty()) {
        details = tr("无详细信息");
    }

    m_detailsEdit->setPlainText(details);
}

bool RecoveryDialog::dontAskAgain() const
{
    return m_dontAskCheck->isChecked();
}

void RecoveryDialog::onRecoverClicked()
{
    m_selectedAction = Action::Recover;
    LOG_INFO("User chose to recover session");
    accept();
}

void RecoveryDialog::onDiscardClicked()
{
    m_selectedAction = Action::Discard;
    LOG_INFO("User chose to discard recovery data");
    accept();
}

void RecoveryDialog::onLaterClicked()
{
    m_selectedAction = Action::Later;
    LOG_INFO("User chose to decide later");
    reject();
}

void RecoveryDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

} // namespace ComAssistant
