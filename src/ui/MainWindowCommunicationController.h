/**
 * @file MainWindowCommunicationController.h
 * @brief 主窗口通信生命周期控制器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_MAINWINDOWCOMMUNICATIONCONTROLLER_H
#define COMASSISTANT_MAINWINDOWCOMMUNICATIONCONTROLLER_H

#include "communication/ICommunication.h"
#include "communication/SendDispatcher.h"
#include "config/AppConfig.h"

#include <QObject>
#include <QMetaObject>
#include <QString>

#include <functional>
#include <memory>

namespace ComAssistant {

/**
 * @brief 主窗口通信生命周期控制器。
 *
 * 该类负责从 MainWindow 中接管通信对象的创建、打开、关闭和发送。
 * UI 状态刷新、工作台配置同步、接收数据展示和绘图路由仍由 MainWindow
 * 负责，从而让第一批拆分保持行为稳定且风险可控。
 */
class MainWindowCommunicationController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 通信对象工厂函数类型。
     *
     * 测试可注入假通信对象；生产环境默认调用 CommunicationFactory。
     */
    using CommunicationFactoryFn = std::function<std::unique_ptr<ICommunication>(
        CommType type,
        const SerialConfig& serialConfig,
        const NetworkConfig& networkConfig,
        const HidConfig& hidConfig)>;

    /**
     * @brief TCP Client 自动重连选项。
     */
    struct TcpClientReconnectOptions
    {
        bool enabled = false;       ///< 是否启用自动重连。
        int intervalMs = 3000;      ///< 自动重连间隔，单位毫秒。
    };

    /**
     * @brief TCP Client 自动重连选项提供函数类型。
     */
    using TcpClientReconnectOptionsProvider = std::function<TcpClientReconnectOptions()>;

    /**
     * @brief 文件传输异步发送完成回调。
     * @param success true 表示当前文件传输块已经完成本地发送排空。
     * @param errorMessage success 为 false 时的失败原因。
     */
    using FileTransferSendCallback = std::function<void(bool success, const QString& errorMessage)>;

    explicit MainWindowCommunicationController(QObject* parent = nullptr);
    ~MainWindowCommunicationController() override;

    /**
     * @brief 注入通信对象工厂。
     * @param factory 创建通信对象的函数。
     */
    void setCommunicationFactory(CommunicationFactoryFn factory);

    /**
     * @brief 注入 TCP Client 自动重连选项提供函数。
     * @param provider 返回当前 UI 工作台自动重连配置的函数。
     */
    void setTcpClientReconnectOptionsProvider(TcpClientReconnectOptionsProvider provider);

    /**
     * @brief 使用当前配置创建并打开通信对象。
     *
     * 主要流程：先关闭旧通信对象，再通过工厂创建新对象，随后绑定底层
     * 信号并调用 open()。打开失败时保留对象和错误信息，方便调用方读取
     * lastError() 并显示给用户。
     *
     * @param type 通信类型。
     * @param serialConfig 串口配置。
     * @param networkConfig 网络配置。
     * @param hidConfig HID 配置。
     * @return 打开成功返回 true；创建或打开失败返回 false。
     */
    bool openCurrent(CommType type,
                     const SerialConfig& serialConfig,
                     const NetworkConfig& networkConfig,
                     const HidConfig& hidConfig);

    /**
     * @brief 关闭并释放当前通信对象。
     */
    void closeCurrent();

    /**
     * @brief 发送数据到当前通信对象。
     * @param data 待发送原始字节。
     * @return 成功写入或成功进入发送队列返回 true；未连接、入队失败或写入失败返回 false。
     */
    bool sendData(const QByteArray& data);

    /**
     * @brief 发送文件传输数据，并在本地发送排空后异步回调。
     * @param data 当前文件传输块或 OTA 包。
     * @param callback 排空完成或失败后调用的回调。
     * @return 成功发起发送和排空等待返回 true；未连接、写入失败或已有等待返回 false。
     *
     * 普通发送只需要知道 write() 是否接受数据；文件传输进度条则必须等
     * 串口缓冲按波特率真正排空后才能推进。该函数专门用于 Raw/OTA 文件
     * 传输，避免改变普通手动发送、快捷发送和脚本发送的即时反馈行为。
     */
    bool sendFileTransferDataAsync(const QByteArray& data,
                                   FileTransferSendCallback callback);

    /**
     * @brief 重试当前待发送队列。
     *
     * 底层 write() 失败时，调度器会保留队首任务。连接恢复或用户触发
     * 重试时调用该函数，可以从同一个队首 payload 继续发送。
     *
     * @return 队列已清空或至少成功推进返回 true；仍有失败任务返回 false。
     */
    bool retryPendingSends();

    /**
     * @brief 获取待发送任务数量。
     * @return 发送队列中尚未成功写出的任务数。
     */
    int pendingSendCount() const;

    /**
     * @brief 获取当前通信对象。
     * @return 当前通信对象指针；没有对象时返回 nullptr。
     */
    ICommunication* communication() const;

    /**
     * @brief 判断当前通信是否处于连接状态。
     * @return 已连接返回 true。
     */
    bool isConnected() const;

    /**
     * @brief 获取最近一次控制器或通信对象错误。
     * @return 错误文本；无错误时为空。
     */
    QString lastError() const;

