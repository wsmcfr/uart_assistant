/**
 * @file ControlPanel.h
 * @brief 控件面板容器
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef COMASSISTANT_CONTROLPANEL_H
#define COMASSISTANT_CONTROLPANEL_H

#include <QWidget>
#include <QScrollArea>
#include <QVector>
#include <QJsonArray>
#include "IInteractiveControl.h"

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QComboBox;
class QMenu;

namespace ComAssistant {

/**
 * @brief 控件面板
 *
 * 用于管理和显示交互式控件
 */
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget* parent = nullptr);
    ~ControlPanel() override;

    /**
     * @brief 添加控件
     * @param control 控件指针
     */
    void addControl(IInteractiveControl* control);

    /**
     * @brief 移除控件
     * @param control 控件指针
     */
    void removeControl(IInteractiveControl* control);

    /**
     * @brief 移除所有控件
     */
    void clearControls();

    /**
     * @brief 获取所有控件
     * @return 控件列表
     */
    QVector<IInteractiveControl*> controls() const { return m_controls; }

    /**
     * @brief 获取控件数量
     * @return 控件数量
     */
    int controlCount() const { return m_controls.size(); }

    /**
     * @brief 保存到JSON
     * @return JSON数组
     */
    QJsonArray toJson() const;

    /**
     * @brief 从JSON加载
     * @param array JSON数组
     * @return 是否成功
     */
    bool fromJson(const QJsonArray& array);

    /**
     * @brief 保存配置到文件
     * @param fileName 文件名
     * @return 是否成功
     */
    bool saveToFile(const QString& fileName) const;

    /**
     * @brief 从文件加载配置
     * @param fileName 文件名
     * @return 是否成功
     */
    bool loadFromFile(const QString& fileName);

signals:
    /**
     * @brief 请求发送数据信号
     * @param data 数据
     */
    void sendDataRequested(const QByteArray& data);

    /**
     * @brief 控件添加信号
     * @param control 控件指针
     */
    void controlAdded(IInteractiveControl* control);

    /**
     * @brief 控件移除信号
     * @param control 控件指针
     */
    void controlRemoved(IInteractiveControl* control);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onAddControlClicked();
    void onClearAllClicked();
    void onSaveClicked();
    void onLoadClicked();
    void onControlSendRequested(const QByteArray& data);
    void onControlDeleteRequested();  ///< 处理控件删除请求

private:
    void setupUi();
    void retranslateUi();
    IInteractiveControl* createControl(ControlType type);

    QVBoxLayout* m_controlsLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QComboBox* m_controlTypeCombo = nullptr;

    QVector<IInteractiveControl*> m_controls;
};

} // namespace ComAssistant

#endif // COMASSISTANT_CONTROLPANEL_H
