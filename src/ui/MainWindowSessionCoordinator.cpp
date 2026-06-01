/**
 * @file MainWindowSessionCoordinator.cpp
 * @brief 主窗口会话恢复协调器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "MainWindowSessionCoordinator.h"

#include "widgets/HidReportWorkspaceWidget.h"
#include "widgets/NetworkSettingsWidget.h"
#include "widgets/QuickSendWidget.h"
#include "widgets/SerialSettingsWidget.h"
#include "widgets/TcpClientWorkspaceWidget.h"
#include "widgets/TcpServerWorkspaceWidget.h"
#include "widgets/UdpWorkspaceWidget.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpinBox>

namespace ComAssistant {

void MainWindowSessionCoordinator::setConfigurationWidgets(
    SerialSettingsWidget* serialSettings,
    NetworkSettingsWidget* networkSettings,
    TcpClientWorkspaceWidget* tcpClient,
    TcpServerWorkspaceWidget* tcpServer,
    UdpWorkspaceWidget* udp,
    HidReportWorkspaceWidget* hid)
{
    /*
     * 协调器只保存 MainWindow 创建的控件指针，不调整 QObject 父子关系，
     * 因而不会改变原有 UI 生命周期。
     */
    m_serialSettings = serialSettings;
    m_networkSettings = networkSettings;
    m_tcpClientWorkspace = tcpClient;
    m_tcpServerWorkspace = tcpServer;
    m_udpWorkspace = udp;
    m_hidWorkspace = hid;
}

void MainWindowSessionCoordinator::setToolbarWidgets(QComboBox* commTypeCombo,
                                                     QComboBox* portCombo,
                                                     QComboBox* baudCombo,
                                                     QLineEdit* ipEdit,
                                                     QSpinBox* portSpin,
                                                     QComboBox* displayModeCombo)
{
    m_commTypeCombo = commTypeCombo;
    m_portCombo = portCombo;
    m_baudCombo = baudCombo;
    m_ipEdit = ipEdit;
    m_portSpin = portSpin;
    m_displayModeCombo = displayModeCombo;
}

void MainWindowSessionCoordinator::setQuickSendWidget(QuickSendWidget* quickSendWidget)
{
    m_quickSendWidget = quickSendWidget;
}

MainWindowSessionCoordinator::ApplyResult MainWindowSessionCoordinator::applySession(
    const SessionData& session,
    CommType& currentCommType,
    SerialConfig& serialConfig,
    NetworkConfig& networkConfig,
    HidConfig& hidConfig) const
{
    ApplyResult result;

    /*
     * 先更新 MainWindow 持有的配置对象，再把配置镜像到设置页和通信
     * 工作台，保持原 applySessionDataToUi() 的恢复顺序。
     */
    currentCommType = session.commType;
    serialConfig = session.serialConfig;
    networkConfig = session.networkConfig;
    hidConfig = session.hidConfig;

    if (m_serialSettings) {
        m_serialSettings->setConfig(serialConfig);
    }
    if (m_networkSettings) {
        m_networkSettings->setConfig(networkConfig);
    }
    if (m_tcpClientWorkspace) {
        m_tcpClientWorkspace->setConfig(networkConfig);
    }
    if (m_tcpServerWorkspace) {
        m_tcpServerWorkspace->setConfig(networkConfig);
    }
    if (m_udpWorkspace) {
        m_udpWorkspace->setConfig(networkConfig);
    }
    if (m_hidWorkspace) {
        m_hidWorkspace->setConfig(hidConfig);
    }

    if (m_commTypeCombo) {
        const int commIndex = m_commTypeCombo->findData(static_cast<int>(currentCommType));
        if (commIndex >= 0) {
            QSignalBlocker blocker(m_commTypeCombo);
            m_commTypeCombo->setCurrentIndex(commIndex);
        }
    }

    if (m_baudCombo) {
        const int baudIndex = m_baudCombo->findData(serialConfig.baudRate);
        if (baudIndex >= 0) {
            m_baudCombo->setCurrentIndex(baudIndex);
        } else {
            m_baudCombo->setCurrentText(QString::number(serialConfig.baudRate));
        }
    }

    applyNetworkToolbar(currentCommType, networkConfig);

    if (m_displayModeCombo) {
        const int displayIndex = m_displayModeCombo->findData(session.displayMode);
        if (displayIndex >= 0) {
            m_displayModeCombo->setCurrentIndex(displayIndex);
        }
    }

    if (m_quickSendWidget) {
        m_quickSendWidget->clearAll();
        for (const QuickSendItem& item : session.quickSendItems) {
            m_quickSendWidget->addItem(item);
        }
    }

    result.restoredProtocolType = sanitizeProtocolType(session.protocolType);
    return result;
}

