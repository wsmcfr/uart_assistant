# Phase 3 Communication Reliability Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 完成第三阶段通信和文件传输可靠性升级，让普通发送、快捷发送、脚本发送、文件发送、Raw/OTA 大文件传输和 HID 读写都具备可测试、可追踪、可取消的可靠路径。

**Architecture:** 先在 `src/core/communication` 增加纯发送队列和调度器，主窗口通信控制器只暴露一个发送出口。文件传输侧保持协议职责，但把 Raw/OTA 推进改为按块生成、按发送结果推进，并补齐 Running/Paused/Cancelling/Failed/Completed 状态语义。HID 阻塞 I/O 后续迁移到 worker 线程，Feature Report 使用互斥序列化，关闭时通过线程退出和句柄释放顺序保证安全。

**Tech Stack:** C++17、Qt 5.12.9、Qt Test、CMake、MinGW/vcpkg、hidapi 可选后端。

---

### Task 1: 3.1 发送队列与背压

**Files:**
- Create: `src/core/communication/SendQueue.h`
- Create: `src/core/communication/SendQueue.cpp`
- Create: `src/core/communication/SendDispatcher.h`
- Create: `src/core/communication/SendDispatcher.cpp`
- Create: `tests/unit/TestSendQueue.h`
- Create: `tests/unit/TestSendQueue.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/main.cpp`

**Step 1: Write failing queue tests**

覆盖以下行为：
- 队列默认接受非空数据并保持 FIFO 顺序。
- 空数据被拒绝且记录错误。
- 容量满时拒绝新任务，已有任务保持不变。
- 失败完成不弹出队首，成功完成才弹出并推进。
- 取消清空队列，并把取消数暴露给调用方。

**Step 2: Run red verification**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
```

Expected: FAIL，原因是 `SendQueue` / `TestSendQueue` 尚不存在或新测试未通过。

**Step 3: Implement minimal queue**

实现 `SendQueue`：
- `SendQueueOptions { int maxItems; qint64 maxBytes; }`
- `SendItem { qint64 id; QByteArray payload; QString source; int attempt; }`
- `SendQueue::enqueue()` 做非空、条数、字节容量检查。
- `SendQueue::peek()` 返回队首。
- `SendQueue::completeHead(SendResult)` 成功时弹出，失败时保留并记录错误。
- `SendQueue::cancelAll()` 清空并返回取消数量。

**Step 4: Add dispatcher skeleton tests**

覆盖：
- 未连接写入时调度器保留队首失败项。
- 写入成功时弹出队首并发出完成信号。
- 调度中取消会停止并清空队列。

**Step 5: Implement minimal dispatcher**

实现 `SendDispatcher`：
- 依赖一个写入回调 `std::function<qint64(const QByteArray&)>`。
- 串行消费队列，不并发写同一个 `ICommunication`。
- 写失败时保留队首并停止调度，等待上层连接恢复或手动重试。
- 发送完成、失败、队列变化通过 Qt signals 暴露。

**Step 6: Verify and commit**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe
ctest --test-dir build_release --output-on-failure
git diff --check
git add CMakeLists.txt tests\CMakeLists.txt tests\main.cpp src\core\communication\SendQueue.* src\core\communication\SendDispatcher.* tests\unit\TestSendQueue.*
git commit -m "feat: add send queue and dispatcher"
```

### Task 2: 3.2 接入主窗口发送路径

**Files:**
- Modify: `src/ui/MainWindowCommunicationController.h`
- Modify: `src/ui/MainWindowCommunicationController.cpp`
- Modify: `src/ui/MainWindow.cpp`
- Modify: `tests/unit/TestMainWindowCommunicationController.h`
- Modify: `tests/unit/TestMainWindowCommunicationController.cpp`
- Modify as needed: `resources/help/quickstart.html`

**Step 1: Write failing integration tests**

覆盖：
- `sendData()` 不再直接写，而是进入调度器统一出口。
- 未连接时发送被拒绝且队列不残留。
- 底层写失败时队首保持，恢复后可重试。
- 关闭连接时取消待发送队列。

