# Protocol Config Schema Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build the 4.2 protocol configuration schema foundation so built-in protocols expose default config, validation, normalization, and session-compatible persistence through stable protocol IDs.

**Architecture:** Add a small schema model under `src/core/protocol/`, attach schema/default config to `ProtocolDescriptor`, and keep validation independent from UI. Extend `SessionData` to persist `protocolId`, `protocolConfigVersion`, and `protocolConfig` while keeping legacy `protocolType` compatibility.

**Tech Stack:** C++17, Qt 5.12, Qt Test, CMake, existing `ProtocolRegistry` / `ProtocolFactory` / `SessionData`.

---

### Task 1: Add Red Tests For Protocol Config Schema Validation

**Files:**
- Create: `tests/unit/TestProtocolConfigSchema.h`
- Create: `tests/unit/TestProtocolConfigSchema.cpp`
- Modify: `tests/main.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test header**

Create `tests/unit/TestProtocolConfigSchema.h`:

```cpp
/**
 * @file TestProtocolConfigSchema.h
 * @brief 协议配置 Schema 单元测试头文件
 */

#ifndef TESTPROTOCOLCONFIGSCHEMA_H
#define TESTPROTOCOLCONFIGSCHEMA_H

#include <QObject>
#include <QTest>

/**
 * @brief 协议配置 Schema 测试类
 *
 * 该测试类验证协议配置字段定义、默认值合并、类型校验、范围校验、
 * 枚举校验和十六进制字节字段规范化。
 */
class TestProtocolConfigSchema : public QObject
{
    Q_OBJECT

private slots:
    void fillsMissingDefaults();
    void rejectsInvalidValues();
    void normalizesHexBytes();
};

#endif // TESTPROTOCOLCONFIGSCHEMA_H
```

**Step 2: Write failing tests**

Create `tests/unit/TestProtocolConfigSchema.cpp`:

```cpp
/**
 * @file TestProtocolConfigSchema.cpp
 * @brief 协议配置 Schema 单元测试
 */

#include "TestProtocolConfigSchema.h"

#include "core/protocol/ProtocolConfigSchema.h"

using namespace ComAssistant;

void TestProtocolConfigSchema::fillsMissingDefaults()
{
    /*
     * 缺失字段应自动补默认值。这样旧会话或插件未提供完整配置时，
     * 后续协议实例仍能拿到可用、稳定的规范化配置。
     */
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("timeoutMs"),
        QStringLiteral("超时时间"),
        100,
        1,
        10000,
        QStringLiteral("分帧超时时间")));
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("lineEnding"),
        QStringLiteral("行结束符"),
        QStringLiteral("CRLF"),
        QStringList{QStringLiteral("None"), QStringLiteral("CR"), QStringLiteral("LF"), QStringLiteral("CRLF")},
        QStringLiteral("发送时追加的行结束符")));

    const ProtocolConfigValidationResult result = schema.validate(QVariantMap());

    QVERIFY(result.valid);
    QCOMPARE(result.normalizedConfig.value(QStringLiteral("timeoutMs")).toInt(), 100);
    QCOMPARE(result.normalizedConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("CRLF"));
}

void TestProtocolConfigSchema::rejectsInvalidValues()
{
    /*
     * Schema 是协议配置入口的防线。超出范围和非法枚举必须被明确拒绝，
     * 否则后续 UI、Lua 或插件都可能把坏配置传进协议实现。
     */
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::integer(
        QStringLiteral("slaveAddress"),
        QStringLiteral("从站地址"),
        1,
        1,
        247,
        QStringLiteral("Modbus 从站地址")));
    schema.fields.append(ProtocolConfigField::enumeration(
        QStringLiteral("mode"),
        QStringLiteral("模式"),
        QStringLiteral("RTU"),
        QStringList{QStringLiteral("RTU"), QStringLiteral("ASCII")},
        QStringLiteral("Modbus 帧格式")));

    QVariantMap config;
    config.insert(QStringLiteral("slaveAddress"), 300);
    config.insert(QStringLiteral("mode"), QStringLiteral("TCP"));

    const ProtocolConfigValidationResult result = schema.validate(config);

    QVERIFY(!result.valid);
    QVERIFY(result.errors.size() >= 2);
    QVERIFY(result.errors.join(QStringLiteral("\n")).contains(QStringLiteral("slaveAddress")));
    QVERIFY(result.errors.join(QStringLiteral("\n")).contains(QStringLiteral("mode")));
}

