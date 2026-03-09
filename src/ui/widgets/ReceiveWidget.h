/**
 * @file ReceiveWidget.h
 * @brief 接收区组件
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_RECEIVEWIDGET_H
#define COMASSISTANT_RECEIVEWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QByteArray>
#include <QGroupBox>
#include <QLabel>
#include <QEvent>

namespace ComAssistant {

/**
 * @brief 数据显示格式
 */
enum class DataDisplayFormat {
    Ascii,      ///< ASCII文本
    Hex,        ///< 十六进制
    Mixed       ///< 混合模式
};

/**
 * @brief 接收区组件
 */
class ReceiveWidget : public QWidget {
    Q_OBJECT

public:
    explicit ReceiveWidget(QWidget* parent = nullptr);
    ~ReceiveWidget() override = default;

    /**
     * @brief 追加数据
     */
    void appendData(const QByteArray& data);

    /**
     * @brief 清空显示
     */
    void clear();

    /**
     * @brief 设置显示模式
     */
    void setDataDisplayFormat(DataDisplayFormat format);
    DataDisplayFormat dataDisplayFormat() const { return m_displayFormat; }

    /**
     * @brief 导出到文件
     */
    bool exportToFile(const QString& filePath);

    /**
     * @brief 获取原始数据
     */
    QByteArray rawData() const { return m_rawData; }

private slots:
    void onModeChanged(int index);
    void onClearClicked();
    void onExportClicked();
    void onTimestampToggled(bool checked);
    void onAutoScrollToggled(bool checked);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    QString formatData(const QByteArray& data) const;
    QString formatHex(const QByteArray& data) const;
    QString formatMixed(const QByteArray& data) const;
    QString timestamp() const;
    void refreshDisplay();
    void retranslateUi();

private:
    QGroupBox* m_receiveGroup = nullptr;
    QLabel* m_modeLabel = nullptr;
    QTextEdit* m_displayEdit = nullptr;
    QComboBox* m_modeCombo = nullptr;
    QCheckBox* m_timestampCheck = nullptr;
    QCheckBox* m_autoScrollCheck = nullptr;
    QCheckBox* m_wordWrapCheck = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;

    DataDisplayFormat m_displayFormat = DataDisplayFormat::Ascii;
    QByteArray m_rawData;
    bool m_showTimestamp = false;
    bool m_autoScroll = true;

    static const int MAX_DISPLAY_SIZE = 1024 * 1024;  // 1MB
};

} // namespace ComAssistant

#endif // COMASSISTANT_RECEIVEWIDGET_H
