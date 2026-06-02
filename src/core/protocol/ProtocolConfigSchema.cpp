/**
 * @file ProtocolConfigSchema.cpp
 * @brief 协议配置 Schema 实现
 */

#include "ProtocolConfigSchema.h"

#include <QSet>

namespace ComAssistant {

namespace {

/**
 * @brief 创建通用字段定义
 * @param key 稳定配置键名
 * @param displayName 用户可见名称
 * @param type 字段类型
 * @param defaultValue 默认值
 * @param description 字段说明
 * @return 填充公共字段后的配置字段定义
 */
ProtocolConfigField makeField(const QString& key,
                              const QString& displayName,
                              ProtocolConfigFieldType type,
                              const QVariant& defaultValue,
                              const QString& description)
{
    ProtocolConfigField field;
    field.key = key;
    field.displayName = displayName;
    field.type = type;
    field.defaultValue = defaultValue;
    field.description = description;
    return field;
}

/**
 * @brief 判断字符是否为十六进制字符
 * @param ch 待检查字符
 * @return 如果字符属于 0-9、a-f 或 A-F 返回 true
 */
bool isHexDigit(QChar ch)
{
    return (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) ||
           (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')) ||
           (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'));
}

/**
 * @brief 写入校验错误并标记结果失败
 * @param result 要更新的校验结果
 * @param key 出错字段 key
 * @param reason 错误原因
 */
void addError(ProtocolConfigValidationResult& result,
              const QString& key,
              const QString& reason)
{
    result.valid = false;
    result.errors.append(QStringLiteral("%1: %2").arg(key, reason));
}

} // namespace

ProtocolConfigField ProtocolConfigField::boolean(const QString& key,
                                                 const QString& displayName,
                                                 bool defaultValue,
                                                 const QString& description)
{
    return makeField(key, displayName, ProtocolConfigFieldType::Bool, defaultValue, description);
}

ProtocolConfigField ProtocolConfigField::integer(const QString& key,
                                                 const QString& displayName,
                                                 int defaultValue,
                                                 int minValue,
                                                 int maxValue,
                                                 const QString& description)
{
    ProtocolConfigField field =
        makeField(key, displayName, ProtocolConfigFieldType::Integer, defaultValue, description);
    field.minValue = minValue;
    field.maxValue = maxValue;
    return field;
}

ProtocolConfigField ProtocolConfigField::floating(const QString& key,
                                                  const QString& displayName,
                                                  double defaultValue,
                                                  double minValue,
                                                  double maxValue,
                                                  const QString& description)
{
    ProtocolConfigField field =
        makeField(key, displayName, ProtocolConfigFieldType::Double, defaultValue, description);
    field.minValue = minValue;
    field.maxValue = maxValue;
    return field;
}

ProtocolConfigField ProtocolConfigField::string(const QString& key,
                                                const QString& displayName,
                                                const QString& defaultValue,
                                                const QString& description)
{
    return makeField(key, displayName, ProtocolConfigFieldType::String, defaultValue, description);
}

ProtocolConfigField ProtocolConfigField::bytesHex(const QString& key,
                                                  const QString& displayName,
                                                  const QString& defaultValue,
                                                  const QString& description)
{
    return makeField(key, displayName, ProtocolConfigFieldType::BytesHex, defaultValue, description);
}

ProtocolConfigField ProtocolConfigField::enumeration(const QString& key,
                                                     const QString& displayName,
                                                     const QString& defaultValue,
                                                     const QStringList& enumValues,
                                                     const QString& description)
{
    ProtocolConfigField field =
        makeField(key, displayName, ProtocolConfigFieldType::Enum, defaultValue, description);
    field.enumValues = enumValues;
    return field;
}

QVariantMap ProtocolConfigSchema::defaults() const
{
    QVariantMap result;

    /*
     * 默认配置只来自字段定义，不携带未知字段。这样 descriptor.defaultConfig
     * 可以作为后续会话迁移、配置 UI 和诊断导出的稳定基线。
     */
    for (const ProtocolConfigField& field : fields) {
        bool ok = true;
        if (field.type == ProtocolConfigFieldType::BytesHex) {
            result.insert(field.key, normalizeHexString(field.defaultValue.toString(), &ok));
        } else {
            result.insert(field.key, field.defaultValue);
        }
    }

    return result;
}

ProtocolConfigValidationResult ProtocolConfigSchema::validate(const QVariantMap& config) const
{
    ProtocolConfigValidationResult result;
    QSet<QString> knownKeys;

    /*
     * 先按 schema 字段顺序合并默认值并校验输入值，保证 normalizedConfig
     * 输出顺序和字段定义逻辑一致，便于测试和诊断。
     */
    for (const ProtocolConfigField& field : fields) {
        knownKeys.insert(field.key);

        const QVariant rawValue = config.contains(field.key)
            ? config.value(field.key)
            : field.defaultValue;

        switch (field.type) {
        case ProtocolConfigFieldType::Bool: {
            if (!rawValue.canConvert(QVariant::Bool)) {
                addError(result, field.key, QStringLiteral("不是布尔值"));
                result.normalizedConfig.insert(field.key, field.defaultValue);
                break;
            }
            result.normalizedConfig.insert(field.key, rawValue.toBool());
            break;
        }

        case ProtocolConfigFieldType::Integer: {
            bool ok = false;
            const int value = rawValue.toInt(&ok);
            if (!ok) {
                addError(result, field.key, QStringLiteral("不是整数"));
                result.normalizedConfig.insert(field.key, field.defaultValue);
                break;
            }
            if (field.minValue.isValid() && value < field.minValue.toInt()) {
                addError(result, field.key,
                         QStringLiteral("小于最小值 %1").arg(field.minValue.toInt()));
            }
            if (field.maxValue.isValid() && value > field.maxValue.toInt()) {
                addError(result, field.key,
                         QStringLiteral("大于最大值 %1").arg(field.maxValue.toInt()));
            }
            result.normalizedConfig.insert(field.key, value);
            break;
        }

        case ProtocolConfigFieldType::Double: {
            bool ok = false;
            const double value = rawValue.toDouble(&ok);
            if (!ok) {
                addError(result, field.key, QStringLiteral("不是浮点数"));
                result.normalizedConfig.insert(field.key, field.defaultValue);
                break;
            }
            if (field.minValue.isValid() && value < field.minValue.toDouble()) {
                addError(result, field.key,
                         QStringLiteral("小于最小值 %1").arg(field.minValue.toDouble()));
            }
            if (field.maxValue.isValid() && value > field.maxValue.toDouble()) {
                addError(result, field.key,
                         QStringLiteral("大于最大值 %1").arg(field.maxValue.toDouble()));
            }
            result.normalizedConfig.insert(field.key, value);
            break;
        }

        case ProtocolConfigFieldType::String:
            result.normalizedConfig.insert(field.key, rawValue.toString());
            break;

        case ProtocolConfigFieldType::BytesHex: {
            bool ok = false;
            const QString normalized = normalizeHexString(rawValue.toString(), &ok);
            if (!ok) {
                addError(result, field.key, QStringLiteral("不是有效的十六进制字节文本"));
                result.normalizedConfig.insert(field.key, field.defaultValue);
                break;
            }
            result.normalizedConfig.insert(field.key, normalized);
            break;
        }

        case ProtocolConfigFieldType::Enum: {
            const QString value = rawValue.toString();
            if (!field.enumValues.contains(value)) {
                addError(result, field.key,
                         QStringLiteral("不是允许的枚举值: %1").arg(value));
            }
            result.normalizedConfig.insert(field.key, value);
            break;
        }
        }
    }

    /*
     * 第一版默认保留未知字段并给出 warning，避免未来插件字段或旧文件扩展
     * 被当前版本硬拒绝；关闭 allowUnknownFields 时才把未知字段作为错误。
     */
    for (auto it = config.constBegin(); it != config.constEnd(); ++it) {
        if (knownKeys.contains(it.key())) {
            continue;
        }

        if (allowUnknownFields) {
            result.warnings.append(QStringLiteral("%1: 未知配置字段").arg(it.key()));
            result.normalizedConfig.insert(it.key(), it.value());
        } else {
            addError(result, it.key(), QStringLiteral("未知配置字段"));
        }
    }

    return result;
}

QString ProtocolConfigSchema::normalizeHexString(const QString& value, bool* ok)
{
    QString compact;
    compact.reserve(value.size());

    /*
     * 允许用户输入常见分隔符和空白字符，例如 AA 55、AA-55、AA:55。
     * 其它非十六进制字符会导致校验失败，避免静默吞掉错误配置。
     */
    for (const QChar ch : value) {
        if (ch.isSpace() ||
            ch == QLatin1Char('-') ||
            ch == QLatin1Char(':') ||
            ch == QLatin1Char(',') ||
            ch == QLatin1Char('_')) {
            continue;
        }

        if (!isHexDigit(ch)) {
            if (ok) {
                *ok = false;
            }
            return QString();
        }

        compact.append(ch.toUpper());
    }

    if ((compact.size() % 2) != 0) {
        if (ok) {
            *ok = false;
        }
        return QString();
    }

    QStringList bytes;
    for (int i = 0; i < compact.size(); i += 2) {
        bytes.append(compact.mid(i, 2));
    }

    if (ok) {
        *ok = true;
    }
    return bytes.join(QStringLiteral(" "));
}

} // namespace ComAssistant