void TestProtocolConfigSchema::normalizesHexBytes()
{
    /*
     * 字节字段统一保存为大写空格分隔十六进制文本，避免会话 JSON、
     * 未来配置 UI 和脚本接口之间出现多种表示方式。
     */
    ProtocolConfigSchema schema;
    schema.version = 1;
    schema.fields.append(ProtocolConfigField::bytesHex(
        QStringLiteral("frameHeader"),
        QStringLiteral("帧头"),
        QStringLiteral("AA 55"),
        QStringLiteral("帧头字节")));

    QVariantMap config;
    config.insert(QStringLiteral("frameHeader"), QStringLiteral("aa-55"));

    const ProtocolConfigValidationResult result = schema.validate(config);

    QVERIFY(result.valid);
    QCOMPARE(result.normalizedConfig.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("AA 55"));
}
```

**Step 3: Register test in `tests/main.cpp`**

Add include near `TestProtocolRegistry`:

```cpp
#include "unit/TestProtocolConfigSchema.h"
```

Add a test block before `ProtocolRegistry`:

```cpp
{
    qDebug() << "\n[TEST] ProtocolConfigSchema";
    TestProtocolConfigSchema test;
    status |= QTest::qExec(&test, filteredArgs);
}
```

**Step 4: Register source in `tests/CMakeLists.txt`**

Add to `TEST_SOURCES`:

```cmake
unit/TestProtocolConfigSchema.cpp
```

**Step 5: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `src/core/protocol/ProtocolConfigSchema.h` does not exist.

**Step 6: Do not commit yet**

Continue to Task 2.

### Task 2: Implement Minimal ProtocolConfigSchema

**Files:**
- Create: `src/core/protocol/ProtocolConfigSchema.h`
- Create: `src/core/protocol/ProtocolConfigSchema.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create `ProtocolConfigSchema.h`**

Create `src/core/protocol/ProtocolConfigSchema.h`:

