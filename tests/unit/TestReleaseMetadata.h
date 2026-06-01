/**
 * @file TestReleaseMetadata.h
 * @brief 发布元数据一致性测试头文件
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef TESTRELEASEMETADATA_H
#define TESTRELEASEMETADATA_H

#include <QObject>
#include <QTest>

class TestReleaseMetadata : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证 README 与内嵌帮助的最新发布日期和版本头文件一致。
     */
    void testLatestReleaseDateMatchesBuildDate();
};

#endif // TESTRELEASEMETADATA_H
