/**
 * @file SerialModeWidget.h
 * @brief 串口模式组件（默认模式）
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef SERIALMODEWIDGET_H
#define SERIALMODEWIDGET_H

#include "IModeWidget.h"
#include "widgets/TabbedReceiveWidget.h"
#include "widgets/SendWidget.h"
#include <QSplitter>

namespace ComAssistant {

/**
 * @brief 串口模式组件 - 默认模式，包含接收区和发送区
 */
class SerialModeWidget : public IModeWidget {
    Q_OBJECT

public:
    explicit SerialModeWidget(QWidget* parent = nullptr);
    ~SerialModeWidget() override = default;

    // IModeWidget 接口实现
    QString modeName() const override { return tr("串口"); }
    QString modeIcon() const override { return "serial"; }
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;
    bool exportToFile(const QString& fileName) override;
    void onActivated() override;
    void onDeactivated() override;

    // 访问内部组件
    TabbedReceiveWidget* receiveWidget() { return m_receiveWidget; }
    SendWidget* sendWidget() { return m_sendWidget; }

private:
    void setupUi();

    QSplitter* m_splitter;
    TabbedReceiveWidget* m_receiveWidget;
    SendWidget* m_sendWidget;
};

} // namespace ComAssistant

#endif // SERIALMODEWIDGET_H
