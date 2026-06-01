/**
 * @file UdpWorkspaceWidget.h
 * @brief UDP 专用工作台
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_UDPWORKSPACEWIDGET_H
#define COMASSISTANT_UDPWORKSPACEWIDGET_H

#include "CommunicationWorkspaceWidget.h"
#include "config/AppConfig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>

namespace ComAssistant {

/**
 * @brief UDP 专用工作台。
 *
 * 提供本地绑定、目标地址、最近远端和单播/广播发送入口。
 */
class UdpWorkspaceWidget : public CommunicationWorkspaceWidget
{
    Q_OBJECT

public:
    explicit UdpWorkspaceWidget(QWidget* parent = nullptr);

    void setConfig(const NetworkConfig& config);
    NetworkConfig config() const;
    void addRecentRemote(const QString& ip, int port);

    void setConnected(bool connected) override;
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;

signals:
    /**
     * @brief 请求向指定 UDP 远端发送数据报。
     */
    void sendDatagramRequested(const QByteArray& data, const QString& ip, int port);

private slots:
    void onSendClicked();
    void onRecentRemoteChanged(int index);

private:
    void setupUi();
    void appendLogLine(const QString& direction, const QByteArray& data);

    QLabel* m_stateLabel = nullptr;         ///< UDP 绑定状态标签
    QSpinBox* m_localPortSpin = nullptr;    ///< 本地端口输入框
    QLineEdit* m_remoteIpEdit = nullptr;    ///< 远端 IP 输入框
    QSpinBox* m_remotePortSpin = nullptr;   ///< 远端端口输入框
    QCheckBox* m_broadcastCheck = nullptr;  ///< 广播发送开关
    QComboBox* m_recentRemoteCombo = nullptr; ///< 最近远端列表
    QPlainTextEdit* m_logEdit = nullptr;    ///< 收发日志
    QPlainTextEdit* m_sendEdit = nullptr;   ///< 发送输入框
    QCheckBox* m_hexSendCheck = nullptr;    ///< HEX 发送开关
};

} // namespace ComAssistant

#endif // COMASSISTANT_UDPWORKSPACEWIDGET_H
