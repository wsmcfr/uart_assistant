/**
 * @file PlotterManager.h
 * @brief 绘图窗口管理器
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef PLOTTERMANAGER_H
#define PLOTTERMANAGER_H

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>

namespace ComAssistant {

class PlotterWindow;

/**
 * @brief 绘图窗口管理器类（单例）
 *
 * 管理多个绘图窗口，根据窗口ID创建、查找和关闭窗口。
 */
class PlotterManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return PlotterManager指针
     */
    static PlotterManager* instance();

    /**
     * @brief 创建或获取绘图窗口
     * @param windowId 窗口ID
     * @param title 窗口标题（可选）
     * @return 绘图窗口指针
     */
    PlotterWindow* createWindow(const QString& windowId, const QString& title = QString());

    /**
     * @brief 关闭指定窗口
     * @param windowId 窗口ID
     */
    void closeWindow(const QString& windowId);

    /**
     * @brief 关闭所有窗口
     */
    void closeAllWindows();

    /**
     * @brief 获取指定窗口
     * @param windowId 窗口ID
     * @return 窗口指针，不存在则返回nullptr
     */
    PlotterWindow* getWindow(const QString& windowId);

    /**
     * @brief 检查窗口是否存在
     * @param windowId 窗口ID
     * @return 是否存在
     */
    bool hasWindow(const QString& windowId) const;

    /**
     * @brief 获取所有窗口ID列表
     * @return 窗口ID列表
     */
    QStringList windowIds() const;

    /**
     * @brief 获取窗口数量
     * @return 窗口数量
     */
    int windowCount() const { return m_windows.count(); }

    /**
     * @brief 设置窗口同步模式
     * @param enabled 是否启用同步
     */
    void setSyncEnabled(bool enabled);

    /**
     * @brief 获取同步模式状态
     * @return 是否启用同步
     */
    bool isSyncEnabled() const { return m_syncEnabled; }

    /**
     * @brief 同步所有窗口的X轴范围
     * @param sourceWindow 源窗口ID（不会被更新）
     * @param minX X轴最小值
     * @param maxX X轴最大值
     */
    void syncXAxisRange(const QString& sourceWindow, double minX, double maxX);

    /**
     * @brief 同步所有窗口的暂停状态
     * @param sourceWindow 源窗口ID
     * @param paused 暂停状态
     */
    void syncPausedState(const QString& sourceWindow, bool paused);

    /**
     * @brief 同步所有窗口清空数据
     */
    void syncClearAll();

    /**
     * @brief 路由绘图数据到指定窗口
     * @param windowId 窗口ID
     * @param values Y值列表（多曲线）
     */
    void routeData(const QString& windowId, const QVector<double>& values);

    /**
     * @brief 路由带X值的绘图数据到指定窗口
     * @param windowId 窗口ID
     * @param x X轴值
     * @param values Y值列表（多曲线）
     */
    void routeData(const QString& windowId, double x, const QVector<double>& values);

signals:
    /**
     * @brief 窗口创建信号
     * @param windowId 窗口ID
     */
    void windowCreated(const QString& windowId);

    /**
     * @brief 窗口关闭信号
     * @param windowId 窗口ID
     */
    void windowClosed(const QString& windowId);

    /**
     * @brief 同步模式变化信号
     * @param enabled 是否启用同步
     */
    void syncEnabledChanged(bool enabled);

private slots:
    /**
     * @brief 窗口关闭处理
     * @param windowId 窗口ID
     */
    void onWindowClosed(const QString& windowId);

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    explicit PlotterManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~PlotterManager() override;

    // 禁止拷贝
    PlotterManager(const PlotterManager&) = delete;
    PlotterManager& operator=(const PlotterManager&) = delete;

private:
    QMap<QString, PlotterWindow*> m_windows;  ///< 窗口映射表
    static PlotterManager* s_instance;        ///< 单例实例
    bool m_syncEnabled = false;               ///< 同步模式
    bool m_syncing = false;                   ///< 防止递归同步
};

} // namespace ComAssistant

#endif // PLOTTERMANAGER_H
