/**
 * @file TestHidCommunication.cpp
 * @brief HID 通信工厂与会话配置回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "TestHidCommunication.h"

#include "communication/CommunicationFactory.h"
#include "communication/HidReportCodec.h"
#include "session/SessionData.h"

using namespace ComAssistant;

void TestHidCommunication::testFactoryCreatesHidCommunication()
{
    HidConfig config;
    config.vendorId = 0x1234;
    config.productId = 0x5678;

    const QList<CommType> supportedTypes = CommunicationFactory::supportedTypes();
    QVERIFY2(supportedTypes.contains(CommType::Hid),
             "HID 必须出现在受支持通信类型列表中，避免 UI 与工厂能力不一致。");

    std::unique_ptr<ICommunication> communication =
        CommunicationFactory::create(CommType::Hid, SerialConfig(), NetworkConfig(), config);

    QVERIFY2(communication != nullptr,
             "HID 工厂必须返回通信实例，不能再返回 nullptr。");
    QCOMPARE(communication->type(), CommType::Hid);
    QCOMPARE(communication->typeName(), QStringLiteral("HID"));
}

void TestHidCommunication::testOpenWithoutDeviceIdentityFailsClearly()
{
    std::unique_ptr<ICommunication> communication =
        CommunicationFactory::create(CommType::Hid, SerialConfig(), NetworkConfig(), HidConfig());

    QVERIFY2(communication != nullptr,
             "即使没有选择设备，工厂也应返回对象，由 open() 给出明确错误。");
    QVERIFY2(!communication->open(),
             "没有 path 或 VID/PID 时不能打开 HID 设备。");
    QVERIFY2(communication->lastError().contains(QStringLiteral("HID")),
             qPrintable(QStringLiteral("错误信息应明确指出 HID 配置问题，实际为: %1")
                            .arg(communication->lastError())));
}

void TestHidCommunication::testSessionPersistsHidConfig()
{
    SessionData session;
    session.commType = CommType::Hid;
    session.hidConfig.name = QStringLiteral("Demo HID");
    session.hidConfig.path = QStringLiteral("hid-path-001");
    session.hidConfig.vendorId = 0x1234;
    session.hidConfig.productId = 0x5678;
    session.hidConfig.interfaceNumber = 2;
    session.hidConfig.usagePage = 0xFF00;
    session.hidConfig.usage = 0x0001;
    session.hidConfig.inputReportLength = 64;
    session.hidConfig.outputReportLength = 65;
    session.hidConfig.outReportId = 3;
    session.hidConfig.featureReportLength = 33;
    session.hidConfig.featureReportId = 7;
    session.hidConfig.firstDataIsLength = true;
    session.hidConfig.removeInReportId = true;

    const QJsonObject json = session.toJson();
    const SessionData restored = SessionData::fromJson(json);

    QCOMPARE(restored.commType, CommType::Hid);
    QCOMPARE(restored.hidConfig.name, session.hidConfig.name);
    QCOMPARE(restored.hidConfig.path, session.hidConfig.path);
    QCOMPARE(restored.hidConfig.vendorId, session.hidConfig.vendorId);
    QCOMPARE(restored.hidConfig.productId, session.hidConfig.productId);
    QCOMPARE(restored.hidConfig.interfaceNumber, session.hidConfig.interfaceNumber);
    QCOMPARE(restored.hidConfig.usagePage, session.hidConfig.usagePage);
    QCOMPARE(restored.hidConfig.usage, session.hidConfig.usage);
    QCOMPARE(restored.hidConfig.inputReportLength, session.hidConfig.inputReportLength);
    QCOMPARE(restored.hidConfig.outputReportLength, session.hidConfig.outputReportLength);
    QCOMPARE(restored.hidConfig.outReportId, session.hidConfig.outReportId);
    QCOMPARE(restored.hidConfig.featureReportLength, session.hidConfig.featureReportLength);
    QCOMPARE(restored.hidConfig.featureReportId, session.hidConfig.featureReportId);
    QCOMPARE(restored.hidConfig.firstDataIsLength, session.hidConfig.firstDataIsLength);
    QCOMPARE(restored.hidConfig.removeInReportId, session.hidConfig.removeInReportId);
}

void TestHidCommunication::testOutputReportCodecBuildsPaddedReport()
{
    HidConfig config;
    config.outputReportLength = 8;
    config.outReportId = 0x05;
    config.firstDataIsLength = true;

    const QByteArray report = HidReportCodec::buildOutputReport(config, QByteArray::fromHex("A1B2C3"));

    QCOMPARE(report.size(), 8);
    QCOMPARE(static_cast<quint8>(report.at(0)), static_cast<quint8>(0x05));
    QCOMPARE(static_cast<quint8>(report.at(1)), static_cast<quint8>(3));
    QCOMPARE(report.mid(2, 3), QByteArray::fromHex("A1B2C3"));
    QCOMPARE(report.mid(5), QByteArray::fromHex("000000"));
}

void TestHidCommunication::testInputReportCodecRemovesReportId()
{
    HidConfig config;
    config.removeInReportId = true;

    const QByteArray payload =
        HidReportCodec::normalizeInputReport(config, QByteArray::fromHex("02010203"));

    QCOMPARE(payload, QByteArray::fromHex("010203"));
}

void TestHidCommunication::testFeatureReportCodecUsesFeatureSettings()
{
    HidConfig config;
    config.featureReportLength = 6;
    config.featureReportId = 0x09;

    const QByteArray report =
        HidReportCodec::buildFeatureReport(config, QByteArray::fromHex("112233445566"));

    QCOMPARE(report.size(), 6);
    QCOMPARE(static_cast<quint8>(report.at(0)), static_cast<quint8>(0x09));
    QCOMPARE(report.mid(1), QByteArray::fromHex("1122334455"));
}
