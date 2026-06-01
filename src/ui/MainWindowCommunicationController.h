/**
 * @file MainWindowCommunicationController.h
 * @brief 主窗口通信生命周期控制器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_MAINWINDOWCOMMUNICATIONCONTROLLER_H
#define COMASSISTANT_MAINWINDOWCOMMUNICATIONCONTROLLER_H

#include "communication/ICommunication.h"
#include "config/AppConfig.h"

#include <QObject>
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
     * @return 成功写入返回 true；未连接或写入失败返回 false。
     */
    bool sendData(const QByteArray& data);

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

private:
    CommunicationFactoryFn m_factory;  ///< 通信对象创建函数。
    TcpClientReconnectOptionsProvider m_tcpClientReconnectOptionsProvider; ///< TCP 自动重连配置来源。
    std::unique_ptr<ICommunication> m_communication; ///< 当前通信对象。
    bool m_connected = false;          ///< 控制器记录的连接状态。
    QString m_lastError;               ///< 最近一次错误信息。
};

} // namespace ComAssistant

#endif // COMASSISTANT_MAINWINDOWCOMMUNICATIONCONTROLLER_H
