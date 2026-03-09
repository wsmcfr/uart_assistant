/**
 * @file SessionManager.h
 * @brief 会话管理器
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_SESSIONMANAGER_H
#define COMASSISTANT_SESSIONMANAGER_H

#include "SessionData.h"
#include <QObject>
#include <QTimer>

namespace ComAssistant {

/**
 * @brief 会话管理器（单例）
 * 负责会话的保存、加载、自动保存和恢复
 */
class SessionManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static SessionManager* instance();

    /**
     * @brief 创建新会话
     */
    void newSession();

    /**
     * @brief 保存当前会话
     * @param filePath 文件路径（为空则使用当前路径或提示保存）
     * @return 是否成功
     */
    bool saveSession(const QString& filePath = QString());

    /**
     * @brief 加载会话
     * @param filePath 文件路径
     * @return 是否成功
     */
    bool loadSession(const QString& filePath);

    /**
     * @brief 获取当前会话数据
     */
    SessionData& currentSession() { return m_currentSession; }
    const SessionData& currentSession() const { return m_currentSession; }

    /**
     * @brief 获取当前会话文件路径
     */
    QString currentFilePath() const { return m_currentFilePath; }

    /**
     * @brief 会话是否被修改
     */
    bool isModified() const { return m_modified; }

    /**
     * @brief 标记会话已修改
     */
    void setModified(bool modified = true);

    // 自动保存功能
    void setAutoSaveEnabled(bool enabled);
    bool isAutoSaveEnabled() const { return m_autoSaveEnabled; }

    void setAutoSaveInterval(int seconds);
    int autoSaveInterval() const { return m_autoSaveInterval; }

    // 恢复功能
    bool hasRecoveryData() const;
    QDateTime lastRecoveryTime() const;
    bool recoverSession();
    void clearRecoveryData();

    /**
     * @brief 获取最近打开的会话列表
     */
    QStringList recentSessions() const;

    /**
     * @brief 添加到最近会话列表
     */
    void addToRecentSessions(const QString& filePath);

    /**
     * @brief 清空最近会话列表
     */
    void clearRecentSessions();

signals:
    void sessionCreated();
    void sessionLoaded(const QString& filePath);
    void sessionSaved(const QString& filePath);
    void sessionModified();
    void autoSaveTriggered();
    void recoveryDataFound();

private slots:
    void performAutoSave();

private:
    SessionManager(QObject* parent = nullptr);
    ~SessionManager() override;

    QString autoSavePath() const;
    void loadRecentSessions();
    void saveRecentSessions();

private:
    static SessionManager* s_instance;

    SessionData m_currentSession;
    QString m_currentFilePath;
    bool m_modified = false;

    // 自动保存
    bool m_autoSaveEnabled = true;
    int m_autoSaveInterval = 30;  // 秒
    QTimer* m_autoSaveTimer = nullptr;

    // 最近会话
    QStringList m_recentSessions;
    static constexpr int MAX_RECENT_SESSIONS = 10;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SESSIONMANAGER_H
