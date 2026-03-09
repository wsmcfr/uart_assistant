/**
 * @file ConfigManager.cpp
 * @brief 配置管理器实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "ConfigManager.h"
#include "utils/Logger.h"
#include "version.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace ComAssistant {

ConfigManager* ConfigManager::instance()
{
    static ConfigManager instance;
    return &instance;
}

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    loadDefaults();
}

ConfigManager::~ConfigManager()
{
    if (m_initialized) {
        saveConfig();
    }
}

bool ConfigManager::initialize(const QString& configPath)
{
    if (m_initialized) {
        return true;
    }

    // 确定配置文件路径
    if (configPath.isEmpty()) {
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(appDataPath);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        m_configPath = appDataPath + "/config.ini";
    } else {
        m_configPath = configPath;
    }

    // 创建 QSettings
    // 注意: Qt6 默认使用 UTF-8，无需调用 setIniCodec
    m_settings = std::make_unique<QSettings>(m_configPath, QSettings::IniFormat);

    // 检查是否存在配置文件
    if (QFile::exists(m_configPath)) {
        loadConfig(m_configPath);
    } else {
        // 首次运行，使用默认配置
        m_globalConfig.firstRun = true;
        saveConfig();
    }

    m_initialized = true;
    LOG_INFO(QString("ConfigManager initialized: %1").arg(m_configPath));

    return true;
}

void ConfigManager::loadDefaults()
{
    // 全局配置默认值
    m_globalConfig = GlobalConfig();
    m_globalConfig.language = "zh_CN";
    m_globalConfig.themeIndex = 0;
    m_globalConfig.highDpiScaling = true;
    m_globalConfig.firstRun = true;
    m_globalConfig.lastActivatedTab = "serial";

    // 串口配置默认值
    m_serialConfig = SerialConfig();
    m_serialConfig.baudRate = 115200;
    m_serialConfig.dataBits = DataBits::Eight;
    m_serialConfig.stopBits = StopBits::One;
    m_serialConfig.parity = Parity::None;
    m_serialConfig.flowControl = FlowControl::None;

    // 网络配置默认值
    m_networkConfig = NetworkConfig();
    m_networkConfig.mode = NetworkMode::TcpClient;
    m_networkConfig.serverIp = "127.0.0.1";
    m_networkConfig.serverPort = 8080;

    // HID 配置默认值
    m_hidConfig = HidConfig();
}

bool ConfigManager::loadConfig(const QString& filePath)
{
    if (!m_settings) {
        return false;
    }

    // 读取版本信息
    int version = m_settings->value("General/ConfigVersion", 1).toInt();

    // 读取全局配置
    m_settings->beginGroup("Global");
    m_globalConfig.language = m_settings->value("Language", "zh_CN").toString();
    m_globalConfig.themeIndex = m_settings->value("ThemeIndex", 0).toInt();
    m_globalConfig.windowTitleSuffix = m_settings->value("WindowTitleSuffix", "").toString();
    m_globalConfig.highDpiScaling = m_settings->value("HighDpiScaling", true).toBool();
    m_globalConfig.firstRun = m_settings->value("FirstRun", true).toBool();
    m_globalConfig.lastActivatedTab = m_settings->value("LastActivatedTab", "serial").toString();
    m_settings->endGroup();

    // 读取串口配置
    m_settings->beginGroup("Serial");
    m_serialConfig.portName = m_settings->value("PortName", "").toString();
    m_serialConfig.baudRate = m_settings->value("BaudRate", 115200).toInt();
    m_serialConfig.dataBits = static_cast<DataBits>(m_settings->value("DataBits", 8).toInt());
    m_serialConfig.stopBits = static_cast<StopBits>(m_settings->value("StopBits", 0).toInt());
    m_serialConfig.parity = static_cast<Parity>(m_settings->value("Parity", 0).toInt());
    m_serialConfig.flowControl = static_cast<FlowControl>(m_settings->value("FlowControl", 0).toInt());
    m_serialConfig.readTimeout = m_settings->value("ReadTimeout", 100).toInt();
    m_serialConfig.writeTimeout = m_settings->value("WriteTimeout", 100).toInt();
    m_settings->endGroup();

    // 读取网络配置
    m_settings->beginGroup("Network");
    m_networkConfig.mode = static_cast<NetworkMode>(m_settings->value("Mode", 1).toInt());
    m_networkConfig.serverIp = m_settings->value("ServerIP", "127.0.0.1").toString();
    m_networkConfig.serverPort = m_settings->value("ServerPort", 8080).toInt();
    m_networkConfig.listenPort = m_settings->value("ListenPort", 8080).toInt();
    m_networkConfig.connectTimeout = m_settings->value("ConnectTimeout", 5000).toInt();
    m_networkConfig.maxConnections = m_settings->value("MaxConnections", 10).toInt();
    m_settings->endGroup();

    // 读取HID配置
    m_settings->beginGroup("HID");
    m_hidConfig.vendorId = m_settings->value("VendorId", 0).toUInt();
    m_hidConfig.productId = m_settings->value("ProductId", 0).toUInt();
    m_hidConfig.interfaceNumber = m_settings->value("InterfaceNumber", 0).toUInt();
    m_settings->endGroup();

    LOG_INFO(QString("Configuration loaded from: %1").arg(filePath));
    return true;
}

bool ConfigManager::saveConfig(const QString& filePath)
{
    if (!m_settings) {
        return false;
    }

    QString savePath = filePath.isEmpty() ? m_configPath : filePath;

    // 写入版本信息
    m_settings->setValue("General/ConfigVersion", CONFIG_VERSION_MAJOR);
    m_settings->setValue("General/AppVersion", APP_VERSION);

    // 写入全局配置
    m_settings->beginGroup("Global");
    m_settings->setValue("Language", m_globalConfig.language);
    m_settings->setValue("ThemeIndex", m_globalConfig.themeIndex);
    m_settings->setValue("WindowTitleSuffix", m_globalConfig.windowTitleSuffix);
    m_settings->setValue("HighDpiScaling", m_globalConfig.highDpiScaling);
    m_settings->setValue("FirstRun", m_globalConfig.firstRun);
    m_settings->setValue("LastActivatedTab", m_globalConfig.lastActivatedTab);
    m_settings->endGroup();

    // 写入串口配置
    m_settings->beginGroup("Serial");
    m_settings->setValue("PortName", m_serialConfig.portName);
    m_settings->setValue("BaudRate", m_serialConfig.baudRate);
    m_settings->setValue("DataBits", static_cast<int>(m_serialConfig.dataBits));
    m_settings->setValue("StopBits", static_cast<int>(m_serialConfig.stopBits));
    m_settings->setValue("Parity", static_cast<int>(m_serialConfig.parity));
    m_settings->setValue("FlowControl", static_cast<int>(m_serialConfig.flowControl));
    m_settings->setValue("ReadTimeout", m_serialConfig.readTimeout);
    m_settings->setValue("WriteTimeout", m_serialConfig.writeTimeout);
    m_settings->endGroup();

    // 写入网络配置
    m_settings->beginGroup("Network");
    m_settings->setValue("Mode", static_cast<int>(m_networkConfig.mode));
    m_settings->setValue("ServerIP", m_networkConfig.serverIp);
    m_settings->setValue("ServerPort", m_networkConfig.serverPort);
    m_settings->setValue("ListenPort", m_networkConfig.listenPort);
    m_settings->setValue("ConnectTimeout", m_networkConfig.connectTimeout);
    m_settings->setValue("MaxConnections", m_networkConfig.maxConnections);
    m_settings->endGroup();

    // 写入HID配置
    m_settings->beginGroup("HID");
    m_settings->setValue("VendorId", m_hidConfig.vendorId);
    m_settings->setValue("ProductId", m_hidConfig.productId);
    m_settings->setValue("InterfaceNumber", m_hidConfig.interfaceNumber);
    m_settings->endGroup();

    m_settings->sync();

    LOG_INFO(QString("Configuration saved to: %1").arg(savePath));
    return true;
}

void ConfigManager::resetToDefault()
{
    loadDefaults();
    saveConfig();
    emit configChanged("all");
}

// 全局配置 getter/setter
void ConfigManager::setLanguage(const QString& language)
{
    if (m_globalConfig.language != language) {
        m_globalConfig.language = language;
        emit languageChanged(language);
        emit configChanged("language");
    }
}

QString ConfigManager::language() const
{
    return m_globalConfig.language;
}

void ConfigManager::setThemeIndex(int index)
{
    if (m_globalConfig.themeIndex != index) {
        m_globalConfig.themeIndex = index;
        emit themeChanged(index);
        emit configChanged("theme");
    }
}

int ConfigManager::themeIndex() const
{
    return m_globalConfig.themeIndex;
}

void ConfigManager::setTheme(const QString& theme)
{
    if (m_settings) {
        m_settings->setValue("Global/Theme", theme);
        emit configChanged("theme");
    }
}

QString ConfigManager::theme() const
{
    if (m_settings) {
        return m_settings->value("Global/Theme", "light").toString();
    }
    return "light";
}

void ConfigManager::setWindowTitleSuffix(const QString& suffix)
{
    m_globalConfig.windowTitleSuffix = suffix;
    emit configChanged("windowTitleSuffix");
}

QString ConfigManager::windowTitleSuffix() const
{
    return m_globalConfig.windowTitleSuffix;
}

void ConfigManager::setHighDpiScaling(bool enabled)
{
    m_globalConfig.highDpiScaling = enabled;
    emit configChanged("highDpiScaling");
}

bool ConfigManager::highDpiScaling() const
{
    return m_globalConfig.highDpiScaling;
}

void ConfigManager::setFirstRun(bool firstRun)
{
    m_globalConfig.firstRun = firstRun;
}

bool ConfigManager::isFirstRun() const
{
    return m_globalConfig.firstRun;
}

void ConfigManager::setLastActivatedTab(const QString& tab)
{
    m_globalConfig.lastActivatedTab = tab;
}

QString ConfigManager::lastActivatedTab() const
{
    return m_globalConfig.lastActivatedTab;
}

// 串口配置
void ConfigManager::setSerialConfig(const SerialConfig& config)
{
    m_serialConfig = config;
    emit configChanged("serial");
}

SerialConfig ConfigManager::serialConfig() const
{
    return m_serialConfig;
}

// 网络配置
void ConfigManager::setNetworkConfig(const NetworkConfig& config)
{
    m_networkConfig = config;
    emit configChanged("network");
}

NetworkConfig ConfigManager::networkConfig() const
{
    return m_networkConfig;
}

// HID配置
void ConfigManager::setHidConfig(const HidConfig& config)
{
    m_hidConfig = config;
    emit configChanged("hid");
}

HidConfig ConfigManager::hidConfig() const
{
    return m_hidConfig;
}

// 通用存取
void ConfigManager::setValue(const QString& key, const QVariant& value)
{
    if (m_settings) {
        m_settings->setValue(key, value);
        emit configChanged(key);
    }
}

QVariant ConfigManager::value(const QString& key, const QVariant& defaultValue) const
{
    if (m_settings) {
        return m_settings->value(key, defaultValue);
    }
    return defaultValue;
}

QString ConfigManager::configFilePath() const
{
    return m_configPath;
}

int ConfigManager::configVersion() const
{
    if (m_settings) {
        return m_settings->value("General/ConfigVersion", 1).toInt();
    }
    return 1;
}

void ConfigManager::migrateConfig(int fromVersion, int toVersion)
{
    LOG_INFO(QString("Migrating config from v%1 to v%2").arg(fromVersion).arg(toVersion));
    // 版本迁移逻辑将在需要时实现
}

} // namespace ComAssistant
