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
 * @brief 协议诊断上下文。
 *
 * Builder 的主体事实源仍来自 descriptor、Schema 和当前配置。该上下文只承载
 * 运行期补充信息，例如后续 Lua 协议或插件协议的最近错误，避免把短期状态
 * 写进 ProtocolDescriptor 这种静态元数据。
 */
struct ProtocolDiagnosticsContext
{
    QString recentError; ///< 最近一次协议脚本或扩展运行错误；为空表示暂无错误
};

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
     * @param context 运行期补充诊断上下文，例如 Lua 协议最近错误。
     * @return 完整诊断 JSON 对象。
     *
     * 主要流程：生成应用信息、协议描述、能力标志、配置快照，
     * 然后用 Schema 校验 currentConfig 并写入 validation 节点。
     */
    static QJsonObject build(const ProtocolDescriptor& descriptor,
                             const QVariantMap& currentConfig,
                             const QString& generatedAt = QString(),
                             const ProtocolDiagnosticsContext& context = ProtocolDiagnosticsContext());

private:
    ProtocolDiagnosticsBuilder() = delete;
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLDIAGNOSTICS_H
