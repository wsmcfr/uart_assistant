/**
 * @file TestDisplaySettingsPolicy.h
 * @brief 显示设置默认值与迁移策略单元测试头文件
 * @author ComAssistant Team
 * @date 2026-05-06
 */

#ifndef TESTDISPLAYSETTINGSPOLICY_H
#define TESTDISPLAYSETTINGSPOLICY_H

#include <QObject>
#include <QTest>

class TestDisplaySettingsPolicy : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证缺省情况下会直接使用新的 2MB 默认值。
     */
    void testMissingValueFallsBackToNewDefault();

    /**
     * @brief 验证旧版本遗留的 8MB 默认值会被自动迁移到 2MB。
     */
    void testLegacyDefaultMigratesToTwoMb();

    /**
     * @brief 验证迁移完成后，用户手动设置的 8MB 不会再被自动覆盖。
     */
    void testManualEightMbIsRespectedAfterMigration();
};

#endif // TESTDISPLAYSETTINGSPOLICY_H
