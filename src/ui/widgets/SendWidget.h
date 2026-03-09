/**
 * @file SendWidget.h
 * @brief 发送区组件
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_SENDWIDGET_H
#define COMASSISTANT_SENDWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTimer>
#include <QStringList>
#include <QLabel>

namespace ComAssistant {

/**
 * @brief 发送模式
 */
enum class SendMode {
    Ascii,      ///< ASCII文本
    Hex         ///< 十六进制
};

/**
 * @brief 发送区组件
 */
class SendWidget : public QWidget {
    Q_OBJECT

public:
    explicit SendWidget(QWidget* parent = nullptr);
    ~SendWidget() override = default;

    /**
     * @brief 获取待发送数据
     */
    QByteArray getData() const;

    /**
     * @brief 设置发送模式
     */
    void setSendMode(SendMode mode);
    SendMode sendMode() const { return m_sendMode; }

    /**
     * @brief 设置是否追加换行
     */
    void setAppendNewlineEnabled(bool enabled);

    /**
     * @brief 设置追加换行内容（支持 \r\n / \n / \r）
     */
    void setAppendNewlineSequence(const QString& newline);

    /**
     * @brief 清除历史记录
     */
    void clearHistory();

signals:
    /**
     * @brief 请求发送数据
     */
    void sendRequested(const QByteArray& data);

private slots:
    void onSendClicked();
    void onClearClicked();
    void onModeChanged(int index);
    void onAutoSendToggled(bool checked);
    void onAutoSendTimeout();
    void onHistorySelected(int index);
    void updateLineNumbers();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void addToHistory(const QString& text);
    QByteArray textToBytes(const QString& text) const;
    QString bytesToText(const QByteArray& data) const;

private:
    // 输入区
    QTextEdit* m_inputEdit = nullptr;
    QLabel* m_lineNumberLabel = nullptr;

    // 按钮
    QPushButton* m_sendBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    // 自动发送
    QCheckBox* m_autoSendCheck = nullptr;
    QSpinBox* m_intervalSpin = nullptr;
    QTimer* m_autoSendTimer = nullptr;

    // 时间戳选项
    QCheckBox* m_timestampCheck = nullptr;
    QSpinBox* m_timestampSpin = nullptr;

    // HEX 选项
    QCheckBox* m_hexDisplayCheck = nullptr;
    QCheckBox* m_hexSendCheck = nullptr;

    // 发送选项
    QCheckBox* m_enterSendCheck = nullptr;
    QCheckBox* m_appendNewlineCheck = nullptr;

    // 标签（用于语言切换）
    QLabel* m_timeoutLabel = nullptr;
    QLabel* m_intervalLabel = nullptr;

    SendMode m_sendMode = SendMode::Ascii;
    QString m_appendNewlineSequence = "\r\n";
    QStringList m_history;
    static const int MAX_HISTORY = 20;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SENDWIDGET_H
