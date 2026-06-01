/**
 * @file CommunicationWorkspaceWidget.h
 * @brief 通信类型工作台基类
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_COMMUNICATIONWORKSPACEWIDGET_H
#define COMASSISTANT_COMMUNICATIONWORKSPACEWIDGET_H

#include <QByteArray>
#include <QWidget>

namespace ComAssistant {

/**
 * @brief 通信类型工作台基类。
 *
 * 该类定义网络和 HID 专用工作台共享的状态、日志和发送信号接口。
 * 串口工作台继续使用现有模式组件，不强制继承该类。
 */
class CommunicationWorkspaceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommunicationWorkspaceWidget(QWidget* parent = nullptr);
    ~CommunicationWorkspaceWidget() override = default;

    /**
     * @brief 更新连接状态。
     * @param connected true 表示当前通信后端已打开。
     */
    virtual void setConnected(bool connected);

    /**
     * @brief 追加接收数据到工作台日志。
     * @param data 收到的原始字节。
     */
    virtual void appendReceivedData(const QByteArray& data);

    /**
     * @brief 追加发送数据到工作台日志。
     * @param data 已发送的原始字节。
     */
    virtual void appendSentData(const QByteArray& data);

    /**
     * @brief 清空工作台日志和临时状态。
     */
    virtual void clear();

signals:
    /**
     * @brief 请求主窗口通过当前通信对象发送数据。
     * @param data 待发送原始字节。
     */
    void sendDataRequested(const QByteArray& data);

protected:
    /**
     * @brief 把字节转换成紧凑 HEX 文本。
     * @param data 原始字节。
     * @return 大写 HEX 字符串。
     */
    QString bytesToHex(const QByteArray& data) const;

    /**
     * @brief 解析用户输入的 HEX 或文本。
     *
     * @param text 输入框文本。
     * @param hexMode true 表示按 HEX 解析；false 表示按 UTF-8 文本解析。
     * @return 解析后的字节。
     */
    QByteArray parsePayload(const QString& text, bool hexMode) const;

    bool m_connected = false;  ///< 当前通信后端连接状态
};

} // namespace ComAssistant

#endif // COMASSISTANT_COMMUNICATIONWORKSPACEWIDGET_H
