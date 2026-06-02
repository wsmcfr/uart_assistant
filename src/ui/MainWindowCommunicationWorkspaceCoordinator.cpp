/**
 * @file MainWindowCommunicationWorkspaceCoordinator.cpp
 * @brief 主窗口通信工作台配置协调器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "MainWindowCommunicationWorkspaceCoordinator.h"

#include "widgets/CommunicationWorkspaceWidget.h"
#include "widgets/HidReportWorkspaceWidget.h"
#include "widgets/TcpClientWorkspaceWidget.h"
#include "widgets/TcpServerWorkspaceWidget.h"
#include "widgets/UdpWorkspaceWidget.h"

namespace ComAssistant {

void MainWindowCommunicationWorkspaceCoordinator::setWorkspaces(
    TcpClientWorkspaceWidget* tcpClient,
    TcpServerWorkspaceWidget* tcpServer,
    UdpWorkspaceWidget* udp,
    HidReportWorkspaceWidget* hid)
{
    /*
     * 协调器只缓存 MainWindow 已创建的工作台指针，不改变 QWidget
     * 父子关系。这样抽离配置同步职责时，不会改变 UI 生命周期。
     */
    m_tcpClientWorkspace = tcpClient;
    m_tcpServerWorkspace = tcpServer;
    m_udpWorkspace = udp;
    m_hidWorkspace = hid;
}

CommunicationWorkspaceWidget*
MainWindowCommunicationWorkspaceCoordinator::currentWorkspace(CommType type) const
{
    /*
     * 串口仍使用原有串口显示模式页，不属于这些专用通信工作台。
     * 网络和 HID 模式返回对应页面，供 MainWindow 分发收发日志。
     */
    switch (type) {
        case CommType::TcpClient:
            return m_tcpClientWorkspace;
        case CommType::TcpServer:
            return m_tcpServerWorkspace;
        case CommType::Udp:
            return m_udpWorkspace;
        case CommType::Hid:
            return m_hidWorkspace;
        case CommType::Serial:
        default:
            return nullptr;
    }
}

void MainWindowCommunicationWorkspaceCoordinator::syncWorkspaceToConfig(
    CommType type,
    NetworkConfig& networkConfig,
    HidConfig& hidConfig) const
{
    /*
     * 打开连接前把当前工作台配置写回主窗口配置对象。HID 设备身份
     * 来自顶部设备选择框，因此这里只同步 Report 参数，避免覆盖
     * path、name、VID/PID、interface 和 usage 等设备定位字段。
     */
    switch (type) {
        case CommType::TcpClient:
            if (m_tcpClientWorkspace) {
                networkConfig = m_tcpClientWorkspace->config();
            }
            break;
        case CommType::TcpServer:
            if (m_tcpServerWorkspace) {
                networkConfig = m_tcpServerWorkspace->config();
            }
            break;
        case CommType::Udp:
            if (m_udpWorkspace) {
                networkConfig = m_udpWorkspace->config();
            }
            break;
        case CommType::Hid:
            if (m_hidWorkspace) {
                const HidConfig reportConfig = m_hidWorkspace->config();
                hidConfig.inputReportLength = reportConfig.inputReportLength;
                hidConfig.outputReportLength = reportConfig.outputReportLength;
                hidConfig.featureReportLength = reportConfig.featureReportLength;
                hidConfig.firstDataIsLength = reportConfig.firstDataIsLength;
                hidConfig.outReportId = reportConfig.outReportId;
                hidConfig.featureReportId = reportConfig.featureReportId;
                hidConfig.removeInReportId = reportConfig.removeInReportId;
            }
            break;
        case CommType::Serial:
        default:
            break;
    }
}

} // namespace ComAssistant
