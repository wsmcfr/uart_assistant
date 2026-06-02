# Protocol Diagnostics Export Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a read-only protocol diagnostics feature that shows the active protocol summary and exports a complete JSON snapshot for support, Issue reports, and future Lua/plugin troubleshooting.

**Architecture:** Add a core `ProtocolDiagnosticsBuilder` that converts `ProtocolDescriptor`, current config, validation result, and app metadata into a stable `QJsonObject`. Wrap that JSON in a `ProtocolDiagnosticsDialog` for readable summary, copy, and save actions, then expose it from `MainWindow` through `工具 -> 协议诊断...`.

**Tech Stack:** C++17, Qt 5.12.9 Widgets/Core JSON APIs, Qt Test, CMake, existing `ProtocolDescriptor`, `ProtocolConfigSchema`, `ProtocolFactory`, `IProtocol`, `version.h`, and hamburger menu translation tests.

---

### Task 1: Add Red Tests For ProtocolDiagnosticsBuilder JSON

**Files:**
- Create: `tests/unit/TestProtocolDiagnostics.h`
- Create: `tests/unit/TestProtocolDiagnostics.cpp`
- Modify: `tests/main.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test header**

Create `tests/unit/TestProtocolDiagnostics.h`:

```cpp
/**
 * @file TestProtocolDiagnostics.h
 * @brief 协议诊断导出单元测试头文件
 */

#ifndef TESTPROTOCOLDIAGNOSTICS_H
#define TESTPROTOCOLDIAGNOSTICS_H

#include <QObject>
#include <QTest>

/**
 * @brief 协议诊断导出测试类
 *
 * 该测试类验证协议诊断 JSON 是否完整反映注册中心描述、Schema 字段、
 * 当前配置、规范化配置和校验结果，避免后续 Lua/插件排障时事实源缺失。
 */
class TestProtocolDiagnostics : public QObject
{
    Q_OBJECT

private slots:
    void exportsDescriptorAndCapabilities();
    void exportsSchemaAndConfigs();
    void exportsValidationErrorsForInvalidConfig();
    void exportsRawProtocolWithoutFields();
};

#endif // TESTPROTOCOLDIAGNOSTICS_H
```

**Step 2: Write the failing test body**

Create `tests/unit/TestProtocolDiagnostics.cpp`:

```cpp
/**
 * @file TestProtocolDiagnostics.cpp
 * @brief 协议诊断导出单元测试
 */

#include "TestProtocolDiagnostics.h"

#include "core/protocol/ProtocolDiagnostics.h"
#include "core/protocol/ProtocolFactory.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace ComAssistant;

namespace {

/**
 * @brief 构建一个带非法字段的 EasyHEX 配置。
 * @return frameHeader 非法的配置表，用于验证诊断仍能导出错误。
 */
QVariantMap makeInvalidEasyHexConfig()
{
    QVariantMap config = ProtocolFactory::descriptor(ProtocolType::EasyHex).defaultConfig;
    config.insert(QStringLiteral("frameHeader"), QStringLiteral("AA Z1"));
    return config;
}

} // namespace

void TestProtocolDiagnostics::exportsDescriptorAndCapabilities()
{
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::Ascii);
    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        descriptor.defaultConfig,
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    QCOMPARE(json.value(QStringLiteral("diagnosticVersion")).toInt(), 1);
    QCOMPARE(json.value(QStringLiteral("generatedAt")).toString(), QStringLiteral("2026-06-02T12:30:00+08:00"));

    const QJsonObject protocol = json.value(QStringLiteral("protocol")).toObject();
    QCOMPARE(protocol.value(QStringLiteral("id")).toString(), QStringLiteral("ascii"));
    QCOMPARE(protocol.value(QStringLiteral("displayName")).toString(), QStringLiteral("ASCII"));
    QCOMPARE(protocol.value(QStringLiteral("category")).toString(), QStringLiteral("Basic"));

    const QJsonObject capabilities = json.value(QStringLiteral("capabilities")).toObject();
    QVERIFY(capabilities.value(QStringLiteral("builtin")).toBool());
    QVERIFY(capabilities.value(QStringLiteral("frameBuilder")).toBool());
    QVERIFY(!capabilities.value(QStringLiteral("plotProtocol")).toBool());
}

