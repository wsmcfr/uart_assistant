/**
 * @file ProtocolFactory.cpp
 * @brief 协议工厂实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "ProtocolFactory.h"

namespace ComAssistant {

namespace {

/**
 * @brief 获取已初始化的共享协议注册中心
 * @return 已注册内置协议的注册中心
 *
 * 使用函数内静态对象可以保证按需初始化，避免应用启动时要求显式调用初始化。
 */
ProtocolRegistry& sharedRegistry()
{
    static ProtocolRegistry registry;
    registry.registerBuiltinProtocols();
    return registry;
}

} // namespace

//=============================================================================
// 智能指针版本
//=============================================================================

std::unique_ptr<AsciiProtocol> ProtocolFactory::createAscii()
{
    return std::make_unique<AsciiProtocol>();
}

std::unique_ptr<HexProtocol> ProtocolFactory::createHex()
{
    return std::make_unique<HexProtocol>();
}

std::unique_ptr<ModbusProtocol> ProtocolFactory::createModbus(ModbusMode mode)
{
    auto protocol = std::make_unique<ModbusProtocol>();
    protocol->setMode(mode);
    return protocol;
}

std::unique_ptr<CustomProtocol> ProtocolFactory::createCustom()
{
    return std::make_unique<CustomProtocol>();
}

std::unique_ptr<TextProtocol> ProtocolFactory::createTextPlot()
{
    return std::make_unique<TextProtocol>();
}

std::unique_ptr<StampProtocol> ProtocolFactory::createStampPlot()
{
    return std::make_unique<StampProtocol>();
}

std::unique_ptr<CsvProtocol> ProtocolFactory::createCsvPlot()
{
    return std::make_unique<CsvProtocol>();
}

std::unique_ptr<EasyHexProtocol> ProtocolFactory::createEasyHex()
{
    return std::make_unique<EasyHexProtocol>();
}

std::unique_ptr<JustFloatProtocol> ProtocolFactory::createJustFloat()
{
    return std::make_unique<JustFloatProtocol>();
}

std::unique_ptr<IProtocol> ProtocolFactory::create(ProtocolType type)
{
    /*
     * 通用创建入口委托给注册中心，使旧 API 与平台化能力目录使用同一个事实源。
     * 具体 createAscii/createHex 等函数仍保留，避免破坏依赖具体返回类型的调用方。
     */
    return std::unique_ptr<IProtocol>(create(type, nullptr));
}

std::unique_ptr<IProtocol> ProtocolFactory::create(ProtocolType type, const QVariantMap& config)
{
    /*
     * 智能指针重载不拥有 Qt 父对象，因此直接复用裸指针重载并交给 unique_ptr 管理。
     * 这样两种创建方式会走同一套 Schema 校验、默认值回退和协议配置应用流程。
     */
    return std::unique_ptr<IProtocol>(create(type, config, nullptr));
}

//=============================================================================
// Qt父子对象管理版本
//=============================================================================

AsciiProtocol* ProtocolFactory::createAscii(QObject* parent)
{
    return new AsciiProtocol(parent);
}

HexProtocol* ProtocolFactory::createHex(QObject* parent)
{
    return new HexProtocol(parent);
}

ModbusProtocol* ProtocolFactory::createModbus(ModbusMode mode, QObject* parent)
{
    auto protocol = new ModbusProtocol(parent);
    protocol->setMode(mode);
    return protocol;
}

CustomProtocol* ProtocolFactory::createCustom(QObject* parent)
{
    return new CustomProtocol(parent);
}

TextProtocol* ProtocolFactory::createTextPlot(QObject* parent)
{
    return new TextProtocol(parent);
}

StampProtocol* ProtocolFactory::createStampPlot(QObject* parent)
{
    return new StampProtocol(parent);
}

CsvProtocol* ProtocolFactory::createCsvPlot(QObject* parent)
{
    return new CsvProtocol(parent);
}

EasyHexProtocol* ProtocolFactory::createEasyHex(QObject* parent)
{
    return new EasyHexProtocol(parent);
}

