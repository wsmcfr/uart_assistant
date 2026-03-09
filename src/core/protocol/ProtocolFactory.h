/**
 * @file ProtocolFactory.h
 * @brief 协议工厂
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_PROTOCOLFACTORY_H
#define COMASSISTANT_PROTOCOLFACTORY_H

#include "IProtocol.h"
#include "AsciiProtocol.h"
#include "HexProtocol.h"
#include "ModbusProtocol.h"
#include "CustomProtocol.h"
#include "TextProtocol.h"
#include "StampProtocol.h"
#include "CsvProtocol.h"
#include "EasyHexProtocol.h"
#include "JustFloatProtocol.h"
#include <memory>
#include <QMap>

namespace ComAssistant {

/**
 * @brief 协议工厂类
 */
class ProtocolFactory {
public:
    //=========================================================================
    // 智能指针版本
    //=========================================================================

    /**
     * @brief 创建ASCII协议
     */
    static std::unique_ptr<AsciiProtocol> createAscii();

    /**
     * @brief 创建HEX协议
     */
    static std::unique_ptr<HexProtocol> createHex();

    /**
     * @brief 创建Modbus协议
     */
    static std::unique_ptr<ModbusProtocol> createModbus(ModbusMode mode = ModbusMode::RTU);

    /**
     * @brief 创建自定义协议
     */
    static std::unique_ptr<CustomProtocol> createCustom();

    /**
     * @brief 创建TEXT绘图协议
     */
    static std::unique_ptr<TextProtocol> createTextPlot();

    /**
     * @brief 创建STAMP绘图协议
     */
    static std::unique_ptr<StampProtocol> createStampPlot();

    /**
     * @brief 创建CSV绘图协议
     */
    static std::unique_ptr<CsvProtocol> createCsvPlot();

    /**
     * @brief 创建EasyHEX协议
     */
    static std::unique_ptr<EasyHexProtocol> createEasyHex();

    /**
     * @brief 创建JustFloat协议
     */
    static std::unique_ptr<JustFloatProtocol> createJustFloat();

    /**
     * @brief 根据类型创建协议
     */
    static std::unique_ptr<IProtocol> create(ProtocolType type);

    //=========================================================================
    // Qt父子对象管理版本
    //=========================================================================

    static AsciiProtocol* createAscii(QObject* parent);
    static HexProtocol* createHex(QObject* parent);
    static ModbusProtocol* createModbus(ModbusMode mode, QObject* parent);
    static CustomProtocol* createCustom(QObject* parent);
    static TextProtocol* createTextPlot(QObject* parent);
    static StampProtocol* createStampPlot(QObject* parent);
    static CsvProtocol* createCsvPlot(QObject* parent);
    static EasyHexProtocol* createEasyHex(QObject* parent);
    static JustFloatProtocol* createJustFloat(QObject* parent);
    static IProtocol* create(ProtocolType type, QObject* parent);

    //=========================================================================
    // 工具方法
    //=========================================================================

    /**
     * @brief 获取协议类型名称
     */
    static QString typeName(ProtocolType type);

    /**
     * @brief 获取支持的协议类型列表
     */
    static QList<ProtocolType> supportedTypes();

    /**
     * @brief 注册自定义协议创建器
     */
    static void registerProtocol(const QString& name, ProtocolCreator creator);

    /**
     * @brief 创建已注册的自定义协议
     */
    static IProtocol* createRegistered(const QString& name, QObject* parent = nullptr);

private:
    ProtocolFactory() = delete;

    static QMap<QString, ProtocolCreator>& registeredProtocols();
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLFACTORY_H