void TestProtocolDiagnostics::exportsSchemaAndConfigs()
{
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::EasyHex);
    QVariantMap currentConfig = descriptor.defaultConfig;
    currentConfig.insert(QStringLiteral("frameHeader"), QStringLiteral("aa-55"));

    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        currentConfig,
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    const QJsonObject configuration = json.value(QStringLiteral("configuration")).toObject();
    QCOMPARE(configuration.value(QStringLiteral("configVersion")).toInt(), descriptor.configVersion);
    QCOMPARE(configuration.value(QStringLiteral("schemaVersion")).toInt(), descriptor.configSchema.version);
    QVERIFY(configuration.value(QStringLiteral("fieldCount")).toInt() >= 5);

    const QJsonArray schemaFields = configuration.value(QStringLiteral("schemaFields")).toArray();
    QVERIFY(!schemaFields.isEmpty());
    QCOMPARE(schemaFields.first().toObject().contains(QStringLiteral("key")), true);
    QCOMPARE(schemaFields.first().toObject().contains(QStringLiteral("type")), true);

    const QJsonObject current = configuration.value(QStringLiteral("currentConfig")).toObject();
    const QJsonObject normalized = configuration.value(QStringLiteral("normalizedConfig")).toObject();
    QCOMPARE(current.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("aa-55"));
    QCOMPARE(normalized.value(QStringLiteral("frameHeader")).toString(), QStringLiteral("AA 55"));

    const QJsonObject validation = json.value(QStringLiteral("validation")).toObject();
    QVERIFY(validation.value(QStringLiteral("valid")).toBool());
}

void TestProtocolDiagnostics::exportsValidationErrorsForInvalidConfig()
{
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::EasyHex);
    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        makeInvalidEasyHexConfig(),
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    const QJsonObject validation = json.value(QStringLiteral("validation")).toObject();
    QVERIFY(!validation.value(QStringLiteral("valid")).toBool());
    QVERIFY(!validation.value(QStringLiteral("errors")).toArray().isEmpty());

    const QJsonObject configuration = json.value(QStringLiteral("configuration")).toObject();
    QCOMPARE(configuration.value(QStringLiteral("normalizedConfig")).toObject().isEmpty(), true);
}

void TestProtocolDiagnostics::exportsRawProtocolWithoutFields()
{
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::Raw);
    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        descriptor.defaultConfig,
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    QCOMPARE(json.value(QStringLiteral("protocol")).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("raw"));
    QCOMPARE(json.value(QStringLiteral("configuration")).toObject().value(QStringLiteral("fieldCount")).toInt(), 0);
    QVERIFY(json.value(QStringLiteral("validation")).toObject().value(QStringLiteral("valid")).toBool());
}
```

**Step 3: Register the test in `tests/main.cpp`**

Add include:

```cpp
#include "unit/TestProtocolDiagnostics.h"
```

Add a test block near protocol tests:

```cpp
{
    qDebug() << "\n[TEST] ProtocolDiagnostics";
    TestProtocolDiagnostics test;
    status |= QTest::qExec(&test, filteredArgs);
}
```

**Step 4: Register the test source in `tests/CMakeLists.txt`**

Add to `TEST_SOURCES`:

```cmake
unit/TestProtocolDiagnostics.cpp
```

**Step 5: Run the red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `src/core/protocol/ProtocolDiagnostics.h` does not exist.

**Step 6: Do not commit yet**

Continue to Task 2.

### Task 2: Implement ProtocolDiagnosticsBuilder

**Files:**
- Create: `src/core/protocol/ProtocolDiagnostics.h`
- Create: `src/core/protocol/ProtocolDiagnostics.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create `ProtocolDiagnostics.h`**

Create `src/core/protocol/ProtocolDiagnostics.h`:

