/**
 * @file SideNavigationBar.h
 * @brief 侧边图标导航栏
 * @author ComAssistant Team
 * @date 2026-01-28
 */

#ifndef SIDENAVIGATIONBAR_H
#define SIDENAVIGATIONBAR_H

#include <QWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QEvent>

namespace ComAssistant {

/**
 * @brief 导航项类型枚举
 */
enum class NavItem {
    Connection = 0,  // 连接设置
    Data,            // 数据显示
    Plotter,         // 绘图窗口
    Tools,           // 工具箱
    Settings         // 设置
};

/**
 * @brief 侧边图标导航栏
 *
 * 提供垂直的图标导航，类似VOFA+的设计风格
 */
class SideNavigationBar : public QWidget
{
    Q_OBJECT

public:
    explicit SideNavigationBar(QWidget* parent = nullptr);
    ~SideNavigationBar() override = default;

    /**
     * @brief 设置当前选中的导航项
     */
    void setCurrentItem(NavItem item);

    /**
     * @brief 获取当前选中的导航项
     */
    NavItem currentItem() const;

signals:
    /**
     * @brief 导航项被点击时发出
     */
    void itemClicked(NavItem item);

    /**
     * @brief 主题切换请求
     */
    void themeToggleRequested();

    /**
     * @brief 帮助请求
     */
    void helpRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUI();
    void retranslateUi();
    QPushButton* createNavButton(const QString& icon, const QString& tooltip, NavItem item);
    QPushButton* createActionButton(const QString& icon, const QString& tooltip);

    QVBoxLayout* m_layout;
    QButtonGroup* m_buttonGroup;
    QPushButton* m_themeBtn;
    QPushButton* m_helpBtn;
    NavItem m_currentItem;
};

} // namespace ComAssistant

#endif // SIDENAVIGATIONBAR_H
