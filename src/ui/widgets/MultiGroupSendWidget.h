/**
 * @file MultiGroupSendWidget.h
 * @brief 多分组快捷发送组件
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_MULTIGROUPSENDWIDGET_H
#define COMASSISTANT_MULTIGROUPSENDWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>

namespace ComAssistant {

/**
 * @brief 快捷发送项
 */
struct SendItem {
    QString name;               ///< 显示名称
    QString data;               ///< 发送数据
    QString comment;            ///< 备注说明
    bool isHex = false;         ///< 是否为HEX格式
    bool appendNewline = false; ///< 是否追加换行
    int delayMs = 0;            ///< 发送后延时（毫秒）

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["data"] = data;
        obj["comment"] = comment;
        obj["isHex"] = isHex;
        obj["appendNewline"] = appendNewline;
        obj["delayMs"] = delayMs;
        return obj;
    }

    static SendItem fromJson(const QJsonObject& obj) {
        SendItem item;
        item.name = obj["name"].toString();
        item.data = obj["data"].toString();
        item.comment = obj["comment"].toString();
        item.isHex = obj["isHex"].toBool();
        item.appendNewline = obj["appendNewline"].toBool();
        item.delayMs = obj["delayMs"].toInt();
        return item;
    }
};

/**
 * @brief 发送分组
 */
struct SendGroup {
    QString name;               ///< 分组名称
    QVector<SendItem> items;    ///< 分组内的项目
    bool loopEnabled = false;   ///< 是否循环发送
    int loopCount = 1;          ///< 循环次数（0表示无限）
    int loopIntervalMs = 1000;  ///< 循环间隔（毫秒）

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["name"] = name;
        obj["loopEnabled"] = loopEnabled;
        obj["loopCount"] = loopCount;
        obj["loopIntervalMs"] = loopIntervalMs;

        QJsonArray itemsArray;
        for (const auto& item : items) {
            itemsArray.append(item.toJson());
        }
        obj["items"] = itemsArray;
        return obj;
    }

    static SendGroup fromJson(const QJsonObject& obj) {
        SendGroup group;
        group.name = obj["name"].toString();
        group.loopEnabled = obj["loopEnabled"].toBool();
        group.loopCount = obj["loopCount"].toInt(1);
        group.loopIntervalMs = obj["loopIntervalMs"].toInt(1000);

        QJsonArray itemsArray = obj["items"].toArray();
        for (const auto& itemVal : itemsArray) {
            group.items.append(SendItem::fromJson(itemVal.toObject()));
        }
        return group;
    }
};

/**
 * @brief 分组页面组件
 */
class GroupPageWidget : public QWidget {
    Q_OBJECT

public:
    explicit GroupPageWidget(QWidget* parent = nullptr);

    void setGroup(const SendGroup& group);
    SendGroup group() const;

    void addItem(const SendItem& item);
    void removeItem(int index);
    void updateItem(int index, const SendItem& item);
    void clearItems();

signals:
    void sendRequested(const QByteArray& data);
    void sendSequenceRequested(const QVector<SendItem>& items);
    void configChanged();

private slots:
    void onAddClicked();
    void onSendAllClicked();
    void onLoopToggled(bool checked);
    void onItemSendClicked(int index);
    void onItemContextMenu(int index, const QPoint& pos);

private:
    void setupUi();
    void rebuildButtons();
    QByteArray itemToBytes(const SendItem& item) const;
    QByteArray hexStringToBytes(const QString& hex) const;

private:
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_buttonContainer = nullptr;
    QVBoxLayout* m_buttonLayout = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_sendAllButton = nullptr;
    QCheckBox* m_loopCheck = nullptr;
    QSpinBox* m_loopCountSpin = nullptr;
    QSpinBox* m_loopIntervalSpin = nullptr;

    SendGroup m_group;
    QVector<QWidget*> m_itemWidgets;
};

/**
 * @brief 多分组快捷发送组件
 */
class MultiGroupSendWidget : public QWidget {
    Q_OBJECT

public:
    explicit MultiGroupSendWidget(QWidget* parent = nullptr);
    ~MultiGroupSendWidget() override = default;

    /**
     * @brief 添加分组
     */
    int addGroup(const QString& name);

    /**
     * @brief 移除分组
     */
    void removeGroup(int index);

    /**
     * @brief 重命名分组
     */
    void renameGroup(int index, const QString& name);

    /**
     * @brief 获取分组数量
     */
    int groupCount() const;

    /**
     * @brief 获取当前分组索引
     */
    int currentGroupIndex() const;

    /**
     * @brief 设置当前分组
     */
    void setCurrentGroup(int index);

    /**
     * @brief 获取所有分组
     */
    QVector<SendGroup> groups() const;

    /**
     * @brief 加载配置
     */
    void loadFromJson(const QJsonArray& array);

    /**
     * @brief 保存配置
     */
    QJsonArray saveToJson() const;

    /**
     * @brief 导入配置文件
     */
    bool importFromFile(const QString& filePath);

    /**
     * @brief 导出配置文件
     */
    bool exportToFile(const QString& filePath) const;

signals:
    void sendRequested(const QByteArray& data);
    void configChanged();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onTabContextMenu(const QPoint& pos);
    void onAddGroupClicked();
    void onGroupSendRequested(const QByteArray& data);
    void onSendSequence(const QVector<SendItem>& items);
    void processSendQueue();

private:
    void setupUi();
    void retranslateUi();
    void createDefaultGroups();
    QByteArray itemToBytes(const SendItem& item) const;

private:
    QTabWidget* m_tabWidget = nullptr;
    QPushButton* m_addGroupBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;

    QVector<GroupPageWidget*> m_groupPages;

    // 序列发送队列
    QVector<SendItem> m_sendQueue;
    int m_sendQueueIndex = 0;
    QTimer* m_sendTimer = nullptr;
    int m_loopRemaining = 0;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MULTIGROUPSENDWIDGET_H
