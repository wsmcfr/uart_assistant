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
#include "upgrade/AppUpdateChecker.h"
#include "version.h"

#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
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
#include <QDesktopServices>
#include <QPainter>
#include <QtMath>
#include <QScrollBar>
#include <QSizePolicy>
#include <functional>

namespace ComAssistant {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupToolBar();
    setupStatusBar();
    setupConnections();
    loadSettings();

    // 加载语言设置（必须在 UI 创建后）
    loadLanguage(m_currentLanguage);

    // 如果 main.cpp 已经从自动保存文件恢复了会话，这里把 SessionManager
    // 中的恢复数据真正应用到主窗口；否则恢复只停留在内存里，用户界面仍是默认状态。
    if (SessionManager::instance()->isModified()) {
        applySessionDataToUi(SessionManager::instance()->currentSession());
    }

    m_appUpdateChecker = new AppUpdateChecker(this);
    m_appUpdateChecker->setRepository(APP_REPOSITORY_OWNER, APP_REPOSITORY_NAME);
    m_appUpdateChecker->setCurrentVersion(APP_VERSION);
    connect(m_appUpdateChecker, &AppUpdateChecker::updateAvailable, this, [this](const ReleaseInfo& info, bool manualTriggered) {
        handleUpdateAvailable(info.versionTag,
                              info.versionNumber,
                              info.body,
                              info.htmlUrl,
                              info.downloadUrl,
                              manualTriggered);
    });
    connect(m_appUpdateChecker, &AppUpdateChecker::alreadyLatest, this, [this](const ReleaseInfo& info, bool manualTriggered) {
        handleAlreadyLatest(info.versionTag, manualTriggered);
    });
    connect(m_appUpdateChecker, &AppUpdateChecker::checkFailed, this, &MainWindow::handleUpdateCheckFailed);

    setWindowTitle(QString("%1 v%2").arg(APP_NAME).arg(APP_VERSION));
    if (!m_hasRestoredWindowGeometry) {
        /*
         * 默认窗口给足主工具栏、串口接收选项和发送区横向空间。
         * 用户缩小窗口时各模式内部再用滚动/弹性布局兜底。
         */
        resize(1460, 900);
    }

    QTimer::singleShot(3000, this, [this]() {
        scheduleAutoUpdateCheck();
    });

    m_uiInitialized = true;
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
    mainLayout->setContentsMargins(0, 0, 0, 0);
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

    // 模式专属工具栏容器：使用横向滚动区承载，英文和长按钮不会被右侧裁剪。
    m_modeToolBarScrollArea = new QScrollArea;
    m_modeToolBarScrollArea->setObjectName("modeToolBarScrollArea");
    m_modeToolBarScrollArea->setWidgetResizable(false);
    m_modeToolBarScrollArea->setFrameShape(QFrame::NoFrame);
    m_modeToolBarScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_modeToolBarScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_modeToolBarScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_modeToolBarScrollArea->hide();  // 初始无模式工具栏时隐藏

