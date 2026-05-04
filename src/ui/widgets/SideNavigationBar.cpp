/**
 * @file SideNavigationBar.cpp
 * @brief 侧边图标导航栏实现
 * @author ComAssistant Team
 * @date 2026-01-28
 */

#include "SideNavigationBar.h"
#include <QStyle>
#include <QVariant>

namespace ComAssistant {

SideNavigationBar::SideNavigationBar(QWidget* parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_buttonGroup(nullptr)
    , m_themeBtn(nullptr)
    , m_helpBtn(nullptr)
    , m_currentItem(NavItem::Connection)
{
    setupUI();
}

void SideNavigationBar::setupUI()
{
    setObjectName("sideNavBar");
    setFixedWidth(56);
    setMinimumHeight(400);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(6, 10, 6, 10);
    m_layout->setSpacing(6);

    m_buttonGroup = new QButtonGroup(this);
    m_buttonGroup->setExclusive(true);

    // 导航按钮
    auto* connBtn = createNavButton(QString::fromUtf8("\xF0\x9F\x94\x8C"), tr("连接"), NavItem::Connection);
    auto* dataBtn = createNavButton(QString::fromUtf8("\xF0\x9F\x93\x84"), tr("数据"), NavItem::Data);
    auto* plotBtn = createNavButton(QString::fromUtf8("\xF0\x9F\x93\x88"), tr("绘图"), NavItem::Plotter);
    auto* toolBtn = createNavButton(QString::fromUtf8("\xF0\x9F\x94\xA7"), tr("工具"), NavItem::Tools);
    auto* setBtn = createNavButton(QString::fromUtf8("\xE2\x9A\x99"), tr("设置"), NavItem::Settings);

    m_layout->addWidget(connBtn);
    m_layout->addWidget(dataBtn);
    m_layout->addWidget(plotBtn);
    m_layout->addWidget(toolBtn);
    m_layout->addWidget(setBtn);

    // 弹性空间
    m_layout->addStretch();

    // 底部功能按钮
    m_themeBtn = createActionButton(QString::fromUtf8("\xF0\x9F\x8C\x99"), tr("切换主题"));
    m_helpBtn = createActionButton(QString::fromUtf8("\xE2\x9D\x93"), tr("帮助"));

    m_layout->addWidget(m_themeBtn);
    m_layout->addWidget(m_helpBtn);

    // 信号连接
    connect(m_buttonGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int id) {
        m_currentItem = static_cast<NavItem>(id);
        emit itemClicked(m_currentItem);
    });

    connect(m_themeBtn, &QPushButton::clicked, this, &SideNavigationBar::themeToggleRequested);
    connect(m_helpBtn, &QPushButton::clicked, this, &SideNavigationBar::helpRequested);

    // 默认选中连接
    connBtn->setChecked(true);
}

void SideNavigationBar::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SideNavigationBar::retranslateUi()
{
    if (auto* btn = m_buttonGroup->button(static_cast<int>(NavItem::Connection))) {
        btn->setToolTip(tr("连接"));
    }
    if (auto* btn = m_buttonGroup->button(static_cast<int>(NavItem::Data))) {
        btn->setToolTip(tr("数据"));
    }
    if (auto* btn = m_buttonGroup->button(static_cast<int>(NavItem::Plotter))) {
        btn->setToolTip(tr("绘图"));
    }
    if (auto* btn = m_buttonGroup->button(static_cast<int>(NavItem::Tools))) {
        btn->setToolTip(tr("工具"));
    }
    if (auto* btn = m_buttonGroup->button(static_cast<int>(NavItem::Settings))) {
        btn->setToolTip(tr("设置"));
    }

    if (m_themeBtn) {
        m_themeBtn->setToolTip(tr("切换主题"));
    }
    if (m_helpBtn) {
        m_helpBtn->setToolTip(tr("帮助"));
    }
}

QPushButton* SideNavigationBar::createNavButton(const QString& icon, const QString& tooltip, NavItem item)
{
    auto* btn = new QPushButton(icon, this);
    btn->setObjectName("navButton");
    btn->setFixedSize(42, 42);
    btn->setCheckable(true);
    btn->setToolTip(tooltip);
    btn->setProperty("navItem", QVariant(static_cast<int>(item)));

    m_buttonGroup->addButton(btn, static_cast<int>(item));
    return btn;
}

QPushButton* SideNavigationBar::createActionButton(const QString& icon, const QString& tooltip)
{
    auto* btn = new QPushButton(icon, this);
    btn->setObjectName("navActionButton");
    btn->setFixedSize(42, 42);
    btn->setToolTip(tooltip);
    return btn;
}

void SideNavigationBar::setCurrentItem(NavItem item)
{
    m_currentItem = item;
    if (auto* btn = m_buttonGroup->button(static_cast<int>(item))) {
        btn->setChecked(true);
    }
}

NavItem SideNavigationBar::currentItem() const
{
    return m_currentItem;
}

} // namespace ComAssistant
