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
#include <QPushButton>
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

    /**
     * @brief 请求断开指定客户端连接。
     * @param clientId 当前选中的客户端标识。
     */
    void disconnectClientRequested(const QString& clientId);

protected:
    /**
     * @brief 捕获发送输入框的 Ctrl+Enter 快捷键。
     *
     * @param watched 触发事件的对象。
     * @param event Qt 原始事件。
     * @return true 表示事件已被发送动作消费。
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    /**
     * @brief 发送到当前选中客户端。
     */
    void onSendClientClicked();

    /**
     * @brief 广播发送到所有客户端。
     */
    void onBroadcastClicked();

    /**
     * @brief 请求断开当前选中的客户端。
     */
    void onDisconnectClientClicked();

    /**
     * @brief 把发送输入框内容规范化为大写分组 HEX。
     */
    void onFormatHexClicked();

    /**
     * @brief 把当前服务器日志复制到系统剪贴板。
     */
    void onCopyLogClicked();

    /**
     * @brief 刷新客户端数量、按钮可用状态和状态摘要。
     */
    void updateClientState();

    /**
     * @brief 根据输入内容和发送模式刷新字节数提示。
     */
    void updateSendByteCount();

    /**
     * @brief 根据监听参数和客户端列表刷新状态摘要。
     */
    void updateStatusSummary();

private:
    /**
     * @brief 创建 TCP 服务器工作台界面并建立控件信号。
     */
    void setupUi();

    /**
     * @brief 解析当前发送输入框内容。
     * @return 准备写入客户端连接的原始字节。
     */
    QByteArray currentPayload() const;

    /**
     * @brief 追加一行 TCP 服务器收发日志。
     * @param direction 方向，例如 RX 或 TX。
     * @param data 原始字节。
     */
    void appendLogLine(const QString& direction, const QByteArray& data);

    QLabel* m_stateLabel = nullptr;       ///< 监听状态标签
    QLabel* m_summaryLabel = nullptr;     ///< TCP 服务器状态摘要
    QLabel* m_clientCountLabel = nullptr; ///< 客户端数量提示
    QSpinBox* m_listenPortSpin = nullptr; ///< 监听端口输入框
    QSpinBox* m_maxConnectionsSpin = nullptr; ///< 最大连接数输入框
    QComboBox* m_clientCombo = nullptr;   ///< 客户端选择框
    QCheckBox* m_autoScrollCheck = nullptr; ///< 日志自动滚动开关
    QCheckBox* m_appendNewlineCheck = nullptr; ///< 文本发送时追加换行
    QLabel* m_byteCountLabel = nullptr;   ///< 当前发送字节数提示
    QPlainTextEdit* m_logEdit = nullptr;  ///< 收发日志
    QPlainTextEdit* m_sendEdit = nullptr; ///< 发送输入框
    QCheckBox* m_hexSendCheck = nullptr;  ///< HEX 发送开关
    QPushButton* m_sendClientButton = nullptr; ///< 指定客户端发送按钮
    QPushButton* m_disconnectClientButton = nullptr; ///< 断开选中客户端按钮
};

} // namespace ComAssistant

#endif // COMASSISTANT_TCPSERVERWORKSPACEWIDGET_H
