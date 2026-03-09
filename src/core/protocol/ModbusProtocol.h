/**
 * @file ModbusProtocol.h
 * @brief Modbus协议实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_MODBUSPROTOCOL_H
#define COMASSISTANT_MODBUSPROTOCOL_H

#include "IProtocol.h"
#include <QQueue>

namespace ComAssistant {

/**
 * @brief Modbus功能码
 */
enum class ModbusFunction : quint8 {
    ReadCoils               = 0x01,  ///< 读线圈
    ReadDiscreteInputs      = 0x02,  ///< 读离散输入
    ReadHoldingRegisters    = 0x03,  ///< 读保持寄存器
    ReadInputRegisters      = 0x04,  ///< 读输入寄存器
    WriteSingleCoil         = 0x05,  ///< 写单个线圈
    WriteSingleRegister     = 0x06,  ///< 写单个寄存器
    WriteMultipleCoils      = 0x0F,  ///< 写多个线圈
    WriteMultipleRegisters  = 0x10,  ///< 写多个寄存器
    ReadWriteRegisters      = 0x17   ///< 读写多个寄存器
};

/**
 * @brief Modbus模式
 */
enum class ModbusMode {
    RTU,    ///< RTU模式（二进制）
    ASCII   ///< ASCII模式（文本）
};

/**
 * @brief Modbus请求结构
 */
struct ModbusRequest {
    quint8 slaveAddress = 1;           ///< 从站地址
    ModbusFunction function;            ///< 功能码
    quint16 startAddress = 0;           ///< 起始地址
    quint16 quantity = 1;               ///< 数量
    QByteArray data;                    ///< 写入数据
    int transactionId = 0;              ///< 事务ID（用于匹配响应）
};

/**
 * @brief Modbus响应结构
 */
struct ModbusResponse {
    bool valid = false;                 ///< 是否有效
    quint8 slaveAddress = 0;            ///< 从站地址
    ModbusFunction function;            ///< 功能码
    QByteArray data;                    ///< 响应数据
    quint8 exceptionCode = 0;           ///< 异常码（如果有）
    QString errorMessage;               ///< 错误信息
    int transactionId = 0;              ///< 事务ID
};

/**
 * @brief Modbus协议实现
 *
 * 支持RTU和ASCII模式，包含完整的主站功能
 */
class ModbusProtocol : public IProtocol {
    Q_OBJECT

public:
    explicit ModbusProtocol(QObject* parent = nullptr);
    ~ModbusProtocol() override = default;

    //=========================================================================
    // IProtocol 接口实现
    //=========================================================================

    ProtocolType type() const override { return ProtocolType::Modbus; }
    QString name() const override { return QStringLiteral("Modbus"); }
    QString description() const override { return tr("Modbus RTU/ASCII protocol"); }

    FrameResult parse(const QByteArray& data) override;
    QByteArray build(const QByteArray& payload, const QVariantMap& metadata) override;
    bool validate(const QByteArray& frame) override;
    QByteArray calculateChecksum(const QByteArray& data) override;

    void reset() override;

    //=========================================================================
    // Modbus特有方法
    //=========================================================================

    /**
     * @brief 设置Modbus模式
     */
    void setMode(ModbusMode mode);
    ModbusMode mode() const { return m_mode; }

    /**
     * @brief 设置从站地址
     */
    void setSlaveAddress(quint8 address);
    quint8 slaveAddress() const { return m_slaveAddress; }

    /**
     * @brief 设置响应超时时间
     */
    void setResponseTimeout(int ms);
    int responseTimeout() const { return m_responseTimeout; }

    //=========================================================================
    // 主站请求构建
    //=========================================================================

    /**
     * @brief 读线圈 (0x01)
     */
    QByteArray buildReadCoils(quint8 slave, quint16 startAddr, quint16 quantity);

    /**
     * @brief 读离散输入 (0x02)
     */
    QByteArray buildReadDiscreteInputs(quint8 slave, quint16 startAddr, quint16 quantity);

    /**
     * @brief 读保持寄存器 (0x03)
     */
    QByteArray buildReadHoldingRegisters(quint8 slave, quint16 startAddr, quint16 quantity);

    /**
     * @brief 读输入寄存器 (0x04)
     */
    QByteArray buildReadInputRegisters(quint8 slave, quint16 startAddr, quint16 quantity);

    /**
     * @brief 写单个线圈 (0x05)
     */
    QByteArray buildWriteSingleCoil(quint8 slave, quint16 address, bool value);

    /**
     * @brief 写单个寄存器 (0x06)
     */
    QByteArray buildWriteSingleRegister(quint8 slave, quint16 address, quint16 value);

    /**
     * @brief 写多个线圈 (0x0F)
     */
    QByteArray buildWriteMultipleCoils(quint8 slave, quint16 startAddr, const QVector<bool>& values);

    /**
     * @brief 写多个寄存器 (0x10)
     */
    QByteArray buildWriteMultipleRegisters(quint8 slave, quint16 startAddr, const QVector<quint16>& values);

    //=========================================================================
    // 响应解析
    //=========================================================================

    /**
     * @brief 解析Modbus响应
     */
    ModbusResponse parseResponse(const QByteArray& frame);

    /**
     * @brief 获取异常码描述
     */
    static QString exceptionDescription(quint8 code);

    /**
     * @brief 获取功能码名称
     */
    static QString functionName(ModbusFunction func);

signals:
    /**
     * @brief 收到Modbus响应
     */
    void responseReceived(const ModbusResponse& response);

    /**
     * @brief 响应超时
     */
    void responseTimeout(const ModbusRequest& request);

private:
    // RTU模式帧构建
    QByteArray buildRtuFrame(const QByteArray& pdu, quint8 slave);
    int findRtuFrame();

    // ASCII模式帧构建
    QByteArray buildAsciiFrame(const QByteArray& pdu, quint8 slave);
    int findAsciiFrame();

    // CRC计算
    static quint16 calculateCRC16(const QByteArray& data);

    // LRC计算（ASCII模式）
    static quint8 calculateLRC(const QByteArray& data);

private:
    ModbusMode m_mode = ModbusMode::RTU;
    quint8 m_slaveAddress = 1;
    int m_responseTimeout = 1000;

    // 请求队列（用于匹配响应）
    QQueue<ModbusRequest> m_pendingRequests;
    int m_transactionCounter = 0;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MODBUSPROTOCOL_H
