# Protocol Config UI Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a schema-driven protocol configuration UI so protocol config fields from 4.2 can be viewed, edited, validated, restored to defaults, applied to the active protocol, and persisted through sessions.

**Architecture:** Add a reusable `ProtocolConfigEditor` widget that maps `ProtocolConfigSchema` fields to Qt controls, then wrap it in `ProtocolConfigDialog`. Keep `MainWindow` responsible only for opening the dialog and applying accepted normalized config to the current protocol.

**Tech Stack:** C++17, Qt 5.12.9 Widgets, Qt Test, CMake, existing `ProtocolConfigSchema`, `ProtocolFactory`, `SessionData`, and `MainWindow`.

---

### Task 1: Add Red Tests For ProtocolConfigEditor Widget Generation

**Files:**
- Create: `tests/unit/TestProtocolConfigEditor.h`
- Create: `tests/unit/TestProtocolConfigEditor.cpp`
- Modify: `tests/main.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test header**

Create `tests/unit/TestProtocolConfigEditor.h`:

```cpp
/**
 * @file TestProtocolConfigEditor.h
 * @brief 协议配置编辑器单元测试头文件
 */

#ifndef TESTPROTOCOLCONFIGEDITOR_H
#define TESTPROTOCOLCONFIGEDITOR_H

#include <QObject>
#include <QTest>

/**
 * @brief 协议配置编辑器测试类
 *
 * 该测试类验证 Schema 字段可以生成对应 Qt 控件，并且控件 objectName 稳定，
 * 便于后续对话框、翻译和回归测试定位。
 */
class TestProtocolConfigEditor : public QObject
{
    Q_OBJECT

private slots:
    void buildsWidgetsFromSchema();
};

#endif // TESTPROTOCOLCONFIGEDITOR_H
```

**Step 2: Write the failing test body**

Create `tests/unit/TestProtocolConfigEditor.cpp`:

```cpp
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
```

**Step 3: Register test in `tests/main.cpp`**

Add include near other unit tests:

```cpp
#include "unit/TestProtocolConfigEditor.h"
```

Add a test block near `ProtocolConfigSchema`:

```cpp
{
    qDebug() << "\n[TEST] ProtocolConfigEditor";
    TestProtocolConfigEditor test;
    status |= QTest::qExec(&test, filteredArgs);
}
```

**Step 4: Register source in `tests/CMakeLists.txt`**

Add to `TEST_SOURCES`:

```cmake
unit/TestProtocolConfigEditor.cpp
```

**Step 5: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `src/ui/widgets/ProtocolConfigEditor.h` does not exist.

**Step 6: Do not commit yet**

Continue to Task 2.

### Task 2: Implement Minimal ProtocolConfigEditor Widget Generation

**Files:**
- Create: `src/ui/widgets/ProtocolConfigEditor.h`
- Create: `src/ui/widgets/ProtocolConfigEditor.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Create `ProtocolConfigEditor.h`**

Create `src/ui/widgets/ProtocolConfigEditor.h`:

```cpp
/**
 * @file ProtocolConfigEditor.h
 * @brief 协议配置 Schema 编辑器
 */

#ifndef COMASSISTANT_PROTOCOLCONFIGEDITOR_H
#define COMASSISTANT_PROTOCOLCONFIGEDITOR_H

#include "core/protocol/ProtocolConfigSchema.h"

#include <QMap>
#include <QVariantMap>
#include <QWidget>

class QFormLayout;
class QLabel;

namespace ComAssistant {

/**
 * @brief 根据 ProtocolConfigSchema 自动生成表单控件的编辑器。
 *
 * 该控件只负责 Schema 到 Qt 控件的映射、配置加载、配置读取和错误提示；
 * 不负责创建协议实例，也不直接修改 MainWindow 状态。
 */
class ProtocolConfigEditor : public QWidget
{
    Q_OBJECT

public:
    explicit ProtocolConfigEditor(QWidget* parent = nullptr);

    /**
     * @brief 设置配置 Schema 并重建表单。
     * @param schema 协议配置 Schema。
     */
    void setSchema(const ProtocolConfigSchema& schema);

private:
    QWidget* createFieldWidget(const ProtocolConfigField& field);

    ProtocolConfigSchema m_schema;      ///< 当前 Schema。
    QFormLayout* m_formLayout = nullptr;///< 表单布局。
    QLabel* m_emptyLabel = nullptr;     ///< 空 Schema 提示。
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLCONFIGEDITOR_H
```

