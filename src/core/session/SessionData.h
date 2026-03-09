/**
 * @file SessionData.h
 * @brief 会话数据结构定义
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_SESSIONDATA_H
#define COMASSISTANT_SESSIONDATA_H

#include <QString>
#include <QDateTime>
#include <QByteArray>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

#include "config/AppConfig.h"
#include "communication/ICommunication.h"
#include "ui/widgets/QuickSendWidget.h"

namespace ComAssistant {

/**
 * @brief 带时间戳的数据记录
 */
struct TimestampedData {
    QDateTime timestamp;    ///< 时间戳
    QByteArray data;        ///< 数据
    bool isSent = false;    ///< 是否为发送数据

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
        obj["data"] = QString::fromLatin1(data.toBase64());
        obj["isSent"] = isSent;
        return obj;
    }

    static TimestampedData fromJson(const QJsonObject& obj) {
        TimestampedData record;
        record.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODateWithMs);
        record.data = QByteArray::fromBase64(obj["data"].toString().toLatin1());
        record.isSent = obj["isSent"].toBool();
        return record;
    }
};

/**
 * @brief 绘图窗口配置
 */
struct PlotWindowConfig {
    QString windowId;       ///< 窗口ID
    QString title;          ///< 窗口标题
    QStringList curveNames; ///< 曲线名称列表
    bool autoScale = true;  ///< 自动缩放
    int maxDataPoints = 10000;  ///< 最大数据点数

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["windowId"] = windowId;
        obj["title"] = title;
        obj["curveNames"] = QJsonArray::fromStringList(curveNames);
        obj["autoScale"] = autoScale;
        obj["maxDataPoints"] = maxDataPoints;
        return obj;
    }

    static PlotWindowConfig fromJson(const QJsonObject& obj) {
        PlotWindowConfig config;
        config.windowId = obj["windowId"].toString();
        config.title = obj["title"].toString();
        QJsonArray arr = obj["curveNames"].toArray();
        for (const auto& val : arr) {
            config.curveNames.append(val.toString());
        }
        config.autoScale = obj["autoScale"].toBool(true);
        config.maxDataPoints = obj["maxDataPoints"].toInt(10000);
        return config;
    }
};

/**
 * @brief 会话数据
 */
struct SessionData {
    // 元信息
    QString version = "1.0.0";          ///< 会话文件版本
    QDateTime createTime;               ///< 创建时间
    QDateTime lastModifyTime;           ///< 最后修改时间
    QString name;                       ///< 会话名称
    QString description;                ///< 会话描述

    // 连接配置
    CommType commType = CommType::Serial;   ///< 通信类型
    SerialConfig serialConfig;              ///< 串口配置
    NetworkConfig networkConfig;            ///< 网络配置

    // 协议配置
    int protocolType = 0;               ///< 协议类型
    int displayMode = 0;                ///< 显示模式

    // 接收数据
    QByteArray receivedData;            ///< 原始接收数据
    QVector<TimestampedData> timestampedData;   ///< 带时间戳的数据

    // 发送历史
    QStringList sendHistory;            ///< 发送历史记录

    // 快捷发送配置
    QVector<QuickSendItem> quickSendItems;  ///< 快捷发送项

    // 绘图配置
    QVector<PlotWindowConfig> plotConfigs;  ///< 绘图窗口配置

    // 界面状态
    QByteArray windowGeometry;          ///< 窗口几何信息
    QByteArray splitterState;           ///< 分割器状态

    // 选项状态
    bool timestampEnabled = false;      ///< 时间戳开关
    bool autoScrollEnabled = true;      ///< 自动滚动开关
    bool hexDisplayEnabled = false;     ///< HEX显示开关

