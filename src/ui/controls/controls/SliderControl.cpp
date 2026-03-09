/**
 * @file SliderControl.cpp
 * @brief 滑动条调参控件实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "SliderControl.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QJsonObject>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <cmath>

namespace ComAssistant {

SliderControl::SliderControl(QWidget* parent)
    : IInteractiveControl(parent)
{
    setupUi();

    // 初始化发送限流定时器
    m_sendTimer = new QTimer(this);
    m_sendTimer->setSingleShot(true);
    connect(m_sendTimer, &QTimer::timeout, this, &SliderControl::onSendTimerTimeout);
}

void SliderControl::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // 名称编辑框（可直接编辑）
    m_nameEdit = new QLineEdit(m_name);
    m_nameEdit->setAlignment(Qt::AlignCenter);
    m_nameEdit->setStyleSheet(
        "QLineEdit { "
        "  font-weight: bold; "
        "  border: 1px solid transparent; "
        "  border-radius: 3px; "
        "  background: transparent; "
        "} "
        "QLineEdit:hover { "
        "  border: 1px solid #3498db; "
        "} "
        "QLineEdit:focus { "
        "  border: 1px solid #2980b9; "
        "  background: rgba(52, 152, 219, 0.1); "
        "}"
    );
    m_nameEdit->setPlaceholderText(tr("输入名称..."));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &SliderControl::onNameEdited);
    mainLayout->addWidget(m_nameEdit);

    // 滑动条
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, 1000);  // 使用1000分辨率
    m_slider->setValue(0);
    connect(m_slider, &QSlider::valueChanged, this, &SliderControl::onSliderValueChanged);
    connect(m_slider, &QSlider::sliderPressed, this, &SliderControl::onSliderPressed);
    connect(m_slider, &QSlider::sliderReleased, this, &SliderControl::onSliderReleased);
    mainLayout->addWidget(m_slider);

    // 值显示区域
    QHBoxLayout* valueLayout = new QHBoxLayout;
    valueLayout->setSpacing(4);

    m_valueEdit = new QLineEdit;
    m_valueEdit->setMaximumWidth(80);
    m_valueEdit->setAlignment(Qt::AlignRight);
    connect(m_valueEdit, &QLineEdit::editingFinished, this, &SliderControl::onValueEdited);
    valueLayout->addWidget(m_valueEdit);

    m_unitLabel = new QLabel;
    valueLayout->addWidget(m_unitLabel);

    valueLayout->addStretch();

    m_rangeLabel = new QLabel;
    m_rangeLabel->setStyleSheet("color: gray; font-size: 10px;");
    valueLayout->addWidget(m_rangeLabel);

    mainLayout->addLayout(valueLayout);

    // 初始更新显示
    updateValueDisplay();

    // 设置固定高度
    setFixedHeight(80);
    setMinimumWidth(150);
}

void SliderControl::setControlName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        m_nameEdit->setText(name);
        emit nameChanged(name);
    }
}

void SliderControl::setValue(double value)
{
    value = qBound(m_minValue, value, m_maxValue);
    if (!qFuzzyCompare(m_currentValue, value)) {
        m_currentValue = value;
        m_slider->blockSignals(true);
        m_slider->setValue(valueToSlider(value));
        m_slider->blockSignals(false);
        updateValueDisplay();
        emit valueChanged(value);
    }
}

void SliderControl::setSendTemplate(const QString& tmpl)
{
    m_sendTemplate = tmpl;
}

QByteArray SliderControl::buildSendData() const
{
    QString result = m_sendTemplate;

    // 替换 %s 为控件名称
    result.replace("%s", m_name);

    // 替换数值占位符
    // 支持 %.Nf 格式（N位小数）
    QRegularExpression re("%(\\d*\\.?\\d*)[fd]");
    QRegularExpressionMatch match = re.match(result);
    if (match.hasMatch()) {
        QString format = "%" + match.captured(1) + "f";
        QString valueStr = QString::asprintf(format.toUtf8().constData(), m_currentValue);
        result.replace(match.captured(0), valueStr);
    }

    return result.toUtf8();
}

void SliderControl::bindDataChannel(int channel)
{
    m_boundChannel = channel;
}

void SliderControl::updateDisplayValue(double value)
{
    // 用于显示模式，不触发发送
    m_currentValue = value;
    m_slider->blockSignals(true);
    m_slider->setValue(valueToSlider(value));
    m_slider->blockSignals(false);
    updateValueDisplay();
}

QJsonObject SliderControl::toJson() const
{
    QJsonObject obj;
    obj["type"] = controlTypeName();
    obj["name"] = m_name;
    obj["sendTemplate"] = m_sendTemplate;
    obj["unit"] = m_unit;
    obj["minValue"] = m_minValue;
    obj["maxValue"] = m_maxValue;
    obj["step"] = m_step;
    obj["decimals"] = m_decimals;
    obj["sendInterval"] = m_sendIntervalMs;
    obj["boundChannel"] = m_boundChannel;
    obj["currentValue"] = m_currentValue;
    return obj;
}

bool SliderControl::fromJson(const QJsonObject& obj)
{
    if (obj["type"].toString() != controlTypeName()) {
        return false;
    }

    setControlName(obj["name"].toString("Slider"));
    setSendTemplate(obj["sendTemplate"].toString("SET %s %.2f\n"));
    setUnit(obj["unit"].toString());
    setRange(obj["minValue"].toDouble(0), obj["maxValue"].toDouble(100));
    setStep(obj["step"].toDouble(1));
    setDecimals(obj["decimals"].toInt(2));
    setSendInterval(obj["sendInterval"].toInt(100));
    bindDataChannel(obj["boundChannel"].toInt(-1));
    setValue(obj["currentValue"].toDouble(0));

    return true;
}

void SliderControl::setRange(double min, double max)
{
    if (min >= max) return;
    m_minValue = min;
    m_maxValue = max;
    m_currentValue = qBound(min, m_currentValue, max);
    updateValueDisplay();
}

void SliderControl::setStep(double step)
{
    if (step > 0) {
        m_step = step;
    }
}

void SliderControl::setDecimals(int decimals)
{
    m_decimals = qBound(0, decimals, 10);
    updateValueDisplay();
}

void SliderControl::setSendInterval(int ms)
{
    m_sendIntervalMs = qMax(10, ms);
}

void SliderControl::setUnit(const QString& unit)
{
    m_unit = unit;
    m_unitLabel->setText(unit);
}

void SliderControl::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    IInteractiveControl::changeEvent(event);
}

void SliderControl::onSliderValueChanged(int value)
{
    double newValue = sliderToValue(value);

    // 应用步进
    if (m_step > 0) {
        newValue = std::round(newValue / m_step) * m_step;
    }

    newValue = qBound(m_minValue, newValue, m_maxValue);

    if (!qFuzzyCompare(m_currentValue, newValue)) {
        m_currentValue = newValue;
        updateValueDisplay();
        emit valueChanged(newValue);
        triggerSend();
    }
}

void SliderControl::onSliderPressed()
{
    m_sliderDragging = true;
}

void SliderControl::onSliderReleased()
{
    m_sliderDragging = false;
    // 释放时立即发送最终值
    if (m_pendingSend) {
        m_pendingSend = false;
        m_sendTimer->stop();
        emit sendDataRequested(buildSendData());
    }
}

void SliderControl::onValueEdited()
{
    bool ok;
    double value = m_valueEdit->text().toDouble(&ok);
    if (ok) {
        setValue(value);
        triggerSend();
    } else {
        updateValueDisplay();  // 恢复显示
    }
}

void SliderControl::onSendTimerTimeout()
{
    if (m_pendingSend) {
        m_pendingSend = false;
        emit sendDataRequested(buildSendData());
    }
}

void SliderControl::onNameEdited()
{
    QString newName = m_nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        // 恢复原名称
        m_nameEdit->setText(m_name);
    } else if (newName != m_name) {
        m_name = newName;
        emit nameChanged(m_name);
    }
}

void SliderControl::retranslateUi()
{
    updateValueDisplay();
}

void SliderControl::updateValueDisplay()
{
    m_valueEdit->setText(QString::number(m_currentValue, 'f', m_decimals));
    m_rangeLabel->setText(QString("[%1 - %2]")
        .arg(m_minValue, 0, 'f', m_decimals)
        .arg(m_maxValue, 0, 'f', m_decimals));
}

void SliderControl::triggerSend()
{
    if (m_sendTemplate.isEmpty()) {
        return;
    }

    if (m_sliderDragging) {
        // 拖动中使用限流
        if (!m_sendTimer->isActive()) {
            emit sendDataRequested(buildSendData());
            m_sendTimer->start(m_sendIntervalMs);
        } else {
            m_pendingSend = true;
        }
    } else {
        // 非拖动直接发送
        emit sendDataRequested(buildSendData());
    }
}

int SliderControl::valueToSlider(double value) const
{
    if (qFuzzyCompare(m_maxValue, m_minValue)) {
        return 0;
    }
    double ratio = (value - m_minValue) / (m_maxValue - m_minValue);
    return static_cast<int>(ratio * 1000);
}

double SliderControl::sliderToValue(int sliderValue) const
{
    double ratio = sliderValue / 1000.0;
    return m_minValue + ratio * (m_maxValue - m_minValue);
}

void SliderControl::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(tr("配置..."), this, &SliderControl::showConfigDialog);
    menu.addSeparator();
    menu.addAction(tr("删除"), this, [this]() {
        emit deleteRequested();
    });
    menu.exec(event->globalPos());
}

void SliderControl::showConfigDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("滑动条配置 - %1").arg(m_name));
    dialog.setMinimumWidth(350);

    QFormLayout* layout = new QFormLayout(&dialog);

    // 发送模板
    QLineEdit* templateEdit = new QLineEdit(m_sendTemplate);
    templateEdit->setPlaceholderText(tr("例如: SET %s %.2f\\n"));
    templateEdit->setToolTip(tr("%s=名称, %.Nf=N位小数数值"));
    layout->addRow(tr("发送模板:"), templateEdit);

    // 最小值
    QDoubleSpinBox* minSpin = new QDoubleSpinBox;
    minSpin->setRange(-1e9, 1e9);
    minSpin->setDecimals(m_decimals);
    minSpin->setValue(m_minValue);
    layout->addRow(tr("最小值:"), minSpin);

    // 最大值
    QDoubleSpinBox* maxSpin = new QDoubleSpinBox;
    maxSpin->setRange(-1e9, 1e9);
    maxSpin->setDecimals(m_decimals);
    maxSpin->setValue(m_maxValue);
    layout->addRow(tr("最大值:"), maxSpin);

    // 步进
    QDoubleSpinBox* stepSpin = new QDoubleSpinBox;
    stepSpin->setRange(0.001, 1000);
    stepSpin->setDecimals(3);
    stepSpin->setValue(m_step);
    layout->addRow(tr("步进:"), stepSpin);

    // 小数位数
    QSpinBox* decimalsSpin = new QSpinBox;
    decimalsSpin->setRange(0, 6);
    decimalsSpin->setValue(m_decimals);
    layout->addRow(tr("小数位数:"), decimalsSpin);

    // 单位
    QLineEdit* unitEdit = new QLineEdit(m_unit);
    unitEdit->setPlaceholderText(tr("例如: mV, Hz, %"));
    layout->addRow(tr("单位:"), unitEdit);

    // 发送间隔
    QSpinBox* intervalSpin = new QSpinBox;
    intervalSpin->setRange(10, 5000);
    intervalSpin->setSuffix(" ms");
    intervalSpin->setValue(m_sendIntervalMs);
    intervalSpin->setToolTip(tr("拖动时最小发送间隔"));
    layout->addRow(tr("发送间隔:"), intervalSpin);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        // 应用设置
        setSendTemplate(templateEdit->text());
        setDecimals(decimalsSpin->value());
        setRange(minSpin->value(), maxSpin->value());
        setStep(stepSpin->value());
        setUnit(unitEdit->text());
        setSendInterval(intervalSpin->value());
    }
}

} // namespace ComAssistant
