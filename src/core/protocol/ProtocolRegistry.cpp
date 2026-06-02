/**
 * @file ProtocolRegistry.cpp
 * @brief 协议注册中心实现
 */

#include "ProtocolRegistry.h"

#include "AsciiProtocol.h"
#include "CsvProtocol.h"
#include "CustomProtocol.h"
#include "EasyHexProtocol.h"
#include "HexProtocol.h"
#include "JustFloatProtocol.h"
#include "ModbusProtocol.h"
#include "StampProtocol.h"
#include "TextProtocol.h"

namespace ComAssistant {

namespace {

/**
 * @brief 构造协议描述
 * @param id 稳定协议 ID
 * @param displayName 用户可见协议名称
 * @param description 协议说明
 * @param category 协议分类
 * @param legacyType 旧版协议枚举映射
 * @param plotProtocol 是否为绘图协议
 * @param frameBuilder 是否支持构帧
 * @return 填充好的协议描述
 *
 * 内置协议注册时有大量重复字段，通过该辅助函数集中初始化，
 * 后续增加默认配置或能力标志时不需要在每个注册点重复样板代码。
 */
ProtocolDescriptor makeDescriptor(const QString& id,
                                  const QString& displayName,
                                  const QString& description,
                                  ProtocolCategory category,
                                  ProtocolType legacyType,
                                  bool plotProtocol,
                                  bool frameBuilder)
{
    ProtocolDescriptor descriptor;
    descriptor.id = id;
    descriptor.displayName = displayName;
    descriptor.description = description;
    descriptor.category = category;
    descriptor.legacyType = legacyType;
    descriptor.builtin = true;
    descriptor.plotProtocol = plotProtocol;
    descriptor.frameBuilder = frameBuilder;
    return descriptor;
}

} // namespace

bool ProtocolRegistry::registerProtocol(const ProtocolDescriptor& descriptor,
                                        ProtocolCreator creator,
                                        QString* errorMessage)
{
    /*
     * Task 2 的最小实现先支持合法内置协议注册。
     * 更严格的空 ID、重复 ID、空创建器错误信息会在后续红测试中继续收紧。
     */
    if (descriptor.id.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("协议 ID 不能为空");
        }
        return false;
    }

    if (m_descriptors.contains(descriptor.id)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("协议 ID 已注册: %1").arg(descriptor.id);
        }
        return false;
    }

    if (!creator && descriptor.legacyType != ProtocolType::Raw) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("协议创建器不能为空: %1").arg(descriptor.id);
        }
        return false;
    }

    m_orderedIds.append(descriptor.id);
    m_descriptors.insert(descriptor.id, descriptor);
    m_creators.insert(descriptor.id, creator);
    return true;
}

void ProtocolRegistry::registerBuiltinProtocols()
{
    /*
     * 内置协议按旧版 supportedTypes() 顺序注册，保证能力列表和旧 UI 顺序稳定。
     * Raw 代表无协议，因此登记描述但不提供创建器。
     */
    if (!m_orderedIds.isEmpty()) {
        return;
    }

    registerProtocol(
        makeDescriptor(QStringLiteral("raw"),
                       QStringLiteral("Raw"),
                       QStringLiteral("原始数据（无协议）"),
                       ProtocolCategory::Basic,
                       ProtocolType::Raw,
                       false,
                       false),
        ProtocolCreator());

    registerProtocol(
        makeDescriptor(QStringLiteral("ascii"),
                       QStringLiteral("ASCII"),
                       QStringLiteral("ASCII文本协议"),
                       ProtocolCategory::Basic,
                       ProtocolType::Ascii,
                       false,
                       true),
        [](QObject* parent) -> IProtocol* { return new AsciiProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("hex"),
                       QStringLiteral("HEX"),
                       QStringLiteral("十六进制协议"),
                       ProtocolCategory::Basic,
                       ProtocolType::Hex,
                       false,
                       true),
        [](QObject* parent) -> IProtocol* { return new HexProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("modbus"),
                       QStringLiteral("Modbus"),
                       QStringLiteral("Modbus RTU/ASCII工业通信协议"),
                       ProtocolCategory::Industrial,
                       ProtocolType::Modbus,
                       false,
                       true),
        [](QObject* parent) -> IProtocol* { return new ModbusProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("custom"),
                       QStringLiteral("Custom"),
                       QStringLiteral("自定义协议"),
                       ProtocolCategory::Custom,
                       ProtocolType::Custom,
                       false,
                       true),
        [](QObject* parent) -> IProtocol* { return new CustomProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("easyhex"),
                       QStringLiteral("EasyHEX"),
                       QStringLiteral("简易十六进制协议"),
                       ProtocolCategory::Custom,
                       ProtocolType::EasyHex,
                       false,
                       true),
        [](QObject* parent) -> IProtocol* { return new EasyHexProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.text"),
                       QStringLiteral("TEXT绘图"),
                       QStringLiteral("TEXT绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::TextPlot,
                       true,
                       true),
        [](QObject* parent) -> IProtocol* { return new TextProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.stamp"),
                       QStringLiteral("STAMP绘图"),
                       QStringLiteral("STAMP绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::StampPlot,
                       true,
                       true),
        [](QObject* parent) -> IProtocol* { return new StampProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.csv"),
                       QStringLiteral("CSV绘图"),
                       QStringLiteral("CSV绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::CsvPlot,
                       true,
                       true),
        [](QObject* parent) -> IProtocol* { return new CsvProtocol(parent); });

    registerProtocol(
        makeDescriptor(QStringLiteral("plot.justfloat"),
                       QStringLiteral("JustFloat"),
                       QStringLiteral("JustFloat二进制绘图协议"),
                       ProtocolCategory::Plot,
                       ProtocolType::JustFloat,
                       true,
                       true),
        [](QObject* parent) -> IProtocol* { return new JustFloatProtocol(parent); });
}

bool ProtocolRegistry::contains(const QString& id) const
{
    return m_descriptors.contains(id);
}

ProtocolDescriptor ProtocolRegistry::descriptor(const QString& id) const
{
    return m_descriptors.value(id);
}

QList<ProtocolDescriptor> ProtocolRegistry::descriptors() const
{
    QList<ProtocolDescriptor> result;
    result.reserve(m_orderedIds.size());

    for (const QString& id : m_orderedIds) {
        result.append(m_descriptors.value(id));
    }

    return result;
}

IProtocol* ProtocolRegistry::create(const QString& id, QObject* parent) const
{
    const ProtocolCreator creator = m_creators.value(id);
    if (!creator) {
        return nullptr;
    }

    return creator(parent);
}

} // namespace ComAssistant
