/**
 * @file TestMemoryUtils.h
 * @brief 进程内存整理辅助工具回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMEMORYUTILS_H
#define TESTMEMORYUTILS_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证 MemoryUtils 的平台分支可被测试覆盖。
 */
class TestMemoryUtils : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Windows 构建应启用工作集整理分支，非 Windows 构建应显式报告不支持。
     */
    void testTrimProcessMemorySupportMatchesPlatform();
};

#endif // TESTMEMORYUTILS_H