signals:
    /**
     * @brief 转发底层通信收到的数据。
     */
    void dataReceived(const QByteArray& data);

    /**
     * @brief 转发底层通信已发送的数据。
     */
    void dataSent(const QByteArray& data);

    /**
     * @brief 转发或生成连接状态变化。
     */
    void connectionStatusChanged(bool connected);

    /**
     * @brief 转发底层通信错误。
     */
    void errorOccurred(const QString& error);

    /**
     * @brief 转发 TCP Server 客户端连接事件。
     */
    void tcpServerClientConnected(const QString& clientId);

    /**
     * @brief 转发 TCP Server 客户端断开事件。
     */
    void tcpServerClientDisconnected(const QString& clientId);

    /**
     * @brief 转发 UDP 最近远端事件。
     */
    void udpDatagramRemoteReceived(const QString& senderIp, int senderPort);

private:
    /**
     * @brief 创建默认通信对象工厂。
     * @return 默认工厂函数。
     */
    static CommunicationFactoryFn defaultCommunicationFactory();

    /**
     * @brief 绑定当前通信对象的通用信号和类型特化信号。
     */
    void bindCurrentCommunicationSignals();

    /**
     * @brief 对 TCP Client 应用自动重连设置。
     */
    void applyTcpClientOptionsIfNeeded();

    /**
     * @brief 把发送调度器绑定到当前通信对象。
     */
    void bindSendDispatcher();

    /**
     * @brief 完成当前文件传输异步发送请求。
     * @param requestId 请求编号，用于忽略旧连接或旧请求的迟到信号。
     * @param success drain 是否成功。
     * @param errorMessage 失败原因。
     */
    void completeFileTransferSend(qint64 requestId,
                                  bool success,
                                  const QString& errorMessage);

private:
    CommunicationFactoryFn m_factory;  ///< 通信对象创建函数。
    TcpClientReconnectOptionsProvider m_tcpClientReconnectOptionsProvider; ///< TCP 自动重连配置来源。
    std::unique_ptr<ICommunication> m_communication; ///< 当前通信对象。
    SendDispatcher m_sendDispatcher;   ///< 统一发送队列调度器。
    bool m_connected = false;          ///< 控制器记录的连接状态。
    QString m_lastError;               ///< 最近一次错误信息。
    qint64 m_nextFileTransferRequestId = 1; ///< 下一个文件传输异步发送请求编号。
    qint64 m_activeFileTransferRequestId = 0; ///< 当前等待 drain 的文件传输请求编号。
    QByteArray m_activeFileTransferPayload; ///< 当前等待 drain 的文件传输 payload，成功后用于统计 TX。
    FileTransferSendCallback m_fileTransferSendCallback; ///< 当前等待 drain 的回调。
    QMetaObject::Connection m_fileTransferDrainConnection; ///< 当前文件发送 drain 信号连接。
};

} // namespace ComAssistant

#endif // COMASSISTANT_MAINWINDOWCOMMUNICATIONCONTROLLER_H
