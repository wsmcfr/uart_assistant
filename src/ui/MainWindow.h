/**
 * @file MainWindow.h
 * @brief 主窗口
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_MAINWINDOW_H
#define COMASSISTANT_MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QTabWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTranslator>
#include <QUrl>
#include <memory>

#include "communication/ICommunication.h"
#include "communication/CommunicationFactory.h"
#include "protocol/IProtocol.h"
#include "config/AppConfig.h"
#include "modes/IModeWidget.h"
#include "session/SessionManager.h"
#include "session/SessionData.h"

namespace ComAssistant {

/**
 * @brief 通信显示模式
 */
enum class DisplayMode {
    Serial,     ///< 串口模式 - 默认模式，实时收发显示
    Terminal,   ///< 终端模式 - 类似命令行终端风格
    Frame,      ///< 帧模式 - 按帧解析显示
    Debug       ///< 调试模式 - 带时间戳和方向标记
};

// 前向声明
class SideNavigationBar;
class SerialSettingsWidget;
class NetworkSettingsWidget;
class SendWidget;
class TabbedReceiveWidget;
class DataStatistics;
class HelpDialog;
class QuickSendWidget;
class ToolboxDialog;
class ScriptEditorDialog;
class ExportDialog;
class FileTransferDialog;
class MacroWidget;
class MultiPortWidget;
class ModbusAnalyzerWidget;
class SearchWidget;
class DataWindowManager;
class DataWindowConfigDialog;
class ControlPanel;
class DataTableWidget;
class AppUpdateChecker;

// 模式组件前向声明
class SerialModeWidget;
class TerminalModeWidget;
class FrameModeWidget;
class DebugModeWidget;

