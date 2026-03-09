/**
 * @file SliderControl.h
 * @brief 滑动条调参控件
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef COMASSISTANT_SLIDERCONTROL_H
#define COMASSISTANT_SLIDERCONTROL_H

#include "../IInteractiveControl.h"
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>

namespace ComAssistant {

/**
 * @brief 滑动条调参控件
 *
 * 用于实时调节参数并发送到设备
 */
class SliderControl : public IInteractiveControl
{
    Q_OBJECT

public:
    explicit SliderControl(QWidget* parent = nullptr);
    ~SliderControl() override = default;

    // IInteractiveControl 接口实现
    ControlType controlType() const override { return ControlType::Slider; }
    QString controlTypeName() const override { return QStringLiteral("Slider"); }
    QString controlName() const override { return m_name; }
    void setControlName(const QString& name) override;
    double currentValue() const override { return m_currentValue; }
    void setValue(double value) override;

    void setSendTemplate(const QString& tmpl) override;
    QString sendTemplate() const override { return m_sendTemplate; }
    QByteArray buildSendData() const override;

    void bindDataChannel(int channel) override;
    int boundDataChannel() const override { return m_boundChannel; }
    void updateDisplayValue(double value) override;

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& obj) override;

    // 滑动条专属配置
    void setRange(double min, double max);
    double minimum() const { return m_minValue; }
    double maximum() const { return m_maxValue; }

    void setStep(double step);
    double step() const { return m_step; }

    void setDecimals(int decimals);
    int decimals() const { return m_decimals; }

    void setSendInterval(int ms);
    int sendInterval() const { return m_sendIntervalMs; }

    void setUnit(const QString& unit);
    QString unit() const { return m_unit; }

protected:
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onSliderValueChanged(int value);
    void onSliderPressed();
    void onSliderReleased();
    void onValueEdited();
    void onSendTimerTimeout();
    void onNameEdited();  ///< 名称编辑完成
    void showConfigDialog();  ///< 显示配置对话框

private:
    void setupUi();
    void retranslateUi();
    void updateValueDisplay();
    void triggerSend();
    int valueToSlider(double value) const;
    double sliderToValue(int sliderValue) const;

    // UI组件
    QLineEdit* m_nameEdit = nullptr;  ///< 可编辑的名称
    QSlider* m_slider = nullptr;
    QLineEdit* m_valueEdit = nullptr;
    QLabel* m_unitLabel = nullptr;
    QLabel* m_rangeLabel = nullptr;

    // 配置
    QString m_name = "Slider";
    QString m_sendTemplate = "SET %s %.2f\n";
    QString m_unit;
    double m_minValue = 0.0;
    double m_maxValue = 100.0;
    double m_step = 1.0;
    double m_currentValue = 0.0;
    int m_decimals = 2;
    int m_sendIntervalMs = 100;  // 最小发送间隔
    int m_boundChannel = -1;

    // 发送限流
    QTimer* m_sendTimer = nullptr;
    bool m_pendingSend = false;
    bool m_sliderDragging = false;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SLIDERCONTROL_H
