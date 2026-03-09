/**
 * @file AutoSaveManager.cpp
 * @brief 自动保存管理器实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "AutoSaveManager.h"
#include "Logger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QCoreApplication>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ComAssistant {

AutoSaveManager* AutoSaveManager::instance()
{
    static AutoSaveManager instance;
    return &instance;
}

AutoSaveManager::AutoSaveManager()
    : QObject(nullptr)
{
    m_autoSaveTimer = new QTimer(this);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &AutoSaveManager::onAutoSaveTimer);
}

AutoSaveManager::~AutoSaveManager()
{
    if (m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->stop();
    }
    removeLockFile();
}

void AutoSaveManager::initialize(const QString& dataPath)
{
    m_dataPath = dataPath;
    m_autoSaveFile = dataPath + "/autosave.cas";
    m_lockFile = dataPath + "/session.lock";

    // 确保目录存在
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 检查是否有恢复数据（通过锁文件判断上次是否正常退出）
    if (isLockFileValid()) {
        // 锁文件存在意味着上次没有正常退出
        if (QFile::exists(m_autoSaveFile)) {
            QDateTime time = recoveryDataTime();
            LOG_INFO(QString("Recovery data found from: %1").arg(time.toString()));
            emit recoveryDataFound(time);
        }
    }

    // 创建新的锁文件
    createLockFile();

    // 启动自动保存定时器
    if (m_autoSaveEnabled) {
        m_autoSaveTimer->start(m_autoSaveInterval * 1000);
    }

    LOG_INFO(QString("AutoSaveManager initialized. Interval: %1s, Enabled: %2")
        .arg(m_autoSaveInterval)
        .arg(m_autoSaveEnabled ? "true" : "false"));
}

void AutoSaveManager::setAutoSaveEnabled(bool enabled)
{
    m_autoSaveEnabled = enabled;

    if (enabled && !m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->start(m_autoSaveInterval * 1000);
        LOG_INFO("Auto save enabled");
    } else if (!enabled && m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->stop();
        LOG_INFO("Auto save disabled");
    }
}

void AutoSaveManager::setAutoSaveInterval(int seconds)
{
    if (seconds < 5) {
        seconds = 5;  // 最小5秒
    }
    if (seconds > 3600) {
        seconds = 3600;  // 最大1小时
    }

    m_autoSaveInterval = seconds;

    if (m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->setInterval(seconds * 1000);
    }

    LOG_INFO(QString("Auto save interval set to: %1s").arg(seconds));
}

void AutoSaveManager::setSaveCallback(SaveCallback callback)
{
    m_saveCallback = std::move(callback);
}

void AutoSaveManager::setLoadCallback(LoadCallback callback)
{
    m_loadCallback = std::move(callback);
}

bool AutoSaveManager::hasRecoveryData() const
{
    return QFile::exists(m_autoSaveFile) && isLockFileValid();
}

QDateTime AutoSaveManager::recoveryDataTime() const
{
    if (!QFile::exists(m_autoSaveFile)) {
        return QDateTime();
    }

    QFileInfo info(m_autoSaveFile);
    return info.lastModified();
}

QString AutoSaveManager::recoveryFilePath() const
{
    return m_autoSaveFile;
}

bool AutoSaveManager::performRecovery()
{
    if (!m_loadCallback) {
        LOG_ERROR("No load callback registered");
        return false;
    }

    if (!QFile::exists(m_autoSaveFile)) {
        LOG_ERROR("Recovery file does not exist");
        return false;
    }

    LOG_INFO(QString("Performing recovery from: %1").arg(m_autoSaveFile));

    bool result = m_loadCallback(m_autoSaveFile);

    if (result) {
        LOG_INFO("Recovery successful");
        // 恢复成功后清除恢复数据
        clearRecoveryData();
    } else {
        LOG_ERROR("Recovery failed");
    }

    return result;
}

void AutoSaveManager::clearRecoveryData()
{
    if (QFile::exists(m_autoSaveFile)) {
        QFile::remove(m_autoSaveFile);
        LOG_INFO("Recovery data cleared");
    }

    // 重新创建锁文件（标记当前会话）
    createLockFile();
}

void AutoSaveManager::saveNow()
{
    if (!m_saveCallback) {
        LOG_WARN("No save callback registered");
        return;
    }

    if (m_saveCallback(m_autoSaveFile)) {
        LOG_DEBUG(QString("Auto saved to: %1").arg(m_autoSaveFile));
        emit autoSaved(m_autoSaveFile);
    } else {
        LOG_ERROR("Auto save failed");
        emit autoSaveFailed("Save callback returned false");
    }
}

void AutoSaveManager::markCleanExit()
{
    // 停止定时器
    if (m_autoSaveTimer->isActive()) {
        m_autoSaveTimer->stop();
    }

    // 删除自动保存文件（正常退出不需要恢复）
    if (QFile::exists(m_autoSaveFile)) {
        QFile::remove(m_autoSaveFile);
    }

    // 删除锁文件
    removeLockFile();

    LOG_INFO("Clean exit marked");
}

void AutoSaveManager::onAutoSaveTimer()
{
    if (m_saveCallback) {
        saveNow();
    }
}

void AutoSaveManager::createLockFile()
{
    QFile file(m_lockFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        stream << QCoreApplication::applicationPid() << "\n";
        file.close();
    }
}

void AutoSaveManager::removeLockFile()
{
    if (QFile::exists(m_lockFile)) {
        QFile::remove(m_lockFile);
    }
}

bool AutoSaveManager::isLockFileValid() const
{
    if (!QFile::exists(m_lockFile)) {
        return false;
    }

    // 读取锁文件内容
    QFile file(m_lockFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    QString dateStr = stream.readLine();
    QString pidStr = stream.readLine();
    file.close();

    // 检查时间是否在合理范围内（7天以内）
    QDateTime lockTime = QDateTime::fromString(dateStr, Qt::ISODate);
    if (!lockTime.isValid()) {
        return false;
    }

    qint64 daysAgo = lockTime.daysTo(QDateTime::currentDateTime());
    if (daysAgo > 7) {
        // 锁文件太旧，可能是意外遗留
        return false;
    }

    // 检查进程是否还在运行（Windows）
#ifdef _WIN32
    bool ok;
    DWORD pid = pidStr.toULong(&ok);
    if (ok && pid != 0) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess != nullptr) {
            // 进程还在运行，不是崩溃
            CloseHandle(hProcess);
            return false;
        }
    }
#endif

    return true;
}

} // namespace ComAssistant