```cpp
/**
 * @file ProtocolConfigSchema.h
 * @brief 协议配置 Schema 定义
 */

#ifndef COMASSISTANT_PROTOCOLCONFIGSCHEMA_H
#define COMASSISTANT_PROTOCOLCONFIGSCHEMA_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

namespace ComAssistant {

/**
 * @brief 协议配置字段类型
 *
 * 字段类型用于统一校验会话、后续 UI、脚本和插件传入的协议配置。
 */
enum class ProtocolConfigFieldType
{
    Bool,       ///< 布尔值
    Integer,    ///< 整数
    Double,     ///< 浮点数
    String,     ///< 字符串
    BytesHex,   ///< 十六进制字节文本，例如 AA 55
    Enum        ///< 字符串枚举
};

/**
 * @brief 协议配置字段定义
 *
 * 描述一个配置项的 key、显示名、类型、默认值、范围、枚举选项和说明。
 */
struct ProtocolConfigField
{
    QString key;                                      ///< 配置键名，必须稳定
    QString displayName;                              ///< 面向用户显示的名称
    QString description;                              ///< 字段说明
    ProtocolConfigFieldType type = ProtocolConfigFieldType::String; ///< 字段类型
    QVariant defaultValue;                            ///< 默认值
    QVariant minValue;                                ///< 数值最小值，可为空
    QVariant maxValue;                                ///< 数值最大值，可为空
    QStringList enumValues;                           ///< 枚举允许值
    bool required = false;                            ///< 是否必填；缺失时仍优先补默认值

    static ProtocolConfigField boolean(const QString& key,
                                       const QString& displayName,
                                       bool defaultValue,
                                       const QString& description);
    static ProtocolConfigField integer(const QString& key,
                                       const QString& displayName,
                                       int defaultValue,
                                       int minValue,
                                       int maxValue,
                                       const QString& description);
    static ProtocolConfigField floating(const QString& key,
                                        const QString& displayName,
                                        double defaultValue,
                                        double minValue,
                                        double maxValue,
                                        const QString& description);
    static ProtocolConfigField string(const QString& key,
                                      const QString& displayName,
                                      const QString& defaultValue,
                                      const QString& description);
    static ProtocolConfigField bytesHex(const QString& key,
                                        const QString& displayName,
                                        const QString& defaultValue,
                                        const QString& description);
    static ProtocolConfigField enumeration(const QString& key,
                                           const QString& displayName,
                                           const QString& defaultValue,
                                           const QStringList& enumValues,
                                           const QString& description);
};

/**
 * @brief 协议配置校验结果
 */
struct ProtocolConfigValidationResult
{
    bool valid = true;                 ///< 是否校验通过
    QVariantMap normalizedConfig;      ///< 合并默认值并规范化后的配置
    QStringList errors;                ///< 阻止应用配置的错误
    QStringList warnings;              ///< 可继续使用但需要提示的警告
};

/**
 * @brief 协议配置 Schema
 *
 * Schema 负责为某个协议定义配置字段，并提供默认值合并与校验能力。
 */
struct ProtocolConfigSchema
{
    int version = 1;                   ///< 配置版本
    bool allowUnknownFields = true;    ///< 是否允许未知字段
    QList<ProtocolConfigField> fields; ///< 字段列表

    /**
     * @brief 生成默认配置
     * @return 所有字段默认值组成的配置表
     */
    QVariantMap defaults() const;

    /**
     * @brief 校验并规范化配置
     * @param config 输入配置，可以只包含部分字段
     * @return 校验结果，包含规范化配置、错误和警告
     */
    ProtocolConfigValidationResult validate(const QVariantMap& config) const;

private:
    static QString normalizeHexString(const QString& value, bool* ok);
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLCONFIGSCHEMA_H
```

**Step 2: Create `ProtocolConfigSchema.cpp`**

Implement:

- Field factory helpers.
- `defaults()`.
- `validate()` with type conversion, range checks, enum checks, unknown field warnings.
- `normalizeHexString()` using only Qt 5.12-compatible APIs.

Important implementation notes:

- For `Bool`, accept `QVariant::Bool`; if convertible, use `toBool()`.
- For `Integer`, use `toInt(&ok)` and compare against `minValue` / `maxValue`.
- For `Double`, use `toDouble(&ok)`.
- For `Enum`, compare string value exactly with `enumValues`.
- For `BytesHex`, remove common separators and whitespace, reject odd hex length or non-hex characters, output uppercase pairs separated by spaces. Empty string is valid.
- Preserve unknown fields in `normalizedConfig` when `allowUnknownFields == true`; append warning.
- Reject unknown fields when `allowUnknownFields == false`; append error.

**Step 3: Wire CMake**

Modify root `CMakeLists.txt`:

Add header/source near other protocol files:

```cmake
src/core/protocol/ProtocolConfigSchema.h
src/core/protocol/ProtocolConfigSchema.cpp
```

Modify `tests/CMakeLists.txt`:

Add header to `add_executable` list:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolConfigSchema.h
```

Add source to `target_sources`:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolConfigSchema.cpp
```

**Step 4: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS for `TestProtocolConfigSchema`.

**Step 5: Commit**

