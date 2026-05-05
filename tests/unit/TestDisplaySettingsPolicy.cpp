/**
 * @file TestDisplaySettingsPolicy.cpp
 * @brief 显示设置默认值与迁移策略单元测试实现
 * @author ComAssistant Team
 * @date 2026-05-06
 */

#include "TestDisplaySettingsPolicy.h"

#include "core/config/DisplaySettingsPolicy.h"

#include <QTemporaryDir>

using namespace ComAssistant;

namespace {

/**
 * @brief 创建指向临时 ini 文件的 QSettings，避免污染用户真实配置。
 * @param directory 当前测试用到的临时目录。
 * @return 指向临时配置文件的 QSettings 实例。
 */
QSettings createTempSettings(const QTemporaryDir& directory)
{
    return QSettings(directory.filePath("display-settings.ini"), QSettings::IniFormat);
}

} // namespace

void TestDisplaySettingsPolicy::testMissingValueFallsBackToNewDefault()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QSettings settings = createTempSettings(tempDir);

    const int hexBufferMb = loadHexBufferMbSetting(settings);

    QCOMPARE(hexBufferMb, kDefaultHexBufferMb);
    QCOMPARE(settings.value("Display/HexBufferMB").toInt(), kDefaultHexBufferMb);
    QCOMPARE(settings.value("Display/HexBufferMBMigrationDone").toBool(), true);
}

void TestDisplaySettingsPolicy::testLegacyDefaultMigratesToTwoMb()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QSettings settings = createTempSettings(tempDir);
    settings.setValue("Display/HexBufferMB", kLegacyDefaultHexBufferMb);
    settings.sync();

    const int hexBufferMb = loadHexBufferMbSetting(settings);

    QCOMPARE(hexBufferMb, kDefaultHexBufferMb);
    QCOMPARE(settings.value("Display/HexBufferMB").toInt(), kDefaultHexBufferMb);
    QCOMPARE(settings.value("Display/HexBufferMBMigrationDone").toBool(), true);
}

void TestDisplaySettingsPolicy::testManualEightMbIsRespectedAfterMigration()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QSettings settings = createTempSettings(tempDir);

    /*
     * 先触发一次迁移，模拟用户已经完成从旧版本升级。
     * 后续再手动设回 8MB 时，读取逻辑必须尊重这个显式选择。
     */
    QCOMPARE(loadHexBufferMbSetting(settings), kDefaultHexBufferMb);

    saveHexBufferMbSetting(settings, kLegacyDefaultHexBufferMb);
    settings.sync();

    const int hexBufferMb = loadHexBufferMbSetting(settings);

    QCOMPARE(hexBufferMb, kLegacyDefaultHexBufferMb);
    QCOMPARE(settings.value("Display/HexBufferMB").toInt(), kLegacyDefaultHexBufferMb);
    QCOMPARE(settings.value("Display/HexBufferMBMigrationDone").toBool(), true);
}
