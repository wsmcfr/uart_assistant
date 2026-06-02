/**
 * @file MainWindowProtocolState.h
 * @brief 主窗口稳定协议状态协调器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_MAINWINDOWPROTOCOLSTATE_H
#define COMASSISTANT_MAINWINDOWPROTOCOLSTATE_H

#include "protocol/IProtocol.h"
#include "protocol/ProtocolDescriptor.h"
#include "protocol/ProtocolDiagnostics.h"

#include <QString>
#include <QList>
#include <QVariantMap>
#include <memory>

namespace ComAssistant {

/**
 * @brief 主窗口当前接收协议状态。
 *
 * 该类把 MainWindow 中“稳定协议 ID、旧版绘图枚举、协议实例、协议配置、
 * 最近运行错误”收束到一个可测试对象。MainWindow 仍负责 UI 菜单、状态栏
 * 和绘图路由；本类只维护协议事实源，避免 lua.script 这种非旧版协议被
 * ProtocolType::Raw 误判为原始接收模式。
 */
class MainWindowProtocolState
{
public:
    /**
     * @brief 构造默认 Raw 协议状态。
     */
    MainWindowProtocolState();

    /**
     * @brief 按稳定协议 ID 切换当前协议。
     * @param protocolId 稳定协议 ID，例如 raw、plot.text、lua.script。
     * @param config 待应用配置；会通过协议 Schema 校验并补齐默认值。
     *
     * 主要流程：校验协议 ID，未知 ID 回退 raw；按 descriptor 规范化配置；
     * 可创建协议通过注册中心创建 IProtocol 实例；最后同步旧版 ProtocolType。
     */
    void switchById(const QString& protocolId,
                    const QVariantMap& config = QVariantMap());

    /**
     * @brief 按旧版绘图/基础枚举切换当前协议。
     * @param type 旧版 ProtocolType。
     * @param config 待应用配置；通常用于旧菜单或旧会话迁移。
     */
    void switchByLegacyType(ProtocolType type,
                            const QVariantMap& config = QVariantMap());

    /**
     * @brief 对当前非绘图协议执行通用帧解析。
     * @param data 本次接收到的原始数据块。
     * @return 当前协议返回的帧解析结果；Raw 或绘图协议返回默认结果。
     *
     * 绘图协议由 MainWindowPlotDataRouter 处理，本函数只服务 Lua/后续插件
     * 类接收协议。错误会写入 recentProtocolError() 和 diagnosticsContext()。
     */
    FrameResult parseNonPlotData(const QByteArray& data);

    /**
     * @brief 当前稳定协议 ID。
     * @return 已校验的协议 ID；未知协议会显示为 raw。
     */
    QString protocolId() const { return m_protocolId; }

    /**
     * @brief 当前旧版协议枚举。
     * @return 当前 descriptor 的 legacyType，lua.script 固定为 Raw。
     */
    ProtocolType protocolType() const { return m_protocolType; }

    /**
     * @brief 当前协议描述。
     * @return 已注册协议的 descriptor。
     */
    ProtocolDescriptor descriptor() const { return m_descriptor; }

    /**
     * @brief 当前协议配置。
     * @return Schema 规范化后的配置。
     */
    QVariantMap config() const { return m_config; }

    /**
     * @brief 当前协议实例。
     * @return Raw 返回 nullptr，其余可创建协议返回 IProtocol 指针。
     */
    IProtocol* protocol() const { return m_protocol.get(); }

    /**
     * @brief 最近一次协议运行错误。
     * @return 错误文本；为空表示没有当前需要展示的运行错误。
     */
    QString recentProtocolError() const { return m_recentProtocolError; }

    /**
     * @brief 构造协议诊断运行期上下文。
     * @return 包含最近错误的诊断上下文。
     */
    ProtocolDiagnosticsContext diagnosticsContext() const;

    /**
     * @brief 当前协议是否是真正的 Raw 接收模式。
     * @return 只有稳定协议 ID 为 raw 时返回 true。
     */
    bool isRawProtocol() const { return m_protocolId == QStringLiteral("raw"); }

    /**
     * @brief 返回主窗口“接收协议”菜单可选协议。
     * @return 按注册顺序排列的协议描述，包含 Raw、旧版绘图协议和 Lua 等可创建协议。
     *
     * 该列表用于稳定协议 ID 驱动的用户选择入口。不可创建且非 Raw 的协议
     * 不应暴露给用户，避免菜单项切换后没有实际接收解析器。
     */
    static QList<ProtocolDescriptor> receiveProtocolChoices();

private:
    /**
     * @brief 通过 Schema 规范化配置。
     * @param descriptor 当前协议描述。
     * @param config 外部传入配置。
     * @return 校验通过时返回规范化配置，否则返回 descriptor 默认配置。
     */
    static QVariantMap normalizedConfigFor(const ProtocolDescriptor& descriptor,
                                           const QVariantMap& config);

    QString m_protocolId = QStringLiteral("raw"); ///< 当前稳定协议 ID，保存/恢复/诊断事实源。
    ProtocolType m_protocolType = ProtocolType::Raw; ///< 旧版绘图菜单使用的协议枚举。
    ProtocolDescriptor m_descriptor; ///< 当前协议 descriptor，来自注册中心。
    QVariantMap m_config; ///< 当前协议的 Schema 规范化配置。
    std::unique_ptr<IProtocol> m_protocol; ///< 当前协议实例，Raw 为空。
    QString m_recentProtocolError; ///< 最近协议运行错误，用于状态栏和诊断导出。
};

} // namespace ComAssistant

#endif // COMASSISTANT_MAINWINDOWPROTOCOLSTATE_H
