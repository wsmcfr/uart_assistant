# 开发者指南

本文档面向希望参与 ComAssistant 开发的开发者。

相关文档：

- [工程审计与优化清单](project-audit-optimization.md)

## 架构概述

ComAssistant 采用模块化架构设计：

```
src/
├── core/                   # 核心模块
│   ├── communication/      # 通信模块（串口、TCP、UDP、USB HID）
│   ├── protocol/           # 协议解析模块
│   ├── config/             # 配置管理
│   ├── utils/              # 工具类
│   ├── script/             # 脚本引擎
│   ├── session/            # 会话管理
│   ├── export/             # 数据导出
│   └── theme/              # 主题管理
├── ui/                     # 用户界面
│   ├── widgets/            # UI 组件
│   ├── dialogs/            # 对话框
│   └── syntax/             # 语法高亮
└── third_party/            # 第三方库
    └── qcustomplot/        # 绘图库
```

## 开发环境

### 必需工具
- CMake 3.14+
- Qt 5.12.9
- MinGW 7.3 64-bit（本地）或 Visual Studio 2017+ / MSVC 工具集（CI/发布）
- vcpkg（包管理器）

### 可选工具
- NSIS（安装包制作）
- 7-Zip（便携版打包）

## 构建步骤

### 1. 克隆仓库

```bash
git clone https://github.com/wsmcfr/uart_assistant.git
cd uart_assistant
```

### 2. 安装依赖

```bash
vcpkg install spdlog nlohmann-json lua zlib hidapi
```

### 3. 配置 CMake

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
```

### 4. 编译

```bash
cmake --build build --config Debug
```

### 5. 运行

```bash
./build/Debug/ComAssistant.exe
```

## 代码规范

### 命名约定
- 类名：PascalCase（如 `MainWindow`）
- 函数名：camelCase（如 `onDataReceived`）
- 成员变量：m_ 前缀（如 `m_portCombo`）
- 常量：UPPER_CASE（如 `MAX_BUFFER_SIZE`）

### 文件组织
- 头文件和源文件分离
- 每个类一个文件
- 使用前向声明减少头文件依赖

### 注释规范
- 使用 Doxygen 风格注释
- 公共 API 必须有文档注释
- 复杂逻辑需要行内注释

### 修复完整性原则

| 原则 | 要求 |
|------|------|
| 完整实现，不做最小补丁 | 每个缺陷修复必须覆盖真实业务链路、关键状态、失败恢复、边界输入和旧接口兼容，不能只写一段让当前单个测试通过的局部绕法。 |
| 用户可见行为一致 | 若代码行为、UI 入口、帮助文档或用户指南存在不一致，必须在同一项修复中统一，不能留下“按钮能点但核心没做”的状态。 |
| 可靠性优先 | 通信、文件传输、导出、脚本等链路必须处理部分成功、超时、失败、取消、重试和资源释放，避免把异常路径误报为成功。 |
| 内存优化不降功能 | 降低内存占用时不得丢数据、改变协议语义或绕过用户显式配置；需要通过有界缓存、流式处理或清空释放容量来保持功能不变。 |
| 测试覆盖边界 | 每项行为修复至少覆盖成功路径、失败路径和关键边界；涉及恢复能力时必须验证恢复后不会重复发送、漏发或破坏状态。 |

## 模块说明

### 通信模块 (communication)

基于接口设计：
```cpp
class ICommunication {
public:
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray& data) = 0;
    virtual QByteArray readAll() = 0;
