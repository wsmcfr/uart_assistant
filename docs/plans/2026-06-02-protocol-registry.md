# Protocol Registry Foundation Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build the 4.1 protocol registry foundation so built-in protocols are described, registered, queried, and created through a stable capability registry while old `ProtocolFactory` APIs keep working.

**Architecture:** Add `ProtocolDescriptor` for metadata and `ProtocolRegistry` for registration/query/creation. Register all built-in protocols lazily, keep Raw as a describable no-protocol entry, and route `ProtocolFactory` through the registry without changing callers.

**Tech Stack:** C++17, Qt 5.12, Qt Test, CMake, existing `IProtocol` implementations.

---

### Task 1: Add Red Tests For Built-In Protocol Metadata

**Files:**
- Create: `tests/unit/TestProtocolRegistry.h`
- Create: `tests/unit/TestProtocolRegistry.cpp`
- Modify: `tests/main.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

Create `tests/unit/TestProtocolRegistry.h`:

```cpp
/**
 * @file TestProtocolRegistry.h
 * @brief 协议注册中心单元测试头文件
 */

#ifndef TESTPROTOCOLREGISTRY_H
#define TESTPROTOCOLREGISTRY_H

#include <QObject>
#include <QTest>

class TestProtocolRegistry : public QObject
{
    Q_OBJECT

private slots:
    void builtinDescriptorsAreRegistered();
};

#endif // TESTPROTOCOLREGISTRY_H
```

Create `tests/unit/TestProtocolRegistry.cpp`:

```cpp
/**
 * @file TestProtocolRegistry.cpp
 * @brief 协议注册中心单元测试
 */

#include "TestProtocolRegistry.h"

#include "core/protocol/ProtocolRegistry.h"

using namespace ComAssistant;

void TestProtocolRegistry::builtinDescriptorsAreRegistered()
{
    /*
     * 内置协议目录是后续配置 schema、Lua 协议和诊断包共用的事实源。
     * 这里先锁定数量、稳定 ID 和旧版类型映射，避免平台化第一步丢协议。
     */
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const QList<ProtocolDescriptor> descriptors = registry.descriptors();
    QCOMPARE(descriptors.size(), 10);

    QVERIFY(registry.contains(QStringLiteral("raw")));
    QVERIFY(registry.contains(QStringLiteral("ascii")));
    QVERIFY(registry.contains(QStringLiteral("hex")));
    QVERIFY(registry.contains(QStringLiteral("modbus")));
    QVERIFY(registry.contains(QStringLiteral("custom")));
    QVERIFY(registry.contains(QStringLiteral("easyhex")));
    QVERIFY(registry.contains(QStringLiteral("plot.text")));
    QVERIFY(registry.contains(QStringLiteral("plot.stamp")));
    QVERIFY(registry.contains(QStringLiteral("plot.csv")));
    QVERIFY(registry.contains(QStringLiteral("plot.justfloat")));

    const ProtocolDescriptor ascii = registry.descriptor(QStringLiteral("ascii"));
    QCOMPARE(ascii.id, QStringLiteral("ascii"));
    QCOMPARE(ascii.displayName, QStringLiteral("ASCII"));
    QCOMPARE(ascii.legacyType, ProtocolType::Ascii);
    QVERIFY(ascii.builtin);
    QVERIFY(ascii.frameBuilder);
    QVERIFY(!ascii.plotProtocol);
}
```

Modify `tests/main.cpp`:

```cpp
#include "unit/TestProtocolRegistry.h"
```

Add this block before the existing utility tests or near other protocol/core tests:

```cpp
{
    qDebug() << "\n[TEST] ProtocolRegistry";
    TestProtocolRegistry test;
    status |= QTest::qExec(&test, filteredArgs);
}
```

Modify `tests/CMakeLists.txt` and add to `TEST_SOURCES`:

```cmake
unit/TestProtocolRegistry.cpp
```

**Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `core/protocol/ProtocolRegistry.h` does not exist.

**Step 3: Commit is not allowed yet**

Do not commit red tests alone unless the user explicitly asks. Continue to Task 2.

### Task 2: Implement ProtocolDescriptor And Minimal Registry

**Files:**
- Create: `src/core/protocol/ProtocolDescriptor.h`
- Create: `src/core/protocol/ProtocolRegistry.h`
- Create: `src/core/protocol/ProtocolRegistry.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write minimal implementation**