JustFloatProtocol* ProtocolFactory::createJustFloat(QObject* parent)
{
    return new JustFloatProtocol(parent);
}

IProtocol* ProtocolFactory::create(ProtocolType type, QObject* parent)
{
    return sharedRegistry().create(typeId(type), parent);
}

IProtocol* ProtocolFactory::create(ProtocolType type, const QVariantMap& config, QObject* parent)
{
    /*
     * 带配置创建入口以协议描述为事实源：
     * 1. 根据旧版枚举找到稳定协议 ID 和描述；
     * 2. 通过 Schema 校验并规范化调用方配置；
     * 3. 校验失败时回退到描述中的默认配置；
     * 4. 创建真实协议实例后调用虚函数 setConfig(QVariantMap) 应用配置。
     *
     * 这样会话恢复、后续配置 UI 和脚本入口即使传入坏配置，也不会把非法状态
     * 直接塞进协议实现。
     */
    const QString id = typeId(type);
    if (id.isEmpty()) {
        return nullptr;
    }

    const ProtocolDescriptor protocolDescriptor = sharedRegistry().descriptor(id);
    if (protocolDescriptor.id.isEmpty()) {
        return nullptr;
    }

    ProtocolConfigValidationResult validation =
        protocolDescriptor.configSchema.validate(config);
    QVariantMap effectiveConfig = validation.valid
        ? validation.normalizedConfig
        : protocolDescriptor.defaultConfig;

    IProtocol* protocol = sharedRegistry().create(id, parent);
    if (!protocol) {
        return nullptr;
    }

    protocol->setConfig(effectiveConfig);
    return protocol;
}

//=============================================================================
// 工具方法
//=============================================================================

QString ProtocolFactory::typeName(ProtocolType type)
{
    const ProtocolDescriptor protocolDescriptor = descriptor(type);
    if (!protocolDescriptor.displayName.isEmpty()) {
        return protocolDescriptor.displayName;
    }

    return QStringLiteral("Unknown");
}

QString ProtocolFactory::typeId(ProtocolType type)
{
    /*
     * ID 映射通过注册中心反查旧版枚举，确保新增能力查询与 supportedTypes()
     * 使用同一份注册顺序和描述数据。
     */
    const QList<ProtocolDescriptor> protocolDescriptors = sharedRegistry().descriptors();
    for (const ProtocolDescriptor& protocolDescriptor : protocolDescriptors) {
        if (!protocolDescriptor.legacyCompatible) {
            continue;
        }

        if (protocolDescriptor.legacyType == type) {
            return protocolDescriptor.id;
        }
    }

    return QString();
}

ProtocolDescriptor ProtocolFactory::descriptor(ProtocolType type)
{
    const QString id = typeId(type);
    if (id.isEmpty()) {
        return ProtocolDescriptor();
    }

    return sharedRegistry().descriptor(id);
}

const ProtocolRegistry& ProtocolFactory::registry()
{
    return sharedRegistry();
}

QList<ProtocolType> ProtocolFactory::supportedTypes()
{
    QList<ProtocolType> types;
    const QList<ProtocolDescriptor> protocolDescriptors = sharedRegistry().descriptors();
    types.reserve(protocolDescriptors.size());

    for (const ProtocolDescriptor& protocolDescriptor : protocolDescriptors) {
        if (!protocolDescriptor.legacyCompatible) {
            continue;
        }

        types.append(protocolDescriptor.legacyType);
    }

    return types;
}

QMap<QString, ProtocolCreator>& ProtocolFactory::registeredProtocols()
{
    static QMap<QString, ProtocolCreator> protocols;
    return protocols;
}

void ProtocolFactory::registerProtocol(const QString& name, ProtocolCreator creator)
{
    registeredProtocols()[name] = creator;
}

IProtocol* ProtocolFactory::createRegistered(const QString& name, QObject* parent)
{
    auto& protocols = registeredProtocols();
    if (protocols.contains(name)) {
        return protocols[name](parent);
    }
    return nullptr;
}

} // namespace ComAssistant
