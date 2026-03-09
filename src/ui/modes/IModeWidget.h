/**
 * @file IModeWidget.h
 * @brief 模式组件接口定义
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef IMODEWIDGET_H
#define IMODEWIDGET_H

#include <QWidget>
#include <QByteArray>

namespace ComAssistant {

/**
 * @brief 模式组件接口
 *
 * 所有模式组件必须实现此接口，提供统一的数据处理和控制方法
 */
class IModeWidget : public QWidget {
    Q_OBJECT

public:
    explicit IModeWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~IModeWidget() = default;

    /**
     * @brief 获取模式名称
     */
    virtual QString modeName() const = 0;

    /**
     * @brief 获取模式图标名称
     */
    virtual QString modeIcon() const { return QString(); }

    /**
     * @brief 处理接收到的数据
     */
    virtual void appendReceivedData(const QByteArray& data) = 0;

    /**
     * @brief 处理发送的数据（用于调试模式等需要显示发送数据的模式）
     */
    virtual void appendSentData(const QByteArray& data) { Q_UNUSED(data) }

    /**
     * @brief 清空显示
     */
    virtual void clear() = 0;

    /**
     * @brief 导出数据到文件
     */
    virtual bool exportToFile(const QString& fileName) = 0;

    /**
     * @brief 模式激活时调用
     */
    virtual void onActivated() {}

    /**
     * @brief 模式停用时调用
     */
    virtual void onDeactivated() {}

    /**
     * @brief 获取模式专用工具栏（可选）
     */
    virtual QWidget* modeToolBar() { return nullptr; }

    /**
     * @brief 设置连接状态
     */
    virtual void setConnected(bool connected) { m_connected = connected; }

    /**
     * @brief 获取连接状态
     */
    bool isConnected() const { return m_connected; }

signals:
    /**
     * @brief 请求发送数据
     */
    void sendDataRequested(const QByteArray& data);

    /**
     * @brief 数据已接收
     */
    void dataReceived(const QByteArray& data);

    /**
     * @brief 状态消息
     */
    void statusMessage(const QString& message);

protected:
    bool m_connected = false;
};

} // namespace ComAssistant

#endif // IMODEWIDGET_H
