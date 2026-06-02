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

    /**
     * @brief 验证非法注册会被拒绝
     *
     * 注册中心是后续插件化入口，必须拒绝空 ID、重复 ID 和缺失创建器。
     */
    void rejectsInvalidRegistrations();

    /**
     * @brief 验证 Raw 协议保持旧行为
     *
     * Raw 可以作为能力被描述，但它表示无协议，创建实例时必须继续返回空指针。
     */
    void keepsRawCompatibility();
};

#endif // TESTPROTOCOLREGISTRY_H
