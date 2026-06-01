/**
 * @file TcpServerWorkspaceWidget.h
 * @brief TCP 服务器专用工作台
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_TCPSERVERWORKSPACEWIDGET_H
#define COMASSISTANT_TCPSERVERWORKSPACEWIDGET_H

#include "CommunicationWorkspaceWidget.h"
#include "config/AppConfig.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QStringList>

namespace ComAssistant {

/**
 * @brief TCP 服务器专用工作台。
 *
 * 展示监听参数和客户端列表，并提供“指定客户端发送”和“广播发送”。
 */
class TcpServerWorkspaceWidget : public CommunicationWorkspaceWidget
{
    Q_OBJECT

public:
    explicit TcpServerWorkspaceWidget(QWidget* parent = nullptr);

    void setConfig(const NetworkConfig& config);
    NetworkConfig config() const;
    void setClients(const QStringList& clients);
    void addClient(const QString& clientId);
    void removeClient(const QString& clientId);

    void setConnected(bool connected) override;
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;

signals:
    /**
     * @brief 请求向指定客户端发送数据。
     */
    void sendToClientRequested(const QString& clientId, const QByteArray& data);

    /**
     * @brief 请求向所有客户端广播数据。
     */
    void broadcastDataRequested(const QByteArray& data);

private slots:
    void onSendClientClicked();
    void onBroadcastClicked();

private:
    void setupUi();
    QByteArray currentPayload() const;
    void appendLogLine(const QString& direction, const QByteArray& data);

    QLabel* m_stateLabel = nullptr;       ///< 监听状态标签
    QSpinBox* m_listenPortSpin = nullptr; ///< 监听端口输入框
    QSpinBox* m_maxConnectionsSpin = nullptr; ///< 最大连接数输入框
    QComboBox* m_clientCombo = nullptr;   ///< 客户端选择框
    QPlainTextEdit* m_logEdit = nullptr;  ///< 收发日志
    QPlainTextEdit* m_sendEdit = nullptr; ///< 发送输入框
    QCheckBox* m_hexSendCheck = nullptr;  ///< HEX 发送开关
};

} // namespace ComAssistant

#endif // COMASSISTANT_TCPSERVERWORKSPACEWIDGET_H