signals:
    void dataReceived(const QByteArray& data);
    void errorOccurred(const QString& error);
};
```

当前通信工厂支持串口、TCP 客户端、TCP 服务端、UDP 与 USB HID。HID 后端通过 `hidapi` 启用；未安装 hidapi 时工程仍可编译，但运行时会给出明确的后端未启用提示。

TCP Server、UDP 与 HID 的主接收路径是 `dataReceived` 信号；`readAll()` 只作为旧调用者兼容入口。兼容缓存必须通过 `ICommunication::appendToReceiveBuffer()` 维护，并按 `bufferSize()` 只保留最近尾部数据；运行中调小 `bufferSize()` 时必须调用 `trimReceiveBuffer()` 立即裁剪已有缓存。新增通信后端时不得在信号已分发后再维护无界影子缓存。

### 数据导出模块 (export)

`MainWindow` 的主导出入口必须使用结构化 `DataRecord` 历史，而不是从当前显示文本反推。普通接收、普通发送、TCP Server 定向/广播、UDP 指定目标、HID Output/Feature Report 等成功收发路径都应进入 `recordSentData()`、`recordAuxiliaryReceivedData()` 或 `appendExportHistoryRecord()`，保证 `文件 -> 导出数据` 与真实收发链路一致。

主窗口导出历史是有界缓存：记录数和 payload 字节数都达到上限时只保留最近记录；清空当前数据必须调用 `clearExportHistory()` 释放容量。新增导出格式或过滤项时，应复用 `DataExporter` 内部过滤规则，避免预览、统计和真实文件导出结果不一致。

真实文件导出不得通过 `filteredRecords()` 复制全部匹配记录，也不得先调用 `exportToString()` 或 `exportToBytes()` 拼完整文件内容。`DataExporter::exportToFile()` 必须复用 `exportToDevice()` 的流式核心：先只计数和统计字节，再按匹配记录逐条写入 `QIODevice`。`exportToString()`、`exportToBytes()` 只作为预览和旧接口兼容路径，不能作为大历史文件导出的实现基础。

### 数据显示窗口 (widgets)

主接收页和数据分窗都不能只依赖 `QTextDocument::setMaximumBlockCount()` 控制内存。该机制只对换行后的 block 有效，遇到设备持续输出无换行长流时会形成单个巨大 block。显示控件必须同时按字符总量裁剪头部、保留尾部最新内容，并在清空或裁剪后清理不需要的 undo 栈。

数据表格视图清空时不能只对记录容器和 `QStandardItemModel` 调用普通 `clear()`/`removeRows()`。`DataTableWidget::clearAll()` 必须释放已落屏记录、待刷新记录和旧表格模型的历史容量；重建空模型后仍要保留代理过滤、排序、表头翻译和后续新增记录能力。新增表格类缓存时，清空动作应同样释放可重建容量。

### 文件传输模块 (transfer)

XMODEM/YMODEM/YMODEM-G 标准协议发送不得使用 `readAll()` 把整文件载入内存。发送路径必须保持源 `QFile` 打开并按当前协议块读取：XMODEM 按 128/1024 字节块读取，YMODEM 与 YMODEM-G 数据包按 1024 字节块读取，0 号头包只读取文件名和大小等元数据。

普通 XMODEM/YMODEM 的重发能力通过“只缓存当前待 ACK 的完整协议包”实现：数据包收到 `NAK` 或等待 ACK 超时只重发 `m_lastPacket`，EOT 阶段超时只重发 EOT 控制字节。每个数据包 ACK 后必须清零当前重试计数，避免不同数据包的偶发 NAK 累加。完成、取消、失败、接收方 `CAN` 或 YMODEM 批量切换文件时必须释放当前文件句柄。

YMODEM-G 使用 `G` 握手并跳过逐数据包 ACK，不得复用普通 YMODEM 的 `WaitingDataAck` 推进方式。发送端必须用事件循环或定时器逐包读取和发送，避免一次性构造所有协议包；EOT 后仍等待接收端 ACK 和下一次 `G` 再发送空头结束。接收端校验通过的数据包直接写入文件且不 ACK；校验、序号或状态错误必须发送 CAN 中止并释放文件句柄，因为 G 模式没有 NAK 重传语义。

XMODEM 接收不得把所有数据块 append 到 `m_fileData` 后再写文件。接收模式必须保留半包 `m_receiveBuffer`，只有在完整包到齐并通过包号/校验验证后才写入目标 `QFile`；EOT 阶段只负责 ACK、flush 和关闭句柄。标准 XMODEM 没有真实文件大小字段，不能默认裁剪最后一包的 `CPMEOF`，否则可能误删用户真实数据。

### 协议模块 (protocol)

支持多种协议解析：
- Raw：原始数据
- Ascii：ASCII 文本
- Hex：十六进制
- Modbus：Modbus RTU
- Text/Stamp/Csv：绘图协议

### 脚本模块 (script)

生产脚本链路统一使用 `LuaSandbox`。脚本编辑器、`lua.script` 接收协议和后台执行 worker 都应通过 `LuaSandboxOptions` 注入通信状态、发送回调、取消回调和输出限制；新增脚本能力必须先明确沙箱边界，再补单元测试覆盖成功、错误和取消路径。

旧 `LuaEngine` 源码保留在 `src/core/script/` 仅作历史参考，不进入主程序或测试构建目标。不得给旧引擎新增 UI、协议或控制器入口；如果未来需要恢复某项能力，应迁移到 `LuaSandbox` 并保持标准库、阻塞调用和通信 API 的受控边界。

同样，`AutoSaveManager` 与旧 `IAPUpgrader` 源码当前不进入构建目标。会话恢复走现有会话管理链路；IAP 菜单真实入口复用 `FileTransferDialog::setIAPMode(true)` 和文件传输中心，不再依赖旧升级器模块。

## 添加新功能

### 添加新协议

1. 在 `src/core/protocol/` 创建协议类
2. 继承 `IProtocol` 接口
3. 在 `ProtocolFactory` 中注册
4. 更新 UI 协议选择

### 添加新工具

1. 在 `src/core/utils/` 创建工具类
2. 在 `ToolboxDialog` 中添加界面
3. 添加单元测试

## 测试

### 运行测试

```bash
cmake -B build -S . -DBUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug
```

### 添加测试

在 `tests/unit/` 创建测试文件：
```cpp
class TestMyFeature : public QObject {
    Q_OBJECT
private slots:
    void testSomething();
};
```

## 发布流程

1. 同步更新版本号（`src/version.h` 与 `CMakeLists.txt`）
2. 更新 `CHANGELOG.md`、`README.md` 与内置帮助文档 `resources/help/quickstart.html`
3. 运行所有测试
4. 构建 Release 版本
5. 创建安装包
6. 创建 GitHub Release

## 贡献指南

1. Fork 仓库
2. 创建功能分支
3. 编写代码和测试
4. 提交 Pull Request

详细贡献指南请参阅 [CONTRIBUTING.md](../../CONTRIBUTING.md)
