# 4.6 脚本编辑器接入 LuaSandbox 设计

## 背景

4.5 已新增 `LuaSandbox` 核心执行器，默认禁用高风险 Lua 标准库，支持超时、内存预算、输出捕获和执行隔离。但当前 `ScriptEditorDialog` 仍使用正则模拟执行 `ui.log`、`serial.send` 和 `serial.sendHex`，并没有真正运行 Lua 语义。用户在脚本编辑器里看到的是 Lua 风格界面，实际执行能力却和 4.5 的安全沙箱脱节。

4.6 的目标是让脚本编辑器第一版真正使用 `LuaSandbox` 执行脚本，并开放受控发送 API。接收、后台线程和强取消暂不混入本轮，避免一次改动同时触碰通信接收缓冲、线程生命周期和 UI 取消语义。

## 目标与非目标

| 类型 | 内容 |
|---|---|
| 目标 | `ScriptEditorDialog` 使用 `LuaSandbox::execute()` 运行脚本文本，替代正则模拟执行 |
| 目标 | `LuaSandbox` 在显式启用通信 API 时注册 `serial.send(data)`、`serial.sendHex(hex)` 和 `serial.isOpen()` |
| 目标 | 脚本发送不直接触碰通信对象，只通过回调返回到 `ScriptEditorDialog::sendData(QByteArray)` |
| 目标 | 脚本输出和错误显示到脚本编辑器右侧输出区域 |
| 目标 | 更新脚本编辑器默认示例，让示例代码使用真实沙箱 API |
| 目标 | 通过单元测试覆盖受控通信 API、脚本输出、错误展示和发送信号 |
| 非目标 | 不实现 `serial.receive(timeout)` |
| 非目标 | 不做后台 worker 化执行，不实现跨线程强制取消 |
| 非目标 | 不注册 Lua 协议，不接入 `ProtocolRegistry` |
| 非目标 | 不做权限 UI 或脚本来源信任管理 |
| 非目标 | 不删除历史 `LuaEngine` |

## 方案选择

| 方案 | 做法 | 优点 | 风险 | 结论 |
|---|---|---|---|---|
| A | 只接入 `print` 和纯函数，不开放发送 | 最安全，改动最小 | 用户脚本仍不能做最核心的串口自动化发送 | 不选 |
| B | 接入 `LuaSandbox` 并开放 `serial.send` / `serial.sendHex` | 用户可见价值明确，发送仍走现有 `sendData` 和发送队列 | 仍需明确未连接时错误显示边界 | 推荐 |
| C | 同时实现 `serial.receive`、worker 和强取消 | 脚本自动化完整 | 会同时触碰接收缓冲、线程取消、UI 生命周期，风险偏高 | 暂缓 |

本阶段采用方案 B。

## 核心架构

| 模块 | 职责 |
|---|---|
| `LuaSandboxOptions` | 增加通信 API 回调和连接状态回调，默认仍关闭通信 API |
| `LuaSandbox` | 当且仅当 `allowCommunicationApi=true` 且存在发送回调时注册 `serial` 表 |
| `ScriptEditorDialog` | 构造沙箱选项、执行脚本、渲染输出和错误、把发送回调转成 `sendData` 信号 |
| `MainWindow` | 保持现有连接：`ScriptEditorDialog::sendData` -> `MainWindow::onSendData` -> `MainWindowCommunicationController::sendData` -> `SendQueue` |

## Lua API 设计

| API | 行为 |
|---|---|
| `serial.send(data)` | 接收 Lua 字符串/字节串，转换为 `QByteArray` 后调用发送回调 |
| `serial.sendHex(hex)` | 使用现有 `ConversionUtils::hexStringToBytes()` 转换 HEX 后调用发送回调 |
| `serial.isOpen()` | 调用可选状态回调；没有状态回调时返回 `true`，表示脚本编辑器启用了发送 API |
| `serial.receive(timeout)` | 不注册；脚本调用时会得到 Lua 运行时错误 |

发送回调只返回 `bool`。第一版中，`true` 表示脚本编辑器已把数据交给外层发送链路；最终设备是否成功发送仍由 `MainWindowCommunicationController` 和发送队列决定。

