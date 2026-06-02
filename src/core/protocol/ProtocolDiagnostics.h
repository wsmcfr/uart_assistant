/**
 * @file ProtocolDiagnostics.h
 * @brief 协议能力诊断 JSON 构建器
 */

#ifndef COMASSISTANT_PROTOCOLDIAGNOSTICS_H
#define COMASSISTANT_PROTOCOLDIAGNOSTICS_H

#include "ProtocolDescriptor.h"

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

namespace ComAssistant {

/**
 * @brief 构建协议诊断快照的工具类。
 *
 * 该类只依赖核心协议描述和配置 Schema，不依赖 QWidget。
 * 这样 UI、测试、后续 Lua/插件排障都能复用同一份 JSON 事实源。
 */
class ProtocolDiagnosticsBuilder
{
public:
    /**
     * @brief 构建协议诊断 JSON。
     * @param descriptor 当前协议描述，来自 ProtocolRegistry/ProtocolFactory。
     * @param currentConfig 当前协议实际配置；可包含用户输入的未规范化值。
     * @param generatedAt ISO 时间字符串；为空时使用当前本地时间。
     * @return 完整诊断 JSON 对象。
     *
     * 主要流程：生成应用信息、协议描述、能力标志、配置快照，
     * 然后用 Schema 校验 currentConfig 并写入 validation 节点。
     */
    static QJsonObject build(const ProtocolDescriptor& descriptor,
                             const QVariantMap& currentConfig,
                             const QString& generatedAt = QString());

private:
    ProtocolDiagnosticsBuilder() = delete;
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLDIAGNOSTICS_H
