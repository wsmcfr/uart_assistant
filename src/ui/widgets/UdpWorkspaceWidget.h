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
#include <QPushButton>
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
     * @brief 发送当前 UDP 数据报。
     */
    void onSendClicked();

    /**
     * @brief 最近远端选择变化后同步目标地址。
     * @param index 当前选中项。
     */
    void onRecentRemoteChanged(int index);

    /**
     * @brief 清空最近远端列表。
     */
    void onClearRecentRemoteClicked();

    /**
     * @brief 把发送输入框内容规范化为大写分组 HEX。
     */
    void onFormatHexClicked();

    /**
     * @brief 复制 UDP 日志到系统剪贴板。
     */
    void onCopyLogClicked();

    /**
     * @brief 根据输入内容和发送模式刷新字节数提示。
     */
    void updateSendByteCount();

    /**
     * @brief 根据本地/目标端点和广播开关刷新状态摘要。
     */
    void updateStatusSummary();

private:
    /**
     * @brief 创建 UDP 工作台界面并建立控件信号。
     */
    void setupUi();

    /**
     * @brief 解析当前发送输入框内容。
     * @return 准备发出的 UDP payload。
     */
    QByteArray currentPayload() const;

    /**
     * @brief 追加一行 UDP 收发日志。
     * @param direction 方向，例如 RX 或 TX。
     * @param data 原始字节。
     */
    void appendLogLine(const QString& direction, const QByteArray& data);

    QLabel* m_stateLabel = nullptr;         ///< UDP 绑定状态标签
    QLabel* m_summaryLabel = nullptr;       ///< UDP 状态摘要
    QSpinBox* m_localPortSpin = nullptr;    ///< 本地端口输入框
    QLineEdit* m_remoteIpEdit = nullptr;    ///< 远端 IP 输入框
    QSpinBox* m_remotePortSpin = nullptr;   ///< 远端端口输入框
    QCheckBox* m_broadcastCheck = nullptr;  ///< 广播发送开关
    QComboBox* m_recentRemoteCombo = nullptr; ///< 最近远端列表
    QCheckBox* m_autoScrollCheck = nullptr; ///< 日志自动滚动开关
    QCheckBox* m_appendNewlineCheck = nullptr; ///< 文本发送时追加换行
    QLabel* m_byteCountLabel = nullptr;     ///< 当前发送字节数提示
    QPlainTextEdit* m_logEdit = nullptr;    ///< 收发日志
    QPlainTextEdit* m_sendEdit = nullptr;   ///< 发送输入框
    QCheckBox* m_hexSendCheck = nullptr;    ///< HEX 发送开关

    static constexpr int kMaxRecentRemotes = 30; ///< 最近远端列表上限，避免自动记录无限增长
};

} // namespace ComAssistant

#endif // COMASSISTANT_UDPWORKSPACEWIDGET_H
