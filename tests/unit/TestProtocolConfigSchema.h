/**
 * @file TestProtocolConfigSchema.h
 * @brief 协议配置 Schema 单元测试头文件
 */

#ifndef TESTPROTOCOLCONFIGSCHEMA_H
#define TESTPROTOCOLCONFIGSCHEMA_H

#include <QObject>
#include <QTest>

/**
 * @brief 协议配置 Schema 测试类
 *
 * 该测试类验证协议配置字段定义、默认值合并、类型校验、范围校验、
 * 枚举校验和十六进制字节字段规范化。
 */
class TestProtocolConfigSchema : public QObject
{
    Q_OBJECT

private slots:
    void fillsMissingDefaults();
    void rejectsInvalidValues();
    void normalizesHexBytes();
    void factoryAppliesValidatedAsciiConfig();
    void factoryAppliesValidatedEasyHexConfig();
    void sessionPersistsProtocolIdAndConfig();
    void sessionMigratesLegacyProtocolTypeToProtocolId();
};

#endif // TESTPROTOCOLCONFIGSCHEMA_H
