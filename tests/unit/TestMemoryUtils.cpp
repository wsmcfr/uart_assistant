/**
 * @file TestMemoryUtils.cpp
 * @brief 进程内存整理辅助工具回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMemoryUtils.h"

#include "core/utils/MemoryUtils.h"

#include <QtGlobal>

using namespace ComAssistant;

void TestMemoryUtils::testTrimProcessMemorySupportMatchesPlatform()
{
#ifdef Q_OS_WIN
    QVERIFY2(MemoryUtils::isTrimProcessMemorySupported(),
             "Windows 构建必须启用 HeapCompact/EmptyWorkingSet 分支。");
#else
    QVERIFY2(!MemoryUtils::isTrimProcessMemorySupported(),
             "非 Windows 构建应明确报告不支持工作集整理。");
#endif
}
