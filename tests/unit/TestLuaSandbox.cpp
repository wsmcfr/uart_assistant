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

void TestLuaSandbox::exposesSafeChecksumAndHexUtilities()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral(
            "local bytes = hexToBytes('AA 55 01')\n"
            "print(bytesToHex(bytes))\n"
            "print(crc16(hexToBytes('01 03 00 00 00 02')))\n"
            "print(crc32('abc'))"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.at(0), QStringLiteral("AA 55 01"));
    QVERIFY(result.outputLines.at(1).toInt() > 0);
    QVERIFY(result.outputLines.at(2).toUInt() > 0U);
}

void TestLuaSandbox::keepsCommunicationApiDisabledByDefault()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print(serial == nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.first(), QStringLiteral("true"));
}

void TestLuaSandbox::registersSerialSendWhenCommunicationAllowed()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;

    QList<QByteArray> sentPayloads;
    options.sendCallback = [&sentPayloads](const QByteArray& data) {
        sentPayloads.append(data);
        return true;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.send('AT\\r\\n')"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(sentPayloads.size(), 1);
    QCOMPARE(sentPayloads.first(), QByteArray("AT\r\n"));
}

void TestLuaSandbox::registersSerialSendHexWhenCommunicationAllowed()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;

    QByteArray sent;
    options.sendCallback = [&sent](const QByteArray& data) {
        sent = data;
        return true;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.sendHex('AA 55 01')"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(sent.toHex(' ').toUpper(), QByteArray("AA 55 01"));
}

void TestLuaSandbox::serialApiStaysDisabledWithoutCallback()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print(serial == nil)"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.first(), QStringLiteral("true"));
}

void TestLuaSandbox::serialSendFailureReturnsLuaError()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;
    options.sendCallback = [](const QByteArray&) {
        return false;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.send('data')"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("serial.send failed")));
}

/**
 * @brief 验证 serial.send 失败时能暴露调用方提供的具体原因。
 *
 * 4.8 需要把主窗口发送队列拒绝、未连接或写入失败传回 Lua。
 * 该测试先使用新回调注入稳定错误文本，证明 LuaSandbox 不再只能
 * 返回泛化的 serial.send failed。
 */
void TestLuaSandbox::serialSendFailureCanExposeSpecificReason()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;
    options.sendWithErrorCallback = [](const QByteArray&, QString* error) {
        if (error) {
            *error = QStringLiteral("queue rejected for test");
        }
        return false;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("serial.send('data')"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("serial.send failed")));
    QVERIFY(result.errorMessage.contains(QStringLiteral("queue rejected for test")));
}

void TestLuaSandbox::serialIsOpenUsesCallback()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 500;
    options.memoryLimitKb = 256;
    options.allowCommunicationApi = true;
    options.sendCallback = [](const QByteArray&) {
        return true;
    };
    options.isOpenCallback = []() {
        return false;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("print(serial.isOpen())"),
        options);

    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.outputLines.first(), QStringLiteral("false"));
}

/**
 * @brief 验证外部取消回调能中断 Lua 死循环。
 *
 * 该用例使用无超时配置，确保失败原因来自 interruptCallback，
 * 而不是既有 timeout hook。回调在被 hook 查询数次后返回 true，
 * 模拟 UI 线程点击停止按钮后的取消标记。
 */
void TestLuaSandbox::interruptsInfiniteLoopWhenCallbackRequestsStop()
{
    LuaSandbox sandbox;
    LuaSandboxOptions options;
    options.timeoutMs = 0;
    options.memoryLimitKb = 256;

    int hookChecks = 0;
    options.interruptCallback = [&hookChecks]() {
        ++hookChecks;
        return hookChecks >= 2;
    };

    const LuaSandboxResult result = sandbox.execute(
        QStringLiteral("while true do end"),
        options);

    QVERIFY(!result.success);
    QVERIFY(result.interrupted);
    QVERIFY(!result.timedOut);
    QVERIFY(hookChecks >= 2);
    QVERIFY(result.errorMessage.contains(QStringLiteral("interrupted"),
                                         Qt::CaseInsensitive));
}