**Step 2: Create `ProtocolConfigEditor.cpp`**

Implement:

- Constructor creates a `QVBoxLayout`, `QFormLayout`, and empty label.
- `setSchema()` clears old rows and rebuilds one widget per field.
- `createFieldWidget()` maps:
  - Bool -> `QCheckBox`
  - Integer -> `QSpinBox`
  - Double -> `QDoubleSpinBox`
  - String / BytesHex -> `QLineEdit`
  - Enum -> `QComboBox`
- Set objectName to `protocolConfig_<key>`.
- Set tooltip to field description.
- For numeric controls, apply min/max values when valid.

**Step 3: Wire CMake**

Modify root `CMakeLists.txt` to include:

```cmake
src/ui/widgets/ProtocolConfigEditor.h
src/ui/widgets/ProtocolConfigEditor.cpp
```

Modify `tests/CMakeLists.txt` to compile the new widget source if the test executable manually lists UI sources.

**Step 4: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS for `TestProtocolConfigEditor::buildsWidgetsFromSchema`.

**Step 5: Commit**

Run:

```bash
git add CMakeLists.txt tests/CMakeLists.txt tests/main.cpp tests/unit/TestProtocolConfigEditor.h tests/unit/TestProtocolConfigEditor.cpp src/ui/widgets/ProtocolConfigEditor.h src/ui/widgets/ProtocolConfigEditor.cpp
git commit -m "feat: add protocol config editor widget"
```

### Task 3: Add Editor Config Load, Read, Defaults, And Validation

**Files:**
- Modify: `src/ui/widgets/ProtocolConfigEditor.h`
- Modify: `src/ui/widgets/ProtocolConfigEditor.cpp`
- Modify: `tests/unit/TestProtocolConfigEditor.h`
- Modify: `tests/unit/TestProtocolConfigEditor.cpp`

**Step 1: Add failing tests**

Add slots:

```cpp
void loadsAndReadsConfig();
void restoresDefaults();
void reportsValidationErrors();
```

Add tests:

```cpp
void TestProtocolConfigEditor::loadsAndReadsConfig()
{
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
```

**Step 2: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because editor lacks `setConfig`, `config`, `restoreDefaults`, `validateConfig`, and `errorText`.

**Step 3: Implement editor config methods**

Add public methods:

```cpp
void setConfig(const QVariantMap& config);
QVariantMap config() const;
void restoreDefaults();
ProtocolConfigValidationResult validateConfig();
QString errorText() const;
void clearErrors();
```

Implementation notes:

- Store field widgets in `QMap<QString, QWidget*> m_fieldWidgets`.
- `setConfig()` should read field defaults for missing keys.
- `config()` should collect values by widget type.
- `restoreDefaults()` should call `setConfig(m_schema.defaults())`.
- `validateConfig()` should call `m_schema.validate(config())`, display `errors.join("\n")` in a hidden/shown error label, and return the result.
- Empty schema config should be an empty map and valid.

**Step 4: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 5: Commit**

Run:

```bash
git add src/ui/widgets/ProtocolConfigEditor.h src/ui/widgets/ProtocolConfigEditor.cpp tests/unit/TestProtocolConfigEditor.h tests/unit/TestProtocolConfigEditor.cpp
git commit -m "feat: edit and validate protocol config schema"
```

### Task 4: Add ProtocolConfigDialog

**Files:**
- Create: `src/ui/dialogs/ProtocolConfigDialog.h`
- Create: `src/ui/dialogs/ProtocolConfigDialog.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/unit/TestProtocolConfigEditor.h`
- Modify: `tests/unit/TestProtocolConfigEditor.cpp`

