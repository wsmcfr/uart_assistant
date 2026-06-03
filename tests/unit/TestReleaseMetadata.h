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

    /**
     * @brief 验证无生产入口的遗留模块没有继续编译进主程序和测试目标。
     */
    void testUnusedLegacyModulesAreExcludedFromBuildTargets();
};

#endif // TESTRELEASEMETADATA_H
