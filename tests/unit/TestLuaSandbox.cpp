/**
 * @file TestLuaSandbox.cpp
 * @brief Lua 安全沙箱单元测试
 */

#include "TestLuaSandbox.h"

#include "core/script/LuaSandbox.h"

using namespace ComAssistant;

void TestLuaSandbox::allowsSafeMathStringAndTableUsage()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "local items = {'a', 'b', string.upper('c')}\n"
            "print(math.floor(2.8), table.concat(items, '-'))"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.size(), 1);
    QCOMPARE(result.outputLines.first(), QStringLiteral("2\ta-b-C"));
}

void TestLuaSandbox::blocksUnsafeLibrariesAndLoaders()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "print(os == nil)\n"
            "print(io == nil)\n"
            "print(package == nil)\n"
            "print(debug == nil)\n"
            "print(require == nil)\n"
            "print(dofile == nil)\n"
            "print(loadfile == nil)\n"
            "print(load == nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.size(), 8);
    for (const QString& line : result.outputLines) {
        QCOMPARE(line, QStringLiteral("true"));
    }
}

void TestLuaSandbox::capturesPrintOutput()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print('hello', 42, true, nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines,
             QStringList({QStringLiteral("hello\t42\ttrue\tnil")}));
    QVERIFY(result.elapsedMs >= 0);
}

void TestLuaSandbox::reportsRuntimeErrors()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("error('boom')"),
        options);

    QVERIFY(!result.success);
    QVERIFY(!result.timedOut);
    QVERIFY(!result.memoryExceeded);
    QVERIFY(result.errorMessage.contains(QStringLiteral("boom")));
}

void TestLuaSandbox::timesOutInfiniteLoop()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 20;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("while true do end"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.timedOut);
    QVERIFY(result.errorMessage.contains(QStringLiteral("timeout"),
                                         Qt::CaseInsensitive));
}

void TestLuaSandbox::limitsMemoryUsage()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 32;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "local t = {}\n"
            "for i = 1, 200000 do t[i] = string.rep('x', 64) end"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.memoryExceeded);
    QVERIFY(result.errorMessage.contains(QStringLiteral("memory"),
                                         Qt::CaseInsensitive));
}

void TestLuaSandbox::isolatesExecutions()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult first = sandbox.execute(
        QStringLiteral("leakedValue = 123\nprint(leakedValue)"),
        options);
    const LuaSandboxResult second = sandbox.execute(
        QStringLiteral("print(leakedValue == nil)"),
        options);

    QVERIFY2(first.success, qPrintable(first.errorMessage));
    QVERIFY2(second.success, qPrintable(second.errorMessage));
    QCOMPARE(first.outputLines.first(), QStringLiteral("123"));
    QCOMPARE(second.outputLines.first(), QStringLiteral("true"));
}

void TestLuaSandbox::truncatesExcessiveOutput()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.maxOutputLines = 3;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("for i = 1, 10 do print(i) end"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.size(), 4);
    QCOMPARE(result.outputLines.at(0), QStringLiteral("1"));
    QCOMPARE(result.outputLines.at(2), QStringLiteral("3"));
    QVERIFY(result.outputLines.last().contains(QStringLiteral("truncated"),
                                               Qt::CaseInsensitive));
}
