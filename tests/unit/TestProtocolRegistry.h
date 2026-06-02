/**
 * @file TestProtocolRegistry.h
 * @brief 协议注册中心单元测试头文件
 */

#ifndef TESTPROTOCOLREGISTRY_H
#define TESTPROTOCOLREGISTRY_H

#include <QObject>
#include <QTest>

/**
 * @brief 协议注册中心测试类
 *
 * 该测试类用于验证第四阶段协议平台化基座，确保内置协议能被统一描述、
 * 查询和创建，同时保护旧版 ProtocolFactory 行为不被破坏。
 */
class TestProtocolRegistry : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证全部内置协议描述已注册
     *
     * 测试重点是协议数量、稳定 ID、用户显示名称、旧版枚举映射和基础能力标志。
     */
    void builtinDescriptorsAreRegistered();
};

#endif // TESTPROTOCOLREGISTRY_H