Run:

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/main.cpp tests/unit/TestProtocolConfigSchema.h tests/unit/TestProtocolConfigSchema.cpp src/core/protocol/ProtocolConfigSchema.h src/core/protocol/ProtocolConfigSchema.cpp
git commit -m "feat: add protocol config schema validation"
```

### Task 3: Attach Config Schema To Protocol Descriptors

**Files:**
- Modify: `src/core/protocol/ProtocolDescriptor.h`
- Modify: `src/core/protocol/ProtocolRegistry.cpp`
- Modify: `tests/unit/TestProtocolRegistry.h`
- Modify: `tests/unit/TestProtocolRegistry.cpp`
- Modify: `tests/unit/TestProtocolConfigSchema.h`
- Modify: `tests/unit/TestProtocolConfigSchema.cpp`

**Step 1: Write failing registry tests**

Add slots to `tests/unit/TestProtocolRegistry.h`:

```cpp
void builtinDescriptorsExposeConfigSchema();
void defaultConfigMatchesSchema();
```

Add tests to `tests/unit/TestProtocolRegistry.cpp`:

```cpp
void TestProtocolRegistry::builtinDescriptorsExposeConfigSchema()
{
    /*
     * 4.2 要求协议目录不仅说明能力，还要成为配置事实源。
     * ASCII 和 EasyHEX 是第一批必须暴露字段级 schema 的协议。
     */
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const ProtocolDescriptor ascii = registry.descriptor(QStringLiteral("ascii"));
    QCOMPARE(ascii.configVersion, 1);
    QVERIFY(ascii.configSchema.fields.size() >= 4);
    QVERIFY(ascii.defaultConfig.contains(QStringLiteral("lineEnding")));
    QCOMPARE(ascii.defaultConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("CRLF"));

    const ProtocolDescriptor easyhex = registry.descriptor(QStringLiteral("easyhex"));
    QVERIFY(easyhex.configSchema.fields.size() >= 5);
    QCOMPARE(easyhex.defaultConfig.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("AA 55"));
}

void TestProtocolRegistry::defaultConfigMatchesSchema()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    for (const ProtocolDescriptor& descriptor : registry.descriptors()) {
        const ProtocolConfigValidationResult result =
            descriptor.configSchema.validate(descriptor.defaultConfig);
        QVERIFY2(result.valid, qPrintable(QStringLiteral("默认配置校验失败: %1 %2")
                                          .arg(descriptor.id, result.errors.join(QStringLiteral("; ")))));
        QCOMPARE(result.normalizedConfig, descriptor.defaultConfig);
    }
}
```

Add include:

```cpp
#include "core/protocol/ProtocolConfigSchema.h"
```

**Step 2: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `ProtocolDescriptor` lacks `configVersion` / `configSchema`.

**Step 3: Implement descriptor schema fields**

Modify `src/core/protocol/ProtocolDescriptor.h`:

```cpp
#include "ProtocolConfigSchema.h"
```

Add to `ProtocolDescriptor`:

```cpp
int configVersion = 1;                  ///< 配置版本，第一版为 1
ProtocolConfigSchema configSchema;      ///< 配置 Schema，用于默认值、校验和后续 UI 生成
```

**Step 4: Add built-in schema helpers**

In `ProtocolRegistry.cpp`, add helper functions in anonymous namespace:

- `makeEmptySchema()`
- `makeAsciiSchema()`
- `makeHexSchema()`
- `makeModbusSchema()`
- `makeCustomSchema()`
- `makeEasyHexSchema()`

Update `makeDescriptor()` to accept `ProtocolConfigSchema schema`, set:

```cpp
descriptor.configVersion = schema.version;
descriptor.configSchema = schema;
descriptor.defaultConfig = schema.defaults();
```

For plot protocols, pass `makeEmptySchema()`.

**Step 5: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 6: Commit**

Run:

```bash
git add src/core/protocol/ProtocolDescriptor.h src/core/protocol/ProtocolRegistry.cpp tests/unit/TestProtocolRegistry.h tests/unit/TestProtocolRegistry.cpp
git commit -m "feat: expose protocol config schemas"
```

### Task 4: Create Protocols With Validated Config

**Files:**
- Modify: `src/core/protocol/ProtocolFactory.h`
- Modify: `src/core/protocol/ProtocolFactory.cpp`
- Modify: `src/core/protocol/AsciiProtocol.h`
- Modify: `src/core/protocol/AsciiProtocol.cpp`
- Modify: `src/core/protocol/EasyHexProtocol.h`
- Modify: `src/core/protocol/EasyHexProtocol.cpp`
- Modify: `tests/unit/TestProtocolConfigSchema.h`
- Modify: `tests/unit/TestProtocolConfigSchema.cpp`

**Step 1: Add failing tests**

Add slots:

```cpp
void factoryAppliesValidatedAsciiConfig();
void factoryAppliesValidatedEasyHexConfig();
```

Add tests:

```cpp
void TestProtocolConfigSchema::factoryAppliesValidatedAsciiConfig()
{
    QVariantMap config;
    config.insert(QStringLiteral("lineEnding"), QStringLiteral("LF"));
    config.insert(QStringLiteral("appendLineEnding"), true);
    config.insert(QStringLiteral("timeoutMs"), 250);
    config.insert(QStringLiteral("encoding"), QStringLiteral("UTF-8"));

    std::unique_ptr<IProtocol> protocol(ProtocolFactory::create(ProtocolType::Ascii, config));
    QVERIFY(protocol != nullptr);

    auto* ascii = dynamic_cast<AsciiProtocol*>(protocol.get());
    QVERIFY(ascii != nullptr);
    QCOMPARE(ascii->lineEnding(), LineEnding::LF);
    QCOMPARE(protocol->config().value(QStringLiteral("timeoutMs")).toInt(), 250);
}

