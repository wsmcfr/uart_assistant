/**
 * @file MainWindow.cpp
 * @brief 主窗口实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "MainWindow.h"
#include "HelpDialog.h"
#include "PlotterManager.h"
#include "PlotterWindow.h"
#include "dialogs/ToolboxDialog.h"
#include "dialogs/ScriptEditorDialog.h"
#include "dialogs/SettingsDialog.h"
#include "dialogs/ExportDialog.h"
#include "dialogs/FileTransferDialog.h"
#include "widgets/SerialSettingsWidget.h"
#include "widgets/NetworkSettingsWidget.h"
#include "widgets/SendWidget.h"
#include "widgets/TabbedReceiveWidget.h"
#include "widgets/DataStatistics.h"
#include "widgets/QuickSendWidget.h"
#include "widgets/MacroWidget.h"
#include "widgets/MultiPortWidget.h"
#include "widgets/ModbusAnalyzerWidget.h"
#include "widgets/DataWindowManager.h"
#include "dialogs/DataWindowConfigDialog.h"
#include "controls/ControlPanel.h"
#include "widgets/DataTableWidget.h"
#include "widgets/SideNavigationBar.h"
#include "modes/SerialModeWidget.h"
#include "modes/TerminalModeWidget.h"
#include "modes/FrameModeWidget.h"
#include "modes/DebugModeWidget.h"
#include "utils/Logger.h"
#include "config/ConfigManager.h"
#include "communication/TcpClient.h"
#include "protocol/ProtocolFactory.h"
#include "macro/MacroRecorder.h"
#include "communication/MultiPortManager.h"
#include "transfer/FileTransfer.h"
#include "upgrade/IAPUpgrader.h"
#include "version.h"

#include <QMenuBar>
#include <QToolBar>
#include <QApplication>
#include <QActionGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include <QSerialPortInfo>
#include <QLabel>
#include <QStyle>
#include <QSettings>
#include <QFont>

namespace ComAssistant {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupConnections();
    loadSettings();

    // 加载语言设置（必须在 UI 创建后）
    loadLanguage(m_currentLanguage);

    setWindowTitle(QString("%1 v%2").arg(APP_NAME).arg(APP_VERSION));
    if (!m_hasRestoredWindowGeometry) {
        resize(1200, 800);
    }

    LOG_INFO("MainWindow initialized");
}

MainWindow::~MainWindow()
{
    destroyCommunication();
    saveSettings();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_connected) {
        int ret = QMessageBox::question(this, tr("确认退出"),
            tr("连接仍然活跃。确定要退出吗？"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (ret == QMessageBox::No) {
            event->ignore();
            return;
        }

        onDisconnectClicked();
    }

    saveSettings();
    event->accept();
}

void MainWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 6, 6);
    mainLayout->setSpacing(0);

    // 侧边导航栏（最左侧）
    m_sideNavBar = new SideNavigationBar;
    mainLayout->addWidget(m_sideNavBar);

    // 连接侧边导航栏信号
    connect(m_sideNavBar, &SideNavigationBar::itemClicked, this, [this](NavItem item) {
        switch (item) {
            case NavItem::Connection:
                // 显示连接设置（保持当前行为）
                break;
            case NavItem::Data:
                m_displayModeCombo->setCurrentIndex(0);  // 串口模式
                break;
            case NavItem::Plotter:
                onNewPlotWindow();
                break;
            case NavItem::Tools:
                onToolbox();
                break;
            case NavItem::Settings:
                onSettings();
                break;
        }
    });
    connect(m_sideNavBar, &SideNavigationBar::themeToggleRequested, this, &MainWindow::onThemeToggled);
    connect(m_sideNavBar, &SideNavigationBar::helpRequested, this, &MainWindow::onHelp);

    // 快捷发送区
    m_quickSendWidget = new QuickSendWidget;
    m_quickSendWidget->setMaximumWidth(200);
    mainLayout->addWidget(m_quickSendWidget);

    // 中间区域容器（包含模式工具栏和模式内容）
    QWidget* centerWidget = new QWidget;
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    // 模式专属工具栏容器（预留位置）
    m_modeToolBarContainer = new QWidget;
    m_modeToolBarContainer->setFixedHeight(0);  // 初始隐藏
    centerLayout->addWidget(m_modeToolBarContainer);

    // 模式切换堆栈
    m_modeStack = new QStackedWidget;

    // 创建各模式组件
    m_serialModeWidget = new SerialModeWidget;
    m_terminalModeWidget = new TerminalModeWidget;
    m_frameModeWidget = new FrameModeWidget;
    m_debugModeWidget = new DebugModeWidget;

    // 添加到堆栈（顺序对应 DisplayMode 枚举）
    m_modeStack->addWidget(m_serialModeWidget);     // index 0 = Serial
    m_modeStack->addWidget(m_terminalModeWidget);   // index 1 = Terminal
    m_modeStack->addWidget(m_frameModeWidget);      // index 2 = Frame
    m_modeStack->addWidget(m_debugModeWidget);      // index 3 = Debug

    // 设置默认模式
    m_currentModeWidget = m_serialModeWidget;
    m_modeStack->setCurrentWidget(m_serialModeWidget);

    centerLayout->addWidget(m_modeStack, 1);
    mainLayout->addWidget(centerWidget, 1);

    // 保留设置组件实例但不显示（用于配置和网络模式）
    m_serialSettings = new SerialSettingsWidget;
    m_serialSettings->hide();
    m_networkSettings = new NetworkSettingsWidget;
    m_networkSettings->hide();
    m_statistics = new DataStatistics;
    m_statistics->hide();

    // 保留旧组件引用以便兼容（将由模式组件内部管理）
    m_receiveWidget = m_serialModeWidget->receiveWidget();
    m_sendWidget = m_serialModeWidget->sendWidget();

    // 数据分窗管理器
    m_dataWindowManager = new DataWindowManager(this);
}

void MainWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();

    // 文件菜单
    QMenu* fileMenu = menuBar->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("新建会话(&N)"), this, &MainWindow::onNewSession, QKeySequence::New);
    fileMenu->addAction(tr("保存会话(&S)"), this, &MainWindow::onSaveSession, QKeySequence::Save);
    fileMenu->addAction(tr("加载会话(&L)"), this, &MainWindow::onLoadSession, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("导出数据(&E)..."), this, &MainWindow::onExportData);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出(&X)"), this, &QMainWindow::close, QKeySequence::Quit);

    // 编辑菜单
    QMenu* editMenu = menuBar->addMenu(tr("编辑(&E)"));
    editMenu->addAction(tr("清空全部(&C)"), this, &MainWindow::onClearAll, QKeySequence(Qt::CTRL | Qt::Key_L));
    editMenu->addAction(tr("数据搜索(&F)..."), this, &MainWindow::onDataSearch, QKeySequence::Find);

    // 视图菜单
    QMenu* viewMenu = menuBar->addMenu(tr("视图(&V)"));
    viewMenu->addAction(tr("置顶显示(&A)"))->setCheckable(true);

    // 语言子菜单
    QMenu* languageMenu = viewMenu->addMenu(tr("语言(&L)"));
    QActionGroup* langGroup = new QActionGroup(this);

    QAction* zhAction = languageMenu->addAction(tr("简体中文"));
    zhAction->setCheckable(true);
    zhAction->setData("zh_CN");
    langGroup->addAction(zhAction);

    QAction* enAction = languageMenu->addAction(tr("English"));
    enAction->setCheckable(true);
    enAction->setData("en_US");
    langGroup->addAction(enAction);

    // 从配置读取当前语言设置
    QString currentLang = ConfigManager::instance()->language();
    if (currentLang.isEmpty() || currentLang == "zh_CN") {
        zhAction->setChecked(true);
    } else {
        enAction->setChecked(true);
    }

    connect(langGroup, &QActionGroup::triggered, [this](QAction* action) {
        onLanguageChanged(action->data().toString());
    });

    // 工具菜单
    QMenu* toolsMenu = menuBar->addMenu(tr("工具(&T)"));
    toolsMenu->addAction(tr("工具箱(&B)..."), this, &MainWindow::onToolbox);
    toolsMenu->addAction(tr("脚本编辑器(&S)..."), this, &MainWindow::onScriptEditor);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("文件传输(&F)..."), this, &MainWindow::onFileTransfer);
    toolsMenu->addAction(tr("IAP升级(&I)..."), this, &MainWindow::onIAPUpgrade);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("宏录制/回放(&M)..."), this, &MainWindow::onMacroManager);
    toolsMenu->addAction(tr("多端口管理(&P)..."), this, &MainWindow::onMultiPortManager);
    toolsMenu->addAction(tr("Modbus分析(&A)..."), this, &MainWindow::onModbusAnalyzer);
    toolsMenu->addAction(tr("数据分窗(&W)..."), this, &MainWindow::onDataWindowConfig);
    toolsMenu->addAction(tr("控件面板(&C)..."), this, &MainWindow::onControlPanelToggled);
    toolsMenu->addAction(tr("数据表格(&T)..."), this, &MainWindow::onDataTableToggled);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("设置(&O)..."), this, &MainWindow::onSettings);

    // 绘图菜单
    QMenu* plotMenu = menuBar->addMenu(tr("绘图(&P)"));
    plotMenu->addAction(tr("新建绘图窗口(&N)"), this, &MainWindow::onNewPlotWindow);
    plotMenu->addAction(tr("关闭所有绘图窗口(&C)"), this, &MainWindow::onCloseAllPlotWindows);
    plotMenu->addSeparator();

    // 窗口同步选项
    QAction* syncAction = plotMenu->addAction(tr("窗口同步(&S)"));
    syncAction->setCheckable(true);
    syncAction->setChecked(PlotterManager::instance()->isSyncEnabled());
    syncAction->setToolTip(tr("启用后，所有绘图窗口的暂停状态将同步"));
    connect(syncAction, &QAction::toggled, [](bool checked) {
        PlotterManager::instance()->setSyncEnabled(checked);
    });

    // 帮助菜单
    QMenu* helpMenu = menuBar->addMenu(tr("帮助(&H)"));
    helpMenu->addAction(tr("帮助文档(&D)"), this, &MainWindow::onHelp, QKeySequence::HelpContents);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("关于(&A)"), this, &MainWindow::onAbout);
    helpMenu->addAction(tr("关于 Qt(&Q)"), qApp, &QApplication::aboutQt);
}

void MainWindow::setupToolBar()
{
    m_mainToolBar = addToolBar(tr("主工具栏"));
    m_mainToolBar->setObjectName("mainToolBar");
    m_mainToolBar->setMovable(false);
    m_mainToolBar->setIconSize(QSize(24, 24));

    // 通信类型选择下拉框
    QLabel* commTypeLabel = new QLabel(tr("类型:"));
    m_mainToolBar->addWidget(commTypeLabel);

    m_commTypeCombo = new QComboBox;
    m_commTypeCombo->setObjectName("commTypeCombo");
    m_commTypeCombo->setMinimumWidth(100);
    m_commTypeCombo->setToolTip(tr("选择通信类型"));
    m_commTypeCombo->addItem(tr("串口"), static_cast<int>(CommType::Serial));
    m_commTypeCombo->addItem(tr("TCP客户端"), static_cast<int>(CommType::TcpClient));
    m_commTypeCombo->addItem(tr("TCP服务器"), static_cast<int>(CommType::TcpServer));
    m_commTypeCombo->addItem(tr("UDP"), static_cast<int>(CommType::Udp));
    connect(m_commTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onCommTypeChanged);
    m_mainToolBar->addWidget(m_commTypeCombo);

    m_mainToolBar->addSeparator();

    // 串口选择下拉框
    m_portCombo = new QComboBox;
    m_portCombo->setObjectName("portCombo");
    m_portCombo->setMinimumWidth(180);
    m_portCombo->setToolTip(tr("选择串口"));
    m_portComboAction = m_mainToolBar->addWidget(m_portCombo);

    // 刷新串口按钮（带图标）
    m_refreshBtn = new QPushButton;
    m_refreshBtn->setObjectName("refreshBtn");
    m_refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshBtn->setToolTip(tr("刷新串口列表"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    m_refreshBtnAction = m_mainToolBar->addWidget(m_refreshBtn);

    // 波特率下拉框
    m_baudCombo = new QComboBox;
    m_baudCombo->setObjectName("baudCombo");
    m_baudCombo->setMinimumWidth(100);
    m_baudCombo->setEditable(true);
    m_baudCombo->setToolTip(tr("波特率"));
    QList<int> baudRates = {1200, 2400, 4800, 9600, 14400, 19200, 38400,
                           57600, 115200, 230400, 460800, 921600};
    for (int rate : baudRates) {
        m_baudCombo->addItem(QString::number(rate), rate);
    }
    m_baudCombo->setCurrentText("115200");
    m_baudComboAction = m_mainToolBar->addWidget(m_baudCombo);

    // 网络设置控件容器（初始隐藏）
    m_networkToolbarWidget = new QWidget;
    QHBoxLayout* networkLayout = new QHBoxLayout(m_networkToolbarWidget);
    networkLayout->setContentsMargins(0, 0, 0, 0);
    networkLayout->setSpacing(5);

    QLabel* ipLabel = new QLabel(tr("IP:"));
    networkLayout->addWidget(ipLabel);

    m_ipEdit = new QLineEdit;
    m_ipEdit->setPlaceholderText("127.0.0.1");
    m_ipEdit->setText("127.0.0.1");
    m_ipEdit->setMinimumWidth(120);
    m_ipEdit->setToolTip(tr("远程IP地址（TCP客户端）或本地绑定地址"));
    networkLayout->addWidget(m_ipEdit);

    QLabel* portLabel = new QLabel(tr("端口:"));
    networkLayout->addWidget(portLabel);

    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);
    m_portSpin->setToolTip(tr("端口号"));
    networkLayout->addWidget(m_portSpin);

    m_networkToolbarWidget->setVisible(false);
    m_networkWidgetAction = m_mainToolBar->addWidget(m_networkToolbarWidget);
    m_networkWidgetAction->setVisible(false);  // 初始隐藏

    m_mainToolBar->addSeparator();

    // 打开/关闭串口按钮
    m_openPortBtn = new QPushButton(tr("打开串口"));
    m_openPortBtn->setObjectName("openPortBtn");
    m_openPortBtn->setCheckable(true);
    m_openPortBtn->setMinimumWidth(100);
    connect(m_openPortBtn, &QPushButton::clicked, this, &MainWindow::onOpenPortClicked);
    m_mainToolBar->addWidget(m_openPortBtn);

    m_mainToolBar->addSeparator();

    // 清除按钮（带图标）
    QAction* clearAction = m_mainToolBar->addAction(
        style()->standardIcon(QStyle::SP_DialogResetButton), tr("清空"));
    clearAction->setToolTip(tr("清空接收区和发送区"));
    connect(clearAction, &QAction::triggered, this, &MainWindow::onClearAll);

    // 导出按钮（带图标）
    QAction* exportAction = m_mainToolBar->addAction(
        style()->standardIcon(QStyle::SP_DialogSaveButton), tr("导出"));
    exportAction->setToolTip(tr("导出数据到文件"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportData);

    m_mainToolBar->addSeparator();

    // 显示模式下拉框（放在左侧，更容易看到）
    QLabel* displayModeLabel = new QLabel(tr("模式:"));
    m_mainToolBar->addWidget(displayModeLabel);

    m_displayModeCombo = new QComboBox;
    m_displayModeCombo->setObjectName("displayModeCombo");
    m_displayModeCombo->setMinimumWidth(80);
    m_displayModeCombo->setToolTip(tr("选择显示模式"));
    m_displayModeCombo->addItem(tr("串口"), static_cast<int>(DisplayMode::Serial));
    m_displayModeCombo->addItem(tr("终端"), static_cast<int>(DisplayMode::Terminal));
    m_displayModeCombo->addItem(tr("帧模式"), static_cast<int>(DisplayMode::Frame));
    m_displayModeCombo->addItem(tr("调试"), static_cast<int>(DisplayMode::Debug));
    connect(m_displayModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDisplayModeChanged);
    m_mainToolBar->addWidget(m_displayModeCombo);

    // 弹性空间
    QWidget* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mainToolBar->addWidget(spacer);

    // 协议选择下拉框
    QLabel* protocolLabel = new QLabel(tr("协议:"));
    m_mainToolBar->addWidget(protocolLabel);

    m_protocolCombo = new QComboBox;
    m_protocolCombo->setObjectName("protocolCombo");
    m_protocolCombo->setMinimumWidth(100);
    m_protocolCombo->setToolTip(tr("选择绘图协议"));
    m_protocolCombo->addItem(tr("无"), static_cast<int>(ProtocolType::Raw));
    m_protocolCombo->addItem(tr("TEXT绘图"), static_cast<int>(ProtocolType::TextPlot));
    m_protocolCombo->addItem(tr("STAMP绘图"), static_cast<int>(ProtocolType::StampPlot));
    m_protocolCombo->addItem(tr("CSV绘图"), static_cast<int>(ProtocolType::CsvPlot));
    m_protocolCombo->addItem(tr("JustFloat"), static_cast<int>(ProtocolType::JustFloat));
    connect(m_protocolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProtocolChanged);
    m_mainToolBar->addWidget(m_protocolCombo);

    m_mainToolBar->addSeparator();

    // 主题切换按钮
    m_themeBtn = new QPushButton;
    m_themeBtn->setObjectName("themeBtn");
    m_themeBtn->setFixedSize(32, 32);
    m_themeBtn->setCheckable(true);
    m_themeBtn->setToolTip(tr("切换主题"));
    connect(m_themeBtn, &QPushButton::clicked, this, &MainWindow::onThemeToggled);
    m_mainToolBar->addWidget(m_themeBtn);

    // 连接信号
    connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onToolbarPortChanged);
    connect(m_baudCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onToolbarBaudChanged);

    // 初始刷新串口列表
    refreshPorts();
}

void MainWindow::setupStatusBar()
{
    QStatusBar* statusBar = this->statusBar();

    m_statusLabel = new QLabel(tr("未连接"));
    m_statusLabel->setMinimumWidth(150);
    statusBar->addWidget(m_statusLabel);

    statusBar->addWidget(new QLabel(" | "));

    m_rxLabel = new QLabel(tr("接收: 0"));
    m_rxLabel->setMinimumWidth(100);
    statusBar->addWidget(m_rxLabel);

    m_txLabel = new QLabel(tr("发送: 0"));
    m_txLabel->setMinimumWidth(100);
    statusBar->addWidget(m_txLabel);

    statusBar->addPermanentWidget(new QLabel(" | "));

    m_timeLabel = new QLabel;
    statusBar->addPermanentWidget(m_timeLabel);

    // 状态栏更新定时器
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusTimer->start(1000);
}

void MainWindow::setupConnections()
{
    // 注意：不要直接连接 m_sendWidget->sendRequested，因为它已经通过
    // SerialModeWidget 的 sendDataRequested 转发了，直接连接会导致数据发送两次

    // 快捷发送
    if (m_quickSendWidget) {
        connect(m_quickSendWidget, &QuickSendWidget::sendRequested, this, &MainWindow::onSendData);
    }

    // 串口设置变化
    if (m_serialSettings) {
        connect(m_serialSettings, &SerialSettingsWidget::configChanged,
                [this](const SerialConfig& config) {
            m_serialConfig = config;
        });
    }

    // 网络设置变化
    if (m_networkSettings) {
        connect(m_networkSettings, &NetworkSettingsWidget::configChanged,
                [this](const NetworkConfig& config) {
            m_networkConfig = config;
        });
    }

    // 连接模式组件的发送信号
    connect(m_serialModeWidget, &IModeWidget::sendDataRequested,
            this, &MainWindow::onSendData);
    connect(m_terminalModeWidget, &IModeWidget::sendDataRequested,
            this, &MainWindow::onSendData);
    connect(m_frameModeWidget, &IModeWidget::sendDataRequested,
            this, &MainWindow::onSendData);
    connect(m_debugModeWidget, &IModeWidget::sendDataRequested,
            this, &MainWindow::onSendData);

    // 连接模式组件的状态消息信号
    connect(m_terminalModeWidget, &IModeWidget::statusMessage,
            [this](const QString& msg) { statusBar()->showMessage(msg, 3000); });
    connect(m_frameModeWidget, &IModeWidget::statusMessage,
            [this](const QString& msg) { statusBar()->showMessage(msg, 3000); });
    connect(m_debugModeWidget, &IModeWidget::statusMessage,
            [this](const QString& msg) { statusBar()->showMessage(msg, 3000); });
}

void MainWindow::loadSettings()
{
    auto* configMgr = ConfigManager::instance();
    m_serialConfig = configMgr->serialConfig();
    m_networkConfig = configMgr->networkConfig();

    if (m_serialSettings) {
        m_serialSettings->setConfig(m_serialConfig);
    }
    if (m_networkSettings) {
        m_networkSettings->setConfig(m_networkConfig);
    }

    // 设置工具栏波特率
    if (m_baudCombo) {
        int index = m_baudCombo->findData(m_serialConfig.baudRate);
        if (index >= 0) {
            m_baudCombo->setCurrentIndex(index);
        } else {
            m_baudCombo->setCurrentText(QString::number(m_serialConfig.baudRate));
        }
    }

    // 设置工具栏端口（如果配置了）
    if (m_portCombo && !m_serialConfig.portName.isEmpty()) {
        for (int i = 0; i < m_portCombo->count(); ++i) {
            if (m_portCombo->itemData(i).toString() == m_serialConfig.portName) {
                m_portCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // 加载主题设置
    m_currentTheme = configMgr->theme();
    applyTheme(m_currentTheme);

    // 加载语言设置
    m_currentLanguage = configMgr->language();
    if (m_currentLanguage.isEmpty()) {
        m_currentLanguage = "zh_CN";
    }

    // 恢复窗口几何（可由“记住窗口位置和大小”控制）
    QSettings settings;
    m_hasRestoredWindowGeometry = false;
    if (settings.value("General/RememberWindow", true).toBool()) {
        const QByteArray geometry = settings.value("Window/Geometry").toByteArray();
        if (!geometry.isEmpty()) {
            m_hasRestoredWindowGeometry = restoreGeometry(geometry);
        }
    }

    applyDisplaySettings();
}

void MainWindow::saveSettings()
{
    auto* configMgr = ConfigManager::instance();
    configMgr->setSerialConfig(m_serialConfig);
    configMgr->setNetworkConfig(m_networkConfig);
    configMgr->setTheme(m_currentTheme);
    configMgr->setLanguage(m_currentLanguage);

    QSettings settings;
    if (settings.value("General/RememberWindow", true).toBool()) {
        settings.setValue("Window/Geometry", saveGeometry());
    } else {
        settings.remove("Window/Geometry");
    }
}

void MainWindow::applyDialogSettings()
{
    auto* configMgr = ConfigManager::instance();

    m_serialConfig = configMgr->serialConfig();
    m_networkConfig = configMgr->networkConfig();

    if (m_serialSettings) {
        m_serialSettings->setConfig(m_serialConfig);
    }
    if (m_networkSettings) {
        m_networkSettings->setConfig(m_networkConfig);
    }

    // 刷新工具栏显示（仅更新当前显示，不主动改动连接状态）
    if (m_baudCombo) {
        int index = m_baudCombo->findData(m_serialConfig.baudRate);
        if (index >= 0) {
            m_baudCombo->setCurrentIndex(index);
        } else {
            m_baudCombo->setCurrentText(QString::number(m_serialConfig.baudRate));
        }
    }

    if (m_portCombo && !m_serialConfig.portName.isEmpty()) {
        for (int i = 0; i < m_portCombo->count(); ++i) {
            if (m_portCombo->itemData(i).toString() == m_serialConfig.portName) {
                m_portCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    const QString language = configMgr->language().isEmpty() ? QStringLiteral("zh_CN") : configMgr->language();
    if (m_currentLanguage != language) {
        m_currentLanguage = language;
        loadLanguage(language);
    }

    applyDisplaySettings();
}

void MainWindow::applyDisplaySettings()
{
    QSettings settings;

    const bool showTimestamp = settings.value("Display/ShowTimestamp", false).toBool();
    const bool autoScroll = settings.value("Display/AutoScroll", true).toBool();
    const bool appendNewline = settings.value("Display/AppendNewline", true).toBool();
    const QString newline = settings.value("Display/Newline", "\r\n").toString();
    const QString fontFamily = settings.value("Display/Font", "Consolas").toString();
    const int fontSize = settings.value("Display/FontSize", 10).toInt();
    const int maxLines = settings.value("Display/MaxLines", 10000).toInt();
    QFont displayFont(fontFamily, fontSize);

    if (m_serialModeWidget && m_serialModeWidget->receiveWidget()) {
        auto* receiveWidget = m_serialModeWidget->receiveWidget();
        receiveWidget->setTimestampEnabled(showTimestamp);
        receiveWidget->setAutoScrollEnabled(autoScroll);
        receiveWidget->setDisplayFont(displayFont);
        receiveWidget->setMaxLines(maxLines);
    }

    if (m_dataTableWidget) {
        m_dataTableWidget->setAutoScroll(autoScroll);
    }

    if (m_sendWidget) {
        m_sendWidget->setAppendNewlineEnabled(appendNewline);
        m_sendWidget->setAppendNewlineSequence(newline);
    }
}

void MainWindow::createCommunication()
{
    destroyCommunication();

    m_communication = CommunicationFactory::create(m_currentCommType,
                                                    m_serialConfig, m_networkConfig);

    if (m_communication) {
        if (auto* tcpClient = dynamic_cast<TcpClient*>(m_communication.get())) {
            QSettings settings;
            const bool autoReconnect = settings.value("Serial/AutoReconnect", false).toBool();
            const int reconnectIntervalSec = settings.value("Serial/ReconnectInterval", 3).toInt();
            tcpClient->setAutoReconnect(autoReconnect, reconnectIntervalSec * 1000);
        }

        connect(m_communication.get(), &ICommunication::dataReceived,
                this, &MainWindow::onDataReceived);
        connect(m_communication.get(), &ICommunication::dataSent,
                this, &MainWindow::onDataSent);
        connect(m_communication.get(), &ICommunication::connectionStatusChanged,
                this, &MainWindow::onConnectionStatusChanged);
        connect(m_communication.get(), &ICommunication::errorOccurred,
                this, &MainWindow::onErrorOccurred);
    }
}

void MainWindow::destroyCommunication()
{
    if (m_communication) {
        m_communication->close();
        m_communication.reset();
    }
}

void MainWindow::onConnectClicked()
{
    createCommunication();

    if (!m_communication) {
        QMessageBox::critical(this, tr("错误"), tr("无法创建通信实例"));
        return;
    }

    if (!m_communication->open()) {
        QMessageBox::critical(this, tr("错误"),
            tr("无法打开连接: %1").arg(m_communication->lastError()));
        return;
    }

    m_connected = true;
    LOG_INFO(QString("Connected: %1").arg(m_communication->statusString()));
}

void MainWindow::onDisconnectClicked()
{
    if (m_communication) {
        m_communication->close();
    }
    m_connected = false;
    LOG_INFO("Disconnected");
}

void MainWindow::onDataReceived(const QByteArray& data)
{
    // 将数据发送到当前模式组件
    if (m_currentModeWidget) {
        m_currentModeWidget->appendReceivedData(data);
    }

    if (m_statistics) {
        m_statistics->addRxBytes(data.size());
    }

    // 数据分窗路由
    if (m_dataWindowManager && m_dataWindowManager->hasRules()) {
        m_dataWindowManager->routeData(QString::fromUtf8(data));
    }

    // 数据表格视图更新
    if (m_dataTableWidget) {
        QVector<double> parsedValues;
        // 如果有绘图协议，尝试解析数值
        if (m_currentProtocol && m_currentProtocol->isPlotProtocol()) {
            PlotData plotData = m_currentProtocol->parsePlotData(data);
            if (plotData.valid) {
                parsedValues = plotData.yValues;
            }
        }
        QString protocolName = m_currentProtocol ?
            ProtocolFactory::typeName(m_currentProtocolType) : QString();
        m_dataTableWidget->addReceivedData(data, protocolName, parsedValues);
    }

    // 绘图协议解析和数据路由
    if (m_currentProtocol && m_currentProtocol->isPlotProtocol()) {
        // 将数据添加到缓冲区
        m_plotDataBuffer.append(data);

        // 按行处理数据
        int pos;
        while ((pos = m_plotDataBuffer.indexOf('\n')) >= 0) {
            QByteArray line = m_plotDataBuffer.left(pos);
            m_plotDataBuffer.remove(0, pos + 1);

            // 移除可能的 \r
            if (line.endsWith('\r')) {
                line.chop(1);
            }

            if (line.isEmpty()) continue;

            // 解析绘图数据
            PlotData plotData = m_currentProtocol->parsePlotData(line);
            if (plotData.valid && !plotData.windowId.isEmpty()) {
                // 路由到绘图管理器
                if (plotData.useTimestamp) {
                    PlotterManager::instance()->routeData(
                        plotData.windowId, plotData.timestamp, plotData.yValues);
                } else if (plotData.useCustomX) {
                    PlotterManager::instance()->routeData(
                        plotData.windowId, plotData.xValue, plotData.yValues);
                } else {
                    PlotterManager::instance()->routeData(
                        plotData.windowId, plotData.yValues);
                }
            }
        }

        // 防止缓冲区过大
        if (m_plotDataBuffer.size() > 4096) {
            m_plotDataBuffer.clear();
        }
    }
}

void MainWindow::onDataSent(const QByteArray& data)
{
    // 通知当前模式组件发送了数据
    if (m_currentModeWidget) {
        m_currentModeWidget->appendSentData(data);
    }

    if (m_statistics) {
        m_statistics->addTxBytes(data.size());
    }

    // 数据表格视图更新
    if (m_dataTableWidget) {
        QString protocolName = m_currentProtocol ?
            ProtocolFactory::typeName(m_currentProtocolType) : QString();
        m_dataTableWidget->addSentData(data, protocolName);
    }
}

void MainWindow::onSendData(const QByteArray& data)
{
    if (!m_communication || !m_connected) {
        return;
    }

    qint64 written = m_communication->write(data);
    if (written < 0) {
        LOG_ERROR(QString("Send failed: %1").arg(m_communication->lastError()));
    }
}

void MainWindow::onConnectionStatusChanged(bool connected)
{
    m_connected = connected;

    // 更新所有模式组件的连接状态
    if (m_currentModeWidget) {
        m_currentModeWidget->setConnected(connected);
    }
    if (m_serialModeWidget) {
        m_serialModeWidget->setConnected(connected);
    }
    if (m_terminalModeWidget) {
        m_terminalModeWidget->setConnected(connected);
    }
    if (m_frameModeWidget) {
        m_frameModeWidget->setConnected(connected);
    }
    if (m_debugModeWidget) {
        m_debugModeWidget->setConnected(connected);
    }

    // 更新工具栏按钮状态
    if (m_openPortBtn) {
        if (connected) {
            m_openPortBtn->setText(tr("关闭串口"));
            m_openPortBtn->setChecked(true);
        } else {
            m_openPortBtn->setText(tr("打开串口"));
            m_openPortBtn->setChecked(false);
        }
    }

    if (m_portCombo) {
        m_portCombo->setEnabled(!connected);
    }
    if (m_baudCombo) {
        m_baudCombo->setEnabled(!connected);
    }

    if (connected) {
        m_statusLabel->setText(tr("已连接: %1").arg(
            m_communication ? m_communication->statusString() : QString()));
        m_statusLabel->setStyleSheet("color: green;");
    } else {
        m_statusLabel->setText(tr("未连接"));
        m_statusLabel->setStyleSheet("color: gray;");
    }
}

void MainWindow::onErrorOccurred(const QString& error)
{
    LOG_ERROR(QString("Communication error: %1").arg(error));
    statusBar()->showMessage(tr("错误: %1").arg(error), 5000);
}

void MainWindow::onCommTypeChanged(int index)
{
    Q_UNUSED(index)
    QComboBox* combo = m_commTypeCombo;
    if (!combo) return;

    m_currentCommType = static_cast<CommType>(combo->currentData().toInt());

    // 更新工具栏控件显示和按钮文本
    switch (m_currentCommType) {
        case CommType::Serial:
            // 串口模式：显示串口相关控件
            m_portComboAction->setVisible(true);
            m_refreshBtnAction->setVisible(true);
            m_baudComboAction->setVisible(true);
            m_networkWidgetAction->setVisible(false);
            m_openPortBtn->setText(tr("打开串口"));
            break;
        case CommType::TcpClient:
            // TCP客户端：显示网络设置
            m_portComboAction->setVisible(false);
            m_refreshBtnAction->setVisible(false);
            m_baudComboAction->setVisible(false);
            m_networkWidgetAction->setVisible(true);
            m_ipEdit->setToolTip(tr("服务器IP地址"));
            m_portSpin->setToolTip(tr("服务器端口"));
            m_openPortBtn->setText(tr("连接"));
            break;
        case CommType::TcpServer:
            // TCP服务器：显示网络设置
            m_portComboAction->setVisible(false);
            m_refreshBtnAction->setVisible(false);
            m_baudComboAction->setVisible(false);
            m_networkWidgetAction->setVisible(true);
            m_ipEdit->setToolTip(tr("本地绑定地址（0.0.0.0表示所有）"));
            m_ipEdit->setText("0.0.0.0");
            m_portSpin->setToolTip(tr("监听端口"));
            m_openPortBtn->setText(tr("启动服务"));
            break;
        case CommType::Udp:
            // UDP模式：显示网络设置
            m_portComboAction->setVisible(false);
            m_refreshBtnAction->setVisible(false);
            m_baudComboAction->setVisible(false);
            m_networkWidgetAction->setVisible(true);
            m_ipEdit->setToolTip(tr("目标IP地址"));
            m_portSpin->setToolTip(tr("本地/目标端口"));
            m_openPortBtn->setText(tr("绑定端口"));
            break;
        default:
            break;
    }

    LOG_INFO(QString("Communication type changed to: %1").arg(static_cast<int>(m_currentCommType)));
}

void MainWindow::onNewSession()
{
    onClearAll();
    if (m_statistics) {
        m_statistics->reset();
    }
}

void MainWindow::onSaveSession()
{
    SessionManager* sm = SessionManager::instance();
    SessionData& session = sm->currentSession();

    // 收集当前状态
    session.lastModifyTime = QDateTime::currentDateTime();

    // 通信配置
    session.commType = m_currentCommType;
    session.serialConfig = m_serialConfig;
    session.networkConfig = m_networkConfig;

    // 协议和显示模式
    session.protocolType = static_cast<int>(m_currentProtocolType);
    session.displayMode = static_cast<int>(m_displayMode);

    // 快捷发送项
    if (m_quickSendWidget) {
        session.quickSendItems = m_quickSendWidget->items();
    }

    // 窗口状态
    session.windowGeometry = saveGeometry();
    if (m_mainSplitter) {
        session.splitterState = m_mainSplitter->saveState();
    }

    // 如果没有现有路径，弹出保存对话框
    QString filePath = sm->currentFilePath();
    if (filePath.isEmpty()) {
        filePath = QFileDialog::getSaveFileName(this, tr("保存会话"),
            session.name + ".cas",
            tr("ComAssistant 会话 (*.cas);;所有文件 (*)"));

        if (filePath.isEmpty()) {
            return;  // 用户取消
        }
    }

    if (sm->saveSession(filePath)) {
        statusBar()->showMessage(tr("会话已保存: %1").arg(filePath), 3000);
    } else {
        QMessageBox::warning(this, tr("保存失败"),
            tr("无法保存会话文件。"));
    }
}

void MainWindow::onLoadSession()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("加载会话"),
        QString(),
        tr("ComAssistant 会话 (*.cas);;所有文件 (*)"));

    if (filePath.isEmpty()) {
        return;  // 用户取消
    }

    SessionManager* sm = SessionManager::instance();
    if (!sm->loadSession(filePath)) {
        QMessageBox::warning(this, tr("加载失败"),
            tr("无法加载会话文件。文件可能已损坏或格式不正确。"));
        return;
    }

    // 应用加载的会话数据
    const SessionData& session = sm->currentSession();

    // 通信配置
    m_currentCommType = session.commType;
    m_serialConfig = session.serialConfig;
    m_networkConfig = session.networkConfig;

    // 更新串口设置界面
    if (m_serialSettings) {
        m_serialSettings->setConfig(m_serialConfig);
    }

    // 更新网络设置界面
    if (m_networkSettings) {
        m_networkSettings->setConfig(m_networkConfig);
    }

    // 更新工具栏串口控件
    if (m_portCombo) {
        int portIdx = m_portCombo->findText(m_serialConfig.portName);
        if (portIdx >= 0) {
            m_portCombo->setCurrentIndex(portIdx);
        }
    }
    if (m_baudCombo) {
        int baudIdx = m_baudCombo->findText(QString::number(m_serialConfig.baudRate));
        if (baudIdx >= 0) {
            m_baudCombo->setCurrentIndex(baudIdx);
        }
    }

    // 协议类型
    m_currentProtocolType = static_cast<ProtocolType>(session.protocolType);
    if (m_protocolCombo) {
        m_protocolCombo->setCurrentIndex(session.protocolType);
    }
    m_currentProtocol = ProtocolFactory::create(m_currentProtocolType);

    // 显示模式
    if (m_displayModeCombo && session.displayMode >= 0 && session.displayMode < 4) {
        m_displayModeCombo->setCurrentIndex(session.displayMode);
    }

    // 快捷发送项
    if (m_quickSendWidget) {
        m_quickSendWidget->clearAll();
        for (const auto& item : session.quickSendItems) {
            m_quickSendWidget->addItem(item);
        }
    }

    // 窗口状态
    if (!session.windowGeometry.isEmpty()) {
        restoreGeometry(session.windowGeometry);
    }
    if (m_mainSplitter && !session.splitterState.isEmpty()) {
        m_mainSplitter->restoreState(session.splitterState);
    }

    statusBar()->showMessage(tr("会话已加载: %1").arg(filePath), 3000);
}

void MainWindow::onExportData()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("导出数据"),
        QString(), tr("文本文件 (*.txt);;所有文件 (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    // 使用当前模式组件导出
    if (m_currentModeWidget) {
        m_currentModeWidget->exportToFile(fileName);
    }
}

void MainWindow::onClearAll()
{
    // 清空当前模式组件
    if (m_currentModeWidget) {
        m_currentModeWidget->clear();
    }
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(this);
    connect(&dialog, &SettingsDialog::settingsApplied, this, &MainWindow::applyDialogSettings);
    dialog.exec();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("关于 %1").arg(APP_NAME),
        tr("<h3>%1 v%2</h3>"
           "<p>专业的串口调试助手。</p>"
           "<p>版权所有 (C) 2026 ComAssistant Team</p>"
           "<p>许可证: LGPL v3 / GPL v3</p>")
        .arg(APP_NAME)
        .arg(APP_VERSION));
}

void MainWindow::onHelp()
{
    if (!m_helpDialog) {
        m_helpDialog = new HelpDialog(this);
    }
    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
}

void MainWindow::onToolbox()
{
    if (!m_toolboxDialog) {
        m_toolboxDialog = new ToolboxDialog(this);
    }
    m_toolboxDialog->show();
    m_toolboxDialog->raise();
    m_toolboxDialog->activateWindow();
}

void MainWindow::onScriptEditor()
{
    if (!m_scriptEditorDialog) {
        m_scriptEditorDialog = new ScriptEditorDialog(this);
        // 连接脚本发送信号到主窗口发送槽
        connect(m_scriptEditorDialog, &ScriptEditorDialog::sendData,
                this, &MainWindow::onSendData);
    }
    m_scriptEditorDialog->show();
    m_scriptEditorDialog->raise();
    m_scriptEditorDialog->activateWindow();
}

void MainWindow::updateStatusBar()
{
    m_timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (m_statistics) {
        m_rxLabel->setText(tr("接收: %1").arg(m_statistics->rxBytesFormatted()));
        m_txLabel->setText(tr("发送: %1").arg(m_statistics->txBytesFormatted()));
    }
}

void MainWindow::applyTheme(const QString& theme)
{
    QString qssPath = QString(":/themes/%1.qss").arg(theme);
    QFile qssFile(qssPath);

    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(qssFile.readAll());
        qApp->setStyleSheet(styleSheet);
        qssFile.close();
        LOG_INFO(QString("Theme applied: %1").arg(theme));
    } else {
        LOG_ERROR(QString("Failed to load theme: %1").arg(qssPath));
    }

    // 更新主题按钮图标
    if (m_themeBtn) {
        if (theme == "dark") {
            m_themeBtn->setText(QString::fromUtf8("\xe2\x98\x80"));  // ☀ 太阳
            m_themeBtn->setChecked(true);
        } else {
            m_themeBtn->setText(QString::fromUtf8("\xe2\x98\xbd"));  // ☽ 月亮
            m_themeBtn->setChecked(false);
        }
    }

    // 更新帮助对话框主题
    if (m_helpDialog) {
        m_helpDialog->updateTheme(theme == "dark");
    }
}

void MainWindow::onThemeToggled()
{
    if (m_currentTheme == "light") {
        m_currentTheme = "dark";
    } else {
        m_currentTheme = "light";
    }

    applyTheme(m_currentTheme);
    saveSettings();
}

void MainWindow::onLanguageChanged(const QString& language)
{
    if (m_currentLanguage == language) {
        return;
    }

    m_currentLanguage = language;
    loadLanguage(language);
    ConfigManager::instance()->setLanguage(language);
    saveSettings();
}

void MainWindow::loadLanguage(const QString& language)
{
    auto notifyLanguageChange = []() {
        QEvent event(QEvent::LanguageChange);
        const auto topLevels = QApplication::topLevelWidgets();
        for (QWidget* widget : topLevels) {
            if (widget) {
                QCoreApplication::sendEvent(widget, &event);
            }
        }
    };

    // 移除旧的翻译器
    if (m_translator) {
        qApp->removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    // 中文是默认语言，不需要加载翻译文件
    if (language.isEmpty() || language == "zh_CN") {
        notifyLanguageChange();
        return;
    }

    // 创建新的翻译器
    m_translator = new QTranslator(this);

    // 尝试从多个位置加载翻译文件
    QStringList searchPaths = {
        QCoreApplication::applicationDirPath() + "/translations",
        QCoreApplication::applicationDirPath(),
        ":/translations"
    };

    bool loaded = false;
    for (const QString& path : searchPaths) {
        QString qmFile = path + "/" + language + ".qm";
        if (m_translator->load(qmFile)) {
            qApp->installTranslator(m_translator);
            LOG_INFO(QString("Loaded translation: %1").arg(qmFile));
            loaded = true;
            break;
        }
    }

    if (!loaded) {
        LOG_WARN(QString("Failed to load translation for: %1").arg(language));
        delete m_translator;
        m_translator = nullptr;
    }

    notifyLanguageChange();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi()
{
    // 更新窗口标题
    setWindowTitle(QString("%1 v%2").arg(APP_NAME).arg(APP_VERSION));

    // 重建菜单栏以更新翻译
    menuBar()->clear();
    setupMenuBar();

    // 保存工具栏状态
    QString currentPort = m_portCombo ? m_portCombo->currentData().toString() : QString();
    QString currentBaud = m_baudCombo ? m_baudCombo->currentText() : "115200";
    int displayModeIndex = m_displayModeCombo ? m_displayModeCombo->currentIndex() : 0;
    int protocolIndex = m_protocolCombo ? m_protocolCombo->currentIndex() : 0;
    bool themeChecked = m_themeBtn ? m_themeBtn->isChecked() : false;

    // 删除旧工具栏并重建
    if (m_mainToolBar) {
        removeToolBar(m_mainToolBar);
        m_mainToolBar->deleteLater();
        m_mainToolBar = nullptr;
        m_portCombo = nullptr;
        m_baudCombo = nullptr;
        m_openPortBtn = nullptr;
        m_displayModeCombo = nullptr;
        m_protocolCombo = nullptr;
        m_themeBtn = nullptr;
    }
    setupToolBar();

    // 恢复工具栏状态
    if (m_portCombo && !currentPort.isEmpty()) {
        int idx = m_portCombo->findData(currentPort);
        if (idx >= 0) {
            m_portCombo->setCurrentIndex(idx);
        }
    }
    if (m_baudCombo) {
        m_baudCombo->setCurrentText(currentBaud);
    }
    if (m_displayModeCombo) {
        m_displayModeCombo->setCurrentIndex(displayModeIndex);
    }
    if (m_protocolCombo) {
        m_protocolCombo->setCurrentIndex(protocolIndex);
    }
    if (m_themeBtn) {
        m_themeBtn->setChecked(themeChecked);
    }
    if (m_openPortBtn) {
        m_openPortBtn->setText(m_connected ? tr("关闭串口") : tr("打开串口"));
        m_openPortBtn->setChecked(m_connected);
    }

    // 更新状态栏
    if (m_statusLabel) {
        m_statusLabel->setText(m_connected ? tr("已连接") : tr("未连接"));
    }

    LOG_INFO("UI retranslated");
}

void MainWindow::refreshPorts()
{
    if (!m_portCombo) return;

    QString currentPort = m_portCombo->currentData().toString();

    m_portCombo->blockSignals(true);
    m_portCombo->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto& port : ports) {
        QString displayText = QString("%1 - %2").arg(port.portName(), port.description());
        m_portCombo->addItem(displayText, port.portName());
    }

    // 恢复之前选择的端口
    if (!currentPort.isEmpty()) {
        for (int i = 0; i < m_portCombo->count(); ++i) {
            if (m_portCombo->itemData(i).toString() == currentPort) {
                m_portCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    m_portCombo->blockSignals(false);

    if (m_portCombo->count() > 0 && m_portCombo->currentIndex() >= 0) {
        m_serialConfig.portName = m_portCombo->currentData().toString();
    }
}

void MainWindow::onToolbarPortChanged(int index)
{
    if (index >= 0 && m_portCombo) {
        m_serialConfig.portName = m_portCombo->itemData(index).toString();
    }
}

void MainWindow::onToolbarBaudChanged(int index)
{
    Q_UNUSED(index)
    if (m_baudCombo) {
        bool ok;
        int baudRate = m_baudCombo->currentText().toInt(&ok);
        if (ok) {
            m_serialConfig.baudRate = baudRate;
        }
    }
}

void MainWindow::onOpenPortClicked()
{
    if (m_connected) {
        // 关闭连接
        onDisconnectClicked();

        // 恢复按钮文本和控件状态
        switch (m_currentCommType) {
            case CommType::Serial:
                m_openPortBtn->setText(tr("打开串口"));
                m_portCombo->setEnabled(true);
                m_baudCombo->setEnabled(true);
                break;
            case CommType::TcpClient:
                m_openPortBtn->setText(tr("连接"));
                m_networkToolbarWidget->setEnabled(true);
                break;
            case CommType::TcpServer:
                m_openPortBtn->setText(tr("启动服务"));
                m_networkToolbarWidget->setEnabled(true);
                break;
            case CommType::Udp:
                m_openPortBtn->setText(tr("绑定端口"));
                m_networkToolbarWidget->setEnabled(true);
                break;
            default:
                break;
        }
        m_openPortBtn->setChecked(false);
        m_commTypeCombo->setEnabled(true);
    } else {
        // 根据通信类型更新配置
        switch (m_currentCommType) {
            case CommType::Serial:
                // 更新串口配置
                if (m_portCombo && m_portCombo->currentIndex() >= 0) {
                    m_serialConfig.portName = m_portCombo->currentData().toString();
                }
                if (m_baudCombo) {
                    bool ok;
                    int baudRate = m_baudCombo->currentText().toInt(&ok);
                    if (ok) {
                        m_serialConfig.baudRate = baudRate;
                    }
                }
                break;
            case CommType::TcpClient:
                // 更新TCP客户端配置
                if (m_ipEdit) {
                    m_networkConfig.serverIp = m_ipEdit->text();
                }
                if (m_portSpin) {
                    m_networkConfig.serverPort = m_portSpin->value();
                }
                m_networkConfig.mode = NetworkMode::TcpClient;
                break;
            case CommType::TcpServer:
                // 更新TCP服务器配置
                if (m_portSpin) {
                    m_networkConfig.listenPort = m_portSpin->value();
                }
                m_networkConfig.mode = NetworkMode::TcpServer;
                break;
            case CommType::Udp:
                // 更新UDP配置
                if (m_ipEdit) {
                    m_networkConfig.remoteIp = m_ipEdit->text();
                }
                if (m_portSpin) {
                    m_networkConfig.remotePort = m_portSpin->value();
                    m_networkConfig.listenPort = m_portSpin->value();
                }
                m_networkConfig.mode = NetworkMode::Udp;
                break;
            default:
                break;
        }

        // 建立连接
        onConnectClicked();

        if (m_connected) {
            // 更新按钮文本和控件状态
            switch (m_currentCommType) {
                case CommType::Serial:
                    m_openPortBtn->setText(tr("关闭串口"));
                    m_portCombo->setEnabled(false);
                    m_baudCombo->setEnabled(false);
                    break;
                case CommType::TcpClient:
                    m_openPortBtn->setText(tr("断开"));
                    m_networkToolbarWidget->setEnabled(false);
                    break;
                case CommType::TcpServer:
                    m_openPortBtn->setText(tr("停止服务"));
                    m_networkToolbarWidget->setEnabled(false);
                    break;
                case CommType::Udp:
                    m_openPortBtn->setText(tr("解绑"));
                    m_networkToolbarWidget->setEnabled(false);
                    break;
                default:
                    break;
            }
            m_openPortBtn->setChecked(true);
            m_commTypeCombo->setEnabled(false);
        } else {
            m_openPortBtn->setChecked(false);
        }
    }
}

void MainWindow::onNewPlotWindow()
{
    static int windowCount = 0;
    QString windowId = QString("plot%1").arg(++windowCount);
    QString title = tr("绘图窗口 %1").arg(windowCount);

    PlotterWindow* window = PlotterManager::instance()->createWindow(windowId, title);
    if (window) {
        window->show();
        LOG_INFO(QString("Created plot window: %1").arg(windowId));
    }
}

void MainWindow::onCloseAllPlotWindows()
{
    PlotterManager::instance()->closeAllWindows();
    LOG_INFO("Closed all plot windows");
}

void MainWindow::onProtocolChanged(int index)
{
    if (!m_protocolCombo) return;

    m_currentProtocolType = static_cast<ProtocolType>(
        m_protocolCombo->itemData(index).toInt());

    // 创建对应的协议解析器
    m_currentProtocol.reset();
    if (m_currentProtocolType != ProtocolType::Raw) {
        m_currentProtocol = ProtocolFactory::create(m_currentProtocolType);
    }

    // 清空绘图数据缓冲
    m_plotDataBuffer.clear();

    LOG_INFO(QString("Protocol changed to: %1").arg(
        ProtocolFactory::typeName(m_currentProtocolType)));
}

void MainWindow::onDisplayModeChanged(int index)
{
    if (!m_displayModeCombo || !m_modeStack) return;

    // 获取新模式
    DisplayMode newMode = static_cast<DisplayMode>(
        m_displayModeCombo->itemData(index).toInt());

    if (newMode == m_displayMode && m_currentModeWidget) {
        return;  // 模式未变化
    }

    // 通知旧模式停用
    if (m_currentModeWidget) {
        m_currentModeWidget->onDeactivated();
    }

    // 隐藏旧模式的工具栏
    if (m_modeToolBarContainer->layout()) {
        QLayoutItem* item;
        while ((item = m_modeToolBarContainer->layout()->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->setParent(nullptr);
            }
            delete item;
        }
    }

    // 切换模式
    m_displayMode = newMode;
    IModeWidget* newModeWidget = nullptr;

    switch (m_displayMode) {
        case DisplayMode::Serial:
            newModeWidget = m_serialModeWidget;
            break;
        case DisplayMode::Terminal:
            newModeWidget = m_terminalModeWidget;
            break;
        case DisplayMode::Frame:
            newModeWidget = m_frameModeWidget;
            break;
        case DisplayMode::Debug:
            newModeWidget = m_debugModeWidget;
            break;
    }

    if (newModeWidget) {
        m_currentModeWidget = newModeWidget;
        m_modeStack->setCurrentWidget(newModeWidget);

        // 同步连接状态
        newModeWidget->setConnected(m_connected);

        // 显示模式专属工具栏
        QWidget* modeToolBar = newModeWidget->modeToolBar();
        if (modeToolBar) {
            if (!m_modeToolBarContainer->layout()) {
                m_modeToolBarContainer->setLayout(new QVBoxLayout);
                m_modeToolBarContainer->layout()->setContentsMargins(0, 0, 0, 0);
            }
            m_modeToolBarContainer->layout()->addWidget(modeToolBar);
            modeToolBar->show();
            m_modeToolBarContainer->setFixedHeight(modeToolBar->sizeHint().height());
        } else {
            m_modeToolBarContainer->setFixedHeight(0);
        }

        // 通知新模式激活
        newModeWidget->onActivated();
    }

    QString modeName;
    switch (m_displayMode) {
        case DisplayMode::Serial: modeName = "Serial"; break;
        case DisplayMode::Terminal: modeName = "Terminal"; break;
        case DisplayMode::Frame: modeName = "Frame"; break;
        case DisplayMode::Debug: modeName = "Debug"; break;
    }
    LOG_INFO(QString("Display mode changed to: %1").arg(modeName));
}

void MainWindow::onFileTransfer()
{
    if (!m_fileTransferDialog) {
        m_fileTransferDialog = new FileTransferDialog(this);

        // 连接发送信号
        connect(m_fileTransferDialog, &FileTransferDialog::sendData,
                this, &MainWindow::onSendData);
    }

    // 更新连接状态
    m_fileTransferDialog->setConnected(m_connected);
    m_fileTransferDialog->show();
    m_fileTransferDialog->raise();
    m_fileTransferDialog->activateWindow();
}

void MainWindow::onIAPUpgrade()
{
    if (!m_connected) {
        QMessageBox::warning(this, tr("提示"),
            tr("请先打开串口连接后再进行IAP升级。"));
        return;
    }

    // 使用文件传输对话框的IAP模式
    if (!m_fileTransferDialog) {
        m_fileTransferDialog = new FileTransferDialog(this);
        connect(m_fileTransferDialog, &FileTransferDialog::sendData,
                this, &MainWindow::onSendData);
    }

    m_fileTransferDialog->setConnected(m_connected);
    m_fileTransferDialog->setIAPMode(true);
    m_fileTransferDialog->show();
    m_fileTransferDialog->raise();
    m_fileTransferDialog->activateWindow();
}

void MainWindow::onMacroManager()
{
    if (!m_macroWidget) {
        m_macroWidget = new MacroWidget(this);
        m_macroWidget->setWindowFlags(Qt::Window);
        m_macroWidget->setWindowTitle(tr("宏录制/回放"));
        m_macroWidget->resize(500, 400);

        // 连接发送信号
        connect(m_macroWidget, &MacroWidget::sendData,
                this, &MainWindow::onSendData);
    }

    m_macroWidget->show();
    m_macroWidget->raise();
    m_macroWidget->activateWindow();
}

void MainWindow::onMultiPortManager()
{
    if (!m_multiPortWidget) {
        m_multiPortWidget = new MultiPortWidget(this);
        m_multiPortWidget->setWindowFlags(Qt::Window);
        m_multiPortWidget->setWindowTitle(tr("多端口管理"));
        m_multiPortWidget->resize(700, 500);
    }

    m_multiPortWidget->show();
    m_multiPortWidget->raise();
    m_multiPortWidget->activateWindow();
}

void MainWindow::onModbusAnalyzer()
{
    if (!m_modbusAnalyzerWidget) {
        m_modbusAnalyzerWidget = new ModbusAnalyzerWidget(this);
        m_modbusAnalyzerWidget->setWindowFlags(Qt::Window);
        m_modbusAnalyzerWidget->setWindowTitle(tr("Modbus协议分析"));
        m_modbusAnalyzerWidget->resize(800, 600);
    }

    m_modbusAnalyzerWidget->show();
    m_modbusAnalyzerWidget->raise();
    m_modbusAnalyzerWidget->activateWindow();
}

void MainWindow::onDataWindowConfig()
{
    if (!m_dataWindowManager) {
        return;
    }

    DataWindowConfigDialog dialog(m_dataWindowManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_dataWindowManager->setRules(dialog.rules());
        LOG_INFO(QString("Data window rules updated, count: %1").arg(dialog.rules().size()));
    }
}

void MainWindow::onControlPanelToggled()
{
    if (!m_controlPanel) {
        m_controlPanel = new ControlPanel(this);
        m_controlPanel->setWindowFlags(Qt::Window);
        m_controlPanel->setWindowTitle(tr("控件面板 - 交互式调参"));
        m_controlPanel->resize(350, 400);

        // 连接发送信号
        connect(m_controlPanel, &ControlPanel::sendDataRequested,
                this, &MainWindow::onSendData);
    }

    m_controlPanel->show();
    m_controlPanel->raise();
    m_controlPanel->activateWindow();
}

void MainWindow::onDataTableToggled()
{
    if (!m_dataTableWidget) {
        m_dataTableWidget = new DataTableWidget();
        m_dataTableWidget->setWindowFlags(Qt::Window);
        m_dataTableWidget->setWindowTitle(tr("数据表格视图 - 实时数据监控"));
        m_dataTableWidget->resize(900, 500);
    }

    m_dataTableWidget->show();
    m_dataTableWidget->raise();
    m_dataTableWidget->activateWindow();
}

void MainWindow::onDataSearch()
{
    // 显示接收区域的搜索/过滤功能
    if (m_receiveWidget) {
        m_receiveWidget->showSearchBar();
    }
}

} // namespace ComAssistant
