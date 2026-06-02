# 4.8 脚本编辑器真实连接状态与通信错误语义设计

## 背景

4.6 已把脚本编辑器从正则模拟执行迁移到 `LuaSandbox`，并开放 `serial.send`、`serial.sendHex` 和 `serial.isOpen()`。4.7 又把执行迁移到 `ScriptExecutionWorker` 后台线程，并用 Lua hook 支持停止按钮协作式取消。

当前剩余问题是发送侧语义仍偏“通道可用”：worker 内部把 `serial.isOpen()` 固定为 `true`，`serial.send` 只要发出 UI 信号就返回成功。这样脚本无法区分未连接、发送队列拒绝、底层写入失败或连接中途断开。4.8 的目标是让脚本通信 API 对齐主窗口真实通信状态，并把发送失败转成稳定 Lua 错误。

## 范围

| 类型 | 内容 |
|------|------|
| 目标 | `serial.isOpen()` 反映主窗口当前连接状态 |
| 目标 | `serial.send` / `serial.sendHex` 等待 UI 线程返回发送接受或拒绝结果 |
| 目标 | 未连接、空数据、发送队列拒绝、底层写入失败时返回清晰 Lua 错误 |
| 目标 | worker 仍不直接访问 UI、主窗口或通信对象 |
| 目标 | 保留 `sendData(QByteArray)` 信号作为脚本发送成功后的兼容通知 |
| 非目标 | 不开放 `serial.receive(timeout)` |
| 非目标 | 不注册 Lua 协议，不实现脚本协议 Schema |
| 非目标 | 不改造 `MainWindowCommunicationController` 的通信对象生命周期 |
| 非目标 | 不改变普通发送、文件传输、快捷发送的现有发送入口 |

## 方案比较

| 方案 | 做法 | 优点 | 风险 | 结论 |
|------|------|------|------|------|
| A | worker 继续发 `sendRequested`，UI 异步显示失败 | 改动小 | Lua 调用已经返回，无法把失败变成脚本错误 | 不采用 |
| B | worker 持有同步发送回调，回调用 `BlockingQueuedConnection` 切回 UI 线程 | Lua 可获得真实发送结果，worker 不碰通信对象 | 需要避免 UI 线程死锁，并严格限制回调职责 | 推荐 |
| C | worker 直接调用 `MainWindowCommunicationController` | 代码路径短 | 跨线程触碰 QObject 和通信状态，破坏 4.7 线程边界 | 不采用 |

推荐方案 B。worker 的 Lua 回调仍在后台线程触发，但只调用 `ScriptEditorDialog` 提供的线程安全 wrapper。wrapper 如果已在 UI 线程就直接执行；如果在 worker 线程，则通过 `QMetaObject::invokeMethod(..., Qt::BlockingQueuedConnection)` 切回 UI 线程，等待主窗口发送处理给出接受或拒绝结果。

## 架构

| 组件 | 职责 |
|------|------|
| `LuaSandboxOptions` | 新增带错误文本的发送回调，保留旧布尔回调兼容测试和旧调用 |
| `LuaSandbox` | `serial.send` / `serial.sendHex` 发送失败时把具体原因拼入 Lua 错误 |
| `ScriptExecutionWorker` | 接收连接状态回调和发送回调，注册到 `LuaSandboxOptions` |
| `ScriptEditorDialog` | 保存主窗口注入的连接状态 provider 和发送 handler；负责线程切换、输出提示和兼容信号 |
| `MainWindow` | 打开脚本编辑器时注入 `m_commController->isConnected()` 和 `m_commController->sendData()` |

## 数据流

1. 用户打开脚本编辑器。
2. `MainWindow::onScriptEditor()` 创建对话框，并注入：
   - 连接状态 provider：读取 `m_commController->isConnected()`。
   - 发送 handler：检查连接，调用 `m_commController->sendData(data)`，失败时返回 `lastError()`。
3. 用户运行脚本，`ScriptEditorDialog` 创建 worker。
4. worker 注册沙箱选项：
   - `isOpenCallback` 调用注入的连接状态 wrapper。
   - `sendWithErrorCallback` 调用注入的发送 wrapper。
