/**
 * @file DataWindow.h
 * @brief 数据分窗窗口
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef COMASSISTANT_DATAWINDOW_H
#define COMASSISTANT_DATAWINDOW_H

#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QScrollBar>

namespace ComAssistant {

/**
 * @brief 数据分窗窗口
 *
 * 独立的数据显示窗口，支持智能滚屏、时间戳、清空等功能
 */
class DataWindow : public QWidget
{
    Q_OBJECT

public:
    explicit DataWindow(const QString& name, QWidget* parent = nullptr);
    ~DataWindow() override = default;

    /**
     * @brief 获取窗口名称
     * @return 窗口名称
     */
    QString windowName() const { return m_name; }

    /**
     * @brief 追加数据
     * @param data 数据文本
     */
    void appendData(const QString& data);

    /**
     * @brief 清空数据
     */
    void clear();

    /**
     * @brief 设置时间戳启用
     * @param enabled 是否启用
     */
    void setTimestampEnabled(bool enabled);

    /**
     * @brief 获取时间戳是否启用
     * @return 是否启用
     */
    bool isTimestampEnabled() const { return m_timestampEnabled; }

    /**
     * @brief 设置自动滚动
     * @param enabled 是否启用
     */
    void setAutoScrollEnabled(bool enabled);

    /**
     * @brief 获取自动滚动是否启用
     * @return 是否启用
     */
    bool isAutoScrollEnabled() const { return m_autoScrollEnabled; }

    /**
     * @brief 获取数据行数
     * @return 行数
     */
    int lineCount() const { return m_lineCount; }

    /**
     * @brief 导出数据到文件
     * @param fileName 文件名
     * @return 是否成功
     */
    bool exportToFile(const QString& fileName);

signals:
    /**
     * @brief 窗口关闭信号
     * @param name 窗口名称
     */
    void windowClosed(const QString& name);

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onClearClicked();
    void onExportClicked();
    void onTimestampToggled(bool checked);
    void onAutoScrollToggled(bool checked);
    void onScrollBarValueChanged(int value);

private:
    void setupUi();
    void retranslateUi();
    void showSmartScrollIndicator(const QString& message);
    void hideSmartScrollIndicator();

    QString m_name;
    QTextEdit* m_textEdit = nullptr;
    QCheckBox* m_timestampCheck = nullptr;
    QCheckBox* m_autoScrollCheck = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QLabel* m_statsLabel = nullptr;
    QLabel* m_smartScrollIndicator = nullptr;

    bool m_timestampEnabled = false;
    bool m_autoScrollEnabled = true;
    bool m_smartScrollPaused = false;
    bool m_needTimestamp = true;
    int m_lineCount = 0;
};

} // namespace ComAssistant

#endif // COMASSISTANT_DATAWINDOW_H
