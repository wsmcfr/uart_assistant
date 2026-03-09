/**
 * @file MultiGroupSendWidget.cpp
 * @brief 多分组快捷发送组件实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "MultiGroupSendWidget.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QFile>
#include <QTabBar>
#include <QEvent>

namespace ComAssistant {

// ============== GroupPageWidget 实现 ==============

GroupPageWidget::GroupPageWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void GroupPageWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // 顶部控制区
    auto* controlLayout = new QHBoxLayout();

    m_loopCheck = new QCheckBox(tr("循环发送"), this);
    connect(m_loopCheck, &QCheckBox::toggled, this, &GroupPageWidget::onLoopToggled);

    m_loopCountSpin = new QSpinBox(this);
    m_loopCountSpin->setRange(0, 9999);
    m_loopCountSpin->setValue(1);
    m_loopCountSpin->setPrefix(tr("次数: "));
    m_loopCountSpin->setSpecialValueText(tr("无限"));
    m_loopCountSpin->setEnabled(false);

    m_loopIntervalSpin = new QSpinBox(this);
    m_loopIntervalSpin->setRange(10, 60000);
    m_loopIntervalSpin->setValue(1000);
    m_loopIntervalSpin->setSuffix(" ms");
    m_loopIntervalSpin->setEnabled(false);

    m_sendAllButton = new QPushButton(tr("全部发送"), this);
    m_sendAllButton->setToolTip(tr("按顺序发送所有项目"));
    connect(m_sendAllButton, &QPushButton::clicked, this, &GroupPageWidget::onSendAllClicked);

    controlLayout->addWidget(m_loopCheck);
    controlLayout->addWidget(m_loopCountSpin);
    controlLayout->addWidget(new QLabel(tr("间隔:"), this));
    controlLayout->addWidget(m_loopIntervalSpin);
    controlLayout->addStretch();
    controlLayout->addWidget(m_sendAllButton);

    mainLayout->addLayout(controlLayout);

    // 滚动区域
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_buttonContainer = new QWidget();
    m_buttonLayout = new QVBoxLayout(m_buttonContainer);
    m_buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonLayout->setSpacing(2);
    m_buttonLayout->addStretch();

    m_scrollArea->setWidget(m_buttonContainer);
    mainLayout->addWidget(m_scrollArea);

    // 添加按钮
    m_addButton = new QPushButton(tr("+ 添加项目"), this);
    connect(m_addButton, &QPushButton::clicked, this, &GroupPageWidget::onAddClicked);
    mainLayout->addWidget(m_addButton);
}

void GroupPageWidget::setGroup(const SendGroup& group)
{
    m_group = group;
    m_loopCheck->setChecked(group.loopEnabled);
    m_loopCountSpin->setValue(group.loopCount);
    m_loopIntervalSpin->setValue(group.loopIntervalMs);
    rebuildButtons();
}

SendGroup GroupPageWidget::group() const
{
    SendGroup g = m_group;
    g.loopEnabled = m_loopCheck->isChecked();
    g.loopCount = m_loopCountSpin->value();
    g.loopIntervalMs = m_loopIntervalSpin->value();
    return g;
}

void GroupPageWidget::addItem(const SendItem& item)
{
    m_group.items.append(item);
    rebuildButtons();
    emit configChanged();
}

void GroupPageWidget::removeItem(int index)
{
    if (index >= 0 && index < m_group.items.size()) {
        m_group.items.remove(index);
        rebuildButtons();
        emit configChanged();
    }
}

void GroupPageWidget::updateItem(int index, const SendItem& item)
{
    if (index >= 0 && index < m_group.items.size()) {
        m_group.items[index] = item;
        rebuildButtons();
        emit configChanged();
    }
}

void GroupPageWidget::clearItems()
{
    m_group.items.clear();
    rebuildButtons();
    emit configChanged();
}

void GroupPageWidget::rebuildButtons()
{
    // 清除旧的按钮
    for (auto* widget : m_itemWidgets) {
        m_buttonLayout->removeWidget(widget);
        widget->deleteLater();
    }
    m_itemWidgets.clear();

    // 创建新的按钮
    for (int i = 0; i < m_group.items.size(); ++i) {
        const SendItem& item = m_group.items[i];

        auto* itemWidget = new QWidget();
        auto* itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(2, 2, 2, 2);
        itemLayout->setSpacing(5);

        // 发送按钮
        auto* sendBtn = new QPushButton(item.name.isEmpty() ? tr("发送%1").arg(i + 1) : item.name);
        sendBtn->setMinimumWidth(80);
        sendBtn->setToolTip(item.comment.isEmpty() ? item.data : item.comment);
        connect(sendBtn, &QPushButton::clicked, this, [this, i]() {
            onItemSendClicked(i);
        });

        // 数据预览
        auto* dataLabel = new QLabel(item.data);
        dataLabel->setStyleSheet("color: gray;");
        dataLabel->setMaximumWidth(200);
        dataLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        // HEX标记
        if (item.isHex) {
            auto* hexLabel = new QLabel("[HEX]");
            hexLabel->setStyleSheet("color: orange; font-size: 10px;");
            itemLayout->addWidget(hexLabel);
        }

        // 延时标记
        if (item.delayMs > 0) {
            auto* delayLabel = new QLabel(QString("[%1ms]").arg(item.delayMs));
            delayLabel->setStyleSheet("color: blue; font-size: 10px;");
            itemLayout->addWidget(delayLabel);
        }

        itemLayout->addWidget(sendBtn);
        itemLayout->addWidget(dataLabel, 1);

        // 右键菜单
        itemWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(itemWidget, &QWidget::customContextMenuRequested, this, [this, i](const QPoint& pos) {
            onItemContextMenu(i, pos);
        });

        m_buttonLayout->insertWidget(m_buttonLayout->count() - 1, itemWidget);
        m_itemWidgets.append(itemWidget);
    }
}

void GroupPageWidget::onAddClicked()
{
    // 简单的添加对话框
    bool ok;
    QString name = QInputDialog::getText(this, tr("添加项目"), tr("项目名称:"), QLineEdit::Normal, "", &ok);
    if (!ok) return;

    QString data = QInputDialog::getText(this, tr("添加项目"), tr("发送数据:"), QLineEdit::Normal, "", &ok);
    if (!ok || data.isEmpty()) return;

    SendItem item;
    item.name = name.isEmpty() ? tr("项目%1").arg(m_group.items.size() + 1) : name;
    item.data = data;

    addItem(item);
}

void GroupPageWidget::onSendAllClicked()
{
    if (m_group.items.isEmpty()) return;
    emit sendSequenceRequested(m_group.items);
}

void GroupPageWidget::onLoopToggled(bool checked)
{
    m_loopCountSpin->setEnabled(checked);
    m_loopIntervalSpin->setEnabled(checked);
    emit configChanged();
}

void GroupPageWidget::onItemSendClicked(int index)
{
    if (index >= 0 && index < m_group.items.size()) {
        QByteArray data = itemToBytes(m_group.items[index]);
        emit sendRequested(data);
    }
}

void GroupPageWidget::onItemContextMenu(int index, const QPoint& pos)
{
    QMenu menu;

    QAction* editAction = menu.addAction(tr("编辑"));
    QAction* deleteAction = menu.addAction(tr("删除"));
    menu.addSeparator();
    QAction* moveUpAction = menu.addAction(tr("上移"));
    QAction* moveDownAction = menu.addAction(tr("下移"));

    moveUpAction->setEnabled(index > 0);
    moveDownAction->setEnabled(index < m_group.items.size() - 1);

    QAction* selected = menu.exec(m_itemWidgets[index]->mapToGlobal(pos));

    if (selected == editAction) {
        SendItem& item = m_group.items[index];
        bool ok;
        QString name = QInputDialog::getText(this, tr("编辑项目"), tr("项目名称:"),
                                             QLineEdit::Normal, item.name, &ok);
        if (ok) item.name = name;

        QString data = QInputDialog::getText(this, tr("编辑项目"), tr("发送数据:"),
                                             QLineEdit::Normal, item.data, &ok);
        if (ok) item.data = data;

        rebuildButtons();
        emit configChanged();
    } else if (selected == deleteAction) {
        removeItem(index);
    } else if (selected == moveUpAction && index > 0) {
        std::swap(m_group.items[index], m_group.items[index - 1]);
        rebuildButtons();
        emit configChanged();
    } else if (selected == moveDownAction && index < m_group.items.size() - 1) {
        std::swap(m_group.items[index], m_group.items[index + 1]);
        rebuildButtons();
        emit configChanged();
    }
}

QByteArray GroupPageWidget::itemToBytes(const SendItem& item) const
{
    QByteArray data;
    if (item.isHex) {
        data = hexStringToBytes(item.data);
    } else {
        data = item.data.toUtf8();
    }

    if (item.appendNewline) {
        data.append("\r\n");
    }

    return data;
}

QByteArray GroupPageWidget::hexStringToBytes(const QString& hex) const
{
    QString cleanHex = hex;
    cleanHex.remove(' ').remove('\n').remove('\r');

    QByteArray result;
    for (int i = 0; i + 1 < cleanHex.length(); i += 2) {
        bool ok;
        int byte = cleanHex.mid(i, 2).toInt(&ok, 16);
        if (ok) {
            result.append(static_cast<char>(byte));
        }
    }
    return result;
}

// ============== MultiGroupSendWidget 实现 ==============

MultiGroupSendWidget::MultiGroupSendWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    createDefaultGroups();

    m_sendTimer = new QTimer(this);
    connect(m_sendTimer, &QTimer::timeout, this, &MultiGroupSendWidget::processSendQueue);
}

void MultiGroupSendWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // 工具栏
    auto* toolLayout = new QHBoxLayout();
    m_addGroupBtn = new QPushButton(tr("+ 新建分组"), this);
    m_importBtn = new QPushButton(tr("导入"), this);
    m_exportBtn = new QPushButton(tr("导出"), this);

    connect(m_addGroupBtn, &QPushButton::clicked, this, &MultiGroupSendWidget::onAddGroupClicked);
    connect(m_importBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("导入配置"), "",
                                                    tr("JSON文件 (*.json)"));
        if (!path.isEmpty()) importFromFile(path);
    });
    connect(m_exportBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("导出配置"), "quicksend.json",
                                                    tr("JSON文件 (*.json)"));
        if (!path.isEmpty()) exportToFile(path);
    });

    toolLayout->addWidget(m_addGroupBtn);
    toolLayout->addStretch();
    toolLayout->addWidget(m_importBtn);
    toolLayout->addWidget(m_exportBtn);
    mainLayout->addLayout(toolLayout);

    // 标签页
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (m_groupPages.size() > 1) {
            removeGroup(index);
        } else {
            QMessageBox::warning(this, tr("警告"), tr("至少保留一个分组"));
        }
    });
    connect(m_tabWidget, &QTabWidget::customContextMenuRequested,
            this, &MultiGroupSendWidget::onTabContextMenu);

    mainLayout->addWidget(m_tabWidget);
}

void MultiGroupSendWidget::createDefaultGroups()
{
    addGroup(tr("常用指令"));
    addGroup(tr("AT指令"));
    addGroup(tr("调试"));
    addGroup(tr("自定义"));
}

int MultiGroupSendWidget::addGroup(const QString& name)
{
    auto* page = new GroupPageWidget(this);
    SendGroup group;
    group.name = name;
    page->setGroup(group);

    connect(page, &GroupPageWidget::sendRequested, this, &MultiGroupSendWidget::onGroupSendRequested);
    connect(page, &GroupPageWidget::sendSequenceRequested, this, &MultiGroupSendWidget::onSendSequence);
    connect(page, &GroupPageWidget::configChanged, this, &MultiGroupSendWidget::configChanged);

    m_groupPages.append(page);
    int index = m_tabWidget->addTab(page, name);

    emit configChanged();
    return index;
}

void MultiGroupSendWidget::removeGroup(int index)
{
    if (index >= 0 && index < m_groupPages.size()) {
        m_tabWidget->removeTab(index);
        m_groupPages[index]->deleteLater();
        m_groupPages.remove(index);
        emit configChanged();
    }
}

void MultiGroupSendWidget::renameGroup(int index, const QString& name)
{
    if (index >= 0 && index < m_groupPages.size()) {
        m_tabWidget->setTabText(index, name);
        SendGroup g = m_groupPages[index]->group();
        g.name = name;
        m_groupPages[index]->setGroup(g);
        emit configChanged();
    }
}

int MultiGroupSendWidget::groupCount() const
{
    return m_groupPages.size();
}

int MultiGroupSendWidget::currentGroupIndex() const
{
    return m_tabWidget->currentIndex();
}

void MultiGroupSendWidget::setCurrentGroup(int index)
{
    m_tabWidget->setCurrentIndex(index);
}

QVector<SendGroup> MultiGroupSendWidget::groups() const
{
    QVector<SendGroup> result;
    for (auto* page : m_groupPages) {
        result.append(page->group());
    }
    return result;
}

void MultiGroupSendWidget::loadFromJson(const QJsonArray& array)
{
    // 清除现有分组
    while (!m_groupPages.isEmpty()) {
        removeGroup(0);
    }

    // 加载新分组
    for (const auto& val : array) {
        SendGroup group = SendGroup::fromJson(val.toObject());
        int idx = addGroup(group.name);
        m_groupPages[idx]->setGroup(group);
    }

    // 如果没有分组，创建默认的
    if (m_groupPages.isEmpty()) {
        createDefaultGroups();
    }
}

QJsonArray MultiGroupSendWidget::saveToJson() const
{
    QJsonArray array;
    for (auto* page : m_groupPages) {
        array.append(page->group().toJson());
    }
    return array;
}

bool MultiGroupSendWidget::importFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("错误"), tr("无法打开文件"));
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        QMessageBox::warning(this, tr("错误"), tr("无效的配置文件格式"));
        return false;
    }

    loadFromJson(doc.array());
    return true;
}

bool MultiGroupSendWidget::exportToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(const_cast<MultiGroupSendWidget*>(this),
                           tr("错误"), tr("无法创建文件"));
        return false;
    }

    QJsonDocument doc(saveToJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

void MultiGroupSendWidget::onTabContextMenu(const QPoint& pos)
{
    int index = m_tabWidget->tabBar()->tabAt(pos);
    if (index < 0) return;

    QMenu menu;
    QAction* renameAction = menu.addAction(tr("重命名"));
    QAction* deleteAction = menu.addAction(tr("删除"));
    deleteAction->setEnabled(m_groupPages.size() > 1);

    QAction* selected = menu.exec(m_tabWidget->tabBar()->mapToGlobal(pos));

    if (selected == renameAction) {
        bool ok;
        QString name = QInputDialog::getText(this, tr("重命名分组"), tr("分组名称:"),
                                            QLineEdit::Normal, m_tabWidget->tabText(index), &ok);
        if (ok && !name.isEmpty()) {
            renameGroup(index, name);
        }
    } else if (selected == deleteAction) {
        removeGroup(index);
    }
}

void MultiGroupSendWidget::onAddGroupClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("新建分组"), tr("分组名称:"),
                                        QLineEdit::Normal, tr("新分组"), &ok);
    if (ok && !name.isEmpty()) {
        int idx = addGroup(name);
        m_tabWidget->setCurrentIndex(idx);
    }
}

void MultiGroupSendWidget::onGroupSendRequested(const QByteArray& data)
{
    emit sendRequested(data);
}

void MultiGroupSendWidget::onSendSequence(const QVector<SendItem>& items)
{
    m_sendQueue = items;
    m_sendQueueIndex = 0;

    GroupPageWidget* page = qobject_cast<GroupPageWidget*>(sender());
    if (page) {
        SendGroup g = page->group();
        m_loopRemaining = g.loopEnabled ? (g.loopCount == 0 ? -1 : g.loopCount) : 1;
    } else {
        m_loopRemaining = 1;
    }

    processSendQueue();
}

void MultiGroupSendWidget::processSendQueue()
{
    if (m_sendQueueIndex >= m_sendQueue.size()) {
        // 一轮发送完成
        if (m_loopRemaining > 0) m_loopRemaining--;

        if (m_loopRemaining != 0) {
            // 继续下一轮
            m_sendQueueIndex = 0;
            int currentIdx = currentGroupIndex();
            if (currentIdx >= 0 && currentIdx < m_groupPages.size()) {
                int interval = m_groupPages[currentIdx]->group().loopIntervalMs;
                m_sendTimer->start(interval);
            }
        }
        return;
    }

    const SendItem& item = m_sendQueue[m_sendQueueIndex];
    QByteArray data = itemToBytes(item);
    emit sendRequested(data);

    m_sendQueueIndex++;

    if (m_sendQueueIndex < m_sendQueue.size()) {
        int delay = item.delayMs > 0 ? item.delayMs : 50;
        m_sendTimer->start(delay);
    } else if (m_loopRemaining != 0 && m_loopRemaining != 1) {
        // 等待下一轮
        int currentIdx = currentGroupIndex();
        if (currentIdx >= 0 && currentIdx < m_groupPages.size()) {
            int interval = m_groupPages[currentIdx]->group().loopIntervalMs;
            m_sendTimer->start(interval);
        }
    }
}

QByteArray MultiGroupSendWidget::itemToBytes(const SendItem& item) const
{
    QByteArray data;
    if (item.isHex) {
        QString cleanHex = item.data;
        cleanHex.remove(' ').remove('\n').remove('\r');
        for (int i = 0; i + 1 < cleanHex.length(); i += 2) {
            bool ok;
            int byte = cleanHex.mid(i, 2).toInt(&ok, 16);
            if (ok) data.append(static_cast<char>(byte));
        }
    } else {
        data = item.data.toUtf8();
    }

    if (item.appendNewline) {
        data.append("\r\n");
    }

    return data;
}

void MultiGroupSendWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void MultiGroupSendWidget::retranslateUi()
{
    m_addGroupBtn->setText(tr("+ 新建分组"));
    m_importBtn->setText(tr("导入"));
    m_exportBtn->setText(tr("导出"));
}

} // namespace ComAssistant
