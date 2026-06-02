/**
 * @file ProtocolDescriptor.h
 * @brief 协议能力描述结构
 */

#ifndef COMASSISTANT_PROTOCOLDESCRIPTOR_H
#define COMASSISTANT_PROTOCOLDESCRIPTOR_H

#include "IProtocol.h"
#include "ProtocolConfigSchema.h"

#include <QMetaType>
#include <QString>
#include <QVariantMap>

namespace ComAssistant {

/**
 * @brief 协议分类
 *
 * 分类用于后续 UI 分组、诊断包导出和配置 schema 管理。
 */
enum class ProtocolCategory
{
    Basic,       ///< 基础显示/构帧协议
    Industrial,  ///< 工业通信协议
    Custom,      ///< 用户自定义或可配置协议
    Plot         ///< 绘图协议
};

/**
 * @brief 协议能力描述
 *
 * 描述一个协议的稳定 ID、显示信息、能力标志和旧版枚举映射。
 * 后续外部插件和 Lua 协议也会通过同样结构登记能力。
 */
struct ProtocolDescriptor
{
    QString id;                                           ///< 稳定协议 ID，用于配置、诊断和插件注册
    QString displayName;                                  ///< 面向用户显示的协议名称
    QString description;                                  ///< 协议用途说明
    ProtocolCategory category = ProtocolCategory::Basic;  ///< 协议分类
    ProtocolType legacyType = ProtocolType::Raw;          ///< 旧版 ProtocolType 映射
    bool builtin = true;                                  ///< 是否为内置协议
    bool plotProtocol = false;                            ///< 是否支持绘图数据解析
    bool frameBuilder = false;                            ///< 是否支持 payload 构建发送帧
    int configVersion = 1;                                ///< 配置版本，第一版为 1
    ProtocolConfigSchema configSchema;                    ///< 配置 Schema，用于默认值、校验和后续 UI 生成
    QVariantMap defaultConfig;                            ///< 默认配置，为后续 schema 扩展预留
};

} // namespace ComAssistant

Q_DECLARE_METATYPE(ComAssistant::ProtocolCategory)
Q_DECLARE_METATYPE(ComAssistant::ProtocolDescriptor)

#endif // COMASSISTANT_PROTOCOLDESCRIPTOR_H
