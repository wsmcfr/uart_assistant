# Communication Type Workspaces Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 为 TCP客户端、TCP服务器、UDP、HID 建立真正的通信类型专用调试界面，避免继续复用串口工作区。

**Architecture:** 主窗口新增通信工作台栈，串口继续显示现有模式栈，网络与 HID 类型切换到对应 QWidget。HID 报告构造抽出 `HidReportCodec` 纯逻辑类，UI 只负责参数收集、历史展示和发出发送请求。

**Tech Stack:** C++17、Qt 5.12.9 Widgets、CMake、QtTest、hidapi。

---

### Task 1: HID Report 纯逻辑测试与实现

**Files:**
- Create: `src/core/communication/HidReportCodec.h`
- Create: `src/core/communication/HidReportCodec.cpp`
- Modify: `src/core/communication/HidDevice.cpp`
- Modify: `src/core/config/AppConfig.h`
- Modify: `src/core/config/ConfigManager.cpp`
- Modify: `src/core/session/SessionData.h`
- Modify: `tests/unit/TestHidCommunication.h`
- Modify: `tests/unit/TestHidCommunication.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `CMakeLists.txt`

**Step 1: Write failing tests**

Add tests for:
- Output Report includes Report ID, optional length byte, payload and zero padding.
- Input Report can remove Report ID.
- Feature Report uses independent feature Report ID and length.
- Session persists new Feature fields.

**Step 2: Run tests to verify failure**

Run: `cmake --build build_release --target ComAssistant_tests --config Release`

Expected: FAIL because `HidReportCodec` and Feature fields do not exist yet.

**Step 3: Implement minimal code**

Add `HidReportCodec`:
- `buildOutputReport(const HidConfig&, const QByteArray&)`
- `buildFeatureReport(const HidConfig&, const QByteArray&)`
- `normalizeInputReport(const HidConfig&, const QByteArray&)`

Extend `HidConfig` with:
- `int featureReportLength = 64`
- `quint8 featureReportId = 0`

Update config and session persistence.

**Step 4: Run tests to verify pass**

Run: `ctest --test-dir build_release --output-on-failure`

Expected: PASS.

**Step 5: Commit**

Commit message: `feat: add HID report codec`

### Task 2: 新增通信工作台 Widget

**Files:**
- Create: `src/ui/widgets/CommunicationWorkspaceWidget.h`
- Create: `src/ui/widgets/CommunicationWorkspaceWidget.cpp`
- Create: `src/ui/widgets/TcpClientWorkspaceWidget.h`
- Create: `src/ui/widgets/TcpClientWorkspaceWidget.cpp`
- Create: `src/ui/widgets/TcpServerWorkspaceWidget.h`
- Create: `src/ui/widgets/TcpServerWorkspaceWidget.cpp`
- Create: `src/ui/widgets/UdpWorkspaceWidget.h`
- Create: `src/ui/widgets/UdpWorkspaceWidget.cpp`
- Create: `src/ui/widgets/HidReportWorkspaceWidget.h`
- Create: `src/ui/widgets/HidReportWorkspaceWidget.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/TestCommunicationWorkspaces.h`
- Create: `tests/unit/TestCommunicationWorkspaces.cpp`
- Modify: `tests/main.cpp`

**Step 1: Write failing widget tests**

Test:
- TCP客户端工作台 setConfig/config round-trip and send signal.
- TCP服务器工作台 selected client / broadcast target behavior.
- UDP 工作台 local/remote config round-trip and recent remote insertion.
- HID 工作台 Report 参数 round-trip and Output/Feature send signal.

**Step 2: Run tests to verify failure**

Run: `cmake --build build_release --target ComAssistant_tests --config Release`

Expected: FAIL because new widgets do not exist.

**Step 3: Implement minimal widgets**

Use dense tool-style panels:
- No nested decorative cards.
- Use `QGroupBox`, `QFormLayout`, `QTableWidget`, `QPlainTextEdit`, `QPushButton`.
- All new functions and key branches include Chinese comments.

**Step 4: Run tests to verify pass**

Run: `ctest --test-dir build_release --output-on-failure`

Expected: PASS.

**Step 5: Commit**

Commit message: `feat: add communication workspaces`

### Task 3: MainWindow 集成通信工作台

**Files:**
- Modify: `src/ui/MainWindow.h`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `src/core/communication/HidDevice.h`
- Modify: `src/core/communication/HidDevice.cpp`

**Step 1: Write failing integration-level widget test where feasible**

Prefer testing work台核心行为 through widget tests. If full MainWindow test is too brittle, add focused helper tests for visible widget switching and document manual verification.

**Step 2: Implement MainWindow switching**

Add:
- `QStackedWidget* m_commWorkspaceStack`
- `QWidget* m_serialWorkspacePage`
- `TcpClientWorkspaceWidget* m_tcpClientWorkspace`
- `TcpServerWorkspaceWidget* m_tcpServerWorkspace`
- `UdpWorkspaceWidget* m_udpWorkspace`
- `HidReportWorkspaceWidget* m_hidWorkspace`

Rules:
- 串口类型显示现有 `m_modeToolBarScrollArea + m_modeStack`。
- 非串口类型显示对应工作台，并隐藏右上显示模式控件。
- 打开连接前从当前工作台同步配置。
- 接收/发送数据同时写入当前工作台日志。
- TCP服务器客户端信号连接到工作台。
- UDP datagram sender info 连接到工作台最近远端。
- HID 支持 Set/Get Feature Report。

**Step 3: Run tests/build**

Run:
- `cmake --build build_release --config Release --parallel`
- `ctest --test-dir build_release --output-on-failure`

Expected: build and tests pass.

**Step 4: Commit**

Commit message: `feat: switch communication workspaces by type`

### Task 4: 帮助文档与用户指南

**Files:**
- Modify: `resources/help/quickstart.html`
- Modify: `resources/help/modes.html`
- Modify: `docs/user-guide/quickstart.md`
- Modify: `docs/user-guide/index.md`
- Modify: `docs/user-guide/faq.md`

**Step 1: Update docs**

Document:
- 左上“类型”切换通信工作台。
- 右上“模式”只用于串口显示模式。
- TCP客户端、TCP服务器、UDP、HID Report 工作台入口与关键控件。
- HID Feature Report 与 Report 长度/Report ID 注意事项。

**Step 2: Verify docs are included**

Run: `rg -n "HID Report|TCP客户端工作台|TCP服务器工作台|UDP 工作台|通信工作台" resources/help docs/user-guide`

Expected: all updated docs contain new terms.

**Step 3: Commit**

Commit message: `docs: describe communication workspaces`

### Task 5: 最终验证与发布构建目录检查

**Files:**
- No source changes expected unless verification finds issues.

**Step 1: Configure release build if needed**

Run: `cmake -B build_release -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=F:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic`

**Step 2: Build**

Run: `cmake --build build_release --config Release --parallel`

Expected: exit 0.

**Step 3: Test**

Run: `ctest --test-dir build_release --output-on-failure`

Expected: exit 0.

**Step 4: Inspect diff**

Run:
- `git status --short --branch`
- `git log --oneline -5`

Expected: clean except intentional commits.

