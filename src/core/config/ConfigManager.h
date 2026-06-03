/**
 * @file ConfigManager.h
 * @brief 配置管理器 - 单例模式
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_CONFIGMANAGER_H
#define COMASSISTANT_CONFIGMANAGER_H

#include "AppConfig.h"
#include <QObject>
#include <QString>
#include <QVariant>
#include <QSettings>
#include <memory>

namespace ComAssistant {

/**
 * @brief 配置管理器 - 管理应用程序的所有配置
 */
class ConfigManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ConfigManager* instance();

    /**
     * @brief 初始化配置管理器
     * @param configPath 配置文件路径（为空则使用默认路径）
     * @return 是否初始化成功
     */
    bool initialize(const QString& configPath = QString());

    /**
     * @brief 加载配置文件
     * @param filePath 配置文件路径
     * @return 是否加载成功
     */
    bool loadConfig(const QString& filePath);

    /**
     * @brief 保存配置文件
     * @param filePath 配置文件路径（为空则保存到当前路径）
     * @return 是否保存成功
     */
    bool saveConfig(const QString& filePath = QString());

    /**
     * @brief 重置为默认配置
     */
    void resetToDefault();

    //=========================================================================
    // 全局配置
    //=========================================================================

    void setLanguage(const QString& language);
    QString language() const;

    void setThemeIndex(int index);
    int themeIndex() const;

    void setTheme(const QString& theme);
    QString theme() const;

    void setWindowTitleSuffix(const QString& suffix);
    QString windowTitleSuffix() const;

    void setHighDpiScaling(bool enabled);
    bool highDpiScaling() const;

    void setFirstRun(bool firstRun);
    bool isFirstRun() const;

    void setLastActivatedTab(const QString& tab);
    QString lastActivatedTab() const;

    //=========================================================================
    // 串口配置
    //=========================================================================

    void setSerialConfig(const SerialConfig& config);
    SerialConfig serialConfig() const;

    //=========================================================================
    // 网络配置
    //=========================================================================

    void setNetworkConfig(const NetworkConfig& config);
    NetworkConfig networkConfig() const;

    //=========================================================================
    // HID配置
    //=========================================================================

    void setHidConfig(const HidConfig& config);
    HidConfig hidConfig() const;

    //=========================================================================
    // 通用存取方法
    //=========================================================================

    void setValue(const QString& key, const QVariant& value);
    QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 获取配置文件路径
     */
    QString configFilePath() const;

    /**
     * @brief 获取配置文件版本
     */
    int configVersion() const;

#ifdef COMASSISTANT_TESTS
    /**
     * @brief 测试专用：重置单例到未初始化状态。
     *
     * 主要流程：先同步并释放当前 QSettings，再恢复默认内存配置和初始化标记。
     * 该接口只在单元测试目标中编译，避免测试自定义配置文件路径污染
     * 后续主窗口测试或真实应用配置。
     */
    void resetForTest();
#endif

signals:
    /**
     * @brief 配置变更信号
     */
    void configChanged(const QString& key);

    /**
     * @brief 语言变更信号
     */
    void languageChanged(const QString& language);

    /**
     * @brief 主题变更信号
     */
    void themeChanged(int themeIndex);

private:
    ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void loadDefaults();
    bool writeConfigToSettings(QSettings& settings);      ///< 将当前内存配置写入指定 QSettings，支持默认保存和显式路径导出复用
    void migrateConfig(int fromVersion, int toVersion);

    std::unique_ptr<QSettings> m_settings;
    QString m_configPath;
    bool m_initialized = false;

    // 配置数据
    GlobalConfig m_globalConfig;
    SerialConfig m_serialConfig;
    NetworkConfig m_networkConfig;
    HidConfig m_hidConfig;
};

} // namespace ComAssistant

#endif // COMASSISTANT_CONFIGMANAGER_H
