/**
 * @file TestReleaseMetadata.cpp
 * @brief 发布元数据一致性测试实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TestReleaseMetadata.h"

#include "version.h"

#include <QDir>
#include <QFile>

#ifndef COMASSISTANT_SOURCE_DIR
#error "COMASSISTANT_SOURCE_DIR is not defined"
#endif

namespace {

/**
 * @brief 读取仓库内文本文件。
 * @param relativePath 相对仓库根目录的路径。
 * @return 文件内容；打开失败时返回空字符串。
 */
QString readProjectFile(const QString& relativePath)
{
    const QString path = QDir::cleanPath(
        QStringLiteral(COMASSISTANT_SOURCE_DIR) + QLatin1Char('/') + relativePath);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

void TestReleaseMetadata::testLatestReleaseDateMatchesBuildDate()
{
    const QString readme = readProjectFile(QStringLiteral("README.md"));
    const QString quickstart = readProjectFile(QStringLiteral("resources/help/quickstart.html"));
    const QString expectedVersion = QStringLiteral("v") + QStringLiteral(APP_VERSION);
    const QString expectedDate = QStringLiteral(APP_BUILD_DATE);

    QVERIFY2(!readme.isEmpty(), "README.md must be readable.");
    QVERIFY2(!quickstart.isEmpty(), "resources/help/quickstart.html must be readable.");

    QVERIFY2(readme.contains(expectedVersion),
             qPrintable(QStringLiteral("README latest release should mention %1.").arg(expectedVersion)));
    QVERIFY2(readme.contains(expectedDate),
             qPrintable(QStringLiteral("README latest release date should match %1.").arg(expectedDate)));

    QVERIFY2(quickstart.contains(expectedVersion),
             qPrintable(QStringLiteral("quickstart latest release should mention %1.").arg(expectedVersion)));
    QVERIFY2(quickstart.contains(expectedDate),
             qPrintable(QStringLiteral("quickstart latest release date should match %1.").arg(expectedDate)));
}