5. Lua 调用 `serial.isOpen()` 时，worker 通过 wrapper 同步读取 UI 线程状态。
6. Lua 调用 `serial.send(data)` 时，worker 通过 wrapper 同步请求 UI 线程发送。
7. UI 线程发送成功：
   - `MainWindowCommunicationController::sendData()` 返回 `true`。
   - 对话框追加 `[发送] ...`，发出兼容 `sendData(QByteArray)` 通知。
   - Lua 调用成功返回。
8. UI 线程发送失败：
   - handler 返回 `accepted=false` 和错误文本。
   - 对话框追加 `[发送失败] ...`。
   - `LuaSandbox` 返回 `serial.send failed: <reason>` 或 `serial.sendHex failed: <reason>`。
   - `handleWorkerFinished()` 按普通 Lua 错误展示。

## 线程规则

| 规则 | 说明 |
|------|------|
| worker 不访问 UI | worker 只能调用无 UI 对象所有权的 `std::function` |
| UI 状态只在 UI 线程读写 | 对话框 wrapper 会检测当前线程，必要时阻塞投递到 UI 线程 |
| 不在 UI 线程使用阻塞投递 | 当前线程已是 UI 时直接执行，避免死锁 |
| 发送 handler 必须短小 | handler 只做连接检查和本地发送队列入队/写入，不做长耗时等待 |
| 取消仍由 Lua hook 处理 | 4.8 不改变 4.7 的停止按钮语义 |

## 错误语义

| 场景 | Lua 错误或返回 |
|------|----------------|
| `serial.isOpen()` 且已连接 | `true` |
| `serial.isOpen()` 且未连接 | `false` |
| `serial.send('')` | `serial.send failed: 脚本发送数据不能为空` |
| 未连接发送 | `serial.send failed: 当前连接未打开` |
| 队列拒绝或写入失败 | `serial.send failed: <MainWindowCommunicationController::lastError()>` |
| HEX 发送失败 | `serial.sendHex failed: <reason>` |
| 发送成功 | Lua 调用返回，无额外返回值 |

## 测试计划

| 测试 | 目的 |
|------|------|
| `TestLuaSandbox::serialSendFailureCanExposeSpecificReason` | 证明 LuaSandbox 能把发送回调的具体失败原因暴露为 Lua 错误 |
| `TestScriptEditorDialog::serialIsOpenReflectsInjectedConnectionState` | 证明脚本编辑器的 `serial.isOpen()` 使用注入状态 |
| `TestScriptEditorDialog::serialSendRejectsWhenConnectionClosed` | 证明未连接发送不会发出发送信号，且输出明确错误 |
| `TestScriptEditorDialog::serialSendReportsHandlerFailureReason` | 证明发送 handler 拒绝时 Lua 输出包含具体失败原因 |
| 既有脚本发送测试 | 证明发送成功路径仍会发出 `sendData(QByteArray)` 兼容通知 |

## 文档影响

- `docs/user-guide/scripting.md` 需要更新：`serial.isOpen()` 现在是真实连接状态，发送失败会成为 Lua 错误。
- `resources/help/protocols.html` 需要更新脚本沙箱说明：发送 API 现在会校验连接和发送队列结果。
- `resources/help/quickstart.html` 预计不需要更新，因为快速连接、普通发送、接收、文件传输、会话保存和版本信息主流程不变；实现后仍需复核。

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 阻塞投递造成 UI 死锁 | wrapper 检查当前线程，UI 线程直接执行 |
| 发送 handler 变成长耗时操作 | handler 仅调用现有本地发送入口，不等待设备响应 |
| 兼容 `sendData` 信号导致重复发送 | MainWindow 4.8 改为注入发送 handler，不再把脚本 `sendData` 信号连接到 `onSendData()` |
| 对话框关闭时 worker 仍在请求发送 | wrapper 使用 `QPointer` 和取消标记；对象失效时返回拒绝 |
| 错误文本过空泛 | handler 优先返回 `lastError()`，为空时使用稳定兜底文案 |
