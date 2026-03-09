/**
 * @file DataWindowManager.h
 * @brief 数据分窗管理器
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef COMASSISTANT_DATAWINDOWMANAGER_H
#define COMASSISTANT_DATAWINDOWMANAGER_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QVector>
#include <QRegularExpression>

namespace ComAssistant {

class DataWindow;

/**
 * @brief 数据分窗规则
 */
struct WindowRule {
    QString windowName;      ///< 目标窗口名称
    QString pattern;         ///< 匹配模式（关键字或正则表达式）
    bool isRegex = false;    ///< 是否为正则表达式
    bool caseSensitive = false;  ///< 是否区分大小写
    bool enabled = true;     ///< 是否启用

    // 缓存的正则表达式对象
    mutable QRegularExpression regex;
    mutable bool regexCompiled = false;
};

/**
 * @brief 数据分窗管理器
 *
 * 根据配置的规则将接收到的数据路由到不同的窗口显示
 */
class DataWindowManager : public QObject
{
    Q_OBJECT

public:
    explicit DataWindowManager(QWidget* parent = nullptr);
    ~DataWindowManager() override;

    /**
     * @brief 添加数据窗口
     * @param name 窗口名称
     * @return 是否成功
     */
    bool addWindow(const QString& name);

    /**
     * @brief 移除数据窗口
     * @param name 窗口名称
     */
    void removeWindow(const QString& name);

    /**
     * @brief 获取数据窗口
     * @param name 窗口名称
     * @return 窗口指针，不存在返回nullptr
     */
    DataWindow* window(const QString& name) const;

    /**
     * @brief 获取所有窗口名称
     * @return 窗口名称列表
     */
    QStringList windowNames() const;

    /**
     * @brief 添加分窗规则
     * @param rule 规则
     */
    void addRule(const WindowRule& rule);

    /**
     * @brief 移除分窗规则
     * @param index 规则索引
     */
    void removeRule(int index);

    /**
     * @brief 清空所有规则
     */
    void clearRules();

    /**
     * @brief 获取所有规则
     * @return 规则列表
     */
    QVector<WindowRule> rules() const { return m_rules; }

    /**
     * @brief 设置规则列表
     * @param rules 规则列表
     */
    void setRules(const QVector<WindowRule>& rules);

    /**
     * @brief 是否有配置的规则
     * @return 是否有规则
     */
    bool hasRules() const { return !m_rules.isEmpty(); }

    /**
     * @brief 路由数据到对应窗口
     * @param data 数据文本
     * @return 是否匹配到任何规则
     */
    bool routeData(const QString& data);

    /**
     * @brief 显示所有数据窗口
     */
    void showAllWindows();

    /**
     * @brief 隐藏所有数据窗口
     */
    void hideAllWindows();

    /**
     * @brief 清空所有窗口数据
     */
    void clearAllData();

    /**
     * @brief 保存配置到JSON
     * @return JSON数据
     */
    QByteArray saveConfig() const;

    /**
     * @brief 从JSON加载配置
     * @param data JSON数据
     * @return 是否成功
     */
    bool loadConfig(const QByteArray& data);

signals:
    /**
     * @brief 数据被路由信号
     * @param windowName 目标窗口名称
     * @param data 数据内容
     */
    void dataRouted(const QString& windowName, const QString& data);

    /**
     * @brief 窗口添加信号
     * @param name 窗口名称
     */
    void windowAdded(const QString& name);

    /**
     * @brief 窗口移除信号
     * @param name 窗口名称
     */
    void windowRemoved(const QString& name);

private:
    bool matchRule(const WindowRule& rule, const QString& data);
    void compileRegex(const WindowRule& rule) const;

    QWidget* m_parentWidget;
    QMap<QString, DataWindow*> m_windows;
    QVector<WindowRule> m_rules;
};

} // namespace ComAssistant

#endif // COMASSISTANT_DATAWINDOWMANAGER_H
