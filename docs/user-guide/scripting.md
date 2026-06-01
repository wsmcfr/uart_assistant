# 脚本功能

ComAssistant 内置 Lua 脚本引擎，用于自动化发送、简单等待、数据转换和校验计算。当前脚本能力偏轻量，适合重复调试步骤，不建议运行不可信脚本。

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

- 当前 Lua 引擎会打开 Lua 标准库，请只运行自己编写或可信来源的脚本。
- `sleep(ms)` 会阻塞脚本执行，长时间等待会影响交互体验。
- 当前脚本执行取消、沙箱隔离和权限控制仍属于后续升级方向。