## 脚本编辑器行为

| 场景 | 行为 |
|---|---|
| 点击运行 | 禁用运行按钮，启用停止按钮视觉状态，清晰输出“开始执行脚本” |
| 脚本 `print` | 把 `LuaSandboxResult.outputLines` 逐行写到输出区域 |
| 脚本发送 | 发送回调 emit `sendData(data)`，同时输出 `[发送]` 或 `[发送HEX]` 摘要 |
| 脚本成功 | 输出“脚本执行完成” |
| 脚本错误 | 输出红色错误行，包含沙箱错误文本 |
| 脚本超时 | 输出红色超时错误行，按钮恢复 |
| 点击停止 | 4.6 不做强取消；如果脚本正在同步执行，停止按钮不会中断当前 Lua pcall，超时保护负责兜底 |

默认沙箱限制：

| 选项 | 值 |
|---|---|
| `timeoutMs` | 3000 |
| `memoryLimitKb` | 2048 |
| `maxOutputLines` | 500 |
| `allowCommunicationApi` | true，仅脚本编辑器入口设置 |

## 默认示例脚本

示例脚本改为真实 LuaSandbox API：

```lua
print("脚本加载完成")

if serial.isOpen() then
    serial.send("AT\r\n")
    serial.sendHex("AA 55 01 02 03")
end

local bytes = hexToBytes("01 03 00 00 00 02")
print("CRC16:", crc16(bytes))
```

不再展示 `utils.sleep()` 或 `ui.log()`，因为 4.6 不注册这些旧模拟 API。

## 错误处理

| 错误 | 表现 |
|---|---|
| Lua 语法错误 | 输出红色错误行，运行按钮恢复 |
| 运行时错误 | 输出红色错误行，运行按钮恢复 |
| 超时 | 输出红色超时错误，运行按钮恢复 |
| 内存超限 | 输出红色内存错误，运行按钮恢复 |
| 发送回调失败 | Lua API 抛出错误，让脚本执行结果失败 |
| 未连接导致外层发送失败 | 4.6 不在脚本层判断；外层状态栏和通信控制器继续按现有行为提示 |

## 测试计划

| 测试 | 覆盖点 |
|---|---|
| `TestLuaSandbox::registersSerialSendWhenCommunicationAllowed` | 显式启用通信 API 后 `serial.send` 调用发送回调 |
| `TestLuaSandbox::registersSerialSendHexWhenCommunicationAllowed` | `serial.sendHex` 按 HEX 转换后调用发送回调 |
| `TestLuaSandbox::serialApiStaysDisabledWithoutCallback` | 默认或无回调时不注册 `serial` |
| `TestLuaSandbox::serialSendFailureReturnsLuaError` | 发送回调失败会使执行失败 |
| `TestScriptEditorDialog::runScriptUsesLuaSandboxPrintOutput` | 编辑器运行真实 Lua，输出 `print` 内容 |
| `TestScriptEditorDialog::runScriptEmitsSendDataFromSerialSend` | `serial.send` 触发 `sendData` 信号 |
| `TestScriptEditorDialog::runScriptShowsLuaErrors` | Lua 错误显示到输出区 |

## 文档影响

| 文件 | 动作 |
|---|---|
| `docs/user-guide/scripting.md` | 更新脚本编辑器已接入沙箱、支持 `serial.send` / `serial.sendHex`，说明 `serial.receive` 暂未开放 |
| `resources/help/protocols.html` | 如果仍只讲协议平台，可不变；如提到 Lua 扩展则补充脚本编辑器已先接入沙箱发送 |
| `resources/help/quickstart.html` | 预计不更新，因为快速连接、发送、接收、会话保存主流程不变；实现后复核 |

## 后续阶段

| 阶段 | 方向 |
|---|---|
| 4.7 | worker 化脚本执行、停止按钮真实取消、`sleep(ms)` 安全实现 |
| 4.8 | `serial.receive(timeout)`，需要设计接收缓冲、超时和线程同步 |
| 4.9 | Lua 协议注册，接入 `ProtocolDescriptor`、`ProtocolConfigSchema` 和诊断导出 |
