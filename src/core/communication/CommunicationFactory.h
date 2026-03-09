/**
 * @file CommunicationFactory.h
 * @brief 通信工厂 - 创建各种通信实例
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_COMMUNICATIONFACTORY_H
#define COMASSISTANT_COMMUNICATIONFACTORY_H

#include "ICommunication.h"
#include "SerialPort.h"
#include "TcpClient.h"
#include "TcpServer.h"
#include "UdpSocket.h"
#include "config/AppConfig.h"
#include <memory>

namespace ComAssistant {

/**
 * @brief 通信工厂类
 *
 * 使用智能指针管理内存，避免内存泄漏
 */
class CommunicationFactory {
public:
    //=========================================================================
    // 智能指针版本（推荐）
    //=========================================================================

    /**
     * @brief 创建串口实例
     * @param config 串口配置
     * @return unique_ptr<ICommunication>
     */
    static std::unique_ptr<ICommunication> createSerial(const SerialConfig& config);

    /**
     * @brief 创建TCP客户端实例
     * @param config 网络配置
     * @return unique_ptr<ICommunication>
     */
    static std::unique_ptr<ICommunication> createTcpClient(const NetworkConfig& config);

    /**
     * @brief 创建TCP服务端实例
     * @param config 网络配置
     * @return unique_ptr<ICommunication>
     */
    static std::unique_ptr<ICommunication> createTcpServer(const NetworkConfig& config);

    /**
     * @brief 创建UDP实例
     * @param config 网络配置
     * @return unique_ptr<ICommunication>
     */
    static std::unique_ptr<ICommunication> createUdp(const NetworkConfig& config);

    /**
     * @brief 根据类型创建通信实例
     * @param type 通信类型
     * @param serialConfig 串口配置（type为Serial时使用）
     * @param networkConfig 网络配置（type为网络类型时使用）
     * @return unique_ptr<ICommunication>
     */
    static std::unique_ptr<ICommunication> create(
        CommType type,
        const SerialConfig& serialConfig = SerialConfig(),
        const NetworkConfig& networkConfig = NetworkConfig()
    );

    //=========================================================================
    // Qt父子对象管理版本
    //=========================================================================

    /**
     * @brief 创建串口实例（Qt父子对象管理）
     * @param config 串口配置
     * @param parent 父对象（负责delete）
     * @return ICommunication* (由parent管理生命周期)
     */
    static ICommunication* createSerial(const SerialConfig& config, QObject* parent);

    /**
     * @brief 创建TCP客户端实例（Qt父子对象管理）
     */
    static ICommunication* createTcpClient(const NetworkConfig& config, QObject* parent);

    /**
     * @brief 创建TCP服务端实例（Qt父子对象管理）
     */
    static ICommunication* createTcpServer(const NetworkConfig& config, QObject* parent);

    /**
     * @brief 创建UDP实例（Qt父子对象管理）
     */
    static ICommunication* createUdp(const NetworkConfig& config, QObject* parent);

    //=========================================================================
    // 工具方法
    //=========================================================================

    /**
     * @brief 获取通信类型名称
     */
    static QString typeName(CommType type);

    /**
     * @brief 获取支持的通信类型列表
     */
    static QList<CommType> supportedTypes();

private:
    CommunicationFactory() = delete;
};

} // namespace ComAssistant

#endif // COMASSISTANT_COMMUNICATIONFACTORY_H
