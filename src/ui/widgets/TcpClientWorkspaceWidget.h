/**
 * @file TcpClientWorkspaceWidget.h
 * @brief TCP 客户端专用工作台
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_TCPCLIENTWORKSPACEWIDGET_H
#define COMASSISTANT_TCPCLIENTWORKSPACEWIDGET_H

#include "CommunicationWorkspaceWidget.h"
#include "config/AppConfig.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>

namespace ComAssistant {

/**
 * @brief TCP 客户端专用工作台。
 *
 * 提供服务器地址、连接超时、自动重连和网络流收发日志。发送动作
 * 通过 sendDataRequested 交给主窗口，保持通信对象生命周期集中管理。
 */
class TcpClientWorkspaceWidget : public CommunicationWorkspaceWidget
{
    Q_OBJECT

public:
    explicit TcpClientWorkspaceWidget(QWidget* parent = nullptr);

    /**
     * @brief 应用 TCP 客户端配置到界面。
     * @param config 网络配置。
     */
    void setConfig(const NetworkConfig& config);

    /**
     * @brief 从界面读取 TCP 客户端配置。
     * @return 当前界面配置。
     */
    NetworkConfig config() const;

    /**
     * @brief 读取是否启用自动重连。
     * @return true 表示断线后尝试重连。
     */
    bool autoReconnectEnabled() const;

    /**
     * @brief 读取自动重连间隔。
     * @return 重连间隔毫秒数。
     */
    int reconnectIntervalMs() const;

    void setConnected(bool connected) override;
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;

private slots:
    /**
     * @brief 发送按钮点击处理。
     */
    void onSendClicked();

private:
    void setupUi();
    void appendLogLine(const QString& direction, const QByteArray& data);

    QLabel* m_stateLabel = nullptr;             ///< 连接状态标签
    QLineEdit* m_serverIpEdit = nullptr;        ///< 服务器 IP 输入框
    QSpinBox* m_serverPortSpin = nullptr;       ///< 服务器端口输入框
    QSpinBox* m_timeoutSpin = nullptr;          ///< 连接超时输入框
    QCheckBox* m_autoReconnectCheck = nullptr;  ///< 自动重连开关
    QSpinBox* m_reconnectIntervalSpin = nullptr;///< 自动重连间隔
    QPlainTextEdit* m_logEdit = nullptr;        ///< 收发日志
    QPlainTextEdit* m_sendEdit = nullptr;       ///< 发送输入框
    QCheckBox* m_hexSendCheck = nullptr;        ///< HEX 发送开关
};

} // namespace ComAssistant

#endif // COMASSISTANT_TCPCLIENTWORKSPACEWIDGET_H
