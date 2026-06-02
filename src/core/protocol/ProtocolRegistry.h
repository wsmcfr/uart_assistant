/**
 * @file ProtocolRegistry.h
 * @brief 协议注册中心
 */

#ifndef COMASSISTANT_PROTOCOLREGISTRY_H
#define COMASSISTANT_PROTOCOLREGISTRY_H

#include "ProtocolDescriptor.h"

#include <QList>
#include <QMap>
#include <QString>

namespace ComAssistant {

/**
 * @brief 协议注册中心
 *
 * 注册中心保存协议描述与创建器，让协议能力可以被查询、创建和后续扩展。
 */
class ProtocolRegistry
{
public:
    /**
     * @brief 注册一个协议能力
     * @param descriptor 协议描述，id 必须非空且不能重复
     * @param creator 协议创建器；Raw 可为空，因为 Raw 表示无协议
     * @param errorMessage 注册失败时写入原因
     * @return 注册是否成功
     */
    bool registerProtocol(const ProtocolDescriptor& descriptor,
                          ProtocolCreator creator,
                          QString* errorMessage = nullptr);

    /**
     * @brief 注册全部内置协议
     *
     * 该函数可重复调用，第二次不会重复注册。
     */
    void registerBuiltinProtocols();

    /**
     * @brief 判断指定协议 ID 是否存在
     * @param id 稳定协议 ID
     * @return 如果协议已注册返回 true
     */
    bool contains(const QString& id) const;

    /**
     * @brief 查询指定协议描述
     * @param id 稳定协议 ID
     * @return 已注册的协议描述；未知 ID 返回默认空描述
     */
    ProtocolDescriptor descriptor(const QString& id) const;

    /**
     * @brief 返回全部协议描述
     * @return 按注册顺序排列的协议描述列表
     */
    QList<ProtocolDescriptor> descriptors() const;

    /**
     * @brief 按分类返回协议描述列表
     * @param category 目标协议分类
     * @return 保持注册顺序的协议描述列表
     */
    QList<ProtocolDescriptor> descriptorsByCategory(ProtocolCategory category) const;

    /**
     * @brief 按 ID 创建协议实例
     * @param id 稳定协议 ID
     * @param parent Qt 父对象，传入后由父子关系管理生命周期
     * @return 协议实例；未知 ID 或 Raw 返回 nullptr
     */
    IProtocol* create(const QString& id, QObject* parent = nullptr) const;

private:
    QList<QString> m_orderedIds;                     ///< 注册顺序，用于保持稳定列表输出
    QMap<QString, ProtocolDescriptor> m_descriptors; ///< 协议 ID 到描述的映射
    QMap<QString, ProtocolCreator> m_creators;       ///< 协议 ID 到创建器的映射
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLREGISTRY_H
