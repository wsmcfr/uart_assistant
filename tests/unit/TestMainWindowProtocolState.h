/**
 * @file TestMainWindowProtocolState.h
 * @brief 主窗口协议状态协调器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMAINWINDOWPROTOCOLSTATE_H
#define TESTMAINWINDOWPROTOCOLSTATE_H

#include <QObject>
#include <QTest>

/**
 * @brief 主窗口稳定协议 ID 状态回归测试。
 *
 * 该测试聚焦 MainWindow 将要复用的协议状态协调器：稳定协议 ID、旧版绘图
 * ProtocolType、协议实例、配置、Lua 最近错误和诊断上下文必须保持一致。
 */
class TestMainWindowProtocolState : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Lua 协议应能按稳定 ID 创建并保留旧版 Raw 枚举。
     */
    void restoresLuaProtocolByStableId();

    /**
     * @brief 未知稳定协议 ID 应安全回退 Raw，避免误用旧绘图枚举。
     */
    void unknownProtocolIdFallsBackToRaw();

    /**
     * @brief Lua 解析错误应写入最近错误和诊断上下文。
     */
    void recordsLuaParseErrorForDiagnostics();

    /**
     * @brief 切回旧版绘图协议时应同步稳定 ID 和菜单用枚举。
     */
    void switchesPlotProtocolByLegacyType();

    /**
     * @brief 接收协议候选应包含 Lua 这类稳定 ID 协议。
     */
    void receiveProtocolChoicesIncludeLuaScript();
};

#endif // TESTMAINWINDOWPROTOCOLSTATE_H
