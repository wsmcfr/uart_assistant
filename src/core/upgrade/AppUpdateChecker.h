/**
 * @file AppUpdateChecker.h
 * @brief 应用更新检查器（GitHub Releases）
 */

#ifndef COMASSISTANT_APPUPDATECHECKER_H
#define COMASSISTANT_APPUPDATECHECKER_H

#include <QObject>
#include <QUrl>
#include <QHash>
#include <QJsonObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace ComAssistant {

/**
 * @brief 发布版本信息
 */
struct ReleaseInfo {
    QString versionTag;       ///< 原始标签（例如 v1.2.3）
    QString versionNumber;    ///< 规范化版本（例如 1.2.3）
    QString releaseName;      ///< 发布标题
    QString body;             ///< 发布说明
    QUrl htmlUrl;             ///< 发布页面链接
    QUrl downloadUrl;         ///< 推荐下载链接（优先资产）
    bool prerelease = false;  ///< 是否预发布
    bool draft = false;       ///< 是否草稿
};

/**
 * @brief 应用更新检查器
 */
class AppUpdateChecker : public QObject {
    Q_OBJECT

public:
    explicit AppUpdateChecker(QObject* parent = nullptr);

    /**
     * @brief 设置仓库信息
     */
    void setRepository(const QString& owner, const QString& repo);

    /**
     * @brief 设置当前应用版本
     */
    void setCurrentVersion(const QString& version);

    /**
     * @brief 检查更新
     * @param manualTriggered true 表示用户手动触发，false 表示后台自动检查
     */
    void checkForUpdates(bool manualTriggered);

    /**
     * @brief 比较两个版本号
     * @return >0: lhs 更新；0: 相等；<0: lhs 更旧
     */
    static int compareVersions(const QString& lhs, const QString& rhs);

signals:
    void updateAvailable(const ComAssistant::ReleaseInfo& info, bool manualTriggered);
    void alreadyLatest(const ComAssistant::ReleaseInfo& info, bool manualTriggered);
    void checkFailed(const QString& error, bool manualTriggered);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QUrl makeLatestReleaseApiUrl() const;
    static QString normalizeVersion(const QString& rawVersion);
    static QUrl selectDownloadUrl(const QJsonObject& releaseObject);
    static bool parseReleaseInfo(const QJsonObject& releaseObject, ReleaseInfo& info);

private:
    QNetworkAccessManager* m_networkManager = nullptr;
    QHash<QNetworkReply*, bool> m_requestManualMap;
    QString m_owner;
    QString m_repo;
    QString m_currentVersion;
};

} // namespace ComAssistant

Q_DECLARE_METATYPE(ComAssistant::ReleaseInfo)

#endif // COMASSISTANT_APPUPDATECHECKER_H
