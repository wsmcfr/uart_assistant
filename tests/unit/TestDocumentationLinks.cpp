/**
 * @file TestDocumentationLinks.cpp
 * @brief 用户指南文档链接检查测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestDocumentationLinks.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

#ifndef COMASSISTANT_SOURCE_DIR
#error "COMASSISTANT_SOURCE_DIR is not defined"
#endif

namespace {

/**
 * @brief 获取仓库根目录的规范路径。
 * @return 仓库根目录绝对路径。
 */
QString projectRootPath()
{
    return QDir::cleanPath(QStringLiteral(COMASSISTANT_SOURCE_DIR));
}

/**
 * @brief 读取 UTF-8 文本文档。
 * @param filePath 要读取的绝对文件路径。
 * @return 文件内容；读取失败时返回空字符串。
 */
QString readUtf8File(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

/**
 * @brief 判断链接是否是需要在仓库内解析的本地 Markdown 链接。
 * @param link Markdown 中解析出的链接目标。
 * @return 本地 Markdown 链接返回 true，外部 URL、锚点和 mailto 等返回 false。
 */
bool isLocalMarkdownLink(const QString& link)
{
    const QString trimmed = link.trimmed();
    if (trimmed.isEmpty() ||
        trimmed.startsWith(QLatin1Char('#')) ||
        trimmed.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        trimmed.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) ||
        trimmed.startsWith(QStringLiteral("mailto:"), Qt::CaseInsensitive)) {
        return false;
    }

    const QString pathOnly = trimmed.section(QLatin1Char('#'), 0, 0);
    return pathOnly.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive);
}

/**
 * @brief 将 Markdown 链接目标转换为文件系统路径。
 * @param baseDir 当前 Markdown 文件所在目录。
 * @param link Markdown 链接目标，可能包含锚点。
 * @return 规范化后的绝对路径。
 */
QString resolveMarkdownLink(const QDir& baseDir, const QString& link)
{
    const QString pathOnly = link.section(QLatin1Char('#'), 0, 0).trimmed();
    return QDir::cleanPath(baseDir.absoluteFilePath(pathOnly));
}

} // namespace

void TestDocumentationLinks::testUserGuideMarkdownLinksResolve()
{
    /*
     * 这里只扫描用户指南目录，因为当前问题集中在用户指南索引和快速
     * 入门章节。测试发现缺失文件后会输出完整相对路径，方便维护者
     * 直接补文档或修链接。
     */
    const QDir guideDir(projectRootPath() + QStringLiteral("/docs/user-guide"));
    QVERIFY2(guideDir.exists(), "docs/user-guide directory must exist.");

    const QStringList fileNames = guideDir.entryList(QStringList() << QStringLiteral("*.md"),
                                                     QDir::Files,
                                                     QDir::Name);
    QVERIFY2(!fileNames.isEmpty(), "docs/user-guide must contain Markdown files.");

    QStringList missingLinks;
    const QRegularExpression markdownLinkPattern(QStringLiteral("\\[[^\\]]+\\]\\(([^)]+)\\)"));

    for (const QString& fileName : fileNames) {
        const QString filePath = guideDir.absoluteFilePath(fileName);
        const QString content = readUtf8File(filePath);
        QVERIFY2(!content.isEmpty(),
                 qPrintable(QStringLiteral("User guide file must be readable: %1").arg(fileName)));

        QRegularExpressionMatchIterator it = markdownLinkPattern.globalMatch(content);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString link = match.captured(1);
            if (!isLocalMarkdownLink(link)) {
                continue;
            }

            const QString resolvedPath = resolveMarkdownLink(guideDir, link);
            if (!QFile::exists(resolvedPath)) {
                missingLinks.append(QStringLiteral("%1 -> %2").arg(fileName, link));
            }
        }
    }

    QVERIFY2(missingLinks.isEmpty(),
             qPrintable(QStringLiteral("Missing local Markdown links:\n%1")
                            .arg(missingLinks.join(QLatin1Char('\n')))));
}
