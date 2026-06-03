/**
 * @file TestConfigManager.cpp
 * @brief 配置管理器回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestConfigManager.h"

#include "core/config/ConfigManager.h"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

using namespace ComAssistant;

void TestConfigManager::testSaveConfigWritesExplicitPathWithoutChangingCurrentSettings()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString primaryPath = tempDir.filePath(QStringLiteral("primary.ini"));
    const QString exportPath = tempDir.filePath(QStringLiteral("exported.ini"));

    ConfigManager* manager = ConfigManager::instance();
    manager->resetForTest();
    QVERIFY(manager->initialize(primaryPath));

    /*
     * 先写入当前主配置，再显式保存到另一个路径。旧实现虽然计算了
     * savePath，但所有 setValue() 仍写 m_settings，导致 exported.ini
     * 不会创建，primary.ini 反而被误写。
     */
    manager->setLanguage(QStringLiteral("en_US"));
    manager->setThemeIndex(1);
    manager->setWindowTitleSuffix(QStringLiteral("primary-before-export"));
    QVERIFY(manager->saveConfig());

    manager->setLanguage(QStringLiteral("zh_CN"));
    manager->setThemeIndex(0);
    manager->setWindowTitleSuffix(QStringLiteral("export-target"));
    QVERIFY(manager->saveConfig(exportPath));

    QVERIFY2(QFile::exists(exportPath),
             "saveConfig(filePath) 必须真实创建并写入传入的配置文件路径。");

    QSettings exportedSettings(exportPath, QSettings::IniFormat);
    QCOMPARE(exportedSettings.value(QStringLiteral("Global/Language")).toString(),
             QStringLiteral("zh_CN"));
    QCOMPARE(exportedSettings.value(QStringLiteral("Global/ThemeIndex")).toInt(), 0);
    QCOMPARE(exportedSettings.value(QStringLiteral("Global/WindowTitleSuffix")).toString(),
             QStringLiteral("export-target"));

    QSettings primarySettings(primaryPath, QSettings::IniFormat);
    QCOMPARE(primarySettings.value(QStringLiteral("Global/Language")).toString(),
             QStringLiteral("en_US"));
    QCOMPARE(primarySettings.value(QStringLiteral("Global/ThemeIndex")).toInt(), 1);
    QCOMPARE(primarySettings.value(QStringLiteral("Global/WindowTitleSuffix")).toString(),
             QStringLiteral("primary-before-export"));

    /*
     * 显式保存到其它路径是导出当前内存配置，不应切换 ConfigManager
     * 继续使用的主配置路径。后续普通 saveConfig() 仍应写回 primary.ini。
     */
    QCOMPARE(manager->configFilePath(), primaryPath);
    manager->setWindowTitleSuffix(QStringLiteral("primary-after-export"));
    QVERIFY(manager->saveConfig());
    primarySettings.sync();
    QCOMPARE(primarySettings.value(QStringLiteral("Global/WindowTitleSuffix")).toString(),
             QStringLiteral("primary-after-export"));

    manager->resetForTest();
}
