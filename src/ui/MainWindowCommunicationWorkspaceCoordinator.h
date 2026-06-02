/**
 * @file MainWindowCommunicationWorkspaceCoordinator.h
 * @brief 主窗口通信工作台配置协调器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_MAINWINDOWCOMMUNICATIONWORKSPACECOORDINATOR_H
#define COMASSISTANT_MAINWINDOWCOMMUNICATIONWORKSPACECOORDINATOR_H

#include "communication/ICommunication.h"
#include "config/AppConfig.h"

namespace ComAssistant {

class CommunicationWorkspaceWidget;
class HidReportWorkspaceWidget;
class TcpClientWorkspaceWidget;
class TcpServerWorkspaceWidget;
class UdpWorkspaceWidget;

/**
 * @brief 主窗口通信工作台配置协调器。
 *
 * 该类只负责根据通信类型选择当前专用工作台，并在打开连接前把
 * TCP/UDP/HID 工作台中的配置同步回主窗口持有的配置对象。它不拥有
 * QWidget 生命周期，所有工作台仍由 MainWindow 创建和销毁。
 */
class MainWindowCommunicationWorkspaceCoordinator
{
public:
    /**
     * @brief 绑定 MainWindow 创建的通信专用工作台。
     *
     * @param tcpClient TCP Client 工作台。
     * @param tcpServer TCP Server 工作台。
     * @param udp UDP 工作台。
     * @param hid HID Report 工作台。
     */
    void setWorkspaces(TcpClientWorkspaceWidget* tcpClient,
                       TcpServerWorkspaceWidget* tcpServer,
                       UdpWorkspaceWidget* udp,
                       HidReportWorkspaceWidget* hid);

    /**
     * @brief 获取指定通信类型对应的非串口工作台。
     *
     * @param type 当前通信类型。
     * @return TCP/UDP/HID 工作台；串口模式或未绑定时返回 nullptr。
     */
    CommunicationWorkspaceWidget* currentWorkspace(CommType type) const;

    /**
     * @brief 从当前通信类型对应的工作台同步配置。
     *
     * 网络模式会整体替换 NetworkConfig；HID 模式只同步 Report 参数，
     * 不覆盖工具栏选择出来的设备路径、VID/PID、接口号和 usage 信息。
     *
     * @param type 当前通信类型。
     * @param networkConfig 待更新的网络配置。
     * @param hidConfig 待更新的 HID 配置。
     */
    void syncWorkspaceToConfig(CommType type,
                               NetworkConfig& networkConfig,
                               HidConfig& hidConfig) const;

private:
    TcpClientWorkspaceWidget* m_tcpClientWorkspace = nullptr; ///< TCP Client 工作台，不拥有生命周期。
    TcpServerWorkspaceWidget* m_tcpServerWorkspace = nullptr; ///< TCP Server 工作台，不拥有生命周期。
    UdpWorkspaceWidget* m_udpWorkspace = nullptr;             ///< UDP 工作台，不拥有生命周期。
    HidReportWorkspaceWidget* m_hidWorkspace = nullptr;       ///< HID Report 工作台，不拥有生命周期。
};

} // namespace ComAssistant

#endif // COMASSISTANT_MAINWINDOWCOMMUNICATIONWORKSPACECOORDINATOR_H