    /**
     * @brief 转换为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;

        // 元信息
        obj["version"] = version;
        obj["createTime"] = createTime.toString(Qt::ISODateWithMs);
        obj["lastModifyTime"] = lastModifyTime.toString(Qt::ISODateWithMs);
        obj["name"] = name;
        obj["description"] = description;

        // 连接配置
        obj["commType"] = static_cast<int>(commType);

        // 串口配置
        QJsonObject serialObj;
        serialObj["portName"] = serialConfig.portName;
        serialObj["baudRate"] = serialConfig.baudRate;
        serialObj["dataBits"] = static_cast<int>(serialConfig.dataBits);
        serialObj["parity"] = static_cast<int>(serialConfig.parity);
        serialObj["stopBits"] = static_cast<int>(serialConfig.stopBits);
        serialObj["flowControl"] = static_cast<int>(serialConfig.flowControl);
        obj["serialConfig"] = serialObj;

        // 网络配置
        QJsonObject networkObj;
        networkObj["mode"] = static_cast<int>(networkConfig.mode);
        networkObj["serverIp"] = networkConfig.serverIp;
        networkObj["serverPort"] = networkConfig.serverPort;
        networkObj["listenPort"] = networkConfig.listenPort;
        networkObj["remoteIp"] = networkConfig.remoteIp;
        networkObj["remotePort"] = networkConfig.remotePort;
        obj["networkConfig"] = networkObj;

        // 协议和显示模式
        obj["protocolType"] = protocolType;
        obj["displayMode"] = displayMode;

        // 接收数据
        obj["receivedData"] = QString::fromLatin1(receivedData.toBase64());

        QJsonArray dataArray;
        for (const auto& record : timestampedData) {
            dataArray.append(record.toJson());
        }
        obj["timestampedData"] = dataArray;

        // 发送历史
        obj["sendHistory"] = QJsonArray::fromStringList(sendHistory);

        // 快捷发送
        QJsonArray quickSendArray;
        for (const auto& item : quickSendItems) {
            quickSendArray.append(item.toJson());
        }
        obj["quickSendItems"] = quickSendArray;

        // 绘图配置
        QJsonArray plotArray;
        for (const auto& config : plotConfigs) {
            plotArray.append(config.toJson());
        }
        obj["plotConfigs"] = plotArray;

        // 界面状态
        obj["windowGeometry"] = QString::fromLatin1(windowGeometry.toBase64());
        obj["splitterState"] = QString::fromLatin1(splitterState.toBase64());

        // 选项状态
        obj["timestampEnabled"] = timestampEnabled;
        obj["autoScrollEnabled"] = autoScrollEnabled;
        obj["hexDisplayEnabled"] = hexDisplayEnabled;

        return obj;
    }

    /**
     * @brief 从JSON对象解析
     */
    static SessionData fromJson(const QJsonObject& obj) {
        SessionData data;

        // 元信息
        data.version = obj["version"].toString("1.0.0");
        data.createTime = QDateTime::fromString(obj["createTime"].toString(), Qt::ISODateWithMs);
        data.lastModifyTime = QDateTime::fromString(obj["lastModifyTime"].toString(), Qt::ISODateWithMs);
        data.name = obj["name"].toString();
        data.description = obj["description"].toString();

        // 连接配置
        data.commType = static_cast<CommType>(obj["commType"].toInt());

        // 串口配置
        QJsonObject serialObj = obj["serialConfig"].toObject();
        data.serialConfig.portName = serialObj["portName"].toString();
        data.serialConfig.baudRate = serialObj["baudRate"].toInt(115200);
        data.serialConfig.dataBits = static_cast<DataBits>(serialObj["dataBits"].toInt(8));
        data.serialConfig.parity = static_cast<Parity>(serialObj["parity"].toInt(0));
        data.serialConfig.stopBits = static_cast<StopBits>(serialObj["stopBits"].toInt(0));
        data.serialConfig.flowControl = static_cast<FlowControl>(serialObj["flowControl"].toInt(0));

        // 网络配置
        QJsonObject networkObj = obj["networkConfig"].toObject();
        data.networkConfig.mode = static_cast<NetworkMode>(networkObj["mode"].toInt(1));
        data.networkConfig.serverIp = networkObj["serverIp"].toString("127.0.0.1");
        data.networkConfig.serverPort = networkObj["serverPort"].toInt(8080);
        data.networkConfig.listenPort = networkObj["listenPort"].toInt(8080);
        data.networkConfig.remoteIp = networkObj["remoteIp"].toString();
        data.networkConfig.remotePort = networkObj["remotePort"].toInt(0);

        // 协议和显示模式
        data.protocolType = obj["protocolType"].toInt();
        data.displayMode = obj["displayMode"].toInt();

        // 接收数据
        data.receivedData = QByteArray::fromBase64(obj["receivedData"].toString().toLatin1());

        QJsonArray dataArray = obj["timestampedData"].toArray();
        for (const auto& val : dataArray) {
            data.timestampedData.append(TimestampedData::fromJson(val.toObject()));
        }

        // 发送历史
        QJsonArray historyArray = obj["sendHistory"].toArray();
        for (const auto& val : historyArray) {
            data.sendHistory.append(val.toString());
        }

        // 快捷发送
        QJsonArray quickSendArray = obj["quickSendItems"].toArray();
        for (const auto& val : quickSendArray) {
            data.quickSendItems.append(QuickSendItem::fromJson(val.toObject()));
        }

        // 绘图配置
        QJsonArray plotArray = obj["plotConfigs"].toArray();
        for (const auto& val : plotArray) {
            data.plotConfigs.append(PlotWindowConfig::fromJson(val.toObject()));
        }

        // 界面状态
        data.windowGeometry = QByteArray::fromBase64(obj["windowGeometry"].toString().toLatin1());
        data.splitterState = QByteArray::fromBase64(obj["splitterState"].toString().toLatin1());

        // 选项状态
        data.timestampEnabled = obj["timestampEnabled"].toBool();
        data.autoScrollEnabled = obj["autoScrollEnabled"].toBool(true);
        data.hexDisplayEnabled = obj["hexDisplayEnabled"].toBool();

        return data;
    }
};

} // namespace ComAssistant

#endif // COMASSISTANT_SESSIONDATA_H