/**
 * @brief 主窗口类
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    // 连接操作
    void onConnectClicked();
    void onDisconnectClicked();

    // 数据处理
    void onDataReceived(const QByteArray& data);
    void onDataSent(const QByteArray& data);
    void onSendData(const QByteArray& data);

    // 状态变化
    void onConnectionStatusChanged(bool connected);
    void onErrorOccurred(const QString& error);

    // 通信类型切换
    void onCommTypeChanged(int index);

    // 菜单操作
    void onNewSession();
    void onSaveSession();
    void onLoadSession();
    void onExportData();
    void onClearAll();

    // 设置
    void onSettings();
    void onCheckForUpdates();
    void onAbout();
    void onHelp();
    void onToolbox();
    void onScriptEditor();

    // 新功能
    void onFileTransfer();
    void onIAPUpgrade();
    void onMacroManager();
    void onMultiPortManager();
    void onModbusAnalyzer();
    void onDataSearch();
    void onDataWindowConfig();  ///< 数据分窗配置
    void onControlPanelToggled();  ///< 控件面板显示切换
    void onDataTableToggled();  ///< 数据表格视图切换

    // 主题
    void onThemeToggled();

    // 语言切换
    void onLanguageChanged(const QString& language);

    // 工具栏串口控制
    void onToolbarPortChanged(int index);
    void onToolbarBaudChanged(int index);
    void onOpenPortClicked();
    void refreshPorts();

    // 状态栏更新
    void updateStatusBar();

    // 绘图相关
    void onNewPlotWindow();
    void onCloseAllPlotWindows();
    void onProtocolChanged(int index);

    // 显示模式
    void onDisplayModeChanged(int index);

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void applyDialogSettings();
    void applyDisplaySettings();

    // 创建通信实例
    void createCommunication();
    void destroyCommunication();

    // 主题应用
    void applyTheme(const QString& theme);

    // 语言切换
    void loadLanguage(const QString& language);
    void retranslateUi();

    // 应用更新
    void scheduleAutoUpdateCheck();
    void checkForAppUpdates(bool manualTriggered);
    void handleUpdateAvailable(const QString& latestTag,
                               const QString& latestVersion,
                               const QString& releaseNotes,
                               const QUrl& releaseUrl,
                               const QUrl& downloadUrl,
                               bool manualTriggered);
    void handleAlreadyLatest(const QString& latestTag, bool manualTriggered);
    void handleUpdateCheckFailed(const QString& error, bool manualTriggered);

private:
    // 通信相关
    std::unique_ptr<ICommunication> m_communication;
    CommType m_currentCommType = CommType::Serial;
    bool m_connected = false;

    // 配置
    SerialConfig m_serialConfig;
    NetworkConfig m_networkConfig;

    // UI组件
    SideNavigationBar* m_sideNavBar = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    QTabWidget* m_settingsTab = nullptr;

    // 设置面板
    SerialSettingsWidget* m_serialSettings = nullptr;
    NetworkSettingsWidget* m_networkSettings = nullptr;

    // 数据面板
    SendWidget* m_sendWidget = nullptr;
    TabbedReceiveWidget* m_receiveWidget = nullptr;
    QuickSendWidget* m_quickSendWidget = nullptr;

    // 数据统计
    DataStatistics* m_statistics = nullptr;

    // 状态栏组件
    QLabel* m_statusLabel = nullptr;
    QLabel* m_rxLabel = nullptr;
    QLabel* m_txLabel = nullptr;
    QLabel* m_timeLabel = nullptr;

    // 定时器
    QTimer* m_statusTimer = nullptr;

    // 主题
    QPushButton* m_themeBtn = nullptr;
    QString m_currentTheme = "light";
    QString m_currentLanguage = "zh_CN";  // 当前语言

    // 工具栏串口控件
    QToolBar* m_mainToolBar = nullptr;
    QComboBox* m_commTypeCombo = nullptr;  ///< 通信类型选择
    QComboBox* m_portCombo = nullptr;
    QComboBox* m_baudCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;   ///< 刷新按钮
    QPushButton* m_openPortBtn = nullptr;

    // 工具栏控件对应的Action（用于控制可见性）
    QAction* m_portComboAction = nullptr;
    QAction* m_refreshBtnAction = nullptr;
    QAction* m_baudComboAction = nullptr;
    QAction* m_networkWidgetAction = nullptr;

    // 工具栏网络控件
    QWidget* m_networkToolbarWidget = nullptr;  ///< 网络设置容器
    QLineEdit* m_ipEdit = nullptr;              ///< IP地址输入
    QSpinBox* m_portSpin = nullptr;             ///< 端口号输入

    // 对话框
    HelpDialog* m_helpDialog = nullptr;
    ToolboxDialog* m_toolboxDialog = nullptr;
    ScriptEditorDialog* m_scriptEditorDialog = nullptr;
    ExportDialog* m_exportDialog = nullptr;
    FileTransferDialog* m_fileTransferDialog = nullptr;

    // 功能窗口
    MacroWidget* m_macroWidget = nullptr;
    MultiPortWidget* m_multiPortWidget = nullptr;
    ModbusAnalyzerWidget* m_modbusAnalyzerWidget = nullptr;
    DataWindowManager* m_dataWindowManager = nullptr;  ///< 数据分窗管理器
    ControlPanel* m_controlPanel = nullptr;  ///< 控件面板
    DataTableWidget* m_dataTableWidget = nullptr;  ///< 数据表格视图

    // 绘图协议相关
    std::unique_ptr<IProtocol> m_currentProtocol;
    ProtocolType m_currentProtocolType = ProtocolType::Raw;
    QComboBox* m_protocolCombo = nullptr;
    QByteArray m_plotDataBuffer;  ///< 绘图数据缓冲（按行处理）

    // 显示模式
    DisplayMode m_displayMode = DisplayMode::Serial;
    QComboBox* m_displayModeCombo = nullptr;

    // 模式组件
    QStackedWidget* m_modeStack = nullptr;
    IModeWidget* m_currentModeWidget = nullptr;
    SerialModeWidget* m_serialModeWidget = nullptr;
    TerminalModeWidget* m_terminalModeWidget = nullptr;
    FrameModeWidget* m_frameModeWidget = nullptr;
    DebugModeWidget* m_debugModeWidget = nullptr;
    QWidget* m_modeToolBarContainer = nullptr;  ///< 模式专属工具栏容器

    // 翻译器
    QTranslator* m_translator = nullptr;
    bool m_hasRestoredWindowGeometry = false;

    // 应用更新
    AppUpdateChecker* m_appUpdateChecker = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MAINWINDOW_H
