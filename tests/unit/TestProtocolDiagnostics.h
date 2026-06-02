/**
 * @file TestProtocolDiagnostics.h
 * @brief 协议诊断导出单元测试头文件
 */

#ifndef TESTPROTOCOLDIAGNOSTICS_H
#define TESTPROTOCOLDIAGNOSTICS_H

#include <QObject>
#include <QTest>

/**
 * @brief 协议诊断导出测试类
 *
 * 该测试类验证协议诊断 JSON 是否完整反映注册中心描述、Schema 字段、
 * 当前配置、规范化配置和校验结果，避免后续 Lua/插件排障时事实源缺失。
 */
class TestProtocolDiagnostics : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证诊断 JSON 导出协议描述和能力标志。
     */
    void exportsDescriptorAndCapabilities();

    /**
     * @brief 验证诊断 JSON 导出 Schema 字段、当前配置和规范化配置。
     */
    void exportsSchemaAndConfigs();

    /**
     * @brief 验证配置非法时诊断 JSON 保留校验错误。
     */
    void exportsValidationErrorsForInvalidConfig();

    /**
     * @brief 验证 Raw/空 Schema 协议也能安全导出。
     */
    void exportsRawProtocolWithoutFields();

    /**
     * @brief 验证 Lua 脚本协议导出专用诊断节点。
     *
     * Lua 协议 4.9 只登记元数据，但诊断 JSON 必须能说明沙箱选项、
     * 接收 API 状态和最近错误，便于后续协议脚本化排障。
     */
    void exportsLuaProtocolDiagnostics();

    /**
     * @brief 验证诊断对话框展示只读 JSON 文本。
     */
    void dialogShowsSummaryAndJson();

    /**
     * @brief 验证诊断对话框提供复制和保存按钮。
     */
    void dialogCopyAndSaveButtonsExist();
};

#endif // TESTPROTOCOLDIAGNOSTICS_H
