/**
 * @file ProtocolFactory.cpp
 * @brief 协议工厂实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "ProtocolFactory.h"

namespace ComAssistant {

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
    switch (type) {
        case ProtocolType::Ascii:
            return createAscii();
        case ProtocolType::Hex:
            return createHex();
        case ProtocolType::Modbus:
            return createModbus();
        case ProtocolType::Custom:
            return createCustom();
        case ProtocolType::EasyHex:
            return createEasyHex();
        case ProtocolType::TextPlot:
            return createTextPlot();
        case ProtocolType::StampPlot:
            return createStampPlot();
        case ProtocolType::CsvPlot:
            return createCsvPlot();
        case ProtocolType::JustFloat:
            return createJustFloat();
        case ProtocolType::Raw:
        default:
            return nullptr;
    }
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
    switch (type) {
        case ProtocolType::Ascii:
            return createAscii(parent);
        case ProtocolType::Hex:
            return createHex(parent);
        case ProtocolType::Modbus:
            return createModbus(ModbusMode::RTU, parent);
        case ProtocolType::Custom:
            return createCustom(parent);
        case ProtocolType::EasyHex:
            return createEasyHex(parent);
        case ProtocolType::TextPlot:
            return createTextPlot(parent);
        case ProtocolType::StampPlot:
            return createStampPlot(parent);
        case ProtocolType::CsvPlot:
            return createCsvPlot(parent);
        case ProtocolType::JustFloat:
            return createJustFloat(parent);
        case ProtocolType::Raw:
        default:
            return nullptr;
    }
}

//=============================================================================
// 工具方法
//=============================================================================

QString ProtocolFactory::typeName(ProtocolType type)
{
    switch (type) {
        case ProtocolType::Raw:       return QStringLiteral("Raw");
        case ProtocolType::Ascii:     return QStringLiteral("ASCII");
        case ProtocolType::Hex:       return QStringLiteral("HEX");
        case ProtocolType::Modbus:    return QStringLiteral("Modbus");
        case ProtocolType::Custom:    return QStringLiteral("Custom");
        case ProtocolType::EasyHex:   return QStringLiteral("EasyHEX");
        case ProtocolType::TextPlot:  return QStringLiteral("TEXT绘图");
        case ProtocolType::StampPlot: return QStringLiteral("STAMP绘图");
        case ProtocolType::CsvPlot:   return QStringLiteral("CSV绘图");
        case ProtocolType::JustFloat: return QStringLiteral("JustFloat");
        default:                      return QStringLiteral("Unknown");
    }
}

QList<ProtocolType> ProtocolFactory::supportedTypes()
{
    return {
        ProtocolType::Raw,
        ProtocolType::Ascii,
        ProtocolType::Hex,
        ProtocolType::Modbus,
        ProtocolType::Custom,
        ProtocolType::EasyHex,
        ProtocolType::TextPlot,
        ProtocolType::StampPlot,
        ProtocolType::CsvPlot,
        ProtocolType::JustFloat
    };
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
