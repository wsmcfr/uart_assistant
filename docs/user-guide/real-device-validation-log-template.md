# 真实设备验证日志模板

复制本模板为 `real-device-validation-YYYY-MM-DD-设备名.md` 后填写。所有失败项都要保留原始参数和错误文本，便于后续复现。

## 基本信息

| 字段 | 记录 |
|------|------|
| 验证日期 |  |
| 验证人员 |  |
| ComAssistant commit |  |
| 可执行文件 | `D:\comassistant\build_release\ComAssistant.exe` |
| Windows 版本 |  |
| 设备型号 |  |
| 固件版本 |  |
| 备注 |  |

## 文件与协议参数

| 字段 | 记录 |
|------|------|
| 文件名 |  |
| 文件大小 |  |
| CRC32/SHA256 |  |
| 串口参数 |  |
| Raw 块大小/间隔 |  |
| OTA magic/块大小/间隔 |  |
| OTA ACK token/超时/重试 |  |
| X/YMODEM 模式 |  |
| HID VID/PID/接口号 |  |
| HID Input/Output/Feature 长度 |  |
| HID Report ID |  |

## 自动化验证

| 命令 | 结果 | 备注 |
|------|------|------|
| `cmake --build build_release --config Release --target ComAssistant_tests --parallel` |  |  |
| `.\build_release\tests\ComAssistant_tests.exe` |  |  |
| `ctest --test-dir build_release --output-on-failure` |  |  |
| `cmake --build build_release --config Release --target ComAssistant --parallel` |  |  |

## Raw 分块发送

| 场景 | 参数 | 结果 | 证据 |
|------|------|------|------|
| 小文件发送 |  |  |  |
| 大文件发送 |  |  |  |
| 暂停/继续 |  |  |  |
| 取消 |  |  |  |
| 断开/失败 |  |  |  |

## 自定义 OTA

| 场景 | 参数 | 结果 | 证据 |
|------|------|------|------|
| 无 ACK |  |  |  |
| 等待 ACK |  |  |  |
| ACK 超时重试 |  |  |  |
| 暂停/继续 |  |  |  |
| 取消 |  |  |  |

## XMODEM/YMODEM

| 场景 | 参数 | 结果 | 证据 |
|------|------|------|------|
| XMODEM-CRC |  |  |  |
| XMODEM-1K |  |  |  |
| YMODEM |  |  |  |
| NAK 重发 |  |  |  |
| CAN 取消 |  |  |  |

## HID

| 场景 | 参数 | 结果 | 证据 |
|------|------|------|------|
| 枚举 |  |  |  |
| 打开/关闭循环 |  |  |  |
| Output Report |  |  |  |
| Input Report |  |  |  |
| Feature Set |  |  |  |
| Feature Get |  |  |  |
| Feature 中关闭 |  |  |  |

## 失败记录

| 编号 | 阶段 | 现象 | 参数 | 错误文本 | 初步定位 | 后续动作 |
|------|------|------|------|----------|----------|----------|
| 1 |  |  |  |  |  |  |

## 结论

| 项目 | 结论 |
|------|------|
| 是否通过 |  |
| 阻塞问题 |  |
| 需修复项 |  |
| 关联 issue/commit |  |