**Step 1: Add failing dialog test**

Add slot:

```cpp
void dialogAcceptsNormalizedConfig();
```

Add test:

```cpp
void TestProtocolConfigEditor::dialogAcceptsNormalizedConfig()
{
    ProtocolDescriptor descriptor;
    descriptor.id = QStringLiteral("easyhex");
    descriptor.displayName = QStringLiteral("EasyHEX");
    descriptor.description = QStringLiteral("EasyHEX test descriptor");
    descriptor.configSchema = makeEditorTestSchema();
    descriptor.configVersion = descriptor.configSchema.version;
    descriptor.defaultConfig = descriptor.configSchema.defaults();

    ProtocolConfigDialog dialog(descriptor, descriptor.defaultConfig);

    QLineEdit* hexEdit = dialog.findChild<QLineEdit*>(QStringLiteral("protocolConfig_frameHeader"));
    QVERIFY(hexEdit != nullptr);
    hexEdit->setText(QStringLiteral("aa-55"));

    QVERIFY(dialog.acceptConfig());
    QCOMPARE(dialog.normalizedConfig().value(QStringLiteral("frameHeader")).toString(), QStringLiteral("AA 55"));
}
```

Add include:

```cpp
#include "core/protocol/ProtocolDescriptor.h"
#include "ui/dialogs/ProtocolConfigDialog.h"
```

**Step 2: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL because `ProtocolConfigDialog.h` does not exist.

**Step 3: Implement dialog**

Create `ProtocolConfigDialog` with:

- Constructor `ProtocolConfigDialog(const ProtocolDescriptor& descriptor, const QVariantMap& initialConfig, QWidget* parent = nullptr)`.
- `QLabel` title/description.
- `ProtocolConfigEditor`.
- `QDialogButtonBox` with Ok/Cancel.
- `QPushButton` restore defaults button.
- Public:

```cpp
QVariantMap normalizedConfig() const;
bool acceptConfig();
```

Implementation notes:

- `acceptConfig()` calls editor `validateConfig()`.
- If valid, store `m_normalizedConfig = result.normalizedConfig`, call `accept()` only when invoked by Ok button, and return true.
- If invalid, return false and keep dialog open.
- Restore defaults button calls editor `restoreDefaults()`.

**Step 4: Wire CMake**