**Step 2: Run red verification**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe -functions
```

Expected: FAIL，新集成测试展示现有直接写路径无法满足失败保持/取消语义。

**Step 3: Integrate dispatcher**

控制器拥有 `SendDispatcher`：
- `openCurrent()` 后设置写入回调到当前通信对象。
- `closeCurrent()` 取消队列并释放通信对象。
- `sendData()` 只做连接检查、入队和触发调度。
- 保留现有 `dataSent` 对外语义，不改变 UI 展示。

**Step 4: Ensure all UI send paths use controller outlet**

检查普通发送、快捷发送、脚本发送、文件传输信号和 IAP 升级发送回调：
- 能走 `MainWindow::onSendData()` 的路径继续走控制器。
- TCP Server 定向发送、UDP 指定远端、HID Feature Report 作为协议特化出口暂不混入普通字节队列，但要在计划和验证清单中明确边界。

**Step 5: Verify and commit**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe
ctest --test-dir build_release --output-on-failure
git diff --check
git add src\ui\MainWindowCommunicationController.* src\ui\MainWindow.cpp tests\unit\TestMainWindowCommunicationController.* resources\help\quickstart.html
git commit -m "feat: route main window sends through dispatcher"
```

### Task 3: 3.3 Raw/OTA 大文件流式发送

**Files:**
- Modify: `src/core/transfer/FileTransfer.h`
- Modify: `src/core/transfer/FileTransfer.cpp`
- Modify: `tests/unit/TestFileTransfer.h`
- Modify: `tests/unit/TestFileTransfer.cpp`
- Modify as needed: `resources/help/quickstart.html`

**Step 1: Write failing streaming tests**

覆盖：
- Raw 大文件启动后只读取并发出当前块，不预生成完整包列表。
- Raw 进度只在外部确认发送成功后推进。
- OTA header、data、end 保持流式读取。
- 暂停/取消期间不再继续读取下一块。

**Step 2: Run red verification**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe
```

Expected: FAIL，现有 Raw/OTA 在 `emit sendData()` 后立即推进进度，无法等待队列确认。

**Step 3: Add send acknowledgement hook**

给 `FileTransfer` 增加最小确认接口：
- `onChunkAccepted()` 或 `notifySendResult(bool success, const QString& error)`。
- Raw/OTA 在 emit 当前包后进入等待队列结果，不重复读取下一块。
- 失败时根据状态机转 Failed 或等待重试。

**Step 4: Wire transfer dialog/main window acknowledgement**

主窗口在控制器发送成功入队/失败时回调当前文件传输对象：
- 入队成功不等同于设备 ACK，但可以作为本地发送管道已接受的推进信号。
- 队列失败或连接断开时停止传输并暴露错误。

**Step 5: Verify and commit**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe
ctest --test-dir build_release --output-on-failure
git diff --check
git add src\core\transfer\FileTransfer.* tests\unit\TestFileTransfer.* src\ui\MainWindow.cpp resources\help\quickstart.html
git commit -m "feat: stream raw and ota transfers through send queue"
```

### Task 4: 3.4 文件传输可靠性状态机

**Files:**
- Modify: `src/core/transfer/FileTransfer.h`
- Modify: `src/core/transfer/FileTransfer.cpp`
- Modify: `src/ui/dialogs/FileTransferDialog.cpp`
- Modify: `tests/unit/TestFileTransfer.h`
- Modify: `tests/unit/TestFileTransfer.cpp`
- Modify as needed: `resources/help/quickstart.html`

**Step 1: Write failing state transition tests**

覆盖：
- `Idle -> Running -> Paused -> Running -> Completed`
- `Running -> Cancelling -> Cancelled`
- `Running -> Failed`
- 失败后错误信息保留在 progress。
- 超时/重试次数更新 progress。

**Step 2: Run red verification**

Expected: FAIL，因为现有枚举没有 Running/Paused/Cancelling/Failed 的统一语义。

**Step 3: Implement compatibility state model**

在不大幅破坏 UI 的前提下升级状态：
- 新增 `Running`、`Paused`、`Cancelling`、`Failed`。
- 保留旧状态名或映射旧状态，避免已有 UI 文案立即失效。
- Raw/OTA/X/YMODEM 使用统一 helper：`markRunning()`、`markPaused()`、`markCancelling()`、`markFailed()`、`markCompleted()`。

**Step 4: Update dialog rendering**

