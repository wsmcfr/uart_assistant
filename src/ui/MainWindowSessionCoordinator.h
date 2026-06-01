/**
 * @file MainWindowSessionCoordinator.h
 * @brief 主窗口会话恢复协调器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_MAINWINDOWSESSIONCOORDINATOR_H
#define COMASSISTANT_MAINWINDOWSESSIONCOORDINATOR_H

#include "communication/ICommunication.h"
#include "config/AppConfig.h"
#include "protocol/IProtocol.h"
#include "session/SessionData.h"

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace ComAssistant {

class HidReportWorkspaceWidget;
class NetworkSettingsWidget;
class QuickSendWidget;
class SerialSettingsWidget;
class TcpClientWorkspaceWidget;
class TcpServerWorkspaceWidget;
class UdpWorkspaceWidget;

/**
 * @brief 主窗口会话恢复协调器。
 *
 * 该类负责把 SessionData 中的配置应用到主窗口持有的配置对象、设置页、
 * 通信工作台和轻量工具栏控件。它不拥有 QWidget 生命周期，也不触发
 * MainWindow 的端口刷新、协议创建、窗口几何恢复或页面切换，这些仍由
 * MainWindow 统一编排。
 */
class MainWindowSessionCoordinator
{
public:
    /**
     * @brief 会话应用结果。
     */
    struct ApplyResult
    {
        ProtocolType restoredProtocolType = ProtocolType::Raw; ///< 校验后的协议类型。
    };

    /**
     * @brief 绑定设置页和通信工作台。
     */
    void setConfigurationWidgets(SerialSettingsWidget* serialSettings,
                                 NetworkSettingsWidget* networkSettings,
                                 TcpClientWorkspaceWidget* tcpClient,
                                 TcpServerWorkspaceWidget* tcpServer,
                                 UdpWorkspaceWidget* udp,
                                 HidReportWorkspaceWidget* hid);

    /**
     * @brief 绑定主工具栏中与会话恢复有关的控件。
     */
    void setToolbarWidgets(QComboBox* commTypeCombo,
                           QComboBox* portCombo,
                           QComboBox* baudCombo,
                           QLineEdit* ipEdit,
                           QSpinBox* portSpin,
                           QComboBox* displayModeCombo);

    /**
     * @brief 绑定快捷发送控件。
     */
    void setQuickSendWidget(QuickSendWidget* quickSendWidget);

    /**
     * @brief 应用会话中的配置和轻量 UI 状态。
     *
     * @param session 待恢复的会话数据。
     * @param currentCommType 主窗口当前通信类型引用，会被更新。
     * @param serialConfig 主窗口串口配置引用，会被更新。
     * @param networkConfig 主窗口网络配置引用，会被更新。
     * @param hidConfig 主窗口 HID 配置引用，会被更新。
     * @return 校验后的协议类型等后续 MainWindow 需要继续处理的信息。
     */
    ApplyResult applySession(const SessionData& session,
                             CommType& currentCommType,
                             SerialConfig& serialConfig,
                             NetworkConfig& networkConfig,
                             HidConfig& hidConfig) const;

    /**
     * @brief 在 MainWindow 刷新端口/设备列表后重新选中会话目标。
     *
     * @param currentCommType 当前通信类型。
     * @param serialConfig 当前串口配置。
     * @param hidConfig 当前 HID 配置。
     * @return 找到并选中目标时返回 true。
     */
    bool selectRestoredPort(CommType currentCommType,
                            const SerialConfig& serialConfig,
                            const HidConfig& hidConfig) const;

    /**
     * @brief 校验会话协议类型。
     * @param protocolValue 会话文件中的协议枚举整数。
     * @return 支持的协议类型；非法值回退 Raw。
     */
    static ProtocolType sanitizeProtocolType(int protocolValue);

private:
    /**
     * @brief 根据通信类型同步顶部网络工具栏 IP 和端口显示。
     */
    void applyNetworkToolbar(CommType currentCommType,
                             const NetworkConfig& networkConfig) const;

    SerialSettingsWidget* m_serialSettings = nullptr;     ///< 串口设置页，不拥有生命周期。
    NetworkSettingsWidget* m_networkSettings = nullptr;   ///< 网络设置页，不拥有生命周期。
    TcpClientWorkspaceWidget* m_tcpClientWorkspace = nullptr; ///< TCP Client 工作台，不拥有生命周期。
    TcpServerWorkspaceWidget* m_tcpServerWorkspace = nullptr; ///< TCP Server 工作台，不拥有生命周期。
    UdpWorkspaceWidget* m_udpWorkspace = nullptr;         ///< UDP 工作台，不拥有生命周期。
    HidReportWorkspaceWidget* m_hidWorkspace = nullptr;   ///< HID 工作台，不拥有生命周期。
    QuickSendWidget* m_quickSendWidget = nullptr;         ///< 快捷发送控件，不拥有生命周期。

    QComboBox* m_commTypeCombo = nullptr;    ///< 通信类型下拉框，不拥有生命周期。
    QComboBox* m_portCombo = nullptr;        ///< 串口/HID 设备下拉框，不拥有生命周期。
    QComboBox* m_baudCombo = nullptr;        ///< 波特率下拉框，不拥有生命周期。
    QLineEdit* m_ipEdit = nullptr;           ///< 顶部网络 IP 输入框，不拥有生命周期。
    QSpinBox* m_portSpin = nullptr;          ///< 顶部网络端口输入框，不拥有生命周期。
    QComboBox* m_displayModeCombo = nullptr; ///< 显示模式下拉框，不拥有生命周期。
};

} // namespace ComAssistant

#endif // COMASSISTANT_MAINWINDOWSESSIONCOORDINATOR_H