Add dialog source/header to root CMake and tests CMake as needed.

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
git add CMakeLists.txt tests/CMakeLists.txt src/ui/dialogs/ProtocolConfigDialog.h src/ui/dialogs/ProtocolConfigDialog.cpp tests/unit/TestProtocolConfigEditor.h tests/unit/TestProtocolConfigEditor.cpp
git commit -m "feat: add protocol config dialog"
```

### Task 5: Integrate Dialog Into MainWindow

**Files:**
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `tests/unit/TestTranslationCompleteness.cpp`
- Modify if needed: `translations/*.ts`

**Step 1: Add failing coverage for menu text**

In `tests/unit/TestTranslationCompleteness.cpp`, extend the hamburger menu critical translations list with:

```cpp
{QStringLiteral("协议配置..."), QStringLiteral("Protocol Config...")},
```

If the test suite uses a different structure, add the new source text in the same style as other hamburger menu items.

**Step 2: Run red test**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: FAIL because the new translation/menu source text is absent or unfinished.

**Step 3: Add MainWindow slot and menu entry**

In `MainWindow.h`, add private slot:

```cpp
void onProtocolConfig();
```

In `MainWindow.cpp`:

- Include `dialogs/ProtocolConfigDialog.h`.
- In `populateHamburgerMenu()`, add to Tools menu near Modbus/Data Window tools:

```cpp
toolsMenu->addAction(tr("协议配置..."), this, &MainWindow::onProtocolConfig);
```

Implement `onProtocolConfig()`:

```cpp
void MainWindow::onProtocolConfig()
{
    const ProtocolDescriptor descriptor = ProtocolFactory::descriptor(m_currentProtocolType);
    QVariantMap currentConfig = m_currentProtocol ? m_currentProtocol->config()
                                                  : descriptor.defaultConfig;
    if (currentConfig.isEmpty()) {
        currentConfig = descriptor.defaultConfig;
    }

    ProtocolConfigDialog dialog(descriptor, currentConfig, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QVariantMap normalizedConfig = dialog.normalizedConfig();
    if (m_currentProtocol) {
        m_currentProtocol->setConfig(normalizedConfig);
    }
    statusBar()->showMessage(tr("协议配置已应用"), 3000);
}
```

Notes:

- Raw/empty Schema is handled inside dialog/editor.
- Do not force-create a Raw protocol instance.
- If tests require English translation, update `.ts` files using the project’s existing translation workflow or direct TS edit pattern.

**Step 4: Run tests**

Run:

```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS.

**Step 5: Commit**

Run:

```bash
git add src/ui/MainWindow.h src/ui/MainWindow.cpp tests/unit/TestTranslationCompleteness.cpp translations
git commit -m "feat: expose protocol config dialog"
```

If no translation files changed, omit `translations`.

### Task 6: Update Help Documentation

**Files:**
- Modify: `resources/help/protocols.html`
- Modify if needed: `resources/help/quickstart.html`

**Step 1: Update protocols help**

Add near the protocol capability/config paragraph:

```html
<p><strong>中文：</strong>可通过 <code>工具 -> 协议配置...</code> 打开当前协议的 Schema 配置面板，编辑后会先校验并规范化，再应用到当前协议并随会话保存。</p>
<p><strong>English:</strong> Use <code>Tools -> Protocol Config...</code> to open the schema-driven config panel for the current protocol. Values are validated and normalized before being applied and saved with the session.</p>
```

**Step 2: Decide quickstart**

If only an advanced tools menu item is added and connection/send/receive quick-start flow remains unchanged, do not update `quickstart.html`. Final report must state:

```text
resources/help/quickstart.html 未更新，因为快速连接、发送、接收、会话保存快捷键和主流程没有变化；协议配置入口已写入 protocols.html。
```

If implementation changes quick-start workflow, update quickstart accordingly.

**Step 3: Run documentation tests**

Run:

```bash
build_release\tests\ComAssistant_tests.exe -o -,txt
```

Expected: PASS including `DocumentationLinks`.

**Step 4: Commit**

Run:

```bash
git add resources/help/protocols.html resources/help/quickstart.html
git commit -m "docs: document protocol config dialog"
```

If quickstart did not change, omit it from `git add`.

### Task 7: Final Verification And RecallLoom Update

**Files:**
- Update via RecallLoom helper as needed:
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

- Test executable prints `All tests PASSED!`.
- CTest reports `100% tests passed, 0 tests failed out of 1`.
- Release build deploys `D:\comassistant\build_release\ComAssistant.exe`.
- `git diff --check` exits 0.
- RecallLoom validation shows `Errors: 0`; resolve warnings if possible.

**Step 2: Update RecallLoom**

Use revision-aware helpers:

```bash
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\preflight_context_check.py .
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\commit_context_file.py ...
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\append_daily_log_entry.py ...
python C:\Users\caofengrui\.agents\skills\recallloom\scripts\validate_context.py .
```

Record:

- 4.3 commit range.
- Verification results.
- Documentation update and quickstart decision.
- Remaining risks: no plugin/Lua execution, no external protocol UI beyond built-in Schema, no real hardware validation.

**Step 3: Final status**

Run:

```bash
git status --short --branch
git log --oneline -12
```

Expected: clean tracked worktree on `phase4/protocol-registry`.

**Step 4: Report in Chinese**

Include:

- Branch.
- Commits.
- Verification commands and results.
- Documentation updates.
- RecallLoom layers updated.
- Next recommended stage, likely 4.4 protocol capability diagnostics or Lua sandbox design.
