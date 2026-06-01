/**
 * @file MainWindowCommunicationController.cpp
 * @brief 主窗口通信生命周期控制器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "MainWindowCommunicationController.h"

#include "communication/CommunicationFactory.h"
#include "communication/TcpClient.h"
#include "communication/TcpServer.h"
#include "communication/UdpSocket.h"
#include "utils/Logger.h"

namespace ComAssistant {

MainWindowCommunicationController::MainWindowCommunicationController(QObject* parent)
    : QObject(parent)
    , m_factory(defaultCommunicationFactory())
{
    /*
     * 默认使用生产通信工厂。测试路径可通过 setCommunicationFactory()
     * 替换为假对象，从而不依赖真实串口、网络或 HID 设备。
     */
}

MainWindowCommunicationController::~MainWindowCommunicationController()
{
    /*
     * 控制器拥有当前通信对象的生命周期。析构时主动关闭，确保底层
     * 端口、Socket 或 HID 句柄不会依赖 unique_ptr 析构的隐式顺序。
     */
    closeCurrent();
}

void MainWindowCommunicationController::setCommunicationFactory(CommunicationFactoryFn factory)
{
    /*
     * 空工厂会让控制器无法创建通信对象。这里回退到默认工厂，
     * 让生产路径在误传空函数时仍保持可用。
     */
    m_factory = factory ? std::move(factory) : defaultCommunicationFactory();
}

void MainWindowCommunicationController::setTcpClientReconnectOptionsProvider(
    TcpClientReconnectOptionsProvider provider)
{
    /*
     * 控制器不直接读取 MainWindow 的 UI 控件，只保存一个轻量回调。
     * 创建 TCP Client 时再调用回调，避免 UI 层和通信生命周期层强耦合。
     */
    m_tcpClientReconnectOptionsProvider = std::move(provider);
}

bool MainWindowCommunicationController::openCurrent(CommType type,
                                                   const SerialConfig& serialConfig,
                                                   const NetworkConfig& networkConfig,
                                                   const HidConfig& hidConfig)
{
    /*
     * 先关闭旧对象再创建新对象，保证一次只存在一个活动通信实例。
     * 打开失败时保留新对象，调用方可以继续读取 lastError() 并显示
     * 与该通信实例相关的错误信息。
     */
    closeCurrent();
    m_lastError.clear();

    m_communication = m_factory(type, serialConfig, networkConfig, hidConfig);
    if (!m_communication) {
        m_connected = false;
        m_lastError = tr("无法创建通信实例");
        return false;
    }

    applyTcpClientOptionsIfNeeded();
    bindCurrentCommunicationSignals();

    if (!m_communication->open()) {
        m_connected = false;
        m_lastError = m_communication->lastError();
        if (m_lastError.isEmpty()) {
            m_lastError = tr("无法打开连接");
        }
        return false;
    }

    m_connected = true;
    return true;
}

void MainWindowCommunicationController::closeCurrent()
{
    /*
     * close() 可能触发底层 connectionStatusChanged(false)，因此先让
     * 底层对象完成关闭流程，再释放对象并同步控制器自己的状态缓存。
     */
    if (!m_communication) {
        m_connected = false;
        return;
    }

    m_communication->close();
    m_communication.reset();
    m_connected = false;
}

bool MainWindowCommunicationController::sendData(const QByteArray& data)
{
    /*
     * 发送入口统一检查控制器状态，避免 MainWindow 或各工作台直接
     * 触碰空通信对象。底层写入失败时保留错误，方便 UI 复用提示。
     */
    if (!m_communication || !m_connected) {
        m_lastError = tr("未连接，无法发送数据");
        return false;
    }

    const qint64 written = m_communication->write(data);
    if (written < 0) {
        m_lastError = m_communication->lastError();
        if (m_lastError.isEmpty()) {
            m_lastError = tr("发送失败");
        }
        LOG_ERROR(QString("Send failed: %1").arg(m_lastError));
        return false;
    }

    m_lastError.clear();
    return true;
}

