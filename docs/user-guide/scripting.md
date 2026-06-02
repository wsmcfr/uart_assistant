# 脚本功能

ComAssistant 内置 Lua 脚本能力，用于自动化发送、数据转换、校验计算和自定义接收解析。脚本编辑器当前已接入受限 `LuaSandbox`，每次运行都会创建独立 Lua state，并在后台 worker 线程中执行；`serial.isOpen()` 会读取主窗口当前连接状态，`serial.send` / `serial.sendHex` 会通过受控回调复用现有发送队列，并在发送入口拒绝时返回 Lua 错误。

v1.7.0 起，Lua 脚本协议已作为稳定协议 ID `lua.script` 接入主窗口接收工作流。用户可以通过 **工具 -> 接收协议 -> Lua Script** 选择它，再通过 **工具 -> 协议配置...** 在多行 `scriptSource` 编辑器里编写 `process(data, context)`；配置会随会话保存，并在下次加载时按稳定协议 ID 恢复。旧版 **绘图协议** 菜单仍只负责 Raw/TEXT/STAMP/CSV/JustFloat 绘图链路，不承载 Lua。

## 打开脚本编辑器

1. 通过菜单或工具入口打开 **脚本编辑器**。
2. 编写 Lua 脚本。
3. 确认当前串口、网络或 HID 连接已打开。
4. 执行脚本并查看输出或错误信息。

## 常用 API

| API | 作用 |
|-----|------|
| `print(...)` | 输出调试文本 |
| `serial.send(data)` | 发送原始字符串或字节数据，失败时抛出 Lua 错误 |
| `serial.sendHex(hex)` | 按 HEX 字符串发送数据，失败时抛出 Lua 错误 |
| `serial.isOpen()` | 判断主窗口当前通信连接是否已打开 |
| `hexToBytes(hex)` | HEX 字符串转字节 |
| `bytesToHex(data)` | 字节转 HEX 字符串 |
| `crc16(data)` | 计算 Modbus CRC16 |
| `crc32(data)` | 计算 CRC32 |

> `sleep(ms)` 和 `serial.receive(timeout)` 在当前脚本编辑器沙箱中暂不可用。当前版本只开放后台执行、停止请求、发送、输出、HEX 转换和校验计算；接收 API 会在后续阶段评估。

## 示例：发送 AT 指令

```lua
if not serial.isOpen() then
    print("connection is not open")
    return
end

serial.send("AT\r\n")
print("AT command sent")
```

## 示例：发送 HEX 帧

```lua
local request = hexToBytes("01 03 00 00 00 02")
print("CRC16:", crc16(request))

serial.sendHex("AA 55 01 02 03")
```

## Lua 脚本协议解析器

`lua.script` 的协议解析入口是 `process(data, context)`。`data` 是本次传入的原始字节字符串，`context` 是只读约定表，当前包含 `protocolId`、`entryFunction` 和 `dataLength`。入口函数必须返回 table，协议解析器会把返回值映射为 `FrameResult`。

### 启用接收解析

1. 打开 **工具 -> 接收协议 -> Lua Script**。
2. 打开 **工具 -> 协议配置...**。
3. 在 `scriptSource` 多行编辑器中输入脚本。
4. 根据需要调整 `entryFunction`、`timeoutMs`、`memoryLimitKb`、`maxOutputLines` 和 `allowCommunicationApi`。
5. 点击确定后，后续接收数据会进入当前 Lua 协议实例解析。

配置面板会把校验错误显示在对应字段下方；没有可配置项的协议会显示“当前协议没有可配置项，可直接使用默认行为。”。Lua 脚本运行时错误会显示到状态栏，并记录为最近错误，可通过 **工具 -> 协议诊断...** 查看完整 JSON。

| 返回字段 | 作用 |
|----------|------|
| `valid` | 是否解析出有效帧 |
| `consumedBytes` | 本次消耗的输入字节数，C++ 层会限制到 `0..#data` |
| `frame` | 完整帧原始字节字符串 |
| `payload` | 有效载荷原始字节字符串 |
| `metadata` | 元数据表，当前支持字符串、数字和布尔标量 |
| `error` | 脚本主动返回的错误说明 |

```lua
function process(data, context)
    if #data < 3 then
        return {
            valid = false,
            consumedBytes = 0
        }
    end

    return {
        valid = true,
        consumedBytes = #data,
        frame = data,
        payload = string.sub(data, 2),
        metadata = {
            protocol = context.protocolId,
            length = context.dataLength,
            ok = true
        }
    }
end
```

当前版本只执行 `scriptSource` 内联脚本，不加载 `scriptPath` 外部文件；`build(payload)` 仍原样返回 payload，不调用 Lua 构帧入口。`allowCommunicationApi` 只影响发送类 API 是否注册，`serial.receive(timeout)` 仍未开放。

## 安全说明

- Lua 沙箱默认禁用 `io`、`os`、`package`、`debug`、`require`、`dofile`、`loadfile` 和 `load`，避免脚本直接读写文件、执行系统命令或加载外部模块。
- 沙箱执行支持超时中断、Lua 内存预算、输出行数限制和外部取消请求；脚本超出限制时会返回明确错误。
- 脚本编辑器会在后台线程执行 LuaSandbox，点击“停止”会请求取消并由 Lua hook 在安全检查点中断脚本。取消不是强制杀死线程；如果脚本正在执行长时间 C 回调，需要等回调返回后才会响应。
- `serial.send` 和 `serial.sendHex` 只发起本地发送请求，实际写入仍受当前通信连接、发送队列和连接状态约束；未连接、空数据、队列拒绝或底层写入失败都会作为 Lua 错误显示在输出区。
- `lua.script` 已通过稳定协议 ID 参与会话保存、加载和主窗口接收解析，但不会出现在旧版绘图协议菜单中；接收 API、外部脚本文件加载和脚本化构帧仍需后续设计。
- 仍然建议只运行自己编写或可信来源的脚本；沙箱用于降低 Lua 层风险，不等同于操作系统级隔离。
