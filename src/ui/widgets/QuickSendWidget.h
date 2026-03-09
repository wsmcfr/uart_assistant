/**
 * @file QuickSendWidget.h
 * @brief 快捷发送组件
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_QUICKSENDWIDGET_H
#define COMASSISTANT_QUICKSENDWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>

namespace ComAssistant {

/**
 * @brief 快捷发送项
 */
struct QuickSendItem {
    QString name;           ///< 显示名称
    QString data;           ///< 发送数据
    bool isHex = false;     ///< 是否为HEX格式
    bool appendNewline = false;  ///< 是否追加换行

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["data"] = data;
        obj["isHex"] = isHex;
        obj["appendNewline"] = appendNewline;
        return obj;
    }

    static QuickSendItem fromJson(const QJsonObject& obj) {
        QuickSendItem item;
        item.name = obj["name"].toString();
        item.data = obj["data"].toString();
        item.isHex = obj["isHex"].toBool();
        item.appendNewline = obj["appendNewline"].toBool();
        return item;
    }
};

/**
 * @brief 快捷发送组件
 *
 * 提供预设指令的快速发送功能，支持多个指令按钮
 */
class QuickSendWidget : public QWidget {
    Q_OBJECT

public:
    explicit QuickSendWidget(QWidget* parent = nullptr);
    ~QuickSendWidget() override = default;

    /**
     * @brief 添加快捷发送项
     */
    void addItem(const QuickSendItem& item);

    /**
     * @brief 移除快捷发送项
     */
    void removeItem(int index);

    /**
     * @brief 更新快捷发送项
     */
    void updateItem(int index, const QuickSendItem& item);

    /**
     * @brief 获取所有项
     */
    QVector<QuickSendItem> items() const { return m_items; }

    /**
     * @brief 清空所有项
     */
    void clearAll();

    /**
     * @brief 加载配置
     */
    void loadFromJson(const QJsonArray& array);

    /**
     * @brief 保存配置
     */
    QJsonArray saveToJson() const;

    /**
     * @brief 设置最大显示行数
     */
    void setMaxVisibleRows(int rows);

signals:
    /**
     * @brief 请求发送数据
     */
    void sendRequested(const QByteArray& data);

    /**
     * @brief 配置已更改
     */
    void configChanged();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onAddClicked();
    void onItemSendClicked(int index);
    void onItemContextMenu(int index, const QPoint& pos);
    void onEditItem(int index);
    void onDeleteItem(int index);
    void onMoveUp(int index);
    void onMoveDown(int index);

private:
    void setupUi();
    void rebuildButtons();
    void retranslateUi();
    void localizeBuiltInItemNames();
    QString localizedDefaultNameForData(const QuickSendItem& item) const;
    QByteArray itemToBytes(const QuickSendItem& item) const;
    QByteArray hexStringToBytes(const QString& hex) const;

private:
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_buttonContainer = nullptr;
    QVBoxLayout* m_buttonLayout = nullptr;
    QPushButton* m_addButton = nullptr;
    QLabel* m_titleLabel = nullptr;

    QVector<QuickSendItem> m_items;
    QVector<QPushButton*> m_buttons;

    int m_maxVisibleRows = 8;
};

} // namespace ComAssistant

#endif // COMASSISTANT_QUICKSENDWIDGET_H
