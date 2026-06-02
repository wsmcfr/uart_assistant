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
};

#endif // TESTPROTOCOLDIAGNOSTICS_H