ICommunication* MainWindowCommunicationController::communication() const
{
    /*
     * 当前阶段仍允许 MainWindow 访问类型特化能力，例如 TCP Server
     * 定向发送、UDP 指定目标发送和 HID Feature Report。后续拆分
     * 可以继续把这些特化能力迁移到更细的路由器。
     */
    return m_communication.get();
}

bool MainWindowCommunicationController::isConnected() const
{
    /*
     * 返回控制器缓存的连接状态。缓存由 open()/close() 结果和底层
     * connectionStatusChanged 信号共同维护，供 UI 快速判断发送条件。
     */
    return m_connected;
}

QString MainWindowCommunicationController::lastError() const
{
    /*
     * 返回最近一次控制器层或底层通信层错误。调用方只负责展示，
     * 不需要知道错误来自创建、打开、发送还是底层信号。
     */
    return m_lastError;
}

MainWindowCommunicationController::CommunicationFactoryFn
MainWindowCommunicationController::defaultCommunicationFactory()
{
    /*
     * 默认工厂集中转发到 CommunicationFactory，保留现有通信创建
     * 逻辑不变；测试注入的工厂则绕过真实设备依赖。
     */
    return [](CommType type,
              const SerialConfig& serialConfig,
              const NetworkConfig& networkConfig,
              const HidConfig& hidConfig) {
        return CommunicationFactory::create(type, serialConfig, networkConfig, hidConfig);
    };
}

void MainWindowCommunicationController::bindCurrentCommunicationSignals()
{
    /*
     * 通用信号直接转发给 MainWindow，类型特化信号只在当前对象确实
     * 是对应实现时绑定。这样第一阶段拆分不会改变 UI 层原有路由。
     */
    if (!m_communication) {
        return;
    }

    connect(m_communication.get(), &ICommunication::dataReceived,
            this, &MainWindowCommunicationController::dataReceived);
    connect(m_communication.get(), &ICommunication::dataSent,
            this, &MainWindowCommunicationController::dataSent);
    connect(m_communication.get(), &ICommunication::connectionStatusChanged,
            this, [this](bool connected) {
        m_connected = connected;
        emit connectionStatusChanged(connected);
    });
    connect(m_communication.get(), &ICommunication::errorOccurred,
            this, [this](const QString& error) {
        m_lastError = error;
        emit errorOccurred(error);
    });

    if (auto* tcpServer = dynamic_cast<TcpServer*>(m_communication.get())) {
        connect(tcpServer, &TcpServer::clientConnected,
                this, &MainWindowCommunicationController::tcpServerClientConnected);
        connect(tcpServer, &TcpServer::clientDisconnected,
                this, &MainWindowCommunicationController::tcpServerClientDisconnected);
    }

    if (auto* udpSocket = dynamic_cast<UdpSocket*>(m_communication.get())) {
        connect(udpSocket, &UdpSocket::datagramReceived,
                this, [this](const QByteArray& data, const QString& senderIp, int senderPort) {
            Q_UNUSED(data)
            emit udpDatagramRemoteReceived(senderIp, senderPort);
        });
    }
}

void MainWindowCommunicationController::applyTcpClientOptionsIfNeeded()
{
    /*
     * 自动重连只属于 TCP Client。控制器在创建对象后、open() 前应用
     * 设置，保证首次连接尝试和后续断线重连使用同一组选项。
     */
    auto* tcpClient = dynamic_cast<TcpClient*>(m_communication.get());
    if (!tcpClient || !m_tcpClientReconnectOptionsProvider) {
        return;
    }

    const TcpClientReconnectOptions options = m_tcpClientReconnectOptionsProvider();
    tcpClient->setAutoReconnect(options.enabled, options.intervalMs);
}

} // namespace ComAssistant