    m_modeToolBarContainer = new QWidget;
    m_modeToolBarContainer->setObjectName("modeToolBarContainer");
    m_modeToolBarContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout* modeToolBarLayout = new QVBoxLayout(m_modeToolBarContainer);
    modeToolBarLayout->setContentsMargins(0, 0, 0, 0);
    modeToolBarLayout->setSpacing(0);
    m_modeToolBarScrollArea->setWidget(m_modeToolBarContainer);
    centerLayout->addWidget(m_modeToolBarScrollArea);

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

void MainWindow::setupToolBar()
{
    m_mainToolBar = addToolBar(tr("主工具栏"));
    m_mainToolBar->setObjectName("mainToolBar");
    m_mainToolBar->setMovable(false);
    m_mainToolBar->setIconSize(QSize(20, 20));

    // 汉堡菜单按钮（放在最左侧，使用 QToolButton 获得正确的菜单弹出位置）
    m_hamburgerMenuBtn = new QToolButton;
    m_hamburgerMenuBtn->setObjectName("hamburgerMenuBtn");
    m_hamburgerMenuBtn->setText(QString::fromUtf8("\xE2\x98\xB0"));  // ☰ 汉堡菜单图标
    m_hamburgerMenuBtn->setFixedSize(36, 36);
    m_hamburgerMenuBtn->setToolTip(tr("菜单"));
    m_hamburgerMenuBtn->setPopupMode(QToolButton::InstantPopup);
    m_mainToolBar->addWidget(m_hamburgerMenuBtn);

    // 创建汉堡菜单
    m_hamburgerMenu = new QMenu(this);
    populateHamburgerMenu();

    // 汉堡菜单按钮（手动弹出，避免 setMenu 在 Qt5/Windows 上的定位 bug）
    connect(m_hamburgerMenuBtn, &QToolButton::clicked, this, [this]() {
        if (!m_hamburgerMenu || !m_hamburgerMenuBtn) {
            return;
        }

        // 计算按钮左下角的全局坐标
        QPoint pos = m_hamburgerMenuBtn->mapToGlobal(m_hamburgerMenuBtn->rect().bottomLeft());
        m_hamburgerMenu->exec(pos);
    });

    // 主题切换按钮（紧跟汉堡菜单，始终可见）
    m_themeBtn = new ThemeButton;
    connect(m_themeBtn, &ThemeButton::clicked, this, &MainWindow::onThemeToggled);
    m_mainToolBar->addWidget(m_themeBtn);

    m_mainToolBar->addSeparator();

    // 通信类型选择下拉框
    m_commTypeLabel = new QLabel(tr("类型:"));
    m_mainToolBar->addWidget(m_commTypeLabel);

    m_commTypeCombo = new QComboBox;
    m_commTypeCombo->setObjectName("commTypeCombo");
    m_commTypeCombo->setMinimumWidth(92);
    m_commTypeCombo->setMaximumWidth(132);
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
    /*
     * 顶部工具栏必须优先保持一行可读。串口名可能较长，给它一个合理
     * 最大宽度，超出的内容仍可通过下拉框查看，避免把右侧操作区顶歪。
     */
    m_portCombo->setMinimumWidth(190);
    m_portCombo->setMaximumWidth(260);
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
    m_baudCombo->setMinimumWidth(86);
    m_baudCombo->setMaximumWidth(110);
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

    m_ipLabel = new QLabel(tr("IP:"));
    networkLayout->addWidget(m_ipLabel);

    m_ipEdit = new QLineEdit;
    m_ipEdit->setPlaceholderText("127.0.0.1");
    m_ipEdit->setText("127.0.0.1");
    m_ipEdit->setMinimumWidth(120);
    m_ipEdit->setToolTip(tr("远程IP地址（TCP客户端）或本地绑定地址"));
    networkLayout->addWidget(m_ipEdit);

    m_networkPortLabel = new QLabel(tr("端口:"));
    networkLayout->addWidget(m_networkPortLabel);

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
    m_openPortBtn->setMinimumWidth(96);
    m_openPortBtn->setMaximumWidth(116);
    connect(m_openPortBtn, &QPushButton::clicked, this, &MainWindow::onOpenPortClicked);
    m_mainToolBar->addWidget(m_openPortBtn);

    m_mainToolBar->addSeparator();

    /*
     * 主工具栏左侧是连接参数，右侧是全局操作和显示模式。中间使用弹性
     * spacer 吃掉窗口剩余宽度，避免所有控件挤在左侧、右边留出一整块空白。
     */
    m_mainToolBarSpacer = new QWidget;
    m_mainToolBarSpacer->setObjectName("mainToolBarSpacer");
    m_mainToolBarSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_mainToolBar->addWidget(m_mainToolBarSpacer);

    // 清除按钮（带图标）
    m_clearAction = m_mainToolBar->addAction(
        style()->standardIcon(QStyle::SP_DialogResetButton), tr("清空"));
    m_clearAction->setToolTip(tr("清空接收区和发送区"));
    connect(m_clearAction, &QAction::triggered, this, &MainWindow::onClearAll);

    // 导出按钮（带图标）
    m_exportAction = m_mainToolBar->addAction(
        style()->standardIcon(QStyle::SP_DialogSaveButton), tr("导出"));
    m_exportAction->setToolTip(tr("导出数据到文件"));
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::onExportData);

    m_mainToolBar->addSeparator();

    // 显示模式下拉框
    m_displayModeLabel = new QLabel(tr("模式:"));
    m_mainToolBar->addWidget(m_displayModeLabel);

    m_displayModeCombo = new QComboBox;
    m_displayModeCombo->setObjectName("displayModeCombo");
    m_displayModeCombo->setMinimumWidth(82);
    m_displayModeCombo->setMaximumWidth(112);
    m_displayModeCombo->setToolTip(tr("选择显示模式"));
    m_displayModeCombo->addItem(tr("串口"), static_cast<int>(DisplayMode::Serial));
    m_displayModeCombo->addItem(tr("终端"), static_cast<int>(DisplayMode::Terminal));
    m_displayModeCombo->addItem(tr("帧模式"), static_cast<int>(DisplayMode::Frame));
    m_displayModeCombo->addItem(tr("调试"), static_cast<int>(DisplayMode::Debug));
    connect(m_displayModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDisplayModeChanged);
    m_mainToolBar->addWidget(m_displayModeCombo);

    // 完全隐藏菜单栏（使用汉堡菜单替代）
    menuBar()->hide();
    menuBar()->setMaximumHeight(0);
    menuBar()->setContentsMargins(0, 0, 0, 0);

    // 连接信号
    connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onToolbarPortChanged);
    connect(m_baudCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onToolbarBaudChanged);

    // 初始刷新串口列表
    refreshPorts();
    updateCommunicationWidgetsForType();
    updateConnectionButtonText();
}

void MainWindow::setupStatusBar()
{
    QStatusBar* statusBar = this->statusBar();

    m_statusLabel = new QLabel(tr("未连接"));
    m_statusLabel->setObjectName("statusLabel");
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

    // 绘图协议自动检测器
    m_plotDetector = new PlotProtocolDetector(this);
    connect(m_plotDetector, &PlotProtocolDetector::protocolDetected,
            this, &MainWindow::onPlotProtocolAutoDetected);
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
    const int hexBufferMb = settings.value("Display/HexBufferMB", 8).toInt();
    const int hexBufferBytes = qMax(1, qMin(hexBufferMb, 512)) * 1024 * 1024;
    QFont displayFont(fontFamily, fontSize);

    if (m_serialModeWidget && m_serialModeWidget->receiveWidget()) {
        auto* receiveWidget = m_serialModeWidget->receiveWidget();
        receiveWidget->setTimestampEnabled(showTimestamp);
        receiveWidget->setAutoScrollEnabled(autoScroll);
        receiveWidget->setDisplayFont(displayFont);
        receiveWidget->setMaxLines(maxLines);
        receiveWidget->setHexBufferBytes(hexBufferBytes);
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
        updateCommunicationWidgetsForType();
        updateConnectionButtonText();
        return;
    }

    m_connected = true;
    updateCommunicationWidgetsForType();
    updateConnectionButtonText();
    LOG_INFO(QString("Connected: %1").arg(m_communication->statusString()));
}