```cpp
/**
 * @file ProtocolDiagnostics.h
 * @brief 协议能力诊断 JSON 构建器
 */

#ifndef COMASSISTANT_PROTOCOLDIAGNOSTICS_H
#define COMASSISTANT_PROTOCOLDIAGNOSTICS_H

#include "ProtocolDescriptor.h"

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

namespace ComAssistant {

/**
 * @brief 构建协议诊断快照的工具类。
 *
 * 该类只依赖核心协议描述和配置 Schema，不依赖 QWidget。
 * 这样 UI、测试、后续 Lua/插件排障都能复用同一份 JSON 事实源。
 */
class ProtocolDiagnosticsBuilder
{
public:
    /**
     * @brief 构建协议诊断 JSON。
     * @param descriptor 当前协议描述。
     * @param currentConfig 当前协议实际配置；可包含未规范化值。
     * @param generatedAt ISO 时间字符串；为空时使用当前本地时间。
     * @return 完整诊断 JSON 对象。
     */
    static QJsonObject build(const ProtocolDescriptor& descriptor,
                             const QVariantMap& currentConfig,
                             const QString& generatedAt = QString());

private:
    ProtocolDiagnosticsBuilder() = delete;
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLDIAGNOSTICS_H
```

**Step 2: Implement `ProtocolDiagnostics.cpp`**

Create `src/core/protocol/ProtocolDiagnostics.cpp`.

Implementation requirements:

- Include `ProtocolDiagnostics.h` and `version.h`.
- Use `QDateTime::currentDateTime().toString(Qt::ISODate)` when `generatedAt` is empty.
- Convert `ProtocolCategory` to strings: `Basic`, `Industrial`, `Custom`, `Plot`.
- Convert `ProtocolType` to stable readable strings or integer plus name; minimum required field is `legacyType`.
- Convert `ProtocolConfigFieldType` to strings: `Bool`, `Integer`, `Double`, `String`, `BytesHex`, `Enum`.
- Use `QJsonObject::fromVariantMap()` for `defaultConfig`, `currentConfig`, and valid `normalizedConfig`.
- On invalid config, keep `currentConfig` as given and export empty `normalizedConfig`.
- Convert `QStringList` to `QJsonArray` for errors, warnings, and enum values.
- Export `application` with `APP_NAME`, `APP_VERSION`, `APP_VERSION_STRING`, and `APP_BUILD_DATE`.

**Step 3: Wire CMake**

Modify root `CMakeLists.txt`:

```cmake
src/core/protocol/ProtocolDiagnostics.cpp
src/core/protocol/ProtocolDiagnostics.h
```

Modify `tests/CMakeLists.txt`:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolDiagnostics.h
${CMAKE_CURRENT_SOURCE_DIR}/../src/core/protocol/ProtocolDiagnostics.cpp
```

**Step 4: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS for `TestProtocolDiagnostics`.

**Step 5: Commit**

Run:

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/main.cpp tests/unit/TestProtocolDiagnostics.h tests/unit/TestProtocolDiagnostics.cpp src/core/protocol/ProtocolDiagnostics.h src/core/protocol/ProtocolDiagnostics.cpp
git commit -m "feat: add protocol diagnostics json builder"
```

### Task 3: Add Red Tests For ProtocolDiagnosticsDialog

**Files:**
- Modify: `tests/unit/TestProtocolDiagnostics.h`
- Modify: `tests/unit/TestProtocolDiagnostics.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Add dialog test slots**

Add to `TestProtocolDiagnostics.h`:

```cpp
void dialogShowsSummaryAndJson();
void dialogCopyButtonExists();
```

**Step 2: Add failing dialog tests**

Add includes:

```cpp
#include "ui/dialogs/ProtocolDiagnosticsDialog.h"
#include <QPlainTextEdit>
#include <QPushButton>
```

Add tests:

```cpp
void TestProtocolDiagnostics::dialogShowsSummaryAndJson()
{
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::Ascii);
    const QJsonObject json = ProtocolDiagnosticsBuilder::build(
        descriptor,
        descriptor.defaultConfig,
        QStringLiteral("2026-06-02T12:30:00+08:00"));

    ProtocolDiagnosticsDialog dialog(json);

    auto* jsonEdit = dialog.findChild<QPlainTextEdit*>(QStringLiteral("protocolDiagnosticsJsonEdit"));
    QVERIFY(jsonEdit != nullptr);
    QVERIFY(jsonEdit->isReadOnly());
    QVERIFY(jsonEdit->toPlainText().contains(QStringLiteral("\"diagnosticVersion\"")));
    QVERIFY(jsonEdit->toPlainText().contains(QStringLiteral("\"ascii\"")));
}

