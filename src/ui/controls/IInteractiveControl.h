/**
 * @file IInteractiveControl.h
 * @brief 交互式控件接口
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef COMASSISTANT_IINTERACTIVECONTROL_H
#define COMASSISTANT_IINTERACTIVECONTROL_H

#include <QWidget>
#include <QJsonObject>
#include <QString>

namespace ComAssistant {

/**
 * @brief 控件类型枚举
 */
enum class ControlType {
    Slider,         ///< 滑动条
    Button,         ///< 按钮
    Switch,         ///< 开关
    Knob,           ///< 旋钮
    Led,            ///< LED指示灯
    Gauge,          ///< 仪表盘
    ValueDisplay,   ///< 数值显示
    Attitude3D      ///< 3D姿态显示
};

/**
 * @brief 交互式控件接口
 *
 * 所有交互式控件都必须实现此接口
 */
class IInteractiveControl : public QWidget
{
    Q_OBJECT

public:
    explicit IInteractiveControl(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~IInteractiveControl() = default;

    /**
     * @brief 获取控件类型
     * @return 控件类型
     */
    virtual ControlType controlType() const = 0;

    /**
     * @brief 获取控件类型名称
     * @return 类型名称字符串
     */
    virtual QString controlTypeName() const = 0;

    /**
     * @brief 获取控件名称
     * @return 控件名称
     */
    virtual QString controlName() const = 0;

    /**
     * @brief 设置控件名称
     * @param name 控件名称
     */
    virtual void setControlName(const QString& name) = 0;

    /**
     * @brief 获取当前值
     * @return 当前值
     */
    virtual double currentValue() const = 0;

    /**
     * @brief 设置当前值
     * @param value 新值
     */
    virtual void setValue(double value) = 0;

    // ========== 发送功能 ==========

    /**
     * @brief 设置发送模板
     * @param tmpl 模板字符串，如 "SET KP %.2f\n"
     */
    virtual void setSendTemplate(const QString& tmpl) = 0;

    /**
     * @brief 获取发送模板
     * @return 模板字符串
     */
    virtual QString sendTemplate() const = 0;

    /**
     * @brief 构建发送数据
     * @return 要发送的字节数据
     */
    virtual QByteArray buildSendData() const = 0;

    // ========== 数据绑定（用于显示控件）==========

    /**
     * @brief 绑定数据通道
     * @param channel 通道索引
     */
    virtual void bindDataChannel(int channel) = 0;

    /**
     * @brief 获取绑定的数据通道
     * @return 通道索引，-1表示未绑定
     */
    virtual int boundDataChannel() const = 0;

    /**
     * @brief 更新显示值（由外部数据驱动）
     * @param value 新值
     */
    virtual void updateDisplayValue(double value) = 0;

    // ========== 序列化 ==========

    /**
     * @brief 导出为JSON
     * @return JSON对象
     */
    virtual QJsonObject toJson() const = 0;

    /**
     * @brief 从JSON导入
     * @param obj JSON对象
     * @return 是否成功
     */
    virtual bool fromJson(const QJsonObject& obj) = 0;

signals:
    /**
     * @brief 请求发送数据信号
     * @param data 要发送的数据
     */
    void sendDataRequested(const QByteArray& data);

    /**
     * @brief 值改变信号
     * @param value 新值
     */
    void valueChanged(double value);

    /**
     * @brief 控件名称改变信号
     * @param name 新名称
     */
    void nameChanged(const QString& name);

    /**
     * @brief 请求删除控件信号
     */
    void deleteRequested();
};

} // namespace ComAssistant

#endif // COMASSISTANT_IINTERACTIVECONTROL_H
