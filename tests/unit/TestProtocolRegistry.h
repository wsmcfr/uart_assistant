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

    /**
     * @brief 验证可按稳定协议 ID 创建协议实例
     *
     * 该测试保护后续配置 schema、脚本协议和插件协议共用的创建入口。
     */
    void createsProtocolById();

    /**
     * @brief 验证绘图协议元数据
     *
     * 绘图能力标志是后续 UI 分组、诊断包和协议自动化的重要依据。
     */
    void marksPlotProtocols();

    /**
     * @brief 验证旧版协议工厂 API 保持兼容
     *
     * 旧调用方仍通过 ProtocolType 使用工厂，迁移到注册中心后不能改变行为。
     */
    void keepsFactoryCompatibility();

    /**
     * @brief 验证可按分类筛选协议描述
     *
     * 分类筛选为后续协议 UI 分组、诊断包和插件管理视图提供基础能力。
     */
    void filtersDescriptorsByCategory();

    /**
     * @brief 验证已有外部注册项时仍可补齐内置协议
     *
     * 注册中心后续会承载插件或脚本协议，补注册内置协议时不能因为已有条目就提前退出。
     */
    void builtinRegistrationKeepsExistingProtocols();

    /**
     * @brief 验证内置协议描述暴露配置 Schema
     *
     * 4.2 要求协议注册中心成为配置默认值和字段定义的事实源。
     */
    void builtinDescriptorsExposeConfigSchema();

    /**
     * @brief 验证内置协议默认配置能通过自身 Schema
     *
     * 默认配置如果无法自洽，会导致会话迁移、后续 UI 和协议实例创建都不可靠。
     */
    void defaultConfigMatchesSchema();
};

#endif // TESTPROTOCOLREGISTRY_H
