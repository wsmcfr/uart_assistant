/**
 * @file TestScriptEditorDialog.h
 * @brief 脚本编辑器沙箱执行单元测试头文件
 */

#ifndef TESTSCRIPTEDITORDIALOG_H
#define TESTSCRIPTEDITORDIALOG_H

#include <QObject>
#include <QTest>

/**
 * @brief 脚本编辑器沙箱执行测试类。
 *
 * 该测试类验证脚本编辑器是否真正调用 LuaSandbox 执行脚本，并把输出、
 * 错误和 serial.send 发送请求映射回现有 UI 信号。
 */
class TestScriptEditorDialog : public QObject
{
    Q_OBJECT

private slots:
    void runScriptUsesLuaSandboxPrintOutput();
    void runScriptEmitsSendDataFromSerialSend();
    void runScriptShowsLuaErrors();
    void runScriptReturnsControlBeforeLongScriptFinishes();
    void stopScriptCancelsRunningSandbox();
    void runScriptEmitsSendDataFromWorkerThread();
    void runScriptRestoresButtonsAfterError();
};

#endif // TESTSCRIPTEDITORDIALOG_H
