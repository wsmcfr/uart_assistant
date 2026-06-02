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