void MainWindow::onDisconnectClicked()
{
    if (m_communication) {
        m_communication->close();
    }
    m_connected = false;
    updateCommunicationWidgetsForType();
    updateConnectionButtonText();
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

    // 自动检测绘图协议（仅在未手动选择协议时）
    if (m_currentProtocolType == ProtocolType::Raw && m_plotDetector) {
        m_plotDetector->feedData(data);
        return;
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

    // 断开连接时重置自动检测器
    if (!connected && m_plotDetector) {
        m_plotDetector->reset();
    }

    // 通信实现会在串口、TCP、UDP 的 open/close 路径 emit 状态变化；
    // 主窗口只在这里集中刷新按钮文字和控件可用性，避免网络模式被写回“打开串口”。
    updateCommunicationWidgetsForType();
    updateConnectionButtonText();

    if (connected) {
        m_statusLabel->setText(tr("已连接: %1").arg(
            m_communication ? m_communication->statusString() : QString()));
        m_statusLabel->setProperty("connected", true);
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
    } else {
        m_statusLabel->setText(tr("未连接"));
        m_statusLabel->setProperty("connected", false);
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
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
    updateCommunicationWidgetsForType();
    updateConnectionButtonText();

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

    applySessionDataToUi(sm->currentSession());

    statusBar()->showMessage(tr("会话已加载: %1").arg(filePath), 3000);
}

/**
 * @brief 将会话数据应用到主窗口和子控件。
 *
 * 主要流程：恢复通信配置、工具栏输入框、绘图协议、显示模式、快捷发送项和窗口状态。
 * 该函数同时被“加载会话”和“崩溃恢复”复用，避免恢复流程只更新 SessionManager
 * 内存数据却没有同步到界面。
 *
 * @param session 已由 SessionManager/SessionSerializer 校验并解析出的会话数据。
 */
void MainWindow::applySessionDataToUi(const SessionData& session)
{
    // 通信配置先写回成员变量，再同步到设置面板和工具栏。
    m_currentCommType = session.commType;
    m_serialConfig = session.serialConfig;
    m_networkConfig = session.networkConfig;

    if (m_serialSettings) {
        m_serialSettings->setConfig(m_serialConfig);
    }
    if (m_networkSettings) {
        m_networkSettings->setConfig(m_networkConfig);
    }

    if (m_commTypeCombo) {
        const int commIndex = m_commTypeCombo->findData(static_cast<int>(m_currentCommType));
        if (commIndex >= 0) {
            m_commTypeCombo->blockSignals(true);
            m_commTypeCombo->setCurrentIndex(commIndex);
            m_commTypeCombo->blockSignals(false);
        }
    }

    if (m_portCombo && !m_serialConfig.portName.isEmpty()) {
        int portIdx = m_portCombo->findData(m_serialConfig.portName);
        if (portIdx < 0) {
            portIdx = m_portCombo->findText(m_serialConfig.portName);
        }
        if (portIdx >= 0) {
            m_portCombo->setCurrentIndex(portIdx);
        }
    }

    if (m_baudCombo) {
        const int baudIdx = m_baudCombo->findData(m_serialConfig.baudRate);
        if (baudIdx >= 0) {
            m_baudCombo->setCurrentIndex(baudIdx);
        } else {
            m_baudCombo->setCurrentText(QString::number(m_serialConfig.baudRate));
        }
    }

    if (m_ipEdit) {
        switch (m_currentCommType) {
            case CommType::TcpClient:
                m_ipEdit->setText(m_networkConfig.serverIp);
                break;
            case CommType::TcpServer:
                m_ipEdit->setText(QStringLiteral("0.0.0.0"));
                break;
            case CommType::Udp:
                m_ipEdit->setText(m_networkConfig.remoteIp.isEmpty()
                                      ? QStringLiteral("127.0.0.1")
                                      : m_networkConfig.remoteIp);
                break;
            case CommType::Serial:
            default:
                break;
        }
    }

    if (m_portSpin) {
        switch (m_currentCommType) {
            case CommType::TcpClient:
                m_portSpin->setValue(m_networkConfig.serverPort);
                break;
            case CommType::TcpServer:
                m_portSpin->setValue(m_networkConfig.listenPort);
                break;
            case CommType::Udp:
                m_portSpin->setValue(m_networkConfig.remotePort > 0
                                         ? m_networkConfig.remotePort
                                         : m_networkConfig.listenPort);
                break;
            case CommType::Serial:
            default:
                break;
        }
    }

    // 协议枚举来自会话文件，落在 supportedTypes 外时保守退回 Raw，避免非法值进入工厂。
    ProtocolType restoredProtocol = static_cast<ProtocolType>(session.protocolType);
    if (!ProtocolFactory::supportedTypes().contains(restoredProtocol)) {
        restoredProtocol = ProtocolType::Raw;
    }
    onProtocolTypeChanged(restoredProtocol);
    if (m_protocolActionGroup) {
        for (QAction* action : m_protocolActionGroup->actions()) {
            if (action->data().toInt() == static_cast<int>(m_currentProtocolType)) {
                action->setChecked(true);
                break;
            }
        }
    }

    // 显示模式只接受已知 0..3 范围，损坏会话文件中的非法值不会触发越界切换。
    if (m_displayModeCombo) {
        const int displayIndex = m_displayModeCombo->findData(session.displayMode);
        if (displayIndex >= 0) {
            m_displayModeCombo->setCurrentIndex(displayIndex);
        }
    }

    if (m_quickSendWidget) {
        m_quickSendWidget->clearAll();
        for (const auto& item : session.quickSendItems) {
            m_quickSendWidget->addItem(item);
        }
    }

    if (!session.windowGeometry.isEmpty()) {
        restoreGeometry(session.windowGeometry);
        m_hasRestoredWindowGeometry = true;
    }
    if (m_mainSplitter && !session.splitterState.isEmpty()) {
        m_mainSplitter->restoreState(session.splitterState);
    }

    updateCommunicationWidgetsForType();
    updateConnectionButtonText();
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

void MainWindow::onCheckForUpdates()
{
    checkForAppUpdates(true);
}

void MainWindow::scheduleAutoUpdateCheck()
{
    QSettings settings;
    if (!settings.value("Updates/AutoCheckEnabled", true).toBool()) {
        return;
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QString lastCheckText = settings.value("Updates/LastCheckUtc").toString();
    const QDateTime lastCheckUtc = QDateTime::fromString(lastCheckText, Qt::ISODate);

    // 自动检查默认每天一次，避免频繁请求 GitHub API。
    if (lastCheckUtc.isValid() && lastCheckUtc.secsTo(nowUtc) < 24 * 3600) {
        return;
    }

    checkForAppUpdates(false);
}

void MainWindow::checkForAppUpdates(bool manualTriggered)
{
    if (!m_appUpdateChecker) {
        if (manualTriggered) {
            QMessageBox::warning(this, tr("检查更新"), tr("更新模块未初始化。"));
        }
        return;
    }

    QSettings settings;
    settings.setValue("Updates/LastCheckUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_appUpdateChecker->setCurrentVersion(APP_VERSION);
    m_appUpdateChecker->checkForUpdates(manualTriggered);
}

void MainWindow::handleUpdateAvailable(const QString& latestTag,
                                       const QString& latestVersion,
                                       const QString& releaseNotes,
                                       const QUrl& releaseUrl,
                                       const QUrl& downloadUrl,
                                       bool manualTriggered)
{
    QSettings settings;
    const QString ignoredTag = settings.value("Updates/IgnoredVersion").toString().trimmed();
    if (!manualTriggered && !ignoredTag.isEmpty() &&
        QString::compare(ignoredTag, latestTag, Qt::CaseInsensitive) == 0) {
        return;
    }

    QString notePreview = releaseNotes.trimmed();
    if (notePreview.length() > 600) {
        notePreview = notePreview.left(600) + tr("\n...（更多内容请查看发布页面）");
    }

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setWindowTitle(tr("发现新版本"));
    msgBox.setText(tr("检测到新版本 %1（当前版本 %2）").arg(latestTag, APP_VERSION));
    msgBox.setInformativeText(tr("是否立即打开下载页面？"));

    if (!notePreview.isEmpty()) {
        msgBox.setDetailedText(notePreview);
    }

    QPushButton* downloadButton = msgBox.addButton(tr("下载更新"), QMessageBox::AcceptRole);
    QPushButton* ignoreButton = msgBox.addButton(tr("忽略此版本"), QMessageBox::DestructiveRole);
    QPushButton* laterButton = msgBox.addButton(tr("稍后提醒"), QMessageBox::RejectRole);
    Q_UNUSED(laterButton);

    msgBox.exec();

    if (msgBox.clickedButton() == downloadButton) {
        const QUrl targetUrl = downloadUrl.isValid() ? downloadUrl : releaseUrl;
        if (targetUrl.isValid()) {
            QDesktopServices::openUrl(targetUrl);
        } else {
            QMessageBox::warning(this, tr("下载更新"), tr("未找到可用的下载链接。"));
        }
        return;
    }

    if (msgBox.clickedButton() == ignoreButton) {
        settings.setValue("Updates/IgnoredVersion", latestTag);
    }

    // 如果检测到更高版本，则清除旧的忽略标记，避免用户永远看不到更新提示。
    if (!ignoredTag.isEmpty() && AppUpdateChecker::compareVersions(latestVersion, ignoredTag) > 0) {
        settings.remove("Updates/IgnoredVersion");
    }
}

void MainWindow::handleAlreadyLatest(const QString& latestTag, bool manualTriggered)
{
    if (!manualTriggered) {
        return;
    }

    if (latestTag.isEmpty()) {
        QMessageBox::information(this, tr("检查更新"), tr("当前已是最新版本（%1）。").arg(APP_VERSION));
    } else {
        QMessageBox::information(this,
                                 tr("检查更新"),
                                 tr("当前已是最新版本（本地 %1，最新 %2）。").arg(APP_VERSION, latestTag));
    }
}

void MainWindow::handleUpdateCheckFailed(const QString& error, bool manualTriggered)
{
    LOG_WARN(QString("Update check failed: %1").arg(error));
    if (manualTriggered) {
        QMessageBox::warning(this, tr("检查更新"), error);
    }
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
        qssFile.close();
        qApp->setStyleSheet(styleSheet);
        LOG_INFO(QString("Theme applied: %1").arg(theme));
    } else {
        LOG_ERROR(QString("Failed to load theme: %1").arg(qssPath));
    }

    // 更新主题按钮状态（ThemeButton 自行绘制图标，不受 QSS 干扰）
    if (m_themeBtn) {
        m_themeBtn->setDarkTheme(theme == "dark");
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
        // 递归发送 LanguageChange 事件到所有控件（包括子窗口）
        const auto topLevels = QApplication::topLevelWidgets();
        std::function<void(QWidget*)> sendRecursive = [&](QWidget* w) {
            QCoreApplication::sendEvent(w, &event);
            for (QObject* child : w->children()) {
                if (QWidget* childWidget = qobject_cast<QWidget*>(child)) {
                    sendRecursive(childWidget);
                }
            }
        };
        for (QWidget* widget : topLevels) {
            if (widget) {
                sendRecursive(widget);
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

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    /*
     * 窗口宽度变化后，模式工具栏滚动区的 viewport 宽度也会变化。
     * 重新计算一次容器宽度，保证窄窗口时工具栏自己横向滚动，而不是裁掉尾部控件。
     */
    if (m_uiInitialized && m_displayModeCombo && m_modeToolBarScrollArea) {
        onDisplayModeChanged(m_displayModeCombo->currentIndex());
    }
}

void MainWindow::retranslateUi()
{
    // 初始化完成前不执行更新（构造函数中 loadLanguage 会触发此函数）
    if (!m_uiInitialized) {
        return;
    }

    LOG_INFO("retranslateUi called");

    // 更新窗口标题
    setWindowTitle(QString("%1 v%2").arg(APP_NAME).arg(APP_VERSION));

    // 直接更新工具栏控件文本（不销毁重建，避免 QComboBox item 翻译失效）
    if (m_mainToolBar) {
        m_mainToolBar->setWindowTitle(tr("主工具栏"));
    }
    if (m_hamburgerMenuBtn) {
        m_hamburgerMenuBtn->setToolTip(tr("菜单"));
    }
    if (m_commTypeLabel) {
        m_commTypeLabel->setText(tr("类型:"));
    }

    // 更新通信类型下拉框（clear + addItem 确保翻译生效）
    if (m_commTypeCombo) {
        const QVariant currentTypeData = m_commTypeCombo->currentData();
        m_commTypeCombo->blockSignals(true);
        m_commTypeCombo->clear();
        m_commTypeCombo->addItem(tr("串口"), static_cast<int>(CommType::Serial));
        m_commTypeCombo->addItem(tr("TCP客户端"), static_cast<int>(CommType::TcpClient));
        m_commTypeCombo->addItem(tr("TCP服务器"), static_cast<int>(CommType::TcpServer));
        m_commTypeCombo->addItem(tr("UDP"), static_cast<int>(CommType::Udp));
        const int currentTypeIndex = m_commTypeCombo->findData(currentTypeData);
        m_commTypeCombo->setCurrentIndex(currentTypeIndex >= 0 ? currentTypeIndex : 0);
        m_commTypeCombo->blockSignals(false);
        m_commTypeCombo->setToolTip(tr("选择通信类型"));
    }

    // 更新串口相关控件
    if (m_portCombo) {
        m_portCombo->setToolTip(tr("选择串口"));
    }
    if (m_refreshBtn) {
        m_refreshBtn->setToolTip(tr("刷新串口列表"));
    }
    if (m_baudCombo) {
        m_baudCombo->setToolTip(tr("波特率"));
    }

    // 更新显示模式下拉框（clear + addItem 确保翻译生效）
    if (m_displayModeCombo) {
        const QVariant currentModeData = m_displayModeCombo->currentData();
        m_displayModeCombo->blockSignals(true);
        m_displayModeCombo->clear();
        m_displayModeCombo->addItem(tr("串口"), static_cast<int>(DisplayMode::Serial));
        m_displayModeCombo->addItem(tr("终端"), static_cast<int>(DisplayMode::Terminal));
        m_displayModeCombo->addItem(tr("帧模式"), static_cast<int>(DisplayMode::Frame));
        m_displayModeCombo->addItem(tr("调试"), static_cast<int>(DisplayMode::Debug));
        const int currentModeIndex = m_displayModeCombo->findData(currentModeData);
        m_displayModeCombo->setCurrentIndex(currentModeIndex >= 0 ? currentModeIndex : 0);
        m_displayModeCombo->blockSignals(false);
        m_displayModeCombo->setToolTip(tr("选择显示模式"));
    }

    if (m_ipLabel) {
        m_ipLabel->setText(tr("IP:"));
    }
    if (m_networkPortLabel) {
        m_networkPortLabel->setText(tr("端口:"));
    }
    if (m_clearAction) {
        m_clearAction->setText(tr("清空"));
        m_clearAction->setToolTip(tr("清空接收区和发送区"));
    }
    if (m_exportAction) {
        m_exportAction->setText(tr("导出"));
        m_exportAction->setToolTip(tr("导出数据到文件"));
    }
    if (m_displayModeLabel) {
        m_displayModeLabel->setText(tr("模式:"));
    }

    updateCommunicationWidgetsForType();
    updateConnectionButtonText();

    if (m_themeBtn) {
        m_themeBtn->setToolTip(tr("切换主题"));
    }

    // 更新状态栏
    if (m_statusLabel) {
        m_statusLabel->setText(m_connected ? tr("已连接") : tr("未连接"));
    }

    // 更新汉堡菜单中的所有菜单项（通过重建菜单）
    // 注意：汉堡菜单是 QMenu，需要重建才能更新翻译
    if (m_hamburgerMenu) {
        rebuildHamburgerMenu();
    }

    // 语言切换后模式工具栏的 sizeHint 会变化，重新挂载一次以刷新滚动宽度。
    if (m_displayModeCombo) {
        onDisplayModeChanged(m_displayModeCombo->currentIndex());
    }

    LOG_INFO("UI retranslated");
}

/**
 * @brief 填充主窗口汉堡菜单。
 *
 * 主要流程：清空旧菜单项，重新创建文件/编辑/视图/工具/绘图/帮助菜单，
 * 并把语言选择、绘图协议、窗口同步等可勾选状态同步回当前配置。
 * 此函数同时服务初始化和语言切换，避免两份菜单构建逻辑翻译不一致。
 */
void MainWindow::populateHamburgerMenu()
{
    if (!m_hamburgerMenu) {
        return;
    }

    // 保存当前语言选择状态
    QString currentLang = m_currentLanguage;

    // 清除旧菜单
    m_hamburgerMenu->clear();

    // 重新创建所有菜单项
    QMenu* fileMenu = m_hamburgerMenu->addMenu(tr("文件"));
    fileMenu->addAction(tr("新建会话"), this, &MainWindow::onNewSession, QKeySequence::New);
    fileMenu->addAction(tr("保存会话"), this, &MainWindow::onSaveSession, QKeySequence::Save);
    fileMenu->addAction(tr("加载会话"), this, &MainWindow::onLoadSession, QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("导出数据..."), this, &MainWindow::onExportData);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出"), this, &QMainWindow::close, QKeySequence::Quit);

    QMenu* editMenu = m_hamburgerMenu->addMenu(tr("编辑"));
    editMenu->addAction(tr("清空全部"), this, &MainWindow::onClearAll, QKeySequence(Qt::CTRL | Qt::Key_L));
    editMenu->addAction(tr("数据搜索..."), this, &MainWindow::onDataSearch, QKeySequence::Find);

    QMenu* viewMenu = m_hamburgerMenu->addMenu(tr("视图"));
    viewMenu->addAction(tr("置顶显示"))->setCheckable(true);

    // 语言子菜单
    QMenu* languageMenu = viewMenu->addMenu(tr("语言"));
    QActionGroup* langGroup = new QActionGroup(this);
    QAction* zhAction = languageMenu->addAction(tr("简体中文"));
    zhAction->setCheckable(true);
    zhAction->setData("zh_CN");
    langGroup->addAction(zhAction);

    QAction* enAction = languageMenu->addAction(tr("English"));
    enAction->setCheckable(true);
    enAction->setData("en_US");
    langGroup->addAction(enAction);

    if (currentLang.isEmpty() || currentLang == "zh_CN") {
        zhAction->setChecked(true);
    } else {
        enAction->setChecked(true);
    }

    connect(langGroup, &QActionGroup::triggered, [this](QAction* action) {
        onLanguageChanged(action->data().toString());
    });

    QMenu* toolsMenu = m_hamburgerMenu->addMenu(tr("工具"));
    toolsMenu->addAction(tr("工具箱..."), this, &MainWindow::onToolbox);
    toolsMenu->addAction(tr("脚本编辑器..."), this, &MainWindow::onScriptEditor);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("文件传输..."), this, &MainWindow::onFileTransfer);
    toolsMenu->addAction(tr("IAP升级..."), this, &MainWindow::onIAPUpgrade);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("宏录制/回放..."), this, &MainWindow::onMacroManager);
    toolsMenu->addAction(tr("多端口管理..."), this, &MainWindow::onMultiPortManager);
    toolsMenu->addAction(tr("Modbus分析..."), this, &MainWindow::onModbusAnalyzer);
    toolsMenu->addAction(tr("数据分窗..."), this, &MainWindow::onDataWindowConfig);
    toolsMenu->addAction(tr("控件面板..."), this, &MainWindow::onControlPanelToggled);
    toolsMenu->addAction(tr("数据表格..."), this, &MainWindow::onDataTableToggled);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("设置..."), this, &MainWindow::onSettings);

    QMenu* plotMenu = m_hamburgerMenu->addMenu(tr("绘图"));
    plotMenu->addAction(tr("新建绘图窗口"), this, &MainWindow::onNewPlotWindow);
    plotMenu->addAction(tr("关闭所有绘图窗口"), this, &MainWindow::onCloseAllPlotWindows);
    plotMenu->addSeparator();

    QMenu* protocolMenu = plotMenu->addMenu(tr("绘图协议"));
    QActionGroup* protocolGroup = new QActionGroup(this);
    struct { const char* name; ProtocolType type; } protocols[] = {
        {QT_TR_NOOP("无"), ProtocolType::Raw},
        {QT_TR_NOOP("TEXT绘图"), ProtocolType::TextPlot},
        {QT_TR_NOOP("STAMP绘图"), ProtocolType::StampPlot},
        {QT_TR_NOOP("CSV绘图"), ProtocolType::CsvPlot},
        {QT_TR_NOOP("JustFloat"), ProtocolType::JustFloat},
    };
    for (auto& p : protocols) {
        QAction* act = protocolMenu->addAction(tr(p.name));
        act->setCheckable(true);
        act->setData(static_cast<int>(p.type));
        protocolGroup->addAction(act);
        if (p.type == m_currentProtocolType) {
            act->setChecked(true);
        }
    }
    connect(protocolGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        int typeVal = action->data().toInt();
        onProtocolTypeChanged(static_cast<ProtocolType>(typeVal));
    });
    m_protocolActionGroup = protocolGroup;

    plotMenu->addSeparator();

    QAction* syncAction = plotMenu->addAction(tr("窗口同步"));
    syncAction->setCheckable(true);
    syncAction->setChecked(PlotterManager::instance()->isSyncEnabled());
    syncAction->setToolTip(tr("启用后，所有绘图窗口的暂停状态将同步"));
    connect(syncAction, &QAction::toggled, [](bool checked) {
        PlotterManager::instance()->setSyncEnabled(checked);
    });

    QMenu* helpMenu = m_hamburgerMenu->addMenu(tr("帮助"));
    helpMenu->addAction(tr("帮助文档"), this, &MainWindow::onHelp, QKeySequence::HelpContents);
    helpMenu->addAction(tr("检查更新..."), this, &MainWindow::onCheckForUpdates);
    QAction* autoCheckUpdateAction = helpMenu->addAction(tr("启动时自动检查更新"));
    autoCheckUpdateAction->setCheckable(true);
    autoCheckUpdateAction->setChecked(QSettings().value("Updates/AutoCheckEnabled", true).toBool());
    connect(autoCheckUpdateAction, &QAction::toggled, this, [](bool checked) {
        QSettings settings;
        settings.setValue("Updates/AutoCheckEnabled", checked);
    });
    helpMenu->addSeparator();
    helpMenu->addAction(tr("关于"), this, &MainWindow::onAbout);
    helpMenu->addAction(tr("关于 Qt"), qApp, &QApplication::aboutQt);
}

/**
 * @brief 重建汉堡菜单。
 *
 * 语言切换时 QMenu 已创建的 QAction 不会自动更新文本，所以通过统一的
 * populateHamburgerMenu 重新填充；菜单对象本身保持不变，按钮持有的指针仍然有效。
 */
void MainWindow::rebuildHamburgerMenu()
{
    populateHamburgerMenu();
}

/**
 * @brief 根据当前通信类型刷新工具栏控件。
 *
 * 主要流程：串口模式显示串口和波特率控件，网络模式显示 IP/端口控件；
 * 同时按连接状态禁用正在使用的配置控件，避免连接后继续改参数造成状态不一致。
 */
void MainWindow::updateCommunicationWidgetsForType()
{
    const bool serialMode = (m_currentCommType == CommType::Serial);
    if (m_portComboAction) {
        m_portComboAction->setVisible(serialMode);
    }
    if (m_refreshBtnAction) {
        m_refreshBtnAction->setVisible(serialMode);
    }
    if (m_baudComboAction) {
        m_baudComboAction->setVisible(serialMode);
    }
    if (m_networkWidgetAction) {
        m_networkWidgetAction->setVisible(!serialMode);
    }

    if (m_portCombo) {
        m_portCombo->setEnabled(serialMode && !m_connected);
    }
    if (m_refreshBtn) {
        m_refreshBtn->setEnabled(serialMode && !m_connected);
    }
    if (m_baudCombo) {
        m_baudCombo->setEnabled(serialMode && !m_connected);
    }
    if (m_networkToolbarWidget) {
        m_networkToolbarWidget->setEnabled(!serialMode && !m_connected);
    }
    if (m_commTypeCombo) {
        m_commTypeCombo->setEnabled(!m_connected);
    }

    if (!serialMode && m_ipEdit) {
        switch (m_currentCommType) {
            case CommType::TcpClient:
                m_ipEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
                m_ipEdit->setToolTip(tr("服务器IP地址"));
                break;
            case CommType::TcpServer:
                m_ipEdit->setPlaceholderText(QStringLiteral("0.0.0.0"));
                m_ipEdit->setToolTip(tr("本地绑定地址（0.0.0.0表示所有）"));
                break;
            case CommType::Udp:
                m_ipEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
                m_ipEdit->setToolTip(tr("目标IP地址"));
                break;
            default:
                break;
        }
    }

    if (m_portSpin) {
        switch (m_currentCommType) {
            case CommType::TcpClient:
                m_portSpin->setToolTip(tr("服务器端口"));
                break;
            case CommType::TcpServer:
                m_portSpin->setToolTip(tr("监听端口"));
                break;
            case CommType::Udp:
                m_portSpin->setToolTip(tr("本地/目标端口"));
                break;
            case CommType::Serial:
            default:
                m_portSpin->setToolTip(tr("端口号"));
                break;
        }
    }
}

/**
 * @brief 根据通信类型和连接状态刷新连接按钮文字。
 *
 * 串口、TCP 客户端、TCP 服务端、UDP 的“打开/关闭”动作语义不同，
 * 统一在这里计算文本，避免语言切换或状态变化时网络模式被误写成串口文案。
 */
void MainWindow::updateConnectionButtonText()
{
    if (!m_openPortBtn) {
        return;
    }

    QString buttonText;
    switch (m_currentCommType) {
        case CommType::Serial:
            buttonText = m_connected ? tr("关闭串口") : tr("打开串口");
            break;
        case CommType::TcpClient:
            buttonText = m_connected ? tr("断开") : tr("连接");
            break;
        case CommType::TcpServer:
            buttonText = m_connected ? tr("停止服务") : tr("启动服务");
            break;
        case CommType::Udp:
            buttonText = m_connected ? tr("解绑") : tr("绑定端口");
            break;
        default:
            buttonText = m_connected ? tr("断开") : tr("连接");
            break;
    }

    m_openPortBtn->setText(buttonText);
    m_openPortBtn->setChecked(m_connected);
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
        // 关闭连接后由统一刷新函数恢复控件可编辑状态和对应文案。
        onDisconnectClicked();
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
        updateCommunicationWidgetsForType();
        updateConnectionButtonText();
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

void MainWindow::onProtocolTypeChanged(ProtocolType type)
{
    m_currentProtocolType = type;

    // 创建对应的协议解析器
    m_currentProtocol.reset();
    if (m_currentProtocolType != ProtocolType::Raw) {
        m_currentProtocol = ProtocolFactory::create(m_currentProtocolType);
    }

    // 清空绘图数据缓冲
    m_plotDataBuffer.clear();

    // 重置自动检测器（手动切换协议后停止自动检测）
    if (m_plotDetector) {
        m_plotDetector->reset();
    }

    LOG_INFO(QString("Protocol changed to: %1").arg(
        ProtocolFactory::typeName(m_currentProtocolType)));
}

/**
 * @brief 绘图协议自动检测回调
 *
 * 当 PlotProtocolDetector 确认检测到绘图协议时调用。
 * 创建对应的协议实例，同步菜单选中状态，并在状态栏显示通知。
 */
void MainWindow::onPlotProtocolAutoDetected(ProtocolType type)
{
    // 更新协议状态
    m_currentProtocolType = type;
    m_currentProtocol = ProtocolFactory::create(type);
    m_plotDataBuffer.clear();

    // 同步菜单选中状态
    if (m_protocolActionGroup) {
        for (QAction* action : m_protocolActionGroup->actions()) {
            if (action->data().toInt() == static_cast<int>(type)) {
                action->setChecked(true);
                break;
            }
        }
    }

    // 状态栏通知
    statusBar()->showMessage(
        tr("自动检测到绘图协议: %1").arg(ProtocolFactory::typeName(type)), 5000);

    LOG_INFO(QString("Auto-detected plot protocol: %1")
             .arg(ProtocolFactory::typeName(type)));
}

void MainWindow::onDisplayModeChanged(int index)
{
    if (!m_displayModeCombo || !m_modeStack) return;

    // 获取新模式
    DisplayMode newMode = static_cast<DisplayMode>(
        m_displayModeCombo->itemData(index).toInt());
    const bool modeChanged = (newMode != m_displayMode) || !m_currentModeWidget;

    // 通知旧模式停用
    if (modeChanged && m_currentModeWidget) {
        m_currentModeWidget->onDeactivated();
    }

    // 隐藏旧模式的工具栏：只解除父子关系，工具栏仍归各自模式组件复用。
    if (m_modeToolBarContainer && m_modeToolBarContainer->layout()) {
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
        if (modeChanged) {
            m_modeStack->setCurrentWidget(newModeWidget);
        }

        // 同步连接状态
        newModeWidget->setConnected(m_connected);

        // 显示模式专属工具栏
        QWidget* modeToolBar = newModeWidget->modeToolBar();
        if (modeToolBar) {
            // QToolBar 默认可以压缩动作，英文翻译较长时末尾控件会被裁掉。
            // 这里统一固定为文本工具栏并放入横向滚动区，让模式控件保持可访问。
            if (QToolBar* toolBar = qobject_cast<QToolBar*>(modeToolBar)) {
                toolBar->setMovable(false);
                toolBar->setFloatable(false);
                toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
                toolBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            }

            m_modeToolBarContainer->layout()->addWidget(modeToolBar);
            modeToolBar->show();
            modeToolBar->adjustSize();

            const QSize toolBarSize = modeToolBar->sizeHint();
            const int viewportWidth = m_modeToolBarScrollArea
                ? m_modeToolBarScrollArea->viewport()->width()
                : 0;
            const int contentWidth = qMax(toolBarSize.width(), viewportWidth);
            const int contentHeight = toolBarSize.height();

            /*
             * 横向滚动条只在宽度不够时出现，因此滚动区高度预留滚动条高度。
             * 这样 Frame/Debug/Terminal 在中英文之间切换时，不会因为滚动条
             * 临时出现导致下方内容跳动或工具栏尾部控件不可点击。
             */
            const int scrollBarHeight = m_modeToolBarScrollArea
                ? m_modeToolBarScrollArea->horizontalScrollBar()->sizeHint().height()
                : 0;
            m_modeToolBarContainer->setMinimumSize(contentWidth, contentHeight);
            m_modeToolBarContainer->resize(contentWidth, contentHeight);
            if (m_modeToolBarScrollArea) {
                m_modeToolBarScrollArea->setFixedHeight(contentHeight + scrollBarHeight);
                m_modeToolBarScrollArea->show();
            }
        } else {
            if (m_modeToolBarScrollArea) {
                m_modeToolBarScrollArea->hide();
            }
        }

        // 只有真正切换模式时才触发激活逻辑；语言切换只刷新工具栏尺寸。
        if (modeChanged) {
            newModeWidget->onActivated();
        }
    }

    QString modeName;
    switch (m_displayMode) {
        case DisplayMode::Serial: modeName = "Serial"; break;
        case DisplayMode::Terminal: modeName = "Terminal"; break;
        case DisplayMode::Frame: modeName = "Frame"; break;
        case DisplayMode::Debug: modeName = "Debug"; break;
    }
    if (modeChanged) {
        LOG_INFO(QString("Display mode changed to: %1").arg(modeName));
    }
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
