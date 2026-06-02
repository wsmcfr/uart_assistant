/**
 * @file TestProtocolConfigEditor.cpp
 * @brief 协议配置编辑器单元测试
 */

#include "TestProtocolConfigEditor.h"

#include "core/protocol/ProtocolConfigSchema.h"
#include "ui/widgets/ProtocolConfigEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>

using namespace ComAssistant;

namespace {

/**
 * @brief 创建覆盖全部第一版字段类型的测试 Schema。
 * @return 包含 Bool、Integer、Double、String、BytesHex、Enum 字段的 Schema。
 */
ProtocolConfigSchema makeEditorTestSchema()
{
    ProtocolConfigSchema schema;
    schema.fields.append(ProtocolConfigField::boolean(
        QStringLiteral("enabled"),
        QStringLiteral("启用"),
        true,
        QStringLiteral("是否启用")));
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("timeoutMs"),
        QStringLiteral("超时"),
        100,
        1,
        1000,
        QStringLiteral("超时时间")));
    schema.fields.append(ProtocolConfigField::floating(
        QStringLiteral("gain"),
        QStringLiteral("增益"),
        1.5,
        0.0,
        10.0,
        QStringLiteral("倍率")));
    schema.fields.append(ProtocolConfigField::string(
        QStringLiteral("encoding"),
        QStringLiteral("编码"),
        QStringLiteral("UTF-8"),
        QStringLiteral("文本编码")));
    schema.fields.append(ProtocolConfigField::bytesHex(
        QStringLiteral("frameHeader"),
        QStringLiteral("帧头"),
        QStringLiteral("AA 55"),
        QStringLiteral("帧头字节")));
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("lineEnding"),
        QStringLiteral("行结束符"),
        QStringLiteral("CRLF"),
        QStringList{QStringLiteral("None"), QStringLiteral("CR"), QStringLiteral("LF"), QStringLiteral("CRLF")},
        QStringLiteral("发送行结束符")));
    return schema;
}

} // namespace

void TestProtocolConfigEditor::buildsWidgetsFromSchema()
{
    /*
     * 编辑器必须按 Schema 自动生成控件，且 objectName 使用稳定 key。
     * 这是后续对话框测试和 UI 自动化定位的基础。
     */
    ProtocolConfigEditor editor;
    editor.setSchema(makeEditorTestSchema());

    QVERIFY(editor.findChild<QCheckBox*>(QStringLiteral("protocolConfig_enabled")) != nullptr);
    QVERIFY(editor.findChild<QSpinBox*>(QStringLiteral("protocolConfig_timeoutMs")) != nullptr);
    QVERIFY(editor.findChild<QDoubleSpinBox*>(QStringLiteral("protocolConfig_gain")) != nullptr);
    QVERIFY(editor.findChild<QLineEdit*>(QStringLiteral("protocolConfig_encoding")) != nullptr);
    QVERIFY(editor.findChild<QLineEdit*>(QStringLiteral("protocolConfig_frameHeader")) != nullptr);
    QVERIFY(editor.findChild<QComboBox*>(QStringLiteral("protocolConfig_lineEnding")) != nullptr);
}

void TestProtocolConfigEditor::loadsAndReadsConfig()
{
    /*
     * 编辑器应能把外部配置写入控件，再从控件读回 QVariantMap。
     * 这是对话框确认、会话恢复和后续协议应用共用的数据通道。
     */
    ProtocolConfigEditor editor;
    editor.setSchema(makeEditorTestSchema());

    QVariantMap config;
    config.insert(QStringLiteral("enabled"), false);
    config.insert(QStringLiteral("timeoutMs"), 250);
    config.insert(QStringLiteral("gain"), 2.5);
    config.insert(QStringLiteral("encoding"), QStringLiteral("GBK"));
    config.insert(QStringLiteral("frameHeader"), QStringLiteral("55 AA"));
    config.insert(QStringLiteral("lineEnding"), QStringLiteral("LF"));

    editor.setConfig(config);
    const QVariantMap readConfig = editor.config();

    QCOMPARE(readConfig.value(QStringLiteral("enabled")).toBool(), false);
    QCOMPARE(readConfig.value(QStringLiteral("timeoutMs")).toInt(), 250);
    QCOMPARE(readConfig.value(QStringLiteral("gain")).toDouble(), 2.5);
    QCOMPARE(readConfig.value(QStringLiteral("encoding")).toString(), QStringLiteral("GBK"));
    QCOMPARE(readConfig.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("55 AA"));
    QCOMPARE(readConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("LF"));
}

void TestProtocolConfigEditor::restoresDefaults()
{
    /*
     * 恢复默认值必须直接使用 Schema 默认配置，避免 UI 自行拼默认值导致
     * 十六进制规范化或未来默认值迁移规则和核心层不一致。
     */
    ProtocolConfigEditor editor;
    const ProtocolConfigSchema schema = makeEditorTestSchema();
    editor.setSchema(schema);

    QVariantMap config;
    config.insert(QStringLiteral("timeoutMs"), 250);
    config.insert(QStringLiteral("lineEnding"), QStringLiteral("LF"));
    editor.setConfig(config);
    editor.restoreDefaults();

    QCOMPARE(editor.config(), schema.defaults());
}

void TestProtocolConfigEditor::reportsValidationErrors()
{
    /*
     * 编辑器不应绕过 Schema 校验。非法十六进制文本要被拦截，并把错误
     * 暴露给对话框底部提示区域，而不是静默应用坏配置。
     */
    ProtocolConfigEditor editor;
    editor.setSchema(makeEditorTestSchema());

    QLineEdit* hexEdit = editor.findChild<QLineEdit*>(QStringLiteral("protocolConfig_frameHeader"));
    QVERIFY(hexEdit != nullptr);
    hexEdit->setText(QStringLiteral("AA Z1"));

    const ProtocolConfigValidationResult result = editor.validateConfig();

    QVERIFY(!result.valid);
    QVERIFY(result.errors.join(QStringLiteral("\n")).contains(QStringLiteral("frameHeader")));
    QVERIFY(!editor.errorText().isEmpty());
}