文件传输对话框根据新状态显示按钮可用性和文案：
- Running 可暂停/取消。
- Paused 可继续/取消。
- Cancelling 禁止重复操作。
- Failed 显示错误并允许重新开始。

**Step 5: Verify and commit**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe
ctest --test-dir build_release --output-on-failure
git diff --check
git add src\core\transfer\FileTransfer.* src\ui\dialogs\FileTransferDialog.cpp tests\unit\TestFileTransfer.* resources\help\quickstart.html
git commit -m "feat: harden file transfer state machine"
```

### Task 5: 3.5 HID worker 化

**Files:**
- Create: `src/core/communication/HidWorker.h`
- Create: `src/core/communication/HidWorker.cpp`
- Modify: `src/core/communication/HidDevice.h`
- Modify: `src/core/communication/HidDevice.cpp`
- Modify: `tests/unit/TestHidCommunication.h`
- Modify: `tests/unit/TestHidCommunication.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify as needed: `resources/help/quickstart.html`

**Step 1: Write failing worker tests**

覆盖：
- open 后 worker 线程启动，close 后线程退出。
- Output write 通过 worker 序列化。
- Feature Report Set/Get 互斥，不与普通 Output 并发访问句柄。
- close 期间 pending 操作返回失败但不崩溃。
- 未启用 hidapi 构建仍能返回明确错误。

**Step 2: Run red verification**

Expected: FAIL，现有 HID 读轮询和 Feature 调用仍在对象线程内直接操作。

**Step 3: Implement worker**

设计：
- `HidWorker` 是 QObject，移动到独立 `QThread`。
- worker 持有 hidapi 句柄，负责 open/read/write/feature/close。
- `HidDevice` 只保留 ICommunication 门面和跨线程信号槽。
- 用 `QMutex` 或 worker 单线程队列保证 Feature Report 互斥。

**Step 4: Verify and commit**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe
ctest --test-dir build_release --output-on-failure
git diff --check
git add CMakeLists.txt tests\CMakeLists.txt src\core\communication\HidWorker.* src\core\communication\HidDevice.* tests\unit\TestHidCommunication.* resources\help\quickstart.html
git commit -m "feat: move hid io to worker thread"
```

### Task 6: 3.6 真实设备端到端验证包

**Files:**
- Create: `docs/validation/phase3-device-e2e-checklist.md`
- Create: `docs/validation/phase3-diagnostic-log-template.md`
- Create as needed: `scripts/collect_phase3_diagnostics.ps1`
- Modify: `resources/help/quickstart.html`
- Modify: `.recallloom/rolling_summary.md`
- Modify: `.recallloom/daily_logs/2026-06-02.md`

**Step 1: Create validation checklist**

覆盖：
- 串口 Raw 小文件/大文件/取消/断线。
- OTA 无 ACK/ACK/超时重试/取消。
- XMODEM/YMODEM 正常/NAK/超时。
- TCP/UDP 普通发送队列边界。
- HID Output/Feature/Input/关闭并发。

**Step 2: Create diagnostic package**

诊断记录应包含：
- App 版本、Git commit、构建类型、HID 后端启用状态。
- 通信类型、设备型号、端口/VID/PID、波特率或 Report 长度。
- 操作步骤、预期、实际、日志路径、失败定位字段。

**Step 3: Update user help**

在内置帮助中补充：
- 大文件 Raw/OTA 会按块流式发送。
- 发送队列满、断开或取消时的可见行为。
- HID 关闭/Feature 操作的可靠性说明。

**Step 4: Full verification, memory, push**

Run:
```bash
cmake --build build_release --config Release --target ComAssistant_tests --parallel
.\build_release\tests\ComAssistant_tests.exe
ctest --test-dir build_release --output-on-failure
cmake --build build_release --config Release --target ComAssistant --parallel
git diff --check
git status --short
git log --oneline -6
git push -u origin phase3/communication-reliability
```

Update RecallLoom:
- `rolling_summary.md`: 当前分支、已完成第三阶段、验证命令、剩余风险。
- `daily_logs/2026-06-02.md`: 记录第三阶段完成和推送 commit。

Commit:
```bash
git add docs/validation resources/help/quickstart.html .recallloom
git commit -m "docs: add phase3 device validation package"
git push
```