void TestProtocolConfigSchema::factoryAppliesValidatedEasyHexConfig()
{
    QVariantMap config;
    config.insert(QStringLiteral("frameHeader"), QStringLiteral("55 aa"));
    config.insert(QStringLiteral("frameTail"), QStringLiteral(""));
    config.insert(QStringLiteral("useChecksum"), true);
    config.insert(QStringLiteral("checksumType"), QStringLiteral("XOR8"));
    config.insert(QStringLiteral("lengthFieldOffset"), 2);
    config.insert(QStringLiteral("lengthFieldSize"), 1);

    std::unique_ptr<IProtocol> protocol(ProtocolFactory::create(ProtocolType::EasyHex, config));
    QVERIFY(protocol != nullptr);

    auto* easyhex = dynamic_cast<EasyHexProtocol*>(protocol.get());
    QVERIFY(easyhex != nullptr);
    QCOMPARE(EasyHexProtocol::toHexString(easyhex->easyHexConfig().frameHeader), QStringLiteral("55 AA"));
    QCOMPARE(easyhex->easyHexConfig().checksumType, EasyHexConfig::XOR8);
}
```

Add includes:

```cpp
#include "core/protocol/AsciiProtocol.h"
#include "core/protocol/EasyHexProtocol.h"
#include "core/protocol/ProtocolFactory.h"
#include <memory>
```

**Step 2: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `ProtocolFactory::create(ProtocolType, QVariantMap)` does not exist.

**Step 3: Add factory overloads**

Modify `ProtocolFactory.h`:

```cpp
static std::unique_ptr<IProtocol> create(ProtocolType type, const QVariantMap& config);
static IProtocol* create(ProtocolType type, const QVariantMap& config, QObject* parent);
```

Modify `ProtocolFactory.cpp`:

- Validate config through descriptor schema.
- If validation fails, use descriptor default config.
- Create protocol through existing `create(type, parent)`.
- Apply normalized/default config with `protocol->setConfig(...)`.

**Step 4: Override QVariantMap config in ASCII and EasyHEX**

In `AsciiProtocol.h`, add:

```cpp
void setConfig(const QVariantMap& config) override;
```

In `AsciiProtocol.cpp`, implement mapping:

- `lineEnding`: `None` / `CR` / `LF` / `CRLF`
- `appendLineEnding`
- `timeoutMs`
- `encoding`
- assign `m_config = config`

In `EasyHexProtocol.h`, add:

```cpp
void setConfig(const QVariantMap& config) override;
```

In `EasyHexProtocol.cpp`, implement mapping:

- `frameHeader` / `frameTail`: use `fromHexString()`
- `useChecksum`
- `checksumType`: `SUM8` / `XOR8` / `CRC8`
- `lengthFieldOffset`
- `lengthFieldSize`
- assign `m_config = config`

**Step 5: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 6: Commit**

Run:

```bash
git add src/core/protocol/ProtocolFactory.h src/core/protocol/ProtocolFactory.cpp src/core/protocol/AsciiProtocol.h src/core/protocol/AsciiProtocol.cpp src/core/protocol/EasyHexProtocol.h src/core/protocol/EasyHexProtocol.cpp tests/unit/TestProtocolConfigSchema.h tests/unit/TestProtocolConfigSchema.cpp
git commit -m "feat: create protocols with validated config"
```

### Task 5: Persist Protocol ID And Config In SessionData

**Files:**
- Modify: `src/core/session/SessionData.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `tests/unit/TestProtocolConfigSchema.h`
- Modify: `tests/unit/TestProtocolConfigSchema.cpp`

