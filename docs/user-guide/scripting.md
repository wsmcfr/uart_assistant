# 脚本功能

ComAssistant 内置 Lua 脚本能力，用于自动化发送、数据转换和校验计算。脚本编辑器当前已接入受限 `LuaSandbox`，每次运行都会创建独立 Lua state，并在后台 worker 线程中执行；`serial.isOpen()` 会读取主窗口当前连接状态，`serial.send` / `serial.sendHex` 会通过受控回调复用现有发送队列，并在发送入口拒绝时返回 Lua 错误。

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

## 安全说明

- Lua 沙箱默认禁用 `io`、`os`、`package`、`debug`、`require`、`dofile`、`loadfile` 和 `load`，避免脚本直接读写文件、执行系统命令或加载外部模块。
- 沙箱执行支持超时中断、Lua 内存预算、输出行数限制和外部取消请求；脚本超出限制时会返回明确错误。
- 脚本编辑器会在后台线程执行 LuaSandbox，点击“停止”会请求取消并由 Lua hook 在安全检查点中断脚本。取消不是强制杀死线程；如果脚本正在执行长时间 C 回调，需要等回调返回后才会响应。
- `serial.send` 和 `serial.sendHex` 只发起本地发送请求，实际写入仍受当前通信连接、发送队列和连接状态约束；未连接、空数据、队列拒绝或底层写入失败都会作为 Lua 错误显示在输出区。
- 仍然建议只运行自己编写或可信来源的脚本；沙箱用于降低 Lua 层风险，不等同于操作系统级隔离。