Create `src/core/protocol/ProtocolDescriptor.h`:

```cpp
/**
 * @file ProtocolDescriptor.h
 * @brief 协议能力描述结构
 */

#ifndef COMASSISTANT_PROTOCOLDESCRIPTOR_H
#define COMASSISTANT_PROTOCOLDESCRIPTOR_H

#include "IProtocol.h"

#include <QVariantMap>
#include <QString>

namespace ComAssistant {

/**
 * @brief 协议分类
 *
 * 分类用于后续 UI 分组、诊断包导出和配置 schema 管理。
 */
enum class ProtocolCategory
{
    Basic,      ///< 基础显示/构帧协议
    Industrial, ///< 工业通信协议
    Custom,     ///< 用户自定义或可配置协议
    Plot        ///< 绘图协议
};

/**
 * @brief 协议能力描述
 *
 * 描述一个协议的稳定 ID、显示信息、能力标志和旧版枚举映射。
 * 后续外部插件和 Lua 协议也会通过同样结构登记能力。
 */
struct ProtocolDescriptor
{
    QString id;                         ///< 稳定协议 ID，用于配置、诊断和插件注册
    QString displayName;                ///< 面向用户显示的协议名称
    QString description;                ///< 协议用途说明
    ProtocolCategory category = ProtocolCategory::Basic; ///< 协议分类
    ProtocolType legacyType = ProtocolType::Raw;         ///< 旧版 ProtocolType 映射
    bool builtin = true;                ///< 是否为内置协议
    bool plotProtocol = false;          ///< 是否支持绘图数据解析
    bool frameBuilder = false;          ///< 是否支持 payload 构建发送帧
    QVariantMap defaultConfig;          ///< 默认配置，为后续 schema 扩展预留
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLDESCRIPTOR_H
```

Create `src/core/protocol/ProtocolRegistry.h`:

```cpp
/**
 * @file ProtocolRegistry.h
 * @brief 协议注册中心
 */

#ifndef COMASSISTANT_PROTOCOLREGISTRY_H
#define COMASSISTANT_PROTOCOLREGISTRY_H

#include "ProtocolDescriptor.h"

#include <QList>
#include <QMap>
#include <QString>

namespace ComAssistant {

/**
 * @brief 协议注册中心
 *
 * 注册中心保存协议描述与创建器，让协议能力可以被查询、创建和后续扩展。
 */
class ProtocolRegistry
{
public:
    /**
     * @brief 注册一个协议能力
     * @param descriptor 协议描述，id 必须非空且不能重复
     * @param creator 协议创建器；Raw 可为空，因为 Raw 表示无协议
     * @param errorMessage 注册失败时写入原因
     * @return 注册是否成功
     */
    bool registerProtocol(const ProtocolDescriptor& descriptor,
                          ProtocolCreator creator,
                          QString* errorMessage = nullptr);

    /**
     * @brief 注册全部内置协议
     *
     * 该函数可重复调用，第二次不会重复注册。
     */
    void registerBuiltinProtocols();

    /**
     * @brief 判断指定协议 ID 是否存在
     */
    bool contains(const QString& id) const;

    /**
     * @brief 查询指定协议描述
     */
    ProtocolDescriptor descriptor(const QString& id) const;

    /**
     * @brief 返回全部协议描述
     */
    QList<ProtocolDescriptor> descriptors() const;

    /**
     * @brief 按 ID 创建协议实例
     */
    IProtocol* create(const QString& id, QObject* parent = nullptr) const;

private:
    QList<QString> m_orderedIds;                 ///< 注册顺序，用于保持稳定列表输出
    QMap<QString, ProtocolDescriptor> m_descriptors; ///< 协议 ID 到描述的映射
    QMap<QString, ProtocolCreator> m_creators;   ///< 协议 ID 到创建器的映射
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLREGISTRY_H
```