**Step 1: Add failing session tests**

Add slots:

```cpp
void sessionPersistsProtocolIdAndConfig();
void sessionMigratesLegacyProtocolTypeToProtocolId();
```

Add tests:

```cpp
void TestProtocolConfigSchema::sessionPersistsProtocolIdAndConfig()
{
    SessionData session;
    session.protocolType = static_cast<int>(ProtocolType::Ascii);
    session.protocolId = QStringLiteral("ascii");
    session.protocolConfigVersion = 1;
    session.protocolConfig.insert(QStringLiteral("lineEnding"), QStringLiteral("LF"));
    session.protocolConfig.insert(QStringLiteral("timeoutMs"), 250);

    const QJsonObject json = session.toJson();
    const SessionData restored = SessionData::fromJson(json);

    QCOMPARE(restored.protocolId, QStringLiteral("ascii"));
    QCOMPARE(restored.protocolConfigVersion, 1);
    QCOMPARE(restored.protocolConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("LF"));
    QCOMPARE(restored.protocolConfig.value(QStringLiteral("timeoutMs")).toInt(), 250);
}

void TestProtocolConfigSchema::sessionMigratesLegacyProtocolTypeToProtocolId()
{
    QJsonObject legacy;
    legacy.insert(QStringLiteral("protocolType"), static_cast<int>(ProtocolType::Ascii));

    const SessionData restored = SessionData::fromJson(legacy);

    QCOMPARE(restored.protocolId, QStringLiteral("ascii"));
    QCOMPARE(restored.protocolConfigVersion, 1);
    QCOMPARE(restored.protocolConfig.value(QStringLiteral("lineEnding")).toString(), QStringLiteral("CRLF"));
}
```

Add includes:

```cpp
#include "core/session/SessionData.h"
#include <QJsonObject>
```

**Step 2: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `SessionData` lacks `protocolId` / `protocolConfigVersion` / `protocolConfig`.

**Step 3: Update `SessionData.h`**

Add includes:

```cpp
#include <QVariantMap>
#include <QJsonValue>
#include "protocol/ProtocolFactory.h"
```

Add fields:

```cpp
QString protocolId;                  ///< 稳定协议 ID
int protocolConfigVersion = 1;       ///< 协议配置版本
QVariantMap protocolConfig;          ///< 协议配置
```

In `toJson()`:

- If `protocolId` is empty, derive it from `ProtocolFactory::typeId(static_cast<ProtocolType>(protocolType))`.
- Write `protocolId`, `protocolConfigVersion`, and `protocolConfig` through `QJsonObject::fromVariantMap()`.

In `fromJson()`:

- Read legacy `protocolType` first.
- Read `protocolId`; if empty, derive from `ProtocolFactory::typeId()`.
- If `protocolConfig` exists, read `toVariantMap()`.
- If config missing or empty, get descriptor by `protocolId` and use `defaultConfig`.
- Validate config through descriptor schema; if invalid, use default config.

**Step 4: Update `MainWindow.cpp` save/apply paths**

In `onSaveSession()` after `protocolType`:

