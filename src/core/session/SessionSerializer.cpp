/**
 * @file SessionSerializer.cpp
 * @brief 会话序列化器实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "SessionSerializer.h"
#include <QFile>
#include <QJsonDocument>
#include <QDataStream>

namespace ComAssistant {

QString SessionSerializer::s_lastError;

bool SessionSerializer::saveToFile(const SessionData& data, const QString& filePath, bool compress)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        s_lastError = QString("无法打开文件: %1").arg(file.errorString());
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_12);

    // 写入文件头
    stream.writeRawData(FILE_MAGIC, 4);
    stream << static_cast<quint16>(FILE_VERSION);
    stream << static_cast<quint16>(compress ? 1 : 0);

    // 预留8字节
    quint64 reserved = 0;
    stream << reserved;

    // 序列化数据
    QByteArray jsonData = toJson(data);

    // 压缩或直接写入
    if (compress) {
        QByteArray compressed = qCompress(jsonData, 9);
        stream << compressed;
    } else {
        stream << jsonData;
    }

    file.close();
    return true;
}

bool SessionSerializer::loadFromFile(const QString& filePath, SessionData& data)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        s_lastError = QString("无法打开文件: %1").arg(file.errorString());
        return false;
    }

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_12);

    // 读取文件头
    char magic[4];
    stream.readRawData(magic, 4);

    if (memcmp(magic, FILE_MAGIC, 4) != 0) {
        s_lastError = "无效的会话文件格式";
        file.close();
        return false;
    }

    quint16 version;
    stream >> version;

    if (version > FILE_VERSION) {
        s_lastError = QString("不支持的文件版本: %1").arg(version);
        file.close();
        return false;
    }

    quint16 flags;
    stream >> flags;
    bool compressed = (flags & 1) != 0;

    quint64 reserved;
    stream >> reserved;

    // 读取数据
    QByteArray rawData;
    stream >> rawData;

    // 解压缩
    QByteArray jsonData;
    if (compressed) {
        jsonData = qUncompress(rawData);
        if (jsonData.isEmpty()) {
            s_lastError = "解压缩失败";
            file.close();
            return false;
        }
    } else {
        jsonData = rawData;
    }

    file.close();

    // 解析JSON
    return fromJson(jsonData, data);
}

QByteArray SessionSerializer::toJson(const SessionData& data)
{
    QJsonObject obj = data.toJson();
    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

bool SessionSerializer::fromJson(const QByteArray& json, SessionData& data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json, &error);

    if (error.error != QJsonParseError::NoError) {
        s_lastError = QString("JSON解析错误: %1").arg(error.errorString());
        return false;
    }

    if (!doc.isObject()) {
        s_lastError = "JSON格式错误：根元素不是对象";
        return false;
    }

    data = SessionData::fromJson(doc.object());
    return true;
}

QString SessionSerializer::lastError()
{
    return s_lastError;
}

} // namespace ComAssistant
