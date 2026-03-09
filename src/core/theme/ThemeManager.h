/**
 * @file ThemeManager.h
 * @brief 主题管理器
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_THEMEMANAGER_H
#define COMASSISTANT_THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QColor>

namespace ComAssistant {

/**
 * @brief 主题信息结构
 */
struct ThemeInfo {
    QString name;           ///< 主题名称
    QString displayName;    ///< 显示名称
    QString filePath;       ///< 文件路径
    QString description;    ///< 描述
    bool isBuiltin = false; ///< 是否内置主题
    bool isValid = true;    ///< 是否有效
};

/**
 * @brief 主题管理器
 * 提供主题加载、切换、自定义等功能
 */
class ThemeManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ThemeManager* instance();

    /**
     * @brief 初始化主题管理器
     */
    bool initialize();

    /**
     * @brief 获取可用主题列表
     */
    QStringList availableThemes() const;

    /**
     * @brief 获取主题信息
     */
    ThemeInfo themeInfo(const QString& themeName) const;

    /**
     * @brief 加载主题
     * @param themeName 主题名称
     * @return 是否成功
     */
    bool loadTheme(const QString& themeName);

    /**
     * @brief 获取当前主题名称
     */
    QString currentTheme() const { return m_currentTheme; }

    /**
     * @brief 获取当前主题样式表
     */
    QString currentStyleSheet() const { return m_currentStyleSheet; }

    /**
     * @brief 保存自定义主题
     * @param themeName 主题名称
     * @param qssContent QSS样式表内容
     * @return 是否成功
     */
    bool saveTheme(const QString& themeName, const QString& qssContent);

    /**
     * @brief 删除自定义主题
     * @param themeName 主题名称
     * @return 是否成功
     */
    bool deleteTheme(const QString& themeName);

    /**
     * @brief 导入主题
     * @param filePath QSS文件路径
     * @return 导入的主题名称，失败返回空
     */
    QString importTheme(const QString& filePath);

    /**
     * @brief 导出主题
     * @param themeName 主题名称
     * @param filePath 导出路径
     * @return 是否成功
     */
    bool exportTheme(const QString& themeName, const QString& filePath);

    /**
     * @brief 获取主题目录
     */
    QString builtinThemesDir() const;

    /**
     * @brief 获取用户主题目录
     */
    QString userThemesDir() const;

    /**
     * @brief 重新加载主题列表
     */
    void refreshThemeList();

    /**
     * @brief 读取主题内容
     */
    QString readThemeContent(const QString& themeName) const;

    /**
     * @brief 验证QSS语法
     */
    bool validateQss(const QString& qssContent, QString* errorMsg = nullptr) const;

    /**
     * @brief 获取当前主题是否为深色
     */
    bool isDarkTheme() const;

    // ==================== 主题颜色获取 ====================

    /**
     * @brief 获取主题主色
     */
    QColor primaryColor() const;

    /**
     * @brief 获取主题背景色
     */
    QColor backgroundColor() const;

    /**
     * @brief 获取主题文本色
     */
    QColor textColor() const;

signals:
    /**
     * @brief 主题改变信号
     */
    void themeChanged(const QString& themeName);

    /**
     * @brief 主题列表改变信号
     */
    void themeListChanged();

private:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() override = default;

    // 禁止复制
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    // 扫描主题目录
    void scanBuiltinThemes();
    void scanUserThemes();

    // 加载QSS文件
    QString loadQssFile(const QString& filePath) const;

private:
    static ThemeManager* s_instance;

    QString m_currentTheme;
    QString m_currentStyleSheet;
    QMap<QString, ThemeInfo> m_themes;

    bool m_initialized = false;
};

} // namespace ComAssistant

#endif // COMASSISTANT_THEMEMANAGER_H