void TestProtocolDiagnostics::dialogCopyButtonExists()
{
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(ProtocolType::Ascii);
    ProtocolDiagnosticsDialog dialog(ProtocolDiagnosticsBuilder::build(descriptor, descriptor.defaultConfig));

    QVERIFY(dialog.findChild<QPushButton*>(QStringLiteral("protocolDiagnosticsCopyButton")) != nullptr);
    QVERIFY(dialog.findChild<QPushButton*>(QStringLiteral("protocolDiagnosticsSaveButton")) != nullptr);
}
```

**Step 3: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `ProtocolDiagnosticsDialog.h` does not exist.

**Step 4: Do not commit yet**

Continue to Task 4.

### Task 4: Implement ProtocolDiagnosticsDialog

**Files:**
- Create: `src/ui/dialogs/ProtocolDiagnosticsDialog.h`
- Create: `src/ui/dialogs/ProtocolDiagnosticsDialog.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create `ProtocolDiagnosticsDialog.h`**

Create `src/ui/dialogs/ProtocolDiagnosticsDialog.h`:

```cpp
/**
 * @file ProtocolDiagnosticsDialog.h
 * @brief 协议诊断对话框
 */

#ifndef COMASSISTANT_PROTOCOLDIAGNOSTICSDIALOG_H
#define COMASSISTANT_PROTOCOLDIAGNOSTICSDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace ComAssistant {

/**
 * @brief 展示协议诊断摘要和完整 JSON 的只读对话框。
 *
 * 该对话框不编辑协议配置，只提供可读摘要、完整 JSON、复制和保存能力，
 * 避免与协议配置对话框职责重叠。
 */
class ProtocolDiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 创建协议诊断对话框。
     * @param diagnostics 已构建的诊断 JSON 对象。
     * @param parent 父窗口。
     */
    explicit ProtocolDiagnosticsDialog(const QJsonObject& diagnostics,
                                       QWidget* parent = nullptr);

    /**
     * @brief 返回格式化后的 JSON 文本。
     * @return 用于复制和保存的诊断 JSON 字符串。
     */
    QString jsonText() const;

private slots:
    /**
     * @brief 复制诊断 JSON 到剪贴板。
     */
    void copyJson();

    /**
     * @brief 选择路径并保存诊断 JSON。
     */
    void saveJson();

private:
    /**
     * @brief 创建只读摘要和 JSON 文本区。
     */
    void setupUi();

    /**
     * @brief 从 JSON 中刷新可读摘要标签。
     */
    void populateSummary();

    QJsonObject m_diagnostics;          ///< 完整诊断 JSON。
    QString m_jsonText;                 ///< 格式化 JSON 文本。
    QLabel* m_protocolLabel = nullptr;  ///< 协议概览标签。
    QLabel* m_capabilityLabel = nullptr;///< 能力摘要标签。
    QLabel* m_configLabel = nullptr;    ///< 配置摘要标签。
    QLabel* m_validationLabel = nullptr;///< 校验结果标签。
    QPlainTextEdit* m_jsonEdit = nullptr; ///< 只读 JSON 文本框。
    QPushButton* m_copyButton = nullptr;  ///< 复制 JSON 按钮。
    QPushButton* m_saveButton = nullptr;  ///< 保存 JSON 按钮。
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLDIAGNOSTICSDIALOG_H
```

**Step 2: Implement `ProtocolDiagnosticsDialog.cpp`**

Create `src/ui/dialogs/ProtocolDiagnosticsDialog.cpp`.

Implementation requirements:

