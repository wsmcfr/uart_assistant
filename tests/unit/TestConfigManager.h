/**
 * @file TestConfigManager.h
 * @brief 配置管理器回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTCONFIGMANAGER_H
#define TESTCONFIGMANAGER_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证配置管理器在显式文件路径和默认路径之间的保存行为。
 */
class TestConfigManager : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief saveConfig(filePath) 应真实写入传入路径，且不修改当前主配置路径。
     */
    void testSaveConfigWritesExplicitPathWithoutChangingCurrentSettings();
};

#endif // TESTCONFIGMANAGER_H
