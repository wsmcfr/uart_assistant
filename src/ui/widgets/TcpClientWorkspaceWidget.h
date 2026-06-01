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
#include <QPushButton>
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
     * @brief 发送按钮点击处理。
     */
    void onSendClicked();

    /**
     * @brief 把发送输入框内容规范化为大写分组 HEX。
     */
    void onFormatHexClicked();

    /**
     * @brief 把当前日志复制到系统剪贴板。
     */
    void onCopyLogClicked();

    /**
     * @brief 根据输入内容和发送模式刷新字节数提示。
     */
    void updateSendByteCount();

    /**
     * @brief 根据连接参数和重连设置刷新状态摘要。
     */
    void updateStatusSummary();

private:
    /**
     * @brief 创建 TCP 客户端工作台界面并建立控件信号。
     */
    void setupUi();

    /**
     * @brief 解析当前发送输入框内容。
     * @return 准备写入 TCP 流的原始字节。
     */
    QByteArray currentPayload() const;

    /**
     * @brief 追加一行 TCP 收发日志。
     * @param direction 方向，例如 RX 或 TX。
     * @param data 原始字节。
     */
    void appendLogLine(const QString& direction, const QByteArray& data);

    QLabel* m_stateLabel = nullptr;             ///< 连接状态标签
    QLabel* m_summaryLabel = nullptr;           ///< TCP 客户端状态摘要
    QLineEdit* m_serverIpEdit = nullptr;        ///< 服务器 IP 输入框
    QSpinBox* m_serverPortSpin = nullptr;       ///< 服务器端口输入框
    QSpinBox* m_timeoutSpin = nullptr;          ///< 连接超时输入框
    QCheckBox* m_autoReconnectCheck = nullptr;  ///< 自动重连开关
    QSpinBox* m_reconnectIntervalSpin = nullptr;///< 自动重连间隔
    QCheckBox* m_autoScrollCheck = nullptr;     ///< 日志自动滚动开关
    QCheckBox* m_appendNewlineCheck = nullptr;  ///< 文本发送时追加换行
    QLabel* m_byteCountLabel = nullptr;         ///< 当前发送字节数提示
    QPlainTextEdit* m_logEdit = nullptr;        ///< 收发日志
    QPlainTextEdit* m_sendEdit = nullptr;       ///< 发送输入框
    QCheckBox* m_hexSendCheck = nullptr;        ///< HEX 发送开关
    QPushButton* m_sendButton = nullptr;        ///< 发送按钮
};

} // namespace ComAssistant

#endif // COMASSISTANT_TCPCLIENTWORKSPACEWIDGET_H
