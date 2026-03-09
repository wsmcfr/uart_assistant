/**
 * @file AppUpdateChecker.cpp
 * @brief 应用更新检查器（GitHub Releases）
 */

#include "AppUpdateChecker.h"
#include "version.h"
#include "utils/Logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QVector>

namespace ComAssistant {

AppUpdateChecker::AppUpdateChecker(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_owner(APP_REPOSITORY_OWNER)
    , m_repo(APP_REPOSITORY_NAME)
    , m_currentVersion(APP_VERSION)
{
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AppUpdateChecker::onReplyFinished);
}

void AppUpdateChecker::setRepository(const QString& owner, const QString& repo)
{
    m_owner = owner.trimmed();
    m_repo = repo.trimmed();
}

void AppUpdateChecker::setCurrentVersion(const QString& version)
{
    m_currentVersion = normalizeVersion(version);
}

void AppUpdateChecker::checkForUpdates(bool manualTriggered)
{
    const QUrl apiUrl = makeLatestReleaseApiUrl();
    if (!apiUrl.isValid()) {
        emit checkFailed(tr("更新源配置无效"), manualTriggered);
        return;
    }

    QNetworkRequest request(apiUrl);
    request.setRawHeader("User-Agent", QByteArray(APP_NAME) + "/" + QByteArray(APP_VERSION));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    QNetworkReply* reply = m_networkManager->get(request);
    m_requestManualMap.insert(reply, manualTriggered);
}

void AppUpdateChecker::onReplyFinished(QNetworkReply* reply)
{
    const bool manualTriggered = m_requestManualMap.take(reply);

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = tr("检查更新失败: %1").arg(reply->errorString());
        emit checkFailed(error, manualTriggered);
        reply->deleteLater();
        return;
    }

    const QByteArray payload = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        emit checkFailed(tr("更新响应格式错误"), manualTriggered);
        reply->deleteLater();
        return;
    }

    ReleaseInfo releaseInfo;
    if (!parseReleaseInfo(doc.object(), releaseInfo)) {
        emit checkFailed(tr("无法解析发布版本信息"), manualTriggered);
        reply->deleteLater();
        return;
    }

    const int compareResult = compareVersions(releaseInfo.versionNumber, m_currentVersion);
    if (compareResult > 0) {
        emit updateAvailable(releaseInfo, manualTriggered);
    } else {
        emit alreadyLatest(releaseInfo, manualTriggered);
    }

    reply->deleteLater();
}

QUrl AppUpdateChecker::makeLatestReleaseApiUrl() const
{
    if (m_owner.isEmpty() || m_repo.isEmpty()) {
        return {};
    }
    return QUrl(QString("https://api.github.com/repos/%1/%2/releases/latest").arg(m_owner, m_repo));
}

QString AppUpdateChecker::normalizeVersion(const QString& rawVersion)
{
    QString version = rawVersion.trimmed();
    if (version.startsWith('v', Qt::CaseInsensitive)) {
        version.remove(0, 1);
    }

    const int plusPos = version.indexOf('+');
    if (plusPos > 0) {
        version = version.left(plusPos);
    }

    return version.trimmed();
}

QUrl AppUpdateChecker::selectDownloadUrl(const QJsonObject& releaseObject)
{
    const QJsonArray assets = releaseObject.value("assets").toArray();
    QUrl bestUrl;
    int bestScore = -1;

    for (const QJsonValue& value : assets) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject asset = value.toObject();
        const QString name = asset.value("name").toString();
        const QUrl url(asset.value("browser_download_url").toString());
        if (!url.isValid() || name.isEmpty()) {
            continue;
        }

        int score = 0;
        const QString lowerName = name.toLower();
        if (lowerName.endsWith(".zip") || lowerName.endsWith(".7z") || lowerName.endsWith(".exe")) {
            score += 100;
        }
        if (lowerName.contains("portable")) {
            score += 30;
        }
        if (lowerName.contains("windows") || lowerName.contains("win")) {
            score += 20;
        }
        if (lowerName.contains("x64") || lowerName.contains("amd64")) {
            score += 10;
        }

        if (score > bestScore) {
            bestScore = score;
            bestUrl = url;
        }
    }

    if (bestUrl.isValid()) {
        return bestUrl;
    }
    return QUrl(releaseObject.value("html_url").toString());
}

bool AppUpdateChecker::parseReleaseInfo(const QJsonObject& releaseObject, ReleaseInfo& info)
{
    info.versionTag = releaseObject.value("tag_name").toString().trimmed();
    info.versionNumber = normalizeVersion(info.versionTag);
    info.releaseName = releaseObject.value("name").toString().trimmed();
    info.body = releaseObject.value("body").toString();
    info.htmlUrl = QUrl(releaseObject.value("html_url").toString());
    info.downloadUrl = selectDownloadUrl(releaseObject);
    info.prerelease = releaseObject.value("prerelease").toBool(false);
    info.draft = releaseObject.value("draft").toBool(false);

    if (info.versionTag.isEmpty()) {
        return false;
    }
    if (info.versionNumber.isEmpty()) {
        info.versionNumber = info.versionTag;
    }
    if (!info.downloadUrl.isValid() && !info.htmlUrl.isValid()) {
        return false;
    }
    return true;
}

int AppUpdateChecker::compareVersions(const QString& lhs, const QString& rhs)
{
    static const QRegularExpression numberPattern("(\\d+)");

    const QString normalizedLhs = normalizeVersion(lhs);
    const QString normalizedRhs = normalizeVersion(rhs);

    QVector<int> lhsParts;
    QVector<int> rhsParts;

    auto extractParts = [&](const QString& text, QVector<int>& parts) {
        QRegularExpressionMatchIterator it = numberPattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok) {
                parts.push_back(value);
            }
        }
    };

    extractParts(normalizedLhs, lhsParts);
    extractParts(normalizedRhs, rhsParts);

    if (lhsParts.isEmpty() || rhsParts.isEmpty()) {
        return QString::compare(normalizedLhs, normalizedRhs, Qt::CaseInsensitive);
    }

    const int maxCount = qMax(lhsParts.size(), rhsParts.size());
    for (int i = 0; i < maxCount; ++i) {
        const int left = (i < lhsParts.size()) ? lhsParts[i] : 0;
        const int right = (i < rhsParts.size()) ? rhsParts[i] : 0;
        if (left != right) {
            return (left > right) ? 1 : -1;
        }
    }

    return 0;
}

} // namespace ComAssistant
