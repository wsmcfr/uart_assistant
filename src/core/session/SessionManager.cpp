/**
 * @file SessionManager.cpp
 * @brief 会话管理器实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "SessionManager.h"
#include "SessionSerializer.h"
#include "core/utils/Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>

namespace ComAssistant {

SessionManager* SessionManager::s_instance = nullptr;

SessionManager* SessionManager::instance()
{
    if (!s_instance) {
        s_instance = new SessionManager();
    }
    return s_instance;
}

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &SessionManager::performAutoSave);

    loadRecentSessions();

    // 启动自动保存
    if (m_autoSaveEnabled) {
        m_autoSaveTimer->start(m_autoSaveInterval * 1000);
    }

    // 初始化新会话
    newSession();

    LOG_INFO("SessionManager initialized");
}

SessionManager::~SessionManager()
{
    saveRecentSessions();
    LOG_INFO("SessionManager destroyed");
}

void SessionManager::newSession()
{
    m_currentSession = SessionData();
    m_currentSession.createTime = QDateTime::currentDateTime();
    m_currentSession.lastModifyTime = m_currentSession.createTime;
    m_currentSession.name = tr("新会话");

    m_currentFilePath.clear();
    m_modified = false;

    emit sessionCreated();
    LOG_INFO("New session created");
}

bool SessionManager::saveSession(const QString& filePath)
{
    QString targetPath = filePath;

    if (targetPath.isEmpty()) {
        targetPath = m_currentFilePath;
    }

    if (targetPath.isEmpty()) {
        // 需要外部提示选择路径
        return false;
    }

    // 更新修改时间
    m_currentSession.lastModifyTime = QDateTime::currentDateTime();

    if (SessionSerializer::saveToFile(m_currentSession, targetPath)) {
        m_currentFilePath = targetPath;
        m_modified = false;
        addToRecentSessions(targetPath);
        emit sessionSaved(targetPath);
        LOG_INFO(QString("Session saved: %1").arg(targetPath));
        return true;
    } else {
        LOG_ERROR(QString("Failed to save session: %1").arg(SessionSerializer::lastError()));
        return false;
    }
}

bool SessionManager::loadSession(const QString& filePath)
{
    SessionData data;
    if (SessionSerializer::loadFromFile(filePath, data)) {
        m_currentSession = data;
        m_currentFilePath = filePath;
        m_modified = false;
        addToRecentSessions(filePath);
        emit sessionLoaded(filePath);
        LOG_INFO(QString("Session loaded: %1").arg(filePath));
        return true;
    } else {
        LOG_ERROR(QString("Failed to load session: %1").arg(SessionSerializer::lastError()));
        return false;
    }
}

void SessionManager::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        if (modified) {
            emit sessionModified();
        }
    }
}

void SessionManager::setAutoSaveEnabled(bool enabled)
{
    m_autoSaveEnabled = enabled;
    if (enabled) {
        m_autoSaveTimer->start(m_autoSaveInterval * 1000);
    } else {
        m_autoSaveTimer->stop();
    }
}

void SessionManager::setAutoSaveInterval(int seconds)
{
    m_autoSaveInterval = seconds;
    if (m_autoSaveEnabled && m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->setInterval(seconds * 1000);
    }
}

bool SessionManager::hasRecoveryData() const
{
    QString path = autoSavePath();
    return QFile::exists(path);
}

QDateTime SessionManager::lastRecoveryTime() const
{
    QString path = autoSavePath();
    QFileInfo info(path);
    if (info.exists()) {
        return info.lastModified();
    }
    return QDateTime();
}

bool SessionManager::recoverSession()
{
    QString path = autoSavePath();
    if (!QFile::exists(path)) {
        return false;
    }

    SessionData data;
    if (SessionSerializer::loadFromFile(path, data)) {
        m_currentSession = data;
        m_currentFilePath.clear();  // 恢复的会话没有固定路径
        m_modified = true;  // 标记为已修改，提示用户保存
        emit sessionLoaded(path);
        LOG_INFO("Session recovered from auto-save");
        return true;
    }
    return false;
}

void SessionManager::clearRecoveryData()
{
    QString path = autoSavePath();
    if (QFile::exists(path)) {
        QFile::remove(path);
        LOG_INFO("Recovery data cleared");
    }
}

QStringList SessionManager::recentSessions() const
{
    return m_recentSessions;
}

void SessionManager::addToRecentSessions(const QString& filePath)
{
    // 移除已存在的相同路径
    m_recentSessions.removeAll(filePath);

    // 添加到开头
    m_recentSessions.prepend(filePath);

    // 限制数量
    while (m_recentSessions.size() > MAX_RECENT_SESSIONS) {
        m_recentSessions.removeLast();
    }

    saveRecentSessions();
}

void SessionManager::clearRecentSessions()
{
    m_recentSessions.clear();
    saveRecentSessions();
}

void SessionManager::performAutoSave()
{
    if (!m_modified) {
        return;  // 没有修改，不需要保存
    }

    QString path = autoSavePath();

    // 确保目录存在
    QFileInfo info(path);
    QDir dir = info.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 更新时间
    m_currentSession.lastModifyTime = QDateTime::currentDateTime();

    if (SessionSerializer::saveToFile(m_currentSession, path)) {
        emit autoSaveTriggered();
        LOG_DEBUG("Auto-save completed");
    } else {
        LOG_WARN(QString("Auto-save failed: %1").arg(SessionSerializer::lastError()));
    }
}

QString SessionManager::autoSavePath() const
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData + "/recovery/autosave.cas";
}

void SessionManager::loadRecentSessions()
{
    QSettings settings;
    int count = settings.beginReadArray("RecentSessions");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        QString path = settings.value("path").toString();
        if (QFile::exists(path)) {
            m_recentSessions.append(path);
        }
    }
    settings.endArray();
}

void SessionManager::saveRecentSessions()
{
    QSettings settings;
    settings.beginWriteArray("RecentSessions");
    for (int i = 0; i < m_recentSessions.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("path", m_recentSessions[i]);
    }
    settings.endArray();
}

} // namespace ComAssistant
