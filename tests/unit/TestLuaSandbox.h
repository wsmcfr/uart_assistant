/**
 * @file TestLuaSandbox.h
 * @brief Lua 安全沙箱单元测试头文件
 */

#ifndef TESTLUASANDBOX_H
#define TESTLUASANDBOX_H

#include <QObject>
#include <QTest>

/**
 * @brief Lua 安全沙箱测试类
 *
 * 该测试类验证 LuaSandbox 是否只暴露安全能力，并能在超时、内存超限、
 * 运行时错误和大量输出时返回结构化结果，避免后续脚本协议绕过安全边界。
 */
class TestLuaSandbox : public QObject
{
    Q_OBJECT

private slots:
    void allowsSafeMathStringAndTableUsage();
    void blocksUnsafeLibrariesAndLoaders();
    void capturesPrintOutput();
    void reportsRuntimeErrors();
    void timesOutInfiniteLoop();
    void limitsMemoryUsage();
    void isolatesExecutions();
    void truncatesExcessiveOutput();
    void exposesSafeChecksumAndHexUtilities();
    void keepsCommunicationApiDisabledByDefault();
    void registersSerialSendWhenCommunicationAllowed();
    void registersSerialSendHexWhenCommunicationAllowed();
    void serialApiStaysDisabledWithoutCallback();
    void serialSendFailureReturnsLuaError();
    void serialIsOpenUsesCallback();
};

#endif // TESTLUASANDBOX_H