- Use `QJsonDocument(m_diagnostics).toJson(QJsonDocument::Indented)` for `m_jsonText`.
- Window title: `tr("协议诊断")`.
- Size: about `720 x 560`.
- Summary labels should include protocol name/id, capability booleans, config version/schema version/field count, and validation result.
- `m_jsonEdit` objectName must be `protocolDiagnosticsJsonEdit`.
- `m_copyButton` objectName must be `protocolDiagnosticsCopyButton`.
- `m_saveButton` objectName must be `protocolDiagnosticsSaveButton`.
- `copyJson()` should call `QApplication::clipboard()->setText(m_jsonText)` and show a short information message or update label/status inside the dialog.
- `saveJson()` should call `QFileDialog::getSaveFileName()`, write UTF-8 JSON with `QFile`, and use `QMessageBox::warning()` on failure.
- Add clear Chinese comments for functions, key variables, and error branches.

**Step 3: Wire CMake**

Modify root `CMakeLists.txt`:

```cmake
src/ui/dialogs/ProtocolDiagnosticsDialog.cpp
src/ui/dialogs/ProtocolDiagnosticsDialog.h
```

Modify `tests/CMakeLists.txt`:

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ProtocolDiagnosticsDialog.h
${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/dialogs/ProtocolDiagnosticsDialog.cpp
```

**Step 4: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS for builder and dialog tests.

**Step 5: Commit**

Run:

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/unit/TestProtocolDiagnostics.h tests/unit/TestProtocolDiagnostics.cpp src/ui/dialogs/ProtocolDiagnosticsDialog.h src/ui/dialogs/ProtocolDiagnosticsDialog.cpp
git commit -m "feat: add protocol diagnostics dialog"
```

### Task 5: Expose Diagnostics From MainWindow And Translation Tests

**Files:**
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `tests/unit/TestTranslationCompleteness.cpp`
- Modify: `resources/translations/en_US.ts`

**Step 1: Add failing translation coverage**

In `tests/unit/TestTranslationCompleteness.cpp`, extend `testMainWindowHamburgerMenuTranslations()`:

```cpp
{QStringLiteral("协议诊断..."), QStringLiteral("Protocol Diagnostics...")},
```

Optionally add dialog critical labels if a new context is present:

```cpp
{QStringLiteral("协议诊断"), QStringLiteral("Protocol Diagnostics")},
{QStringLiteral("复制 JSON"), QStringLiteral("Copy JSON")},
{QStringLiteral("保存 JSON..."), QStringLiteral("Save JSON...")}
```

**Step 2: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL because `协议诊断...` translation is missing or the menu text is not present yet.

**Step 3: Add MainWindow slot and menu entry**

In `src/ui/MainWindow.h`, add private slot near `onProtocolConfig()`:

```cpp
void onProtocolDiagnostics();  ///< 打开当前协议的只读诊断导出对话框
```

In `src/ui/MainWindow.cpp`, add includes:

```cpp
#include "dialogs/ProtocolDiagnosticsDialog.h"
#include "protocol/ProtocolDiagnostics.h"
```

In `populateHamburgerMenu()`, add near `协议配置...`:

```cpp
toolsMenu->addAction(tr("协议诊断..."), this, &MainWindow::onProtocolDiagnostics);
```

Implement:

```cpp
void MainWindow::onProtocolDiagnostics()
{
    /*
     * 诊断入口只读取当前协议状态，不修改配置，也不强行创建 Raw 协议实例。
     * 这样用户可以安全导出当前事实源，用于 Issue 或后续 Lua/插件排障。
     */
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(m_currentProtocolType);
    QVariantMap currentConfig = m_currentProtocol
        ? m_currentProtocol->config()
        : descriptor.defaultConfig;
    if (currentConfig.isEmpty()) {
        currentConfig = descriptor.defaultConfig;
    }

    ProtocolDiagnosticsDialog dialog(
        ProtocolDiagnosticsBuilder::build(descriptor, currentConfig),
        this);
    dialog.exec();
}
```

**Step 4: Update English translations**

Edit `resources/translations/en_US.ts` in the existing style:

- `协议诊断...` -> `Protocol Diagnostics...`
- `协议诊断` -> `Protocol Diagnostics`
- `复制 JSON` -> `Copy JSON`
- `保存 JSON...` -> `Save JSON...`
- `关闭` if not already translated in this context.
- Any new summary labels introduced by the dialog.

Keep all translations finished. Do not generate `.qm` manually unless the build target does it.

**Step 5: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS, including `TestTranslationCompleteness`.

**Step 6: Commit**

Run:

```bash
git add src/ui/MainWindow.h src/ui/MainWindow.cpp tests/unit/TestTranslationCompleteness.cpp resources/translations/en_US.ts
git commit -m "feat: expose protocol diagnostics dialog"
```

### Task 6: Document Protocol Diagnostics

**Files:**
- Modify: `resources/help/protocols.html`
- Review: `resources/help/quickstart.html`

**Step 1: Update protocol help**

In `resources/help/protocols.html`, near the protocol capability platform section, add Chinese and English paragraphs:

```html
<p><strong>中文：</strong>可通过 <code>工具 -&gt; 协议诊断...</code> 打开当前协议的只读诊断面板，查看协议 ID、分类、能力、配置版本、Schema 字段数量和当前配置校验结果。</p>
<p><strong>English:</strong> Use <code>Tools -&gt; Protocol Diagnostics...</code> to open a read-only diagnostics panel for the current protocol, including protocol ID, category, capabilities, config version, schema field count, and current config validation result.</p>
<p><strong>中文：</strong>诊断面板支持复制或保存完整 JSON，适合在反馈问题、排查会话配置、后续 Lua 或插件协议接入时作为事实源。</p>
<p><strong>English:</strong> The diagnostics panel can copy or save the full JSON snapshot, which is useful for issue reports, session config troubleshooting, and future Lua or plugin protocol integration.</p>
```

**Step 2: Review quickstart**

Check whether this feature changes quick connection, sending, receiving, session shortcuts, or the main onboarding flow.

Expected decision:

```text
resources/help/quickstart.html 不更新，因为协议诊断是高级工具菜单入口，不改变快速连接、发送、接收、会话保存快捷键和主流程；协议说明页已补充入口和用途。
```

If implementation unexpectedly changes onboarding behavior, update `quickstart.html` in the same commit.

**Step 3: Run docs-related tests**

Run:

```bash
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS including `DocumentationLinks` and translation checks.

**Step 4: Commit**

Run:

```bash
git add resources/help/protocols.html resources/help/quickstart.html
git commit -m "docs: document protocol diagnostics export"
```

If quickstart did not change, omit it from `git add`.

### Task 7: Final Verification And RecallLoom Update

**Files:**
- Update via RecallLoom helpers:
  - `.recallloom/rolling_summary.md`
  - `.recallloom/daily_logs/2026-06-02.md`

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

- `ComAssistant_tests.exe` prints `All tests PASSED!`.
- CTest reports `100% tests passed, 0 tests failed out of 1`.
- Release build deploys `D:\comassistant\build_release\ComAssistant.exe`.
- `git diff --check` exits 0 with no output.
- RecallLoom validation reports `Errors: 0`.

**Step 2: Update RecallLoom**

Use revision-aware helpers:

```bash
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\preflight_context_check.py . --json
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\commit_context_file.py . --file-key rolling_summary --source-file <prepared-summary> --expected-file-revision <rev> --expected-workspace-revision <rev> --writer-id Codex --json
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\append_daily_log_entry.py . --date 2026-06-02 --entry-file <prepared-entry> --expected-workspace-revision <rev> --writer-id Codex --json
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\validate_context.py .
```

Record:

- 4.4 commit range.
- Builder, dialog, menu, translation, and documentation results.
- Final verification command results.
- `resources/help/quickstart.html` decision.
- Remaining risks: no Lua execution, no plugin loading, no real hardware validation.

**Step 3: Final status**

Run:

```bash
git status --short --branch
git log --oneline -12
```

Expected: clean tracked worktree on `phase4/protocol-registry`, ignoring local `.recallloom/` sidecar state.

**Step 4: Report in Chinese**

Include:

- Branch and 4.4 commits.
- Verification commands and results.
- Documentation updates and quickstart decision.
- RecallLoom layers updated.
- Next recommended stage, likely 4.5 Lua safety sandbox design.