bool MainWindowSessionCoordinator::selectRestoredPort(CommType currentCommType,
                                                      const SerialConfig& serialConfig,
                                                      const HidConfig& hidConfig) const
{
    if (!m_portCombo) {
        return false;
    }

    if (currentCommType == CommType::Serial && !serialConfig.portName.isEmpty()) {
        int portIndex = m_portCombo->findData(serialConfig.portName);
        if (portIndex < 0) {
            portIndex = m_portCombo->findText(serialConfig.portName);
        }
        if (portIndex >= 0) {
            m_portCombo->setCurrentIndex(portIndex);
            return true;
        }
    }

    if (currentCommType == CommType::Hid && !hidConfig.path.isEmpty()) {
        int hidIndex = m_portCombo->findData(hidConfig.path);
        if (hidIndex < 0) {
            hidIndex = m_portCombo->findText(hidConfig.name);
        }
        if (hidIndex >= 0) {
            m_portCombo->setCurrentIndex(hidIndex);
            return true;
        }
    }

    return false;
}

ProtocolType MainWindowSessionCoordinator::sanitizeProtocolType(int protocolValue)
{
    const ProtocolType protocolType = static_cast<ProtocolType>(protocolValue);
    switch (protocolType) {
        case ProtocolType::Raw:
        case ProtocolType::Ascii:
        case ProtocolType::Hex:
        case ProtocolType::Modbus:
        case ProtocolType::Custom:
        case ProtocolType::EasyHex:
        case ProtocolType::TextPlot:
        case ProtocolType::StampPlot:
        case ProtocolType::CsvPlot:
        case ProtocolType::JustFloat:
            return protocolType;
        default:
            return ProtocolType::Raw;
    }
}

void MainWindowSessionCoordinator::applyNetworkToolbar(
    CommType currentCommType,
    const NetworkConfig& networkConfig) const
{
    if (m_ipEdit) {
        switch (currentCommType) {
            case CommType::TcpClient:
                m_ipEdit->setText(networkConfig.serverIp);
                break;
            case CommType::TcpServer:
                m_ipEdit->setText(QStringLiteral("0.0.0.0"));
                break;
            case CommType::Udp:
                m_ipEdit->setText(networkConfig.remoteIp.isEmpty()
                                      ? QStringLiteral("127.0.0.1")
                                      : networkConfig.remoteIp);
                break;
            case CommType::Serial:
            case CommType::Hid:
            default:
                break;
        }
    }

    if (m_portSpin) {
        switch (currentCommType) {
            case CommType::TcpClient:
                m_portSpin->setValue(networkConfig.serverPort);
                break;
            case CommType::TcpServer:
                m_portSpin->setValue(networkConfig.listenPort);
                break;
            case CommType::Udp:
                m_portSpin->setValue(networkConfig.remotePort > 0
                                         ? networkConfig.remotePort
                                         : networkConfig.listenPort);
                break;
            case CommType::Serial:
            case CommType::Hid:
            default:
                break;
        }
    }
}

} // namespace ComAssistant
