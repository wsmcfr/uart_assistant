/**
 * @file CommunicationFactory.cpp
 * @brief 通信工厂实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "CommunicationFactory.h"

namespace ComAssistant {

//=============================================================================
// 智能指针版本
//=============================================================================

std::unique_ptr<ICommunication> CommunicationFactory::createSerial(const SerialConfig& config)
{
    return std::make_unique<SerialPort>(config);
}

std::unique_ptr<ICommunication> CommunicationFactory::createTcpClient(const NetworkConfig& config)
{
    return std::make_unique<TcpClient>(config);
}

std::unique_ptr<ICommunication> CommunicationFactory::createTcpServer(const NetworkConfig& config)
{
    return std::make_unique<TcpServer>(config);
}

std::unique_ptr<ICommunication> CommunicationFactory::createUdp(const NetworkConfig& config)
{
    return std::make_unique<UdpSocket>(config);
}

std::unique_ptr<ICommunication> CommunicationFactory::create(
    CommType type,
    const SerialConfig& serialConfig,
    const NetworkConfig& networkConfig)
{
    switch (type) {
        case CommType::Serial:
            return createSerial(serialConfig);
        case CommType::TcpClient:
            return createTcpClient(networkConfig);
        case CommType::TcpServer:
            return createTcpServer(networkConfig);
        case CommType::Udp:
            return createUdp(networkConfig);
        case CommType::Hid:
            // TODO: HID实现
            return nullptr;
        default:
            return nullptr;
    }
}

//=============================================================================
// Qt父子对象管理版本
//=============================================================================

ICommunication* CommunicationFactory::createSerial(const SerialConfig& config, QObject* parent)
{
    return new SerialPort(config, parent);
}

ICommunication* CommunicationFactory::createTcpClient(const NetworkConfig& config, QObject* parent)
{
    return new TcpClient(config, parent);
}

ICommunication* CommunicationFactory::createTcpServer(const NetworkConfig& config, QObject* parent)
{
    return new TcpServer(config, parent);
}

ICommunication* CommunicationFactory::createUdp(const NetworkConfig& config, QObject* parent)
{
    return new UdpSocket(config, parent);
}

//=============================================================================
// 工具方法
//=============================================================================

QString CommunicationFactory::typeName(CommType type)
{
    switch (type) {
        case CommType::Serial:    return QStringLiteral("Serial");
        case CommType::TcpClient: return QStringLiteral("TCP Client");
        case CommType::TcpServer: return QStringLiteral("TCP Server");
        case CommType::Udp:       return QStringLiteral("UDP");
        case CommType::Hid:       return QStringLiteral("HID");
        default:                  return QStringLiteral("Unknown");
    }
}

QList<CommType> CommunicationFactory::supportedTypes()
{
    return {
        CommType::Serial,
        CommType::TcpClient,
        CommType::TcpServer,
        CommType::Udp
        // CommType::Hid  // 暂未实现
    };
}

} // namespace ComAssistant
