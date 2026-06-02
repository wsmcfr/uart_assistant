/**
 * @file ProtocolConfigSchema.h
 * @brief 协议配置 Schema 定义
 */

#ifndef COMASSISTANT_PROTOCOLCONFIGSCHEMA_H
#define COMASSISTANT_PROTOCOLCONFIGSCHEMA_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace ComAssistant {

/**
 * @brief 协议配置字段类型
 *
 * 字段类型用于统一校验会话、后续 UI、脚本和插件传入的协议配置。
 */
enum class ProtocolConfigFieldType
{
    Bool,       ///< 布尔值
    Integer,    ///< 整数
    Double,     ///< 浮点数
    String,     ///< 字符串
    BytesHex,   ///< 十六进制字节文本，例如 AA 55
    Enum        ///< 字符串枚举
};

/**
 * @brief 协议配置字段定义
 *
 * 描述一个配置项的 key、显示名、类型、默认值、范围、枚举选项和说明。
 */
struct ProtocolConfigField
{
    QString key;                                      ///< 配置键名，必须稳定
    QString displayName;                              ///< 面向用户显示的名称
    QString description;                              ///< 字段说明
    ProtocolConfigFieldType type = ProtocolConfigFieldType::String; ///< 字段类型
    QVariant defaultValue;                            ///< 默认值
    QVariant minValue;                                ///< 数值最小值，可为空
    QVariant maxValue;                                ///< 数值最大值，可为空
    QStringList enumValues;                           ///< 枚举允许值
    bool required = false;                            ///< 是否必填；缺失时仍优先补默认值

    /**
     * @brief 创建布尔字段定义
     * @param key 稳定配置键名
     * @param displayName 用户可见名称
     * @param defaultValue 默认布尔值
     * @param description 字段说明
     * @return 填充好的字段定义
     */
    static ProtocolConfigField boolean(const QString& key,
                                       const QString& displayName,
                                       bool defaultValue,
                                       const QString& description);

    /**
     * @brief 创建整数字段定义
     * @param key 稳定配置键名
     * @param displayName 用户可见名称
     * @param defaultValue 默认整数值
     * @param minValue 允许的最小值
     * @param maxValue 允许的最大值
     * @param description 字段说明
     * @return 填充好的字段定义
     */
    static ProtocolConfigField integer(const QString& key,
                                       const QString& displayName,
                                       int defaultValue,
                                       int minValue,
                                       int maxValue,
                                       const QString& description);

    /**
     * @brief 创建浮点字段定义
     * @param key 稳定配置键名
     * @param displayName 用户可见名称
     * @param defaultValue 默认浮点值
     * @param minValue 允许的最小值
     * @param maxValue 允许的最大值
     * @param description 字段说明
     * @return 填充好的字段定义
     */
    static ProtocolConfigField floating(const QString& key,
                                        const QString& displayName,
                                        double defaultValue,
                                        double minValue,
                                        double maxValue,
                                        const QString& description);

    /**
     * @brief 创建字符串字段定义
     * @param key 稳定配置键名
     * @param displayName 用户可见名称
     * @param defaultValue 默认字符串
     * @param description 字段说明
     * @return 填充好的字段定义
     */
    static ProtocolConfigField string(const QString& key,
                                      const QString& displayName,
                                      const QString& defaultValue,
                                      const QString& description);

    /**
     * @brief 创建十六进制字节字段定义
     * @param key 稳定配置键名
     * @param displayName 用户可见名称
     * @param defaultValue 默认十六进制文本
     * @param description 字段说明
     * @return 填充好的字段定义
     */
    static ProtocolConfigField bytesHex(const QString& key,
                                        const QString& displayName,
                                        const QString& defaultValue,
                                        const QString& description);

    /**
     * @brief 创建枚举字段定义
     * @param key 稳定配置键名
     * @param displayName 用户可见名称
     * @param defaultValue 默认枚举值
     * @param enumValues 允许的枚举值列表
     * @param description 字段说明
     * @return 填充好的字段定义
     */
    static ProtocolConfigField enumeration(const QString& key,
                                           const QString& displayName,
                                           const QString& defaultValue,
                                           const QStringList& enumValues,
                                           const QString& description);
};

/**
 * @brief 协议配置校验结果
 */
struct ProtocolConfigValidationResult
{
    bool valid = true;                 ///< 是否校验通过
    QVariantMap normalizedConfig;      ///< 合并默认值并规范化后的配置
    QStringList errors;                ///< 阻止应用配置的错误
    QStringList warnings;              ///< 可继续使用但需要提示的警告
};

/**
 * @brief 协议配置 Schema
 *
 * Schema 负责为某个协议定义配置字段，并提供默认值合并与校验能力。
 */
struct ProtocolConfigSchema
{
    int version = 1;                   ///< 配置版本
    bool allowUnknownFields = true;    ///< 是否允许未知字段
    QList<ProtocolConfigField> fields; ///< 字段列表

    /**
     * @brief 生成默认配置
     * @return 所有字段默认值组成的配置表
     */
    QVariantMap defaults() const;

    /**
     * @brief 校验并规范化配置
     * @param config 输入配置，可以只包含部分字段
     * @return 校验结果，包含规范化配置、错误和警告
     */
    ProtocolConfigValidationResult validate(const QVariantMap& config) const;

private:
    /**
     * @brief 规范化十六进制字节文本
     * @param value 用户输入的十六进制字符串
     * @param ok 输出是否解析成功
     * @return 大写空格分隔的十六进制文本；空输入返回空字符串
     */
    static QString normalizeHexString(const QString& value, bool* ok);
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLCONFIGSCHEMA_H
