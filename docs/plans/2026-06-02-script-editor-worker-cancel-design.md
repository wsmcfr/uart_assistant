# 4.7 脚本编辑器后台执行与取消控制设计

## 背景

4.6 已让脚本编辑器真实调用 `LuaSandbox`，并开放受控 `serial.send` / `serial.sendHex`。但当前 `ScriptEditorDialog::onRunScript()` 仍在 UI 线程同步执行脚本，长循环或大量计算会让对话框短时无响应；停止按钮也只是恢复界面状态，无法真正请求 Lua 执行停止。

4.7 的目标是把脚本执行从 UI 线程移到后台 worker，并让停止按钮通过 `LuaSandbox` hook 触发受控中断。第一版只解决后台执行、取消请求、输出/错误/发送回传和状态恢复，不扩大到 `serial.receive`、Lua 协议注册或插件系统。

## 范围

| 类型 | 内容 |
|------|------|
| 目标 | `ScriptEditorDialog` 点击运行后立即返回 UI 事件循环，脚本在后台线程执行 |
| 目标 | 停止按钮设置取消标记，`LuaSandbox` hook 检测后返回 `interrupted=true` |
| 目标 | `print` 输出、Lua 错误、超时、取消和发送请求通过 Qt queued signal 回到 UI 线程 |
| 目标 | 运行中禁用运行按钮、启用停止按钮；完成、错误、超时、取消后统一恢复 |
| 目标 | 继续复用 `sendData(QByteArray)`，不直接持有通信对象 |
| 非目标 | 不实现 `serial.receive(timeout)` |
| 非目标 | 不做 Lua 协议注册、脚本协议 Schema 或插件加载 |
| 非目标 | 不强杀线程，不调用危险的线程终止 API |
| 非目标 | 不重构历史 `LuaEngine` |

## 方案比较

| 方案 | 做法 | 优点 | 风险 | 结论 |
|------|------|------|------|------|
| A | 仅给同步执行补取消标记 | 改动小 | UI 仍会阻塞，停止按钮无法响应点击 | 不采用 |
| B | `QThread` + `ScriptExecutionWorker` + Lua hook 取消 | 线程边界清晰，取消安全，符合当前沙箱设计 | 需要管理 worker 生命周期和 queued signal | 推荐 |
| C | `QtConcurrent` / thread pool 执行脚本 | 代码较少 | 取消、生命周期、信号归属和测试可控性较差 | 不采用 |

推荐方案 B。`LuaSandbox` 已有 hook 检查超时，加入 `InterruptCallback` 后可以用同一机制处理中断。脚本执行 worker 作为 `QObject` 移动到专用 `QThread`，运行结束后主动退出线程并释放 worker，避免复用线程导致状态串扰。

## 架构

| 组件 | 职责 |
|------|------|
| `LuaSandboxOptions` | 新增 `InterruptCallback`，供 hook 查询外部取消请求 |
| `LuaSandbox` | hook 中先检查取消，再检查超时；取消时设置 `result.interrupted=true` 并通过 `luaL_error` 终止 |
| `ScriptExecutionWorker` | 后台对象，接收脚本文本与沙箱选项，执行 `LuaSandbox::execute()`，通过信号回传结果和发送请求 |
| `ScriptEditorDialog` | 管理运行状态、线程生命周期、取消标记、输出渲染、发送信号转发 |
| `MainWindow` | 保持现有连接：`ScriptEditorDialog::sendData` -> `MainWindow::onSendData` |

## 数据流

1. 用户点击“运行”。
2. `ScriptEditorDialog` 检查未运行后，创建共享取消标记，创建 `QThread` 与 `ScriptExecutionWorker`。
3. 对话框进入运行状态：运行按钮禁用，停止按钮启用，输出 `[系统] 开始执行脚本...`。
4. worker 在线程启动后构造 `LuaSandboxOptions`：
   - `timeoutMs = 3000`
   - `memoryLimitKb = 2048`
   - `maxOutputLines = 500`
   - `allowCommunicationApi = true`
   - `interruptCallback` 读取共享取消标记
   - `sendCallback` 发射 worker 的 `sendRequested(QByteArray)` 信号，并返回非空数据是否可接受
   - `isOpenCallback` 4.7 第一版仍返回 true，真实连接状态注入留后续或同阶段后续小任务