```cpp
session.protocolId = ProtocolFactory::typeId(m_currentProtocolType);
session.protocolConfigVersion = ProtocolFactory::descriptor(m_currentProtocolType).configVersion;
session.protocolConfig = m_currentProtocol ? m_currentProtocol->config()
                                           : ProtocolFactory::descriptor(m_currentProtocolType).defaultConfig;
```

In `applySessionDataToUi()` or `onProtocolTypeChanged()` flow, keep this task minimal:

- Continue restoring by `protocolType` through existing coordinator.
- After `onProtocolTypeChanged()`, if `m_currentProtocol` exists, call `m_currentProtocol->setConfig(session.protocolConfig)`.

**Step 5: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 6: Commit**

Run:

```bash
git add src/core/session/SessionData.h src/ui/MainWindow.cpp tests/unit/TestProtocolConfigSchema.h tests/unit/TestProtocolConfigSchema.cpp
git commit -m "feat: persist protocol config in sessions"
```

### Task 6: Update Protocol Help Documentation

**Files:**
- Modify: `resources/help/protocols.html`
- Modify if needed: `resources/help/quickstart.html`
- Modify: `tests/unit/TestDocumentationLinks.cpp` only if documentation link checks need new coverage

**Step 1: Update `resources/help/protocols.html`**

Add a short paragraph near the protocol capability platform block:

```html
<p><strong>中文：</strong>协议配置现在通过 Schema 管理默认值、字段类型和基础校验；会话文件会保存稳定协议 ID 与协议配置，加载旧会话时会按旧版协议类型自动迁移到默认配置。</p>
<p><strong>English:</strong> Protocol configuration is now managed by schemas for defaults, field types, and basic validation. Session files store stable protocol IDs and protocol config, while legacy sessions migrate from the old protocol type to default config.</p>
```

**Step 2: Decide whether to update `quickstart.html`**

If Task 5 only changes internal session persistence and no quick-start workflow, button, menu, or user step changes, do not update `resources/help/quickstart.html`. In final report state:

```text
resources/help/quickstart.html 未更新，因为快速上手入口、按钮、菜单路径和操作流程没有变化；协议配置恢复说明已写入更相关的 protocols.html。
```

If implementation adds a user-visible session restore note to quick start, update the help version block consistently.

**Step 3: Run documentation-related tests**

Run:

```bash
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS, including `DocumentationLinks`.

**Step 4: Commit**

Run:

```bash
git add resources/help/protocols.html
git commit -m "docs: describe protocol config schemas"
```

### Task 7: Final Verification And RecallLoom Update

**Files:**
- Modify as needed: `.recallloom/rolling_summary.md`
- Modify as needed: `.recallloom/daily_logs/2026-06-02.md`

**Step 1: Run full verification**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
ctest --test-dir build_release --output-on-failure
cmake --build build_release --config Release --target ComAssistant --parallel
git diff --check
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\validate_context.py .
```

Expected:

- Test executable prints `All tests PASSED!`.
- CTest reports `100% tests passed, 0 tests failed out of 1`.
- Release build creates/deploys `D:\comassistant\build_release\ComAssistant.exe`.
- `git diff --check` exits 0.
- RecallLoom validation shows `Errors: 0`; resolve warnings if possible.

**Step 2: Update RecallLoom**

Use revision-aware helpers:

- `preflight_context_check.py .`
- `commit_context_file.py` for `.recallloom/rolling_summary.md`
- `append_daily_log_entry.py` for `.recallloom/daily_logs/2026-06-02.md`
- `validate_context.py .`

Record:

- 4.2 completed commit range.
- Verification results.
- Whether `quickstart.html` was updated and why.
- Remaining risks: no UI config panel, no Lua/plugin execution, no real hardware validation.

**Step 3: Final status**

Run:

```bash
git status --short --branch
git log --oneline -10
```

Expected: clean tracked worktree on `phase4/protocol-registry`.

**Step 4: Report**

Report in Chinese:

- Branch.
- Commits.
- Verification commands and results.
- Documentation updates.
- RecallLoom layers updated.
- Next recommended stage, likely 4.3 protocol config UI or diagnostic export.
