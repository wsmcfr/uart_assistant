/**
 * @file ModbusProtocol.cpp
 * @brief Modbus协议实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "ModbusProtocol.h"
#include "utils/Logger.h"

namespace ComAssistant {

// CRC16查找表（Modbus多项式0x8005，反转）
static const quint16 CRC16_TABLE[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

ModbusProtocol::ModbusProtocol(QObject* parent)
    : IProtocol(parent)
{
}

void ModbusProtocol::setMode(ModbusMode mode)
{
    m_mode = mode;
}

void ModbusProtocol::setSlaveAddress(quint8 address)
{
    m_slaveAddress = address;
}

void ModbusProtocol::setResponseTimeout(int ms)
{
    m_responseTimeout = ms;
}

FrameResult ModbusProtocol::parse(const QByteArray& data)
{
    FrameResult result;

    if (data.isEmpty()) {
        return result;
    }

    m_buffer.append(data);

    int frameLen = (m_mode == ModbusMode::RTU) ? findRtuFrame() : findAsciiFrame();

    if (frameLen > 0) {
        result.frame = m_buffer.left(frameLen);
        result.valid = validate(result.frame);
        result.consumedBytes = frameLen;

        if (result.valid) {
            // 解析响应
            ModbusResponse response = parseResponse(result.frame);
            result.payload = response.data;
            result.metadata["slaveAddress"] = response.slaveAddress;
            result.metadata["function"] = static_cast<int>(response.function);

            if (response.exceptionCode > 0) {
                result.metadata["exception"] = response.exceptionCode;
                result.errorMessage = response.errorMessage;
            }

            emit responseReceived(response);
        }

        m_buffer.remove(0, frameLen);
        emit frameReceived(result);
    } else if (frameLen == -1) {
        m_buffer.remove(0, 1);
        result.consumedBytes = 1;
    }

    return result;
}

QByteArray ModbusProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    quint8 slave = metadata.value("slaveAddress", m_slaveAddress).toUInt();

    if (m_mode == ModbusMode::RTU) {
        return buildRtuFrame(payload, slave);
    } else {
        return buildAsciiFrame(payload, slave);
    }
}

bool ModbusProtocol::validate(const QByteArray& frame)
{
    if (m_mode == ModbusMode::RTU) {
        if (frame.size() < 4) {
            return false;
        }

        // 验证CRC
        QByteArray dataWithoutCrc = frame.left(frame.size() - 2);
        quint16 expectedCrc = calculateCRC16(dataWithoutCrc);
        quint16 actualCrc = static_cast<quint8>(frame[frame.size() - 2]) |
                           (static_cast<quint8>(frame[frame.size() - 1]) << 8);

        return expectedCrc == actualCrc;
    } else {
        // ASCII模式验证
        if (frame.size() < 9 || frame[0] != ':') {
            return false;
        }
        // 验证LRC
        // ... 省略ASCII模式验证
        return true;
    }
}

QByteArray ModbusProtocol::calculateChecksum(const QByteArray& data)
{
    quint16 crc = calculateCRC16(data);
    QByteArray result(2, 0);
    result[0] = static_cast<char>(crc & 0xFF);
    result[1] = static_cast<char>((crc >> 8) & 0xFF);
    return result;
}

void ModbusProtocol::reset()
{
    m_buffer.clear();
    m_pendingRequests.clear();
}

//=============================================================================
// 请求构建
//=============================================================================

QByteArray ModbusProtocol::buildReadCoils(quint8 slave, quint16 startAddr, quint16 quantity)
{
    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::ReadCoils));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    return buildRtuFrame(pdu, slave);
}

QByteArray ModbusProtocol::buildReadDiscreteInputs(quint8 slave, quint16 startAddr, quint16 quantity)
{
    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::ReadDiscreteInputs));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    return buildRtuFrame(pdu, slave);
}

QByteArray ModbusProtocol::buildReadHoldingRegisters(quint8 slave, quint16 startAddr, quint16 quantity)
{
    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::ReadHoldingRegisters));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    return buildRtuFrame(pdu, slave);
}

QByteArray ModbusProtocol::buildReadInputRegisters(quint8 slave, quint16 startAddr, quint16 quantity)
{
    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::ReadInputRegisters));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((quantity >> 8) & 0xFF));
    pdu.append(static_cast<char>(quantity & 0xFF));

    return buildRtuFrame(pdu, slave);
}

QByteArray ModbusProtocol::buildWriteSingleCoil(quint8 slave, quint16 address, bool value)
{
    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::WriteSingleCoil));
    pdu.append(static_cast<char>((address >> 8) & 0xFF));
    pdu.append(static_cast<char>(address & 0xFF));
    pdu.append(value ? static_cast<char>(0xFF) : static_cast<char>(0x00));
    pdu.append(static_cast<char>(0x00));

    return buildRtuFrame(pdu, slave);
}

QByteArray ModbusProtocol::buildWriteSingleRegister(quint8 slave, quint16 address, quint16 value)
{
    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::WriteSingleRegister));
    pdu.append(static_cast<char>((address >> 8) & 0xFF));
    pdu.append(static_cast<char>(address & 0xFF));
    pdu.append(static_cast<char>((value >> 8) & 0xFF));
    pdu.append(static_cast<char>(value & 0xFF));

    return buildRtuFrame(pdu, slave);
}

QByteArray ModbusProtocol::buildWriteMultipleCoils(quint8 slave, quint16 startAddr, const QVector<bool>& values)
{
    int byteCount = (values.size() + 7) / 8;

    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::WriteMultipleCoils));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((values.size() >> 8) & 0xFF));
    pdu.append(static_cast<char>(values.size() & 0xFF));
    pdu.append(static_cast<char>(byteCount));

    // 打包线圈值
    for (int i = 0; i < byteCount; ++i) {
        quint8 byte = 0;
        for (int j = 0; j < 8 && (i * 8 + j) < values.size(); ++j) {
            if (values[i * 8 + j]) {
                byte |= (1 << j);
            }
        }
        pdu.append(static_cast<char>(byte));
    }

    return buildRtuFrame(pdu, slave);
}

QByteArray ModbusProtocol::buildWriteMultipleRegisters(quint8 slave, quint16 startAddr, const QVector<quint16>& values)
{
    QByteArray pdu;
    pdu.append(static_cast<char>(ModbusFunction::WriteMultipleRegisters));
    pdu.append(static_cast<char>((startAddr >> 8) & 0xFF));
    pdu.append(static_cast<char>(startAddr & 0xFF));
    pdu.append(static_cast<char>((values.size() >> 8) & 0xFF));
    pdu.append(static_cast<char>(values.size() & 0xFF));
    pdu.append(static_cast<char>(values.size() * 2));

    for (quint16 value : values) {
        pdu.append(static_cast<char>((value >> 8) & 0xFF));
        pdu.append(static_cast<char>(value & 0xFF));
    }

    return buildRtuFrame(pdu, slave);
}

//=============================================================================
// 响应解析
//=============================================================================

ModbusResponse ModbusProtocol::parseResponse(const QByteArray& frame)
{
    ModbusResponse response;

    if (frame.size() < 4) {
        response.errorMessage = tr("Frame too short");
        return response;
    }

    response.slaveAddress = static_cast<quint8>(frame[0]);
    quint8 funcCode = static_cast<quint8>(frame[1]);

    // 检查是否为异常响应
    if (funcCode & 0x80) {
        response.function = static_cast<ModbusFunction>(funcCode & 0x7F);
        response.exceptionCode = static_cast<quint8>(frame[2]);
        response.errorMessage = exceptionDescription(response.exceptionCode);
        response.valid = true;
        return response;
    }

    response.function = static_cast<ModbusFunction>(funcCode);

    // 提取数据部分（去除地址、功能码和CRC）
    if (frame.size() > 4) {
        response.data = frame.mid(2, frame.size() - 4);
    }

    response.valid = true;
    return response;
}

QString ModbusProtocol::exceptionDescription(quint8 code)
{
    switch (code) {
        case 0x01: return tr("Illegal Function");
        case 0x02: return tr("Illegal Data Address");
        case 0x03: return tr("Illegal Data Value");
        case 0x04: return tr("Slave Device Failure");
        case 0x05: return tr("Acknowledge");
        case 0x06: return tr("Slave Device Busy");
        case 0x08: return tr("Memory Parity Error");
        case 0x0A: return tr("Gateway Path Unavailable");
        case 0x0B: return tr("Gateway Target Device Failed to Respond");
        default:   return tr("Unknown Exception: 0x%1").arg(code, 2, 16, QChar('0'));
    }
}

QString ModbusProtocol::functionName(ModbusFunction func)
{
    switch (func) {
        case ModbusFunction::ReadCoils:              return "Read Coils";
        case ModbusFunction::ReadDiscreteInputs:    return "Read Discrete Inputs";
        case ModbusFunction::ReadHoldingRegisters:  return "Read Holding Registers";
        case ModbusFunction::ReadInputRegisters:    return "Read Input Registers";
        case ModbusFunction::WriteSingleCoil:       return "Write Single Coil";
        case ModbusFunction::WriteSingleRegister:   return "Write Single Register";
        case ModbusFunction::WriteMultipleCoils:    return "Write Multiple Coils";
        case ModbusFunction::WriteMultipleRegisters: return "Write Multiple Registers";
        case ModbusFunction::ReadWriteRegisters:    return "Read/Write Multiple Registers";
        default:                                     return QString("Unknown (0x%1)").arg(static_cast<int>(func), 2, 16, QChar('0'));
    }
}

//=============================================================================
// 内部方法
//=============================================================================

QByteArray ModbusProtocol::buildRtuFrame(const QByteArray& pdu, quint8 slave)
{
    QByteArray frame;
    frame.append(static_cast<char>(slave));
    frame.append(pdu);

    quint16 crc = calculateCRC16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));

    return frame;
}

int ModbusProtocol::findRtuFrame()
{
    // RTU帧最小长度：地址(1) + 功能码(1) + CRC(2) = 4字节
    if (m_buffer.size() < 4) {
        return 0;
    }

    quint8 funcCode = static_cast<quint8>(m_buffer[1]);

    // 异常响应
    if (funcCode & 0x80) {
        if (m_buffer.size() >= 5) {
            return 5;  // 地址 + 功能码 + 异常码 + CRC
        }
        return 0;
    }

    int expectedLen = 0;

    switch (static_cast<ModbusFunction>(funcCode)) {
        case ModbusFunction::ReadCoils:
        case ModbusFunction::ReadDiscreteInputs:
        case ModbusFunction::ReadHoldingRegisters:
        case ModbusFunction::ReadInputRegisters:
            // 响应：地址 + 功能码 + 字节数 + 数据 + CRC
            if (m_buffer.size() >= 3) {
                int byteCount = static_cast<quint8>(m_buffer[2]);
                expectedLen = 3 + byteCount + 2;
            }
            break;

        case ModbusFunction::WriteSingleCoil:
        case ModbusFunction::WriteSingleRegister:
        case ModbusFunction::WriteMultipleCoils:
        case ModbusFunction::WriteMultipleRegisters:
            // 响应固定8字节
            expectedLen = 8;
            break;

        default:
            // 未知功能码，等待更多数据
            return 0;
    }

    if (expectedLen > 0 && m_buffer.size() >= expectedLen) {
        return expectedLen;
    }

    return 0;
}

QByteArray ModbusProtocol::buildAsciiFrame(const QByteArray& pdu, quint8 slave)
{
    QByteArray frame;
    frame.append(static_cast<char>(slave));
    frame.append(pdu);

    quint8 lrc = calculateLRC(frame);

    // 转换为ASCII
    QByteArray ascii;
    ascii.append(':');
    ascii.append(frame.toHex().toUpper());
    ascii.append(QByteArray::number(lrc, 16).rightJustified(2, '0').toUpper());
    ascii.append("\r\n");

    return ascii;
}

int ModbusProtocol::findAsciiFrame()
{
    int startPos = m_buffer.indexOf(':');
    if (startPos == -1) {
        return m_buffer.size() > 0 ? -1 : 0;
    }

    if (startPos > 0) {
        m_buffer.remove(0, startPos);
    }

    int endPos = m_buffer.indexOf("\r\n");
    if (endPos == -1) {
        return 0;
    }

    return endPos + 2;
}

quint16 ModbusProtocol::calculateCRC16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;

    for (char c : data) {
        quint8 index = (crc ^ static_cast<quint8>(c)) & 0xFF;
        crc = (crc >> 8) ^ CRC16_TABLE[index];
    }

    return crc;
}

quint8 ModbusProtocol::calculateLRC(const QByteArray& data)
{
    quint8 lrc = 0;
    for (char c : data) {
        lrc += static_cast<quint8>(c);
    }
    return static_cast<quint8>(-static_cast<qint8>(lrc));
}

} // namespace ComAssistant
