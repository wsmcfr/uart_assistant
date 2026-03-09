/**
 * @file AutoSaveManager.h
 * @brief 自动保存管理器 - 定期保存会话数据，支持崩溃恢复
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_AUTOSAVEMANAGER_H
#define COMASSISTANT_AUTOSAVEMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QDateTime>
#include <functional>

namespace ComAssistant {

// 前向声明
struct SessionData;

/**
 * @brief 自动保存管理器 - 单例模式
 */
class AutoSaveManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 自动保存回调类型
     */
    using SaveCallback = std::function<bool(const QString& path)>;

    /**
     * @brief 恢复回调类型
     */
    using LoadCallback = std::function<bool(const QString& path)>;

    /**
     * @brief 获取单例实例
     */
    static AutoSaveManager* instance();

    /**
     * @brief 初始化自动保存管理器
     * @param dataPath 数据存储目录
     */
    void initialize(const QString& dataPath);

    /**
     * @brief 设置自动保存是否启用
     */
    void setAutoSaveEnabled(bool enabled);

    /**
     * @brief 获取自动保存是否启用
     */
    bool isAutoSaveEnabled() const { return m_autoSaveEnabled; }

    /**
     * @brief 设置自动保存间隔（秒）
     * @param seconds 间隔秒数，默认30秒
     */
    void setAutoSaveInterval(int seconds);

    /**
     * @brief 获取自动保存间隔
     */
    int autoSaveInterval() const { return m_autoSaveInterval; }

    /**
     * @brief 注册保存回调
     */
    void setSaveCallback(SaveCallback callback);

    /**
     * @brief 注册加载回调
     */
    void setLoadCallback(LoadCallback callback);

    /**
     * @brief 检查是否有恢复数据
     */
    bool hasRecoveryData() const;

    /**
     * @brief 获取恢复数据的时间
     */
    QDateTime recoveryDataTime() const;

    /**
     * @brief 获取恢复数据文件路径
     */
    QString recoveryFilePath() const;

    /**
     * @brief 执行恢复
     * @return 是否成功恢复
     */
    bool performRecovery();

    /**
     * @brief 清除恢复数据
     */
    void clearRecoveryData();

    /**
     * @brief 立即执行保存
     */
    void saveNow();

    /**
     * @brief 标记会话已正常结束（清除恢复标记）
     */
    void markCleanExit();

signals:
    /**
     * @brief 自动保存完成
     */
    void autoSaved(const QString& path);

    /**
     * @brief 自动保存失败
     */
    void autoSaveFailed(const QString& error);

    /**
     * @brief 发现恢复数据
     */
    void recoveryDataFound(const QDateTime& time);

private slots:
    void onAutoSaveTimer();

private:
    AutoSaveManager();
    ~AutoSaveManager() override;

    QString m_dataPath;
    QString m_autoSaveFile;
    QString m_lockFile;

    bool m_autoSaveEnabled = true;
    int m_autoSaveInterval = 30;  // 秒

    QTimer* m_autoSaveTimer = nullptr;

    SaveCallback m_saveCallback;
    LoadCallback m_loadCallback;

    void createLockFile();
    void removeLockFile();
    bool isLockFileValid() const;
};

} // namespace ComAssistant

#endif // COMASSISTANT_AUTOSAVEMANAGER_H
