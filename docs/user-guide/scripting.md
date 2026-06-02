# 脚本功能

ComAssistant 内置 Lua 脚本能力，用于自动化发送、简单等待、数据转换和校验计算。当前脚本编辑器仍保持既有轻量执行入口，4.5 已新增独立 Lua 安全沙箱核心，后续会逐步把脚本编辑器和脚本协议扩展迁移到沙箱入口。

## 打开脚本编辑器

1. 通过菜单或工具入口打开 **脚本编辑器**。
2. 编写 Lua 脚本。
3. 确认当前串口、网络或 HID 连接已打开。
4. 执行脚本并查看输出或错误信息。

## 常用 API

| API | 作用 |
|-----|------|
| `print(...)` | 输出调试文本 |
| `sleep(ms)` | 阻塞等待指定毫秒数 |
| `serial.send(data)` | 发送原始字符串或字节数据 |
| `serial.sendHex(hex)` | 按 HEX 字符串发送数据 |
| `serial.receive(timeout)` | 尝试接收数据，超时返回 `nil` |
| `serial.isOpen()` | 判断当前连接是否打开 |
| `hexToBytes(hex)` | HEX 字符串转字节 |
| `bytesToHex(data)` | 字节转 HEX 字符串 |
| `crc16(data)` | 计算 Modbus CRC16 |
| `crc32(data)` | 计算 CRC32 |

## 示例：发送 AT 指令

```lua
if not serial.isOpen() then
    print("connection is not open")
    return
end

serial.send("AT\r\n")
sleep(200)

local data = serial.receive(1000)
if data then
    print(bytesToHex(data))
else
    print("timeout")
end
```

## 示例：发送 HEX 帧

```lua
serial.sendHex("AA 55 01 02 03")
sleep(100)
```

## 安全说明

- 新的 Lua 沙箱核心默认禁用 `io`、`os`、`package`、`debug`、`require`、`dofile`、`loadfile` 和 `load`，避免脚本直接读写文件、执行系统命令或加载外部模块。
- 沙箱执行支持超时中断、Lua 内存预算和输出行数限制；脚本超出限制时会返回明确错误。
- 4.5 阶段先落地沙箱核心基座，脚本编辑器的完整 Lua 运行时迁移仍属于后续升级；当前编辑器入口保持既有行为。
- 仍然建议只运行自己编写或可信来源的脚本；沙箱用于降低 Lua 层风险，不等同于操作系统级隔离。
