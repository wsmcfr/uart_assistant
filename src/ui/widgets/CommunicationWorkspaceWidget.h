/**
 * @file CommunicationWorkspaceWidget.h
 * @brief 通信类型工作台基类
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_COMMUNICATIONWORKSPACEWIDGET_H
#define COMASSISTANT_COMMUNICATIONWORKSPACEWIDGET_H

#include <QByteArray>
#include <QString>
#include <QWidget>

class QPlainTextEdit;

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
     * @brief 通信工作台日志默认保留块数。
     *
     * TCP/UDP/HID 专用工作台的日志只是辅助历史，不能像主接收区那样长期
     * 保留大量内容；设置较小上限可以避免 QTextDocument 随运行时间持续增长。
     */
    static constexpr int kDefaultLogBlockLimit = 2000;

    /**
     * @brief 配置日志控件的轻量历史策略。
     *
     * 主要流程：关闭撤销栈，设置最大 block 数；Qt 会在追加新行时自动移除
     * 最早的 block，从而让网络/HID 历史保持有界。
     *
     * @param logEdit 待配置的日志控件。
     * @param blockLimit 最大保留块数，必须为正数。
     */
    void configureLogEdit(QPlainTextEdit* logEdit,
                          int blockLimit = kDefaultLogBlockLimit) const;

    /**
     * @brief 把字节转换成紧凑 HEX 文本。
     * @param data 原始字节。
     * @return 大写 HEX 字符串。
     */
    QString bytesToHex(const QByteArray& data) const;

    /**
     * @brief 规范化用户输入的 HEX 文本。
     *
     * 主要流程：移除 0x 前缀和非十六进制字符；如果剩余位数为奇数，
     * 在最前面补 0；最后按两个字符一组输出大写 HEX，便于阅读和复制。
     *
     * @param text 用户输入的任意 HEX 文本。
     * @return 规范化后的分组大写 HEX 文本。
     */
    QString normalizeHexText(const QString& text) const;

    /**
     * @brief 生成 payload 的文本预览。
     *
     * 主要流程：按 UTF-8 尝试转换为文本；控制字符替换为点号；
     * 超过 maxChars 时截断并追加省略号，避免日志行被长报文撑开。
     *
     * @param data 原始 payload 字节。
     * @param maxChars 最多保留的预览字符数。
     * @return 适合显示在日志行里的文本摘要。
     */
    QString payloadPreview(const QByteArray& data, int maxChars = 48) const;

    /**
     * @brief 生成统一收发日志行。
     *
     * 日志固定包含时间、方向、数据类型、字节数、HEX 和文本摘要，
     * 让 TCP、UDP、HID 工作台的历史记录保持同一阅读节奏。
     *
     * @param direction 方向，例如 RX 或 TX。
     * @param type 数据类型，例如 TCP、UDP、HID Output。
     * @param data 原始字节。
     * @return 格式化后的单行日志。
     */
    QString formatLogLine(const QString& direction,
                          const QString& type,
                          const QByteArray& data) const;

    /**
     * @brief 追加统一日志行到文本控件。
     *
     * @param logEdit 目标日志控件。
     * @param direction 方向，例如 RX 或 TX。
     * @param type 数据类型，例如 TCP、UDP、HID Feature。
     * @param data 原始字节。
     * @param autoScroll true 表示追加后滚动到底部；false 表示保留当前滚动位置。
     */
    void appendLogLine(QPlainTextEdit* logEdit,
                       const QString& direction,
                       const QString& type,
                       const QByteArray& data,
                       bool autoScroll) const;

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
