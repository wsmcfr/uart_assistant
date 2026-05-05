/**
 * @file main.cpp
 * @brief 程序入口
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QTranslator>
#include <QSettings>

#include "version.h"
#include "core/utils/Logger.h"
#include "core/utils/CrashHandler.h"
#include "core/config/ConfigManager.h"
#include "core/session/SessionManager.h"
#include "ui/MainWindow.h"
#include "ui/dialogs/RecoveryDialog.h"

using namespace ComAssistant;

/**
 * @brief 初始化应用程序数据目录
 */
QString initializeAppDataPath()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appDataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return appDataPath;
}

/**
 * @brief 初始化核心系统
 */
bool initializeCoreSystem(const QString& appDataPath)
{
    // 1. 初始化崩溃处理器
    CrashHandler::instance()->setDumpPath(appDataPath + "/crashes");
    CrashHandler::instance()->setDumpType(CrashHandler::DumpType::Normal);
    CrashHandler::instance()->setMaxDumpFiles(10);
    CrashHandler::instance()->initialize();

    // 2. 初始化日志系统
    if (!Logger::instance()->initialize(appDataPath + "/logs")) {
        return false;
    }

    // 3. 注册崩溃回调（保存配置）
    CrashHandler::instance()->registerCallback([]() {
        ConfigManager::instance()->saveConfig();
        Logger::instance()->flush();
    });

    // 4. 初始化配置管理器
    if (!ConfigManager::instance()->initialize(appDataPath + "/config.ini")) {
        LOG_ERROR("Failed to initialize ConfigManager");
        return false;
    }

    return true;
}

/**
 * @brief 加载翻译文件
 * @note 翻译的动态切换由 MainWindow 管理，这里只记录当前语言设置
 */
void loadTranslation(QApplication& app)
{
    Q_UNUSED(app)
    QString language = ConfigManager::instance()->language();

    // 只记录日志，实际翻译加载由 MainWindow 管理
    if (language.isEmpty() || language == "zh_CN") {
        LOG_INFO("Using default language: zh_CN");
    } else {
        LOG_INFO(QString("Language setting: %1 (will be loaded by MainWindow)").arg(language));
    }
}

int main(int argc, char *argv[])
{
    /*
     * Windows 上的动态 OpenGL 构建默认可能走 ANGLE（libEGL/libGLESV2）。
     * 一旦进程第一次启用 OpenGL，这套运行时通常会常驻到进程生命周期结束，
     * 表现为“OpenGL 关了，但内存基线回不到最初值”。
     *
     * 这里优先强制使用 Desktop OpenGL（opengl32.dll），尽量避开 ANGLE 的
     * 额外常驻占用。若目标机器桌面 OpenGL 能力不足，后续绘图窗口里的
     * OpenGL 开关仍会按现有逻辑自动失败回退到软件绘制，不影响主程序可用性。
     *
     * 同时尊重外部环境变量：如果用户或调试脚本显式设置了 QT_OPENGL，
     * 就不在这里覆盖，便于后续继续诊断兼容性问题。
     */
#ifdef Q_OS_WIN
    if (qEnvironmentVariableIsEmpty("QT_OPENGL")) {
        QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    }
#endif

    // 高DPI支持
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#else
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    // 创建应用程序
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName(APP_NAME);
    app.setApplicationVersion(APP_VERSION);
    app.setOrganizationName(APP_ORGANIZATION);
    app.setOrganizationDomain(APP_DOMAIN);

    // 初始化应用数据目录
    QString appDataPath = initializeAppDataPath();

    // 初始化核心系统
    if (!initializeCoreSystem(appDataPath)) {
        QMessageBox::critical(nullptr, QObject::tr("Error"),
            QObject::tr("Failed to initialize core system.\nPlease check application permissions."));
        return -1;
    }

    LOG_INFO("========================================");
    LOG_INFO(QString("%1 v%2 Starting...").arg(APP_NAME, APP_VERSION));
    LOG_INFO(QString("App Data Path: %1").arg(appDataPath));
    LOG_INFO("========================================");

    // 加载翻译
    loadTranslation(app);

    // 检查首次运行
    if (ConfigManager::instance()->isFirstRun()) {
        LOG_INFO("First run detected");
        ConfigManager::instance()->setFirstRun(false);
        ConfigManager::instance()->saveConfig();
    }

    // 检查是否有恢复数据
    SessionManager* sessionMgr = SessionManager::instance();
    bool recoverySuccess = false;

    if (sessionMgr->hasRecoveryData()) {
        LOG_INFO("Recovery data detected");

        // 检查设置：是否需要弹出恢复对话框
        bool checkRecovery = QSettings().value("General/CheckRecovery", true).toBool();

        if (checkRecovery) {
            // 构建恢复信息
            RecoveryInfo info;
            info.crashTime = sessionMgr->lastRecoveryTime();

            // 弹出恢复对话框
            RecoveryDialog recoveryDialog;
            recoveryDialog.setRecoveryInfo(info);

            if (recoveryDialog.exec() == QDialog::Accepted) {
                switch (recoveryDialog.selectedAction()) {
                    case RecoveryDialog::Action::Recover:
                        LOG_INFO("User chose to recover session");
                        recoverySuccess = sessionMgr->recoverSession();
                        if (!recoverySuccess) {
                            LOG_WARN("Failed to recover session");
                            QMessageBox::warning(nullptr, QObject::tr("恢复失败"),
                                QObject::tr("无法恢复会话数据，将开始新会话。"));
                            sessionMgr->clearRecoveryData();
                        }
                        break;

                    case RecoveryDialog::Action::Discard:
                        LOG_INFO("User chose to discard recovery data");
                        sessionMgr->clearRecoveryData();
                        break;

                    case RecoveryDialog::Action::Later:
                        LOG_INFO("User chose to decide later");
                        // 保留恢复数据，下次再问
                        break;
                }

                // 保存"不再提示"设置
                if (recoveryDialog.dontAskAgain()) {
                    QSettings().setValue("General/CheckRecovery", false);
                    sessionMgr->clearRecoveryData();
                }
            }
        } else {
            // 自动丢弃恢复数据
            LOG_INFO("Auto discarding recovery data (user preference)");
            sessionMgr->clearRecoveryData();
        }
    }

    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();

    LOG_INFO("Application starting main event loop");

    // 运行事件循环
    int result = app.exec();

    // 保存配置
    ConfigManager::instance()->saveConfig();

    LOG_INFO(QString("Application exiting with code: %1").arg(result));

    return result;
}
