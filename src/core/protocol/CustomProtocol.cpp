/**
 * @file CustomProtocol.cpp
 * @brief 自定义协议实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "CustomProtocol.h"
#include "utils/Logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace ComAssistant {

CustomProtocol::CustomProtocol(QObject* parent)
    : IProtocol(parent)
{
    m_frameParser = new FrameParser(this);
    connect(m_frameParser, &FrameParser::frameReady, this, &CustomProtocol::onFrameReady);
}

CustomProtocol::~CustomProtocol()
{
}

void CustomProtocol::setCustomConfig(const CustomProtocolConfig& config)
{
    m_config = config;
    m_frameParser->setConfig(config.frameConfig);
}

void CustomProtocol::setParseCallback(std::function<FrameResult(const QByteArray&)> parser)
{
    m_parseCallback = parser;
}

void CustomProtocol::setBuildCallback(std::function<QByteArray(const QByteArray&, const QVariantMap&)> builder)
{
    m_buildCallback = builder;
}

void CustomProtocol::setValidateCallback(std::function<bool(const QByteArray&)> validator)
{
    m_validateCallback = validator;
}

void CustomProtocol::setChecksumCallback(std::function<QByteArray(const QByteArray&)> calculator)
{
    m_checksumCallback = calculator;
}

FrameResult CustomProtocol::parse(const QByteArray& data)
{
    FrameResult result;

    if (data.isEmpty()) {
        return result;
    }

    // 如果有自定义解析回调，直接使用
    if (m_parseCallback) {
        m_buffer.append(data);
        result = m_parseCallback(m_buffer);
        if (result.valid && result.consumedBytes > 0) {
            m_buffer.remove(0, result.consumedBytes);
            emit frameReceived(result);
        }
        return result;
    }

    // 使用帧解析器
    m_frameParser->feed(data);

    return result;
}

QByteArray CustomProtocol::build(const QByteArray& payload, const QVariantMap& metadata)
{
    // 如果有自定义构建回调
    if (m_buildCallback) {
        return m_buildCallback(payload, metadata);
    }

    // 默认构建逻辑
    QByteArray frame;

    // 添加帧头
    frame.append(m_config.frameConfig.startFlag);

    // 添加长度字段（如果配置了）
    if (m_config.frameConfig.delimiter == FrameDelimiter::LengthField) {
        int len = payload.size();
        int fieldSize = m_config.frameConfig.lengthFieldSize;
        bool bigEndian = m_config.frameConfig.lengthFieldBigEndian;

        if (bigEndian) {
            for (int i = fieldSize - 1; i >= 0; --i) {
                frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
            }
        } else {
            for (int i = 0; i < fieldSize; ++i) {
                frame.append(static_cast<char>((len >> (i * 8)) & 0xFF));
            }
        }
    }

    // 添加数据
    frame.append(payload);

    // 添加校验和
    if (m_config.useChecksum) {
        QByteArray checksum = calculateChecksum(frame);
        frame.append(checksum);
    }

    // 添加帧尾
    frame.append(m_config.frameConfig.endFlag);

    return frame;
}

bool CustomProtocol::validate(const QByteArray& frame)
{
    // 如果有自定义校验回调
    if (m_validateCallback) {
        return m_validateCallback(frame);
    }

    // 检查帧头帧尾
    if (!m_config.frameConfig.startFlag.isEmpty()) {
        if (!frame.startsWith(m_config.frameConfig.startFlag)) {
            return false;
        }
    }

    if (!m_config.frameConfig.endFlag.isEmpty()) {
        if (!frame.endsWith(m_config.frameConfig.endFlag)) {
            return false;
        }
    }

    // 检查校验和
    if (m_config.useChecksum) {
        int checksumPos = m_config.checksumOffset;
        if (checksumPos < 0) {
            // 校验和在帧尾之前
            int tailLen = m_config.frameConfig.endFlag.size();
            checksumPos = frame.size() - tailLen - m_config.checksumSize;
        }

        if (checksumPos < 0 || checksumPos + m_config.checksumSize > frame.size()) {
            return false;
        }

        QByteArray dataForCheck = frame.left(checksumPos);
        QByteArray expected = calculateChecksum(dataForCheck);
        QByteArray actual = frame.mid(checksumPos, m_config.checksumSize);

        if (expected != actual) {
            return false;
        }
    }

    return true;
}

QByteArray CustomProtocol::calculateChecksum(const QByteArray& data)
{
    // 如果有自定义校验和回调
    if (m_checksumCallback) {
        return m_checksumCallback(data);
    }

    // 默认校验和算法
    switch (m_config.checksumAlgo) {
        case CustomProtocolConfig::ChecksumAlgo::Sum8: {
            quint8 sum = 0;
            for (char c : data) {
                sum += static_cast<quint8>(c);
            }
            return QByteArray(1, static_cast<char>(sum));
        }

        case CustomProtocolConfig::ChecksumAlgo::Sum16: {
            quint16 sum = 0;
            for (char c : data) {
                sum += static_cast<quint8>(c);
            }
            QByteArray result(2, 0);
            result[0] = static_cast<char>((sum >> 8) & 0xFF);
            result[1] = static_cast<char>(sum & 0xFF);
            return result;
        }

        case CustomProtocolConfig::ChecksumAlgo::XOR: {
            quint8 xorVal = 0;
            for (char c : data) {
                xorVal ^= static_cast<quint8>(c);
            }
            return QByteArray(1, static_cast<char>(xorVal));
        }

        default:
            return QByteArray();
    }
}

void CustomProtocol::reset()
{
    m_buffer.clear();
    m_frameParser->clear();
}

void CustomProtocol::onFrameReady(const QByteArray& frame)
{
    FrameResult result;
    result.valid = validate(frame);
    result.frame = frame;
    result.consumedBytes = frame.size();

    // 提取payload
    int headLen = m_config.frameConfig.startFlag.size();
    int tailLen = m_config.frameConfig.endFlag.size();
    int checksumLen = m_config.useChecksum ? m_config.checksumSize : 0;

    int payloadStart = headLen;
    int payloadLen = frame.size() - headLen - tailLen - checksumLen;

    // 如果有长度字段，跳过它
    if (m_config.frameConfig.delimiter == FrameDelimiter::LengthField) {
        payloadStart += m_config.frameConfig.lengthFieldSize;
        payloadLen -= m_config.frameConfig.lengthFieldSize;
    }

    if (payloadLen > 0) {
        result.payload = frame.mid(payloadStart, payloadLen);
    }

    emit frameReceived(result);
}

bool CustomProtocol::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Failed to open protocol config file: %1").arg(filePath));
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        LOG_ERROR(QString("JSON parse error: %1").arg(parseError.errorString()));
        return false;
    }

    QJsonObject root = doc.object();

    CustomProtocolConfig config;
    config.name = root["name"].toString("Custom");
    config.description = root["description"].toString();

    // 解析帧配置
    QJsonObject frameObj = root["frame"].toObject();
    config.frameConfig.startFlag = QByteArray::fromHex(frameObj["startFlag"].toString().toLatin1());
    config.frameConfig.endFlag = QByteArray::fromHex(frameObj["endFlag"].toString().toLatin1());
    config.frameConfig.maxFrameSize = frameObj["maxFrameSize"].toInt(65536);

    QString delimiterStr = frameObj["delimiter"].toString("None");
    if (delimiterStr == "FixedLength") {
        config.frameConfig.delimiter = FrameDelimiter::FixedLength;
        config.frameConfig.fixedLength = frameObj["fixedLength"].toInt(0);
    } else if (delimiterStr == "Timeout") {
        config.frameConfig.delimiter = FrameDelimiter::Timeout;
        config.frameConfig.timeoutMs = frameObj["timeoutMs"].toInt(50);
    } else if (delimiterStr == "StartEnd") {
        config.frameConfig.delimiter = FrameDelimiter::StartEnd;
    } else if (delimiterStr == "LengthField") {
        config.frameConfig.delimiter = FrameDelimiter::LengthField;
        config.frameConfig.lengthFieldOffset = frameObj["lengthFieldOffset"].toInt(0);
        config.frameConfig.lengthFieldSize = frameObj["lengthFieldSize"].toInt(1);
        config.frameConfig.lengthFieldBigEndian = frameObj["lengthFieldBigEndian"].toBool(true);
        config.frameConfig.lengthAdjustment = frameObj["lengthAdjustment"].toInt(0);
    }

    // 解析校验和配置
    QJsonObject checksumObj = root["checksum"].toObject();
    config.useChecksum = checksumObj["enabled"].toBool(false);
    config.checksumSize = checksumObj["size"].toInt(1);
    config.checksumOffset = checksumObj["offset"].toInt(-1);

    QString algoStr = checksumObj["algorithm"].toString("Sum8");
    if (algoStr == "Sum8") config.checksumAlgo = CustomProtocolConfig::ChecksumAlgo::Sum8;
    else if (algoStr == "Sum16") config.checksumAlgo = CustomProtocolConfig::ChecksumAlgo::Sum16;
    else if (algoStr == "XOR") config.checksumAlgo = CustomProtocolConfig::ChecksumAlgo::XOR;
    else if (algoStr == "CRC8") config.checksumAlgo = CustomProtocolConfig::ChecksumAlgo::CRC8;
    else if (algoStr == "CRC16") config.checksumAlgo = CustomProtocolConfig::ChecksumAlgo::CRC16;

    setCustomConfig(config);
    LOG_INFO(QString("Loaded custom protocol: %1").arg(config.name));

    return true;
}

bool CustomProtocol::saveToFile(const QString& filePath)
{
    QJsonObject root;
    root["name"] = m_config.name;
    root["description"] = m_config.description;

    // 帧配置
    QJsonObject frameObj;
    frameObj["startFlag"] = QString::fromLatin1(m_config.frameConfig.startFlag.toHex());
    frameObj["endFlag"] = QString::fromLatin1(m_config.frameConfig.endFlag.toHex());
    frameObj["maxFrameSize"] = m_config.frameConfig.maxFrameSize;

    switch (m_config.frameConfig.delimiter) {
        case FrameDelimiter::FixedLength:
            frameObj["delimiter"] = "FixedLength";
            frameObj["fixedLength"] = m_config.frameConfig.fixedLength;
            break;
        case FrameDelimiter::Timeout:
            frameObj["delimiter"] = "Timeout";
            frameObj["timeoutMs"] = m_config.frameConfig.timeoutMs;
            break;
        case FrameDelimiter::StartEnd:
            frameObj["delimiter"] = "StartEnd";
            break;
        case FrameDelimiter::LengthField:
            frameObj["delimiter"] = "LengthField";
            frameObj["lengthFieldOffset"] = m_config.frameConfig.lengthFieldOffset;
            frameObj["lengthFieldSize"] = m_config.frameConfig.lengthFieldSize;
            frameObj["lengthFieldBigEndian"] = m_config.frameConfig.lengthFieldBigEndian;
            frameObj["lengthAdjustment"] = m_config.frameConfig.lengthAdjustment;
            break;
        default:
            frameObj["delimiter"] = "None";
            break;
    }
    root["frame"] = frameObj;

    // 校验和配置
    QJsonObject checksumObj;
    checksumObj["enabled"] = m_config.useChecksum;
    checksumObj["size"] = m_config.checksumSize;
    checksumObj["offset"] = m_config.checksumOffset;

    switch (m_config.checksumAlgo) {
        case CustomProtocolConfig::ChecksumAlgo::Sum8:  checksumObj["algorithm"] = "Sum8"; break;
        case CustomProtocolConfig::ChecksumAlgo::Sum16: checksumObj["algorithm"] = "Sum16"; break;
        case CustomProtocolConfig::ChecksumAlgo::XOR:   checksumObj["algorithm"] = "XOR"; break;
        case CustomProtocolConfig::ChecksumAlgo::CRC8:  checksumObj["algorithm"] = "CRC8"; break;
        case CustomProtocolConfig::ChecksumAlgo::CRC16: checksumObj["algorithm"] = "CRC16"; break;
        default: checksumObj["algorithm"] = "Custom"; break;
    }
    root["checksum"] = checksumObj;

    QJsonDocument doc(root);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("Failed to save protocol config: %1").arg(filePath));
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    LOG_INFO(QString("Saved custom protocol config to: %1").arg(filePath));
    return true;
}

} // namespace ComAssistant
