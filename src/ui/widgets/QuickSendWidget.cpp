/**
 * @file QuickSendWidget.cpp
 * @brief 快捷发送组件实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "QuickSendWidget.h"
#include "core/utils/Logger.h"

#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QJsonObject>
#include <QRegularExpression>
#include <QEvent>
#include <QStringList>

namespace ComAssistant {

QuickSendWidget::QuickSendWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    // 添加一些默认示例
    addItem({"AT", "AT", false, true});
    addItem({"AT+RST", "AT+RST", false, true});
    addItem({tr("查询版本"), "AT+GMR", false, true});
}

void QuickSendWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 标题栏
    QHBoxLayout* headerLayout = new QHBoxLayout;
    m_titleLabel = new QLabel(tr("快捷发送"));
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    headerLayout->addWidget(m_titleLabel);
    headerLayout->addStretch();

    m_addButton = new QPushButton(tr("+"));
    m_addButton->setFixedSize(24, 24);
    m_addButton->setToolTip(tr("添加快捷指令"));
    connect(m_addButton, &QPushButton::clicked, this, &QuickSendWidget::onAddClicked);
    headerLayout->addWidget(m_addButton);

    mainLayout->addLayout(headerLayout);

    // 滚动区域
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_buttonContainer = new QWidget;
    m_buttonLayout = new QVBoxLayout(m_buttonContainer);
    m_buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonLayout->setSpacing(3);
    m_buttonLayout->addStretch();

    m_scrollArea->setWidget(m_buttonContainer);
    mainLayout->addWidget(m_scrollArea);

    setMinimumWidth(150);
}

void QuickSendWidget::addItem(const QuickSendItem& item)
{
    m_items.append(item);
    rebuildButtons();
    emit configChanged();
    LOG_INFO(QString("Quick send item added: %1").arg(item.name));
}

void QuickSendWidget::removeItem(int index)
{
    if (index >= 0 && index < m_items.size()) {
        QString name = m_items[index].name;
        m_items.removeAt(index);
        rebuildButtons();
        emit configChanged();
        LOG_INFO(QString("Quick send item removed: %1").arg(name));
    }
}

void QuickSendWidget::updateItem(int index, const QuickSendItem& item)
{
    if (index >= 0 && index < m_items.size()) {
        m_items[index] = item;
        rebuildButtons();
        emit configChanged();
    }
}

void QuickSendWidget::clearAll()
{
    m_items.clear();
    rebuildButtons();
    emit configChanged();
}

void QuickSendWidget::rebuildButtons()
{
    // 清除旧按钮
    for (QPushButton* btn : m_buttons) {
        m_buttonLayout->removeWidget(btn);
        btn->deleteLater();
    }
    m_buttons.clear();

    // 创建新按钮
    for (int i = 0; i < m_items.size(); ++i) {
        const QuickSendItem& item = m_items[i];

        QPushButton* btn = new QPushButton(item.name);
        btn->setToolTip(QString("%1\n%2: %3")
            .arg(item.name)
            .arg(item.isHex ? "HEX" : "ASCII")
            .arg(item.data));
        btn->setMinimumHeight(28);
        btn->setContextMenuPolicy(Qt::CustomContextMenu);

        // 如果是HEX模式，显示不同样式
        if (item.isHex) {
            btn->setStyleSheet("QPushButton { color: #e67e22; }");
        }

        // 点击发送
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            onItemSendClicked(i);
        });

        // 右键菜单
        connect(btn, &QPushButton::customContextMenuRequested, this, [this, i](const QPoint& pos) {
            onItemContextMenu(i, m_buttons[i]->mapToGlobal(pos));
        });

        m_buttonLayout->insertWidget(m_buttonLayout->count() - 1, btn);
        m_buttons.append(btn);
    }
}

void QuickSendWidget::localizeBuiltInItemNames()
{
    for (QuickSendItem& item : m_items) {
        const QString localizedName = localizedDefaultNameForData(item);
        if (!localizedName.isEmpty() && item.name != localizedName) {
            item.name = localizedName;
        }
    }
}

QString QuickSendWidget::localizedDefaultNameForData(const QuickSendItem& item) const
{
    const QString trimmedData = item.data.trimmed().toUpper();
    if (trimmedData != "AT+GMR") {
        return {};
    }

    static const QStringList kDefaultNames = {
        QStringLiteral("查询版本"),
        QStringLiteral("Query Version")
    };
    if (!kDefaultNames.contains(item.name)) {
        return {};
    }

    return tr("查询版本");
}

void QuickSendWidget::onAddClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("添加快捷指令"));
    dialog.setMinimumWidth(350);
    QFormLayout* layout = new QFormLayout(&dialog);

    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setPlaceholderText(tr("例如: 查询版本"));
    layout->addRow(tr("名称:"), nameEdit);

    QLineEdit* dataEdit = new QLineEdit;
    dataEdit->setPlaceholderText(tr("例如: AT+GMR 或 AA BB CC (HEX)"));
    layout->addRow(tr("数据:"), dataEdit);

    QCheckBox* hexCheck = new QCheckBox(tr("HEX格式"));
    layout->addRow(tr("格式:"), hexCheck);

    QCheckBox* newlineCheck = new QCheckBox(tr("追加换行 (\\r\\n)"));
    newlineCheck->setChecked(true);
    layout->addRow(tr("选项:"), newlineCheck);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString name = nameEdit->text().trimmed();
        QString data = dataEdit->text().trimmed();

        if (name.isEmpty()) {
            name = data.left(20);
        }
        if (data.isEmpty()) {
            QMessageBox::warning(this, tr("错误"), tr("数据不能为空"));
            return;
        }

        QuickSendItem item;
        item.name = name;
        item.data = data;
        item.isHex = hexCheck->isChecked();
        item.appendNewline = newlineCheck->isChecked();
        addItem(item);
    }
}

void QuickSendWidget::onItemSendClicked(int index)
{
    if (index >= 0 && index < m_items.size()) {
        QByteArray data = itemToBytes(m_items[index]);
        if (!data.isEmpty()) {
            emit sendRequested(data);
            LOG_INFO(QString("Quick send: %1 (%2 bytes)")
                .arg(m_items[index].name).arg(data.size()));
        }
    }
}

void QuickSendWidget::onItemContextMenu(int index, const QPoint& pos)
{
    QMenu menu(this);

    QAction* editAction = menu.addAction(tr("编辑(&E)"));
    connect(editAction, &QAction::triggered, this, [this, index]() {
        onEditItem(index);
    });

    menu.addSeparator();

    QAction* moveUpAction = menu.addAction(tr("上移(&U)"));
    moveUpAction->setEnabled(index > 0);
    connect(moveUpAction, &QAction::triggered, this, [this, index]() {
        onMoveUp(index);
    });

    QAction* moveDownAction = menu.addAction(tr("下移(&D)"));
    moveDownAction->setEnabled(index < m_items.size() - 1);
    connect(moveDownAction, &QAction::triggered, this, [this, index]() {
        onMoveDown(index);
    });

    menu.addSeparator();

    QAction* deleteAction = menu.addAction(tr("删除(&D)"));
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    connect(deleteAction, &QAction::triggered, this, [this, index]() {
        onDeleteItem(index);
    });

    menu.exec(pos);
}

void QuickSendWidget::onEditItem(int index)
{
    if (index < 0 || index >= m_items.size()) return;

    QuickSendItem& item = m_items[index];

    QDialog dialog(this);
    dialog.setWindowTitle(tr("编辑快捷指令"));
    dialog.setMinimumWidth(350);
    QFormLayout* layout = new QFormLayout(&dialog);

    QLineEdit* nameEdit = new QLineEdit(item.name);
    layout->addRow(tr("名称:"), nameEdit);

    QLineEdit* dataEdit = new QLineEdit(item.data);
    layout->addRow(tr("数据:"), dataEdit);

    QCheckBox* hexCheck = new QCheckBox(tr("HEX格式"));
    hexCheck->setChecked(item.isHex);
    layout->addRow(tr("格式:"), hexCheck);

    QCheckBox* newlineCheck = new QCheckBox(tr("追加换行 (\\r\\n)"));
    newlineCheck->setChecked(item.appendNewline);
    layout->addRow(tr("选项:"), newlineCheck);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        item.name = nameEdit->text().trimmed();
        item.data = dataEdit->text().trimmed();
        item.isHex = hexCheck->isChecked();
        item.appendNewline = newlineCheck->isChecked();

        if (item.name.isEmpty()) {
            item.name = item.data.left(20);
        }

        rebuildButtons();
        emit configChanged();
    }
}

void QuickSendWidget::onDeleteItem(int index)
{
    if (index < 0 || index >= m_items.size()) return;

    int ret = QMessageBox::question(this, tr("确认删除"),
        tr("确定要删除 \"%1\" 吗?").arg(m_items[index].name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        removeItem(index);
    }
}

void QuickSendWidget::onMoveUp(int index)
{
    if (index > 0 && index < m_items.size()) {
        m_items.swapItemsAt(index, index - 1);
        rebuildButtons();
        emit configChanged();
    }
}

void QuickSendWidget::onMoveDown(int index)
{
    if (index >= 0 && index < m_items.size() - 1) {
        m_items.swapItemsAt(index, index + 1);
        rebuildButtons();
        emit configChanged();
    }
}

QByteArray QuickSendWidget::itemToBytes(const QuickSendItem& item) const
{
    QByteArray data;

    if (item.isHex) {
        data = hexStringToBytes(item.data);
    } else {
        data = item.data.toUtf8();
    }

    if (item.appendNewline && !item.isHex) {
        data.append("\r\n");
    }

    return data;
}

QByteArray QuickSendWidget::hexStringToBytes(const QString& hex) const
{
    QByteArray result;
    QString cleaned = hex;
    cleaned.remove(QRegularExpression("[^0-9A-Fa-f]"));

    for (int i = 0; i + 1 < cleaned.length(); i += 2) {
        bool ok;
        int byte = cleaned.mid(i, 2).toInt(&ok, 16);
        if (ok) {
            result.append(static_cast<char>(byte));
        }
    }

    return result;
}

void QuickSendWidget::loadFromJson(const QJsonArray& array)
{
    m_items.clear();

    for (const QJsonValue& val : array) {
        QJsonObject obj = val.toObject();
        QuickSendItem item;
        item.name = obj["name"].toString();
        item.data = obj["data"].toString();
        item.isHex = obj["isHex"].toBool(false);
        item.appendNewline = obj["appendNewline"].toBool(false);

        if (!item.data.isEmpty()) {
            m_items.append(item);
        }
    }

    localizeBuiltInItemNames();
    rebuildButtons();
    LOG_INFO(QString("Loaded %1 quick send items").arg(m_items.size()));
}

QJsonArray QuickSendWidget::saveToJson() const
{
    QJsonArray array;

    for (const QuickSendItem& item : m_items) {
        QJsonObject obj;
        obj["name"] = item.name;
        obj["data"] = item.data;
        obj["isHex"] = item.isHex;
        obj["appendNewline"] = item.appendNewline;
        array.append(obj);
    }

    return array;
}

void QuickSendWidget::setMaxVisibleRows(int rows)
{
    m_maxVisibleRows = rows;
    int height = rows * 32 + 40;
    m_scrollArea->setMaximumHeight(height);
}

void QuickSendWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void QuickSendWidget::retranslateUi()
{
    // 更新标题
    if (m_titleLabel) {
        m_titleLabel->setText(tr("快捷发送"));
    }

    // 更新添加按钮提示
    if (m_addButton) {
        m_addButton->setToolTip(tr("添加快捷指令"));
    }

    localizeBuiltInItemNames();
    rebuildButtons();
}

} // namespace ComAssistant