Create `src/core/protocol/ProtocolRegistry.cpp` with built-in registration for all 10 protocols. Include every protocol header already included by `ProtocolFactory.h`.

**Step 2: Wire CMake**

Modify `tests/CMakeLists.txt`:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolDescriptor.h
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolRegistry.h
```

Add source:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolRegistry.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/AsciiProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/HexProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ModbusProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/CustomProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/EasyHexProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/TextProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/StampProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/CsvProtocol.cpp
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/JustFloatProtocol.cpp
```

If some protocol source is already linked indirectly, keep only one copy to avoid duplicate symbols.

**Step 3: Run test to verify it passes**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS for `TestProtocolRegistry::builtinDescriptorsAreRegistered`.

**Step 4: Commit**

Run:

```bash
git add tests/unit/TestProtocolRegistry.h tests/unit/TestProtocolRegistry.cpp tests/main.cpp tests/CMakeLists.txt src/core/protocol/ProtocolDescriptor.h src/core/protocol/ProtocolRegistry.h src/core/protocol/ProtocolRegistry.cpp
git commit -m "feat: add protocol registry metadata"
```

### Task 3: Add Registration Error And Raw Compatibility Tests

**Files:**
- Modify: `tests/unit/TestProtocolRegistry.h`
- Modify: `tests/unit/TestProtocolRegistry.cpp`

**Step 1: Write failing tests**

Add slots:

```cpp
void rejectsInvalidRegistrations();
void keepsRawCompatibility();
```

Add tests:

```cpp
void TestProtocolRegistry::rejectsInvalidRegistrations()
{
    /*
     * 注册中心是插件化入口，非法注册必须在入口被拒绝，
     * 避免后续配置、诊断和 UI 看到不完整协议能力。
     */
    ProtocolRegistry registry;
    QString errorMessage;

    ProtocolDescriptor emptyId;
    emptyId.displayName = QStringLiteral("Empty");
    QVERIFY(!registry.registerProtocol(emptyId, [](QObject*) -> IProtocol* { return nullptr; }, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    ProtocolDescriptor ascii;
    ascii.id = QStringLiteral("ascii");
    ascii.displayName = QStringLiteral("ASCII");
    ascii.legacyType = ProtocolType::Ascii;
    QVERIFY(registry.registerProtocol(ascii, [](QObject* parent) -> IProtocol* { return new AsciiProtocol(parent); }));
    QVERIFY(!registry.registerProtocol(ascii, [](QObject* parent) -> IProtocol* { return new AsciiProtocol(parent); }, &errorMessage));
    QVERIFY(!errorMessage.isEmpty());

    ProtocolDescriptor noCreator;
    noCreator.id = QStringLiteral("no.creator");
    noCreator.displayName = QStringLiteral("No Creator");
    noCreator.legacyType = ProtocolType::Custom;
    QVERIFY(!registry.registerProtocol(noCreator, ProtocolCreator(), &errorMessage));
    QVERIFY(!errorMessage.isEmpty());
}

void TestProtocolRegistry::keepsRawCompatibility()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const ProtocolDescriptor raw = registry.descriptor(QStringLiteral("raw"));
    QCOMPARE(raw.id, QStringLiteral("raw"));
    QCOMPARE(raw.legacyType, ProtocolType::Raw);
    QVERIFY(raw.builtin);

    IProtocol* protocol = registry.create(QStringLiteral("raw"));
    QVERIFY(protocol == nullptr);
}
```

Add include:

```cpp
#include "core/protocol/AsciiProtocol.h"
```

**Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL if invalid registration or Raw handling is incomplete.

**Step 3: Implement minimal fix**

Update `ProtocolRegistry::registerProtocol()`:

- Reject empty trimmed ID.
- Reject duplicate ID.
- Allow empty creator only when `descriptor.legacyType == ProtocolType::Raw`.
- Store ordered ID only after validation.

Update `ProtocolRegistry::create()`:

- Return `nullptr` for missing ID.
- Return `nullptr` for Raw or empty creator.

**Step 4: Run test to verify it passes**

Run:

```bash
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 5: Commit**

```bash
git add tests/unit/TestProtocolRegistry.h tests/unit/TestProtocolRegistry.cpp src/core/protocol/ProtocolRegistry.cpp
git commit -m "test: cover protocol registry validation"
```

### Task 4: Add Creation, Plot Metadata, And Factory Compatibility Tests

**Files:**
- Modify: `tests/unit/TestProtocolRegistry.h`
- Modify: `tests/unit/TestProtocolRegistry.cpp`
- Modify: `src/core/protocol/ProtocolFactory.h`
- Modify: `src/core/protocol/ProtocolFactory.cpp`

**Step 1: Write failing tests**

Add slots:

```cpp
void createsProtocolById();
void marksPlotProtocols();
void keepsFactoryCompatibility();
```

Add tests:

```cpp
void TestProtocolRegistry::createsProtocolById()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    std::unique_ptr<IProtocol> ascii(registry.create(QStringLiteral("ascii")));
    QVERIFY(ascii != nullptr);
    QCOMPARE(ascii->type(), ProtocolType::Ascii);

    std::unique_ptr<IProtocol> hex(registry.create(QStringLiteral("hex")));
    QVERIFY(hex != nullptr);
    QCOMPARE(hex->type(), ProtocolType::Hex);

    std::unique_ptr<IProtocol> text(registry.create(QStringLiteral("plot.text")));
    QVERIFY(text != nullptr);
    QVERIFY(text->isPlotProtocol());
}

void TestProtocolRegistry::marksPlotProtocols()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    QVERIFY(registry.descriptor(QStringLiteral("plot.text")).plotProtocol);
    QVERIFY(registry.descriptor(QStringLiteral("plot.stamp")).plotProtocol);
    QVERIFY(registry.descriptor(QStringLiteral("plot.csv")).plotProtocol);
    QVERIFY(registry.descriptor(QStringLiteral("plot.justfloat")).plotProtocol);
    QVERIFY(!registry.descriptor(QStringLiteral("ascii")).plotProtocol);
}

void TestProtocolRegistry::keepsFactoryCompatibility()
{
    const QList<ProtocolType> types = ProtocolFactory::supportedTypes();
    QCOMPARE(types.size(), 10);
    QVERIFY(types.contains(ProtocolType::Raw));
    QVERIFY(types.contains(ProtocolType::JustFloat));

    QCOMPARE(ProtocolFactory::typeName(ProtocolType::Ascii), QStringLiteral("ASCII"));
    QCOMPARE(ProtocolFactory::typeName(ProtocolType::TextPlot), QStringLiteral("TEXT绘图"));

    std::unique_ptr<IProtocol> ascii(ProtocolFactory::create(ProtocolType::Ascii));
    QVERIFY(ascii != nullptr);
    QCOMPARE(ascii->type(), ProtocolType::Ascii);

    std::unique_ptr<IProtocol> raw(ProtocolFactory::create(ProtocolType::Raw));
    QVERIFY(raw == nullptr);
}
```

Add include:

```cpp
#include "core/protocol/ProtocolFactory.h"
```

**Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL until all descriptors and factory routing are correct.

**Step 3: Implement registry-backed factory**

Modify `ProtocolFactory.h`:

- Include `ProtocolRegistry.h`.
- Add static helpers if useful:

```cpp
static QString typeId(ProtocolType type);
static ProtocolDescriptor descriptor(ProtocolType type);
static const ProtocolRegistry& registry();
```

Modify `ProtocolFactory.cpp`:

- Add a function-local static registry initialized with `registerBuiltinProtocols()`.
- Route `create(ProtocolType, QObject*)` through registry by ID.
- Route unique_ptr `create(ProtocolType)` by wrapping the raw pointer returned from registry.
- Route `typeName()` through descriptor displayName, preserving current exact strings.
- Route `supportedTypes()` through descriptor legacy mapping in stable registration order.

Keep dedicated typed factory methods such as `createAscii()` returning concrete types, because existing callers may rely on them.

**Step 4: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 5: Commit**

```bash
git add tests/unit/TestProtocolRegistry.h tests/unit/TestProtocolRegistry.cpp src/core/protocol/ProtocolFactory.h src/core/protocol/ProtocolFactory.cpp src/core/protocol/ProtocolRegistry.cpp src/core/protocol/ProtocolRegistry.h
git commit -m "feat: route protocol factory through registry"
```

### Task 5: Add Category Filtering And Protocol Documentation

**Files:**
- Modify: `src/core/protocol/ProtocolRegistry.h`
- Modify: `src/core/protocol/ProtocolRegistry.cpp`
- Modify: `tests/unit/TestProtocolRegistry.h`
- Modify: `tests/unit/TestProtocolRegistry.cpp`
- Modify: `resources/help/protocols.html`

**Step 1: Write failing test**

Add slot:

```cpp
void filtersDescriptorsByCategory();
```

Add test:

```cpp
void TestProtocolRegistry::filtersDescriptorsByCategory()
{
    ProtocolRegistry registry;
    registry.registerBuiltinProtocols();

    const QList<ProtocolDescriptor> plotProtocols = registry.descriptorsByCategory(ProtocolCategory::Plot);
    QCOMPARE(plotProtocols.size(), 4);
    for (const ProtocolDescriptor& descriptor : plotProtocols) {
        QVERIFY(descriptor.plotProtocol);
        QCOMPARE(descriptor.category, ProtocolCategory::Plot);
    }
}
```

**Step 2: Run test to verify it fails**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `descriptorsByCategory()` does not exist.

**Step 3: Implement minimal code**

Add to `ProtocolRegistry.h`:

```cpp
/**
 * @brief 按分类返回协议描述列表
 * @param category 目标协议分类
 * @return 保持注册顺序的协议描述列表
 */
