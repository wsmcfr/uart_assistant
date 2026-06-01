# Communication Workspaces Polish Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 精细化打磨 TCP客户端、TCP服务器、UDP 和 HID Report 工作台，使其具备专业调试工具所需的状态摘要、日志工具、发送辅助和 Report 预览。

**Architecture:** 保持现有 `CommunicationWorkspaceWidget` 基类和四个专用工作台架构，在基类中沉淀共享日志/输入能力，各工作台只保留自身目标管理和协议差异。HID Report 继续复用 `HidReportCodec` 生成预览和 Feature 报告。

**Tech Stack:** C++17、Qt 5.12.9 Widgets、QtTest、CMake、hidapi。

---

### Task 1: 共享日志与发送辅助

**Files:**
- Modify: `src/ui/widgets/CommunicationWorkspaceWidget.h`
- Modify: `src/ui/widgets/CommunicationWorkspaceWidget.cpp`
- Modify: `tests/unit/TestCommunicationWorkspaces.h`
- Modify: `tests/unit/TestCommunicationWorkspaces.cpp`

**Step 1: Write failing tests**

Add tests for:
- HEX normalize helper turns `0x0a bb c` into `0A BB 0C`
- Text preview replaces control characters and truncates long text
- `formatLogLine()` contains direction, type, length, HEX and text preview

**Step 2: Run test to verify failure**

Run: `cmake --build build_release --target ComAssistant_tests --config Release`

Expected: FAIL because helper APIs do not exist.

**Step 3: Implement shared helpers**

Add protected/public testable helpers:
- `normalizeHexText(QString)`
- `payloadPreview(QByteArray, int maxChars)`
- `formatLogLine(direction, type, data)`
- `appendLogLine(QPlainTextEdit*, direction, type, data, autoScroll)`

**Step 4: Run tests**

Run: `ctest --test-dir build_release --output-on-failure`

Expected: PASS.

**Step 5: Commit**

Commit message: `feat: polish workspace shared helpers`

### Task 2: TCP/UDP 工作台打磨

**Files:**
- Modify: `src/ui/widgets/TcpClientWorkspaceWidget.h`
- Modify: `src/ui/widgets/TcpClientWorkspaceWidget.cpp`
- Modify: `src/ui/widgets/TcpServerWorkspaceWidget.h`
- Modify: `src/ui/widgets/TcpServerWorkspaceWidget.cpp`
- Modify: `src/ui/widgets/UdpWorkspaceWidget.h`
- Modify: `src/ui/widgets/UdpWorkspaceWidget.cpp`
- Modify: `tests/unit/TestCommunicationWorkspaces.cpp`

**Step 1: Write failing tests**

Add tests for:
- Ctrl+Enter triggers send in TCP客户端/UDP send editors.
- HEX 格式化按钮 rewrites input as grouped uppercase HEX.
- 日志清空按钮 clears log.
- UDP 清空最近远端 clears combo.

**Step 2: Run test to verify failure**

Run: `cmake --build build_release --target ComAssistant_tests --config Release`

Expected: FAIL because buttons/object names/shortcuts do not exist.

**Step 3: Implement UI polish**

Add:
- Status summary label per workspace.
- Log toolbar: auto scroll, copy, clear.
- Send toolbar: HEX format, append newline for text mode, byte count label.
- Ctrl+Enter event filter for send editors.
- TCP server client count and disconnect-selected signal.
- UDP clear recent remotes.

**Step 4: Run tests**

Run: `ctest --test-dir build_release --output-on-failure`

Expected: PASS.

**Step 5: Commit**

Commit message: `feat: polish network workspaces`

### Task 3: HID Report 工作台打磨

**Files:**
- Modify: `src/ui/widgets/HidReportWorkspaceWidget.h`
- Modify: `src/ui/widgets/HidReportWorkspaceWidget.cpp`
- Modify: `tests/unit/TestCommunicationWorkspaces.cpp`

**Step 1: Write failing tests**

Add tests for:
- Output payload input updates preview with full Output Report.
- Feature payload input updates preview with full Feature Report.
- Overlong payload shows truncation warning.
- HEX format buttons normalize Output and Feature inputs.

**Step 2: Run test to verify failure**

Run: `cmake --build build_release --target ComAssistant_tests --config Release`

Expected: FAIL because preview labels/buttons do not exist.

**Step 3: Implement HID polish**

Add:
- Output Report preview field.
- Feature Report preview field.
- Payload byte count labels.
- Truncation warning label.
- HEX format buttons.
- Clear/copy history buttons.

**Step 4: Run tests**

Run: `ctest --test-dir build_release --output-on-failure`

Expected: PASS.

**Step 5: Commit**

Commit message: `feat: polish HID report workspace`

### Task 4: 主窗口和文档同步

**Files:**
- Modify: `src/ui/MainWindow.cpp`
- Modify: `resources/help/quickstart.html`
- Modify: `resources/help/modes.html`
- Modify: `docs/user-guide/quickstart.md`
- Modify: `docs/user-guide/faq.md`

**Step 1: Wire optional signals**

If TCP服务器 exposes `disconnectClientRequested`, connect it to `TcpServer::disconnectClient()`.

**Step 2: Update docs**

Document:
- Ctrl+Enter 发送
- 日志清空/复制/自动滚动
- HEX 格式化
- UDP 最近远端清空
- HID Report 预览和截断提示

**Step 3: Verify docs**

Run: `rg -n "Ctrl\\+Enter|HEX格式化|Report 预览|自动滚动|最近远端" resources/help docs/user-guide`

Expected: updated docs contain the new usage terms.

**Step 4: Commit**

Commit message: `docs: describe polished workspaces`

### Task 5: Final build verification

**Files:** no planned changes.

**Step 1: Configure**

Run: `cmake -B build_release -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=F:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic`

Expected: exit 0.

**Step 2: Build**

Run: `cmake --build build_release --config Release --parallel`

Expected: exit 0.

**Step 3: Test**

Run: `ctest --test-dir build_release --output-on-failure`

Expected: 1/1 tests pass.

**Step 4: Status**

Run: `git status --short --branch`

Expected: clean working tree, branch ahead of origin by the new commits.