5. `LuaSandbox::execute()` 执行脚本。
6. worker 完成后发射 `finished(LuaSandboxResult)`。
7. 对话框在 UI 线程接收结果，追加 `result.outputLines`，按结果类型输出完成/错误/超时/已取消，恢复按钮状态，停止并释放线程。

## 取消语义

| 场景 | 行为 |
|------|------|
| 用户点击停止 | 设置取消标记，输出 `[系统] 正在请求停止脚本...`，禁用停止按钮避免重复点击 |
| Lua hook 命中取消 | `LuaSandboxResult.interrupted=true`，`success=false`，错误消息为 `Lua sandbox interrupted` |
| 脚本很快自然结束 | 如果停止请求后脚本自然结束，最终结果仍按实际执行结果展示 |
| 脚本卡在 C 回调 | 第一版不强杀线程；取消需等 Lua 返回到 hook 或 C 回调结束后生效 |
| 对话框析构时仍在运行 | 请求取消，退出线程并等待短时间；如仍未结束，记录状态并让 Qt 对象生命周期安全收尾 |

取消不是强制杀死线程。这样能避免 Lua state、Qt 对象和回调处于半释放状态。代价是某些长时间 C 回调只能在回调返回后取消。

## 线程与 UI 规则

- worker 线程不能直接访问 UI 控件。
- `sendRequested(QByteArray)`、`finished(LuaSandboxResult)`、`started()` 等信号必须通过 queued connection 回到对话框。
- `ScriptEditorDialog::appendOutput()` 只能在 UI 线程调用。
- `sendData(QByteArray)` 仍由对话框在 UI 线程发射，保持 MainWindow 现有连接不变。
- `LuaSandboxResult` 需要注册元类型，供 queued signal 传递。

## 错误处理

| 结果 | UI 输出 |
|------|---------|
| `success=true` | `[系统] 脚本执行完成` |
| `interrupted=true` | `[系统] 脚本已取消` |
| `timedOut=true` | `[错误] Lua sandbox timeout` 或本地化超时说明 |
| `memoryExceeded=true` | `[错误] Lua sandbox memory limit exceeded` |
| 普通 Lua 错误 | `[错误] <Lua 错误文本>` |
| 发送空数据或发送回调失败 | 由 `LuaSandbox` 返回 `serial.send failed` / `serial.sendHex failed` |

## 测试计划

| 测试 | 目的 |
|------|------|
| `TestLuaSandbox::interruptsInfiniteLoopWhenCallbackRequestsStop` | 证明 hook 能通过 `InterruptCallback` 中断死循环 |
| `TestScriptEditorDialog::runScriptReturnsControlBeforeLongScriptFinishes` | 证明点击运行后 UI 可继续处理事件，运行按钮立即禁用 |
| `TestScriptEditorDialog::stopScriptCancelsRunningSandbox` | 证明停止按钮能让死循环脚本返回取消状态 |
| `TestScriptEditorDialog::runScriptEmitsSendDataFromWorkerThread` | 证明后台执行时发送请求仍通过 `sendData(QByteArray)` 到 UI 层 |
| `TestScriptEditorDialog::runScriptRestoresButtonsAfterError` | 证明错误后按钮状态恢复 |

## 文档影响

- `docs/user-guide/scripting.md` 需要更新：脚本编辑器已后台执行，停止按钮会请求取消，但不是强杀线程。
- `resources/help/protocols.html` 通常不需要更新，除非其中 Lua 沙箱段落需要补“后台执行/取消”的用户可见能力。
- `resources/help/quickstart.html` 预计不需要更新，因为快速连接、普通发送、接收、文件传输和会话保存主流程不变；实现完成后仍需复核并在最终说明写明理由。

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| worker 生命周期泄漏 | 对话框持有 `QPointer<QThread>` / `QPointer<ScriptExecutionWorker>`，完成后统一清理 |
| queued signal 传递自定义类型失败 | 为 `LuaSandboxResult` 注册 `Q_DECLARE_METATYPE` 和 `qRegisterMetaType` |
| 停止后重复点击运行 | 状态机区分 Running、Cancelling、Idle，取消中不允许再次运行 |
| 对话框关闭时脚本仍运行 | 析构请求取消并安全等待线程退出 |
| 发送回调在 worker 线程触发 UI 操作 | worker 只发信号，对话框收到后再 append 输出和 emit sendData |