QList<ProtocolDescriptor> descriptorsByCategory(ProtocolCategory category) const;
```

Implement in `ProtocolRegistry.cpp` by iterating `m_orderedIds`.

**Step 4: Update help document**

Modify `resources/help/protocols.html` near the introduction:

```html
<div class="protocol-box">
    <h3>协议能力平台化 / Protocol Capability Platform</h3>
    <p><strong>中文：</strong>内置协议已纳入统一协议能力目录，程序可以按稳定协议 ID 查询协议名称、分类、绘图能力和构帧能力，为后续脚本协议、外部插件和诊断包扩展做准备。</p>
    <p><strong>English:</strong> Built-in protocols are organized through a unified capability registry with stable IDs, categories, plotting capability, and frame-building capability. This prepares the app for future script protocols, external plugins, and diagnostic exports.</p>
</div>
```

Do not update `resources/help/quickstart.html` unless implementation changes a quick-start button, menu, or workflow.

**Step 5: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 6: Commit**

```bash
git add tests/unit/TestProtocolRegistry.h tests/unit/TestProtocolRegistry.cpp src/core/protocol/ProtocolRegistry.h src/core/protocol/ProtocolRegistry.cpp resources/help/protocols.html
git commit -m "feat: expose protocol categories"
```

### Task 6: Final Verification And Memory Update

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

Expected: all commands exit 0. RecallLoom validation must show `Errors: 0`; if warnings appear, resolve or explain them.

**Step 2: Update RecallLoom**

Use RecallLoom revision-aware helper scripts:

- Update `.recallloom/rolling_summary.md` with 4.1 implementation status, verification evidence, remaining risks, and next step.
- Append `.recallloom/daily_logs/2026-06-02.md` with a milestone entry for completed 4.1 implementation.
- Do not hand-edit `state.json`.

**Step 3: Final commit**

If RecallLoom remains ignored, do not force-add it unless project policy changes. Commit tracked code/docs changes:

```bash
git status --short --branch
git add src/core/protocol tests resources/help/protocols.html tests/CMakeLists.txt tests/main.cpp
git commit -m "feat: add protocol registry foundation"
```

If previous task commits already cover all changes, skip this commit.

**Step 4: Report**

Report:

- Branch name.
- Commit hashes created.
- Verification commands and results.
- Whether `resources/help/quickstart.html` was updated; if not, state that quick-start workflow did not change.
- Which RecallLoom layers were updated.
