/**
 * @file RecoveryDialog.h
 * @brief 崩溃恢复对话框
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_RECOVERYDIALOG_H
#define COMASSISTANT_RECOVERYDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QDateTime>
#include <QCheckBox>

namespace ComAssistant {

/**
 * @brief 恢复信息结构
 */
struct RecoveryInfo {
    QDateTime crashTime;        ///< 崩溃时间
    QString sessionFile;        ///< 会话文件路径
    qint64 dataSize = 0;        ///< 数据大小
    int recordCount = 0;        ///< 记录数量
    QString portName;           ///< 串口名称
    QString lastError;          ///< 最后的错误信息
};

/**
 * @brief 崩溃恢复对话框
 */
class RecoveryDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 用户选择的操作
     */
    enum class Action {
        Recover,    ///< 恢复会话
        Discard,    ///< 丢弃数据
        Later       ///< 稍后决定
    };

    explicit RecoveryDialog(QWidget* parent = nullptr);
    ~RecoveryDialog() override = default;

    /**
     * @brief 设置恢复信息
     */
    void setRecoveryInfo(const RecoveryInfo& info);

    /**
     * @brief 获取用户选择的操作
     */
    Action selectedAction() const { return m_selectedAction; }

    /**
     * @brief 是否不再提示
     */
    bool dontAskAgain() const;

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onRecoverClicked();
    void onDiscardClicked();
    void onLaterClicked();

private:
    void setupUi();
    void retranslateUi();
    void updateDetails();

private:
    RecoveryInfo m_info;
    Action m_selectedAction = Action::Later;

    // UI组件
    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_messageLabel = nullptr;
    QTextEdit* m_detailsEdit = nullptr;
    QCheckBox* m_dontAskCheck = nullptr;

    QPushButton* m_recoverBtn = nullptr;
    QPushButton* m_discardBtn = nullptr;
    QPushButton* m_laterBtn = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_RECOVERYDIALOG_H
