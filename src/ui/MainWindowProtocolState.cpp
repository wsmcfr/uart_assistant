/**
 * @file MainWindowProtocolState.cpp
 * @brief 主窗口稳定协议状态协调器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "MainWindowProtocolState.h"

#include "protocol/LuaScriptProtocol.h"
#include "protocol/ProtocolFactory.h"

namespace ComAssistant {

MainWindowProtocolState::MainWindowProtocolState()
{
    switchById(QStringLiteral("raw"));
}

void MainWindowProtocolState::switchById(const QString& protocolId,
                                         const QVariantMap& config)
{
    const ProtocolRegistry& registry = ProtocolFactory::registry();
    const QString requestedId = protocolId.trimmed();
    const QString effectiveId =
        (!requestedId.isEmpty() && registry.contains(requestedId))
            ? requestedId
            : QStringLiteral("raw");

    m_descriptor = registry.descriptor(effectiveId);
    if (m_descriptor.id.isEmpty()) {
        /*
         * 正常内置注册中心一定包含 raw。这个分支只作为极端防御，避免注册
         * 中心异常时 MainWindow 留下半初始化协议状态。
         */
        m_protocolId = QStringLiteral("raw");
        m_protocolType = ProtocolType::Raw;
        m_config.clear();
        m_protocol.reset();
        m_recentProtocolError.clear();
        return;
    }

    m_protocolId = m_descriptor.id;
    m_protocolType = m_descriptor.legacyType;
    m_config = normalizedConfigFor(m_descriptor, config);
    m_protocol.reset();
    m_recentProtocolError.clear();

    if (!m_descriptor.creatable || m_protocolId == QStringLiteral("raw")) {
        return;
    }

    m_protocol.reset(registry.create(m_protocolId, nullptr));
    if (m_protocol) {
        m_protocol->setConfig(m_config);
    }
}

void MainWindowProtocolState::switchByLegacyType(ProtocolType type,
                                                 const QVariantMap& config)
{
    const QString protocolId = ProtocolFactory::typeId(type);
    switchById(protocolId.isEmpty() ? QStringLiteral("raw") : protocolId, config);
}

FrameResult MainWindowProtocolState::parseNonPlotData(const QByteArray& data)
{
    FrameResult result;

    /*
     * Raw 和绘图协议不在这里解析：Raw 只做显示和自动检测，绘图协议交给
     * MainWindowPlotDataRouter。这样 lua.script 的 legacyType 即便是 Raw，
     * 也不会被旧自动检测逻辑吞掉。
     */
    if (!m_protocol || m_descriptor.plotProtocol) {
        return result;
    }

    result = m_protocol->parse(data);
    if (result.valid) {
        m_recentProtocolError.clear();
    } else if (!result.errorMessage.trimmed().isEmpty()) {
        m_recentProtocolError = result.errorMessage.trimmed();
    }

    if (const auto* luaProtocol = dynamic_cast<const LuaScriptProtocol*>(m_protocol.get())) {
        const QString luaRecentError = luaProtocol->recentError().trimmed();
        if (!luaRecentError.isEmpty()) {
            m_recentProtocolError = luaRecentError;
        } else if (result.valid) {
            m_recentProtocolError.clear();
        }
    }

    return result;
}

ProtocolDiagnosticsContext MainWindowProtocolState::diagnosticsContext() const
{
    ProtocolDiagnosticsContext context;
    context.recentError = m_recentProtocolError;
    if (const auto* luaProtocol = dynamic_cast<const LuaScriptProtocol*>(m_protocol.get())) {
        const QString luaRecentError = luaProtocol->recentError().trimmed();
        if (!luaRecentError.isEmpty()) {
            context.recentError = luaRecentError;
        }
    }
    return context;
}

QVariantMap MainWindowProtocolState::normalizedConfigFor(
    const ProtocolDescriptor& descriptor,
    const QVariantMap& config)
{
    if (descriptor.configSchema.fields.isEmpty()) {
        return descriptor.defaultConfig;
    }

    const ProtocolConfigValidationResult validation =
        descriptor.configSchema.validate(config);
    return validation.valid
        ? validation.normalizedConfig
        : descriptor.defaultConfig;
}

} // namespace ComAssistant
