/**
 * @file ThemeManager.cpp
 * @brief 主题管理器实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "ThemeManager.h"
#include "../utils/Logger.h"
#include "../config/ConfigManager.h"

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>

namespace ComAssistant {

// 静态成员初始化
ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager* ThemeManager::instance()
{
    if (!s_instance) {
        s_instance = new ThemeManager();
    }
    return s_instance;
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
{
}

bool ThemeManager::initialize()
{
    if (m_initialized) {
        return true;
    }

    // 确保用户主题目录存在
    QDir userDir(userThemesDir());
    if (!userDir.exists()) {
        userDir.mkpath(".");
    }

    // 扫描主题
    scanBuiltinThemes();
    scanUserThemes();

    // 加载保存的主题
    QString savedTheme = ConfigManager::instance()->theme();
    if (savedTheme.isEmpty()) {
        savedTheme = "light";
    }

    if (!loadTheme(savedTheme)) {
        // 回退到默认主题
        loadTheme("light");
    }

    m_initialized = true;
    LOG_INFO("ThemeManager initialized");
    return true;
}

QStringList ThemeManager::availableThemes() const
{
    return m_themes.keys();
}

ThemeInfo ThemeManager::themeInfo(const QString& themeName) const
{
    return m_themes.value(themeName);
}

bool ThemeManager::loadTheme(const QString& themeName)
{
    if (!m_themes.contains(themeName)) {
        LOG_ERROR(QString("Theme not found: %1").arg(themeName));
        return false;
    }

    ThemeInfo info = m_themes.value(themeName);
    QString qss = loadQssFile(info.filePath);

    if (qss.isEmpty() && !info.filePath.isEmpty()) {
        LOG_ERROR(QString("Failed to load theme file: %1").arg(info.filePath));
        return false;
    }

    m_currentTheme = themeName;
    m_currentStyleSheet = qss;

    // 应用样式表
    qApp->setStyleSheet(qss);

    // 保存设置
    ConfigManager::instance()->setTheme(themeName);

    LOG_INFO(QString("Theme loaded: %1").arg(themeName));
    emit themeChanged(themeName);
    return true;
}

bool ThemeManager::saveTheme(const QString& themeName, const QString& qssContent)
{
    if (themeName.isEmpty()) {
        return false;
    }

    // 验证QSS
    QString errorMsg;
    if (!validateQss(qssContent, &errorMsg)) {
        LOG_ERROR(QString("Invalid QSS: %1").arg(errorMsg));
        return false;
    }

    // 保存到用户主题目录
    QString filePath = userThemesDir() + "/" + themeName + ".qss";
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString("Cannot save theme file: %1").arg(filePath));
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << qssContent;
    file.close();

    // 更新主题列表
    ThemeInfo info;
    info.name = themeName;
    info.displayName = themeName;
    info.filePath = filePath;
    info.isBuiltin = false;
    info.isValid = true;
    m_themes[themeName] = info;

    LOG_INFO(QString("Theme saved: %1").arg(themeName));
    emit themeListChanged();
    return true;
}

bool ThemeManager::deleteTheme(const QString& themeName)
{
    if (!m_themes.contains(themeName)) {
        return false;
    }

    ThemeInfo info = m_themes.value(themeName);
    if (info.isBuiltin) {
        LOG_ERROR("Cannot delete builtin theme");
        return false;
    }

    // 删除文件
    QFile file(info.filePath);
    if (file.exists() && !file.remove()) {
        LOG_ERROR(QString("Cannot delete theme file: %1").arg(info.filePath));
        return false;
    }

    // 从列表中移除
    m_themes.remove(themeName);

    // 如果删除的是当前主题，切换到默认
    if (m_currentTheme == themeName) {
        loadTheme("light");
    }

    LOG_INFO(QString("Theme deleted: %1").arg(themeName));
    emit themeListChanged();
    return true;
}

QString ThemeManager::importTheme(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || fileInfo.suffix().toLower() != "qss") {
        LOG_ERROR(QString("Invalid theme file: %1").arg(filePath));
        return QString();
    }

    QString themeName = fileInfo.baseName();

    // 读取内容
    QString qssContent = loadQssFile(filePath);
    if (qssContent.isEmpty()) {
        return QString();
    }

    // 保存到用户目录
    if (!saveTheme(themeName, qssContent)) {
        return QString();
    }

    LOG_INFO(QString("Theme imported: %1").arg(themeName));
    return themeName;
}

bool ThemeManager::exportTheme(const QString& themeName, const QString& filePath)
{
    if (!m_themes.contains(themeName)) {
        return false;
    }

    QString content = readThemeContent(themeName);
    if (content.isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString("Cannot export theme to: %1").arg(filePath));
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << content;
    file.close();

    LOG_INFO(QString("Theme exported: %1 -> %2").arg(themeName, filePath));
    return true;
}

QString ThemeManager::builtinThemesDir() const
{
    return ":/themes";
}

QString ThemeManager::userThemesDir() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataPath + "/themes";
}

void ThemeManager::refreshThemeList()
{
    m_themes.clear();
    scanBuiltinThemes();
    scanUserThemes();
    emit themeListChanged();
}

QString ThemeManager::readThemeContent(const QString& themeName) const
{
    if (!m_themes.contains(themeName)) {
        return QString();
    }

    ThemeInfo info = m_themes.value(themeName);
    return loadQssFile(info.filePath);
}

bool ThemeManager::validateQss(const QString& qssContent, QString* errorMsg) const
{
    // 基本语法检查
    // 检查括号匹配
    int braceCount = 0;
    for (const QChar& c : qssContent) {
        if (c == '{') braceCount++;
        else if (c == '}') braceCount--;
        if (braceCount < 0) {
            if (errorMsg) *errorMsg = tr("Unmatched closing brace");
            return false;
        }
    }

    if (braceCount != 0) {
        if (errorMsg) *errorMsg = tr("Unmatched opening brace");
        return false;
    }

    return true;
}

bool ThemeManager::isDarkTheme() const
{
    return m_currentTheme.contains("dark", Qt::CaseInsensitive);
}

QColor ThemeManager::primaryColor() const
{
    if (isDarkTheme()) {
        return QColor(52, 152, 219);  // 蓝色
    }
    return QColor(41, 128, 185);  // 深蓝色
}

QColor ThemeManager::backgroundColor() const
{
    if (isDarkTheme()) {
        return QColor(45, 52, 54);  // 深灰
    }
    return QColor(255, 255, 255);  // 白色
}

QColor ThemeManager::textColor() const
{
    if (isDarkTheme()) {
        return QColor(236, 240, 241);  // 浅灰白
    }
    return QColor(44, 62, 80);  // 深蓝灰
}

void ThemeManager::scanBuiltinThemes()
{
    // 扫描内置主题（资源文件）
    QDir resourceDir(builtinThemesDir());

    // 内置主题列表
    QStringList builtinThemes = {"light", "dark"};

    for (const QString& name : builtinThemes) {
        QString filePath = builtinThemesDir() + "/" + name + ".qss";
        QFile file(filePath);

        if (file.exists()) {
            ThemeInfo info;
            info.name = name;
            info.displayName = (name == "light") ? tr("明亮") : tr("暗黑");
            info.filePath = filePath;
            info.isBuiltin = true;
            info.isValid = true;
            m_themes[name] = info;
        }
    }
}

void ThemeManager::scanUserThemes()
{
    // 扫描用户自定义主题
    QDir userDir(userThemesDir());
    if (!userDir.exists()) {
        return;
    }

    QStringList filters;
    filters << "*.qss";
    QFileInfoList files = userDir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo& fileInfo : files) {
        QString name = fileInfo.baseName();

        // 跳过与内置主题同名的
        if (m_themes.contains(name) && m_themes[name].isBuiltin) {
            continue;
        }

        ThemeInfo info;
        info.name = name;
        info.displayName = name;
        info.filePath = fileInfo.absoluteFilePath();
        info.isBuiltin = false;
        info.isValid = true;
        m_themes[name] = info;
    }
}

QString ThemeManager::loadQssFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    QString content = stream.readAll();
    file.close();

    return content;
}

} // namespace ComAssistant
