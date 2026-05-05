/**
 * @file PlotterManager.cpp
 * @brief 绘图窗口管理器实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "PlotterManager.h"
#include "PlotterWindow.h"
#include "core/utils/Logger.h"

namespace ComAssistant {

// 单例实例初始化
PlotterManager* PlotterManager::s_instance = nullptr;

PlotterManager* PlotterManager::instance()
{
    if (!s_instance) {
        s_instance = new PlotterManager();
    }
    return s_instance;
}

PlotterManager::PlotterManager(QObject* parent)
    : QObject(parent)
{
    LOG_INFO("PlotterManager initialized");
}

PlotterManager::~PlotterManager()
{
    closeAllWindows();
    LOG_INFO("PlotterManager destroyed");
}

PlotterWindow* PlotterManager::createWindow(const QString& windowId, const QString& title)
{
    // 如果窗口已存在，直接返回（不抢占焦点）
    if (m_windows.contains(windowId)) {
        PlotterWindow* window = m_windows[windowId];
        // 只确保窗口可见，不激活它（避免抢占焦点）
        if (!window->isVisible()) {
            window->show();
        }
        return window;
    }

    // 创建新窗口
    PlotterWindow* window = new PlotterWindow(windowId);
    window->setAttribute(Qt::WA_DeleteOnClose, true);

    // 设置标题
    if (!title.isEmpty()) {
        window->setWindowTitle(title);
    }

    // 连接关闭信号
    connect(window, &PlotterWindow::windowClosed,
            this, &PlotterManager::onWindowClosed);

    // 保存到映射表
    m_windows[windowId] = window;

    // 显示窗口（只在首次创建时激活）
    window->show();

    LOG_INFO(QString("Created plot window: %1").arg(windowId));
    emit windowCreated(windowId);

    return window;
}

void PlotterManager::closeWindow(const QString& windowId)
{
    if (m_windows.contains(windowId)) {
        PlotterWindow* window = m_windows.take(windowId);
        window->close();
        LOG_INFO(QString("Closed plot window: %1").arg(windowId));
        emit windowClosed(windowId);
    }
}

void PlotterManager::closeAllWindows()
{
    QStringList ids = m_windows.keys();
    for (const QString& id : ids) {
        closeWindow(id);
    }
    LOG_INFO("Closed all plot windows");
}

PlotterWindow* PlotterManager::getWindow(const QString& windowId)
{
    return m_windows.value(windowId, nullptr);
}

bool PlotterManager::hasWindow(const QString& windowId) const
{
    return m_windows.contains(windowId);
}

QStringList PlotterManager::windowIds() const
{
    return m_windows.keys();
}

void PlotterManager::routeData(const QString& windowId, const QVector<double>& values)
{
    // 自动创建窗口（如果不存在）
    PlotterWindow* window = createWindow(windowId);

    if (window && !values.isEmpty()) {
        window->appendMultiData(values);
    }
}

void PlotterManager::routeData(const QString& windowId, double x, const QVector<double>& values)
{
    // 自动创建窗口（如果不存在）
    PlotterWindow* window = createWindow(windowId);

    if (window && !values.isEmpty()) {
        window->appendMultiData(x, values);
    }
}

void PlotterManager::onWindowClosed(const QString& windowId)
{
    if (m_windows.contains(windowId)) {
        m_windows.remove(windowId);
        LOG_INFO(QString("Plot window closed by user: %1").arg(windowId));
        emit windowClosed(windowId);
    }
}

void PlotterManager::setSyncEnabled(bool enabled)
{
    if (m_syncEnabled != enabled) {
        m_syncEnabled = enabled;
        LOG_INFO(QString("Window sync mode: %1").arg(enabled ? "enabled" : "disabled"));
        emit syncEnabledChanged(enabled);
    }
}

void PlotterManager::syncXAxisRange(const QString& sourceWindow, double minX, double maxX)
{
    // 防止递归同步
    if (!m_syncEnabled || m_syncing) {
        return;
    }

    m_syncing = true;

    // 同步所有其他窗口的X轴范围
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.key() != sourceWindow && it.value()) {
            it.value()->setXAxisRange(minX, maxX);
        }
    }

    m_syncing = false;
}

void PlotterManager::syncPausedState(const QString& sourceWindow, bool paused)
{
    // 防止递归同步
    if (!m_syncEnabled || m_syncing) {
        return;
    }

    m_syncing = true;

    // 同步所有其他窗口的暂停状态
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.key() != sourceWindow && it.value()) {
            it.value()->setPaused(paused);
        }
    }

    m_syncing = false;

    LOG_DEBUG(QString("Synced pause state to all windows: %1").arg(paused ? "paused" : "running"));
}

void PlotterManager::syncClearAll()
{
    // 防止递归同步
    if (m_syncing) {
        return;
    }

    m_syncing = true;

    // 清空所有窗口数据
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            it.value()->clearAll();
        }
    }

    m_syncing = false;

    LOG_INFO("Cleared all window data (sync)");
}

} // namespace ComAssistant
