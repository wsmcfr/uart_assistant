/**
 * @file ControlPanel.cpp
 * @brief 控件面板容器实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "ControlPanel.h"
#include "controls/SliderControl.h"
#include "controls/Attitude3DControl.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QMessageBox>
#include <QEvent>

namespace ComAssistant {

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

ControlPanel::~ControlPanel()
{
    clearControls();
}

void ControlPanel::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 工具栏
    QHBoxLayout* toolLayout = new QHBoxLayout;
    toolLayout->setSpacing(4);

    QLabel* typeLabel = new QLabel(tr("控件类型:"));
    toolLayout->addWidget(typeLabel);

    m_controlTypeCombo = new QComboBox;
    m_controlTypeCombo->addItem(tr("滑动条"), static_cast<int>(ControlType::Slider));
    m_controlTypeCombo->addItem(tr("3D姿态"), static_cast<int>(ControlType::Attitude3D));
    // 未来可添加更多控件类型
    // m_controlTypeCombo->addItem(tr("按钮"), static_cast<int>(ControlType::Button));
    // m_controlTypeCombo->addItem(tr("开关"), static_cast<int>(ControlType::Switch));
    toolLayout->addWidget(m_controlTypeCombo);

    m_addBtn = new QPushButton(tr("添加"));
    m_addBtn->setFixedWidth(60);
    connect(m_addBtn, &QPushButton::clicked, this, &ControlPanel::onAddControlClicked);
    toolLayout->addWidget(m_addBtn);

    toolLayout->addStretch();

    m_saveBtn = new QPushButton(tr("保存"));
    m_saveBtn->setFixedWidth(60);
    connect(m_saveBtn, &QPushButton::clicked, this, &ControlPanel::onSaveClicked);
    toolLayout->addWidget(m_saveBtn);

    m_loadBtn = new QPushButton(tr("加载"));
    m_loadBtn->setFixedWidth(60);
    connect(m_loadBtn, &QPushButton::clicked, this, &ControlPanel::onLoadClicked);
    toolLayout->addWidget(m_loadBtn);

    m_clearBtn = new QPushButton(tr("清空"));
    m_clearBtn->setFixedWidth(60);
    connect(m_clearBtn, &QPushButton::clicked, this, &ControlPanel::onClearAllClicked);
    toolLayout->addWidget(m_clearBtn);

    mainLayout->addLayout(toolLayout);

    // 控件滚动区域
    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget* scrollWidget = new QWidget;
    m_controlsLayout = new QVBoxLayout(scrollWidget);
    m_controlsLayout->setContentsMargins(4, 4, 4, 4);
    m_controlsLayout->setSpacing(8);
    m_controlsLayout->addStretch();  // 底部弹簧

    m_scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // 提示文字
    QLabel* hintLabel = new QLabel(tr("提示：添加控件后，拖动滑动条将自动发送数据到设备"));
    hintLabel->setStyleSheet("color: gray; font-size: 10px;");
    hintLabel->setWordWrap(true);
    mainLayout->addWidget(hintLabel);
}

void ControlPanel::addControl(IInteractiveControl* control)
{
    if (!control) return;

    // 连接信号
    connect(control, &IInteractiveControl::sendDataRequested,
            this, &ControlPanel::onControlSendRequested);
    connect(control, &IInteractiveControl::deleteRequested,
            this, &ControlPanel::onControlDeleteRequested);

    // 添加到布局（在弹簧之前）
    int insertIndex = m_controlsLayout->count() - 1;  // 在弹簧之前
    m_controlsLayout->insertWidget(insertIndex, control);

    m_controls.append(control);
    emit controlAdded(control);
}

void ControlPanel::removeControl(IInteractiveControl* control)
{
    if (!control) return;

    int index = m_controls.indexOf(control);
    if (index >= 0) {
        m_controls.removeAt(index);
        m_controlsLayout->removeWidget(control);
        disconnect(control, nullptr, this, nullptr);
        control->deleteLater();
        emit controlRemoved(control);
    }
}

void ControlPanel::clearControls()
{
    for (auto* control : m_controls) {
        m_controlsLayout->removeWidget(control);
        disconnect(control, nullptr, this, nullptr);
        control->deleteLater();
    }
    m_controls.clear();
}

QJsonArray ControlPanel::toJson() const
{
    QJsonArray array;
    for (const auto* control : m_controls) {
        array.append(control->toJson());
    }
    return array;
}

bool ControlPanel::fromJson(const QJsonArray& array)
{
    clearControls();

    for (const QJsonValue& val : array) {
        QJsonObject obj = val.toObject();
        QString type = obj["type"].toString();

        IInteractiveControl* control = nullptr;

        if (type == "Slider") {
            control = new SliderControl(this);
        } else if (type == "Attitude3D") {
            control = new Attitude3DControl(this);
        }
        // 未来可添加更多控件类型
        // else if (type == "Button") { ... }

        if (control) {
            if (control->fromJson(obj)) {
                addControl(control);
            } else {
                delete control;
            }
        }
    }

    return true;
}

bool ControlPanel::saveToFile(const QString& fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonObject root;
    root["version"] = 1;
    root["controls"] = toJson();

    QJsonDocument doc(root);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool ControlPanel::loadFromFile(const QString& fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    QJsonObject root = doc.object();
    return fromJson(root["controls"].toArray());
}

void ControlPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ControlPanel::onAddControlClicked()
{
    ControlType type = static_cast<ControlType>(
        m_controlTypeCombo->currentData().toInt());

    IInteractiveControl* control = createControl(type);
    if (control) {
        // 设置默认名称
        control->setControlName(QString("Param%1").arg(m_controls.size() + 1));
        addControl(control);
    }
}

void ControlPanel::onClearAllClicked()
{
    if (m_controls.isEmpty()) return;

    int ret = QMessageBox::question(this, tr("确认清空"),
        tr("确定要清空所有控件吗？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        clearControls();
    }
}

void ControlPanel::onSaveClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        tr("保存控件配置"),
        "control_panel.json",
        tr("JSON文件 (*.json);;所有文件 (*)"));

    if (!fileName.isEmpty()) {
        if (saveToFile(fileName)) {
            QMessageBox::information(this, tr("保存成功"),
                tr("控件配置已保存到 %1").arg(fileName));
        } else {
            QMessageBox::warning(this, tr("保存失败"),
                tr("无法保存到 %1").arg(fileName));
        }
    }
}

void ControlPanel::onLoadClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("加载控件配置"),
        QString(),
        tr("JSON文件 (*.json);;所有文件 (*)"));

    if (!fileName.isEmpty()) {
        if (loadFromFile(fileName)) {
            QMessageBox::information(this, tr("加载成功"),
                tr("已加载 %1 个控件").arg(m_controls.size()));
        } else {
            QMessageBox::warning(this, tr("加载失败"),
                tr("无法加载 %1").arg(fileName));
        }
    }
}

void ControlPanel::onControlSendRequested(const QByteArray& data)
{
    emit sendDataRequested(data);
}

void ControlPanel::onControlDeleteRequested()
{
    // 获取发送信号的控件
    IInteractiveControl* control = qobject_cast<IInteractiveControl*>(sender());
    if (control) {
        removeControl(control);
    }
}

void ControlPanel::retranslateUi()
{
    if (m_addBtn) m_addBtn->setText(tr("添加"));
    if (m_clearBtn) m_clearBtn->setText(tr("清空"));
    if (m_saveBtn) m_saveBtn->setText(tr("保存"));
    if (m_loadBtn) m_loadBtn->setText(tr("加载"));
}

IInteractiveControl* ControlPanel::createControl(ControlType type)
{
    switch (type) {
    case ControlType::Slider:
        return new SliderControl(this);
    case ControlType::Attitude3D:
        return new Attitude3DControl(this);
    // 未来可添加更多控件类型
    default:
        return nullptr;
    }
}

} // namespace ComAssistant
