# 工程审计与优化清单

本文记录 2026-06-03 对 ComAssistant 工程的审计结论，后续修复必须按“完整实现，不做最小补丁”的原则逐项闭环。

## 修复原则

| 原则 | 要求 | 验收方式 |
|------|------|----------|
| 完整实现，不做最小补丁 | 每个修复都必须覆盖真实业务链路、关键状态、失败恢复、边界输入和旧接口兼容，不接受只为让单个测试通过的局部绕法。 | 每项任务必须写清楚行为目标、边界条件、兼容策略和测试覆盖。 |
| 用户可见行为一致 | 如果代码行为、UI 入口、帮助文档或用户指南存在不一致，必须在同一项修复中统一。 | 代码、UI、`resources/help/quickstart.html`、相关用户文档互相一致。 |
| 可靠性优先于表面完成 | 通信、文件传输、导出等链路必须处理部分成功、超时、失败、取消、重试和资源释放。 | 测试覆盖成功路径、失败路径、边界路径和恢复路径。 |
| 内存优化不降功能 | 降内存时不能丢数据、不能改变协议语义、不能缩短用户显式配置的历史保留能力。 | 通过有界缓存、流式处理或清空释放容量保持功能不变。 |
| 每项修复独立闭环 | 每次只完成一个明确问题域，避免把多个无关改动混在一起。 | 每项完成后运行相关测试，并按需更新帮助文档和 RecallLoom。 |

## 未完成或疑似摆样子功能

| 优先级 | 发现 | 影响 | 证据 | 处理策略 |
|--------|------|------|------|----------|
| 高 | YMODEM 接收入口存在但核心没实现 | UI/文档让用户以为能接收，实际 `processReceivedData()` 只处理发送方向。 | `src/core/transfer/FileTransfer.cpp:1380`、`src/core/transfer/FileTransfer.cpp:1424`、`resources/help/advanced.html:245` | 已完成。实现 YMODEM 接收状态机，支持文件头解析、CRC/包号校验、数据包落盘、双 EOT 收尾、空头批次结束和不安全文件名拦截。 |
| 高 | `SendDispatcher` 没处理部分写入 | `write()` 返回 `0 < written < payload.size()` 时会被误判为完成，串口/TCP/Socket 可能丢尾包。 | `src/core/communication/SendDispatcher.cpp:80` | 已完成。`SendQueueItem::bytesWritten` 记录队首写入偏移，`SendDispatcher` 只发送剩余片段；覆盖多次部分写入、部分写入后失败恢复、0 字节停滞和超量返回异常。 |
| 高 | TCP Server / UDP / HID 有影子接收缓存 | 数据已通过 `dataReceived` 被主窗口消费，同时 append 到内部 `m_readBuffer`；主流程不调用 `readAll()` 时会长期占内存。 | `src/core/communication/TcpServer.cpp`、`src/core/communication/UdpSocket.cpp`、`src/core/communication/HidDevice.cpp` | 已完成。保留 `readAll()` 兼容缓存，但统一通过 `ICommunication::appendToReceiveBuffer()` 按 `bufferSize()` 只保留尾部最近数据；运行中调小 `bufferSize()` 会立即裁剪已有缓存；`readAll()`、`clearBuffer()`、`close()` 会释放兼容缓存容量。 |
| 中高 | 增强导出对话框和 `DataExporter` 基本完整，但主窗口没接上 | 文档写“文件 -> 导出数据可选 TXT/CSV/HTML/JSON”，实际主入口只是简单保存文本。 | `src/ui/MainWindow.cpp:1341`、`src/ui/MainWindow.h:317`、`docs/user-guide/quickstart.md:123` | 已完成。主窗口维护有界结构化收发历史并接入 `ExportDialog`，覆盖串口、TCP、UDP、HID 的 RX/TX 数据；导出对话框过滤统计按真实过滤结果计算；串口/TCP 接收导出按完整文本行合并底层分片，避免一条日志被导出成多条。 |
| 中 | `IAPUpgrader`、`AutoSaveManager`、旧 `LuaEngine` 编译进工程但生产入口基本没用 | 属于遗留/未接线模块，增加维护成本、体积和误用风险。 | `CMakeLists.txt:141`、`CMakeLists.txt:148`、`CMakeLists.txt:156` | 已完成。源码保留作历史参考，但主程序和测试构建目标不再编译这些旧模块；IAP 菜单真实入口复用 `FileTransferDialog` 的文件传输/IAP 模式，脚本生产链路统一使用 `LuaSandbox`。 |
| 中 | 帧模式工具栏露出 XOR/SUM/CRC16 校验选项，但接收校验未真正执行 | 用户选择校验类型后会以为坏帧能被标记，旧实现 `validateFrame()` 跳过校验。 | `src/ui/modes/FrameModeWidget.cpp`、`resources/help/modes.html`、`tests/unit/TestFrameModeWidget.cpp` | 已完成。补齐 XOR/SUM/CRC16 接收校验、错误提示、带帧头尾发送自动追加校验字节，并说明 CRC16 低字节在前；同时拒绝空帧头/帧尾覆盖有效配置，避免接收解析失去边界。 |

## 无损降内存方向

| 建议 | 原因 | 证据 | 处理策略 |
|------|------|------|----------|
| XMODEM/YMODEM 发送改成按块读文件 | 当前标准协议发送会 `readAll()` 整个文件，固件越大峰值越高。 | `src/core/transfer/FileTransfer.cpp:952`、`src/core/transfer/FileTransfer.cpp:1504` | 已完成。发送路径改为 `QFile` 按协议块读取；仅缓存当前待 ACK 的完整协议包用于 NAK/超时重发；EOT 阶段重发 EOT；完成、取消、失败和批量切换文件时释放文件句柄。 |
| XMODEM 接收边收边写文件 | 当前接收块 append 到 `m_fileData`，结束后再写，接收大文件会多占一份内存。 | `src/core/transfer/FileTransfer.cpp:1160` | 已完成。完整包校验通过后立即写入目标 `QFile`；半包保留在 `m_receiveBuffer`；EOT 只 flush/close；取消和失败释放文件句柄。标准 XMODEM 不携带真实文件大小，最后一包 CPMEOF 保持原样写入。 |
| `DataExporter` 改流式过滤、流式写入 | 真实文件导出如果先生成过滤记录和完整 `QString/QByteArray`，大历史导出会产生额外峰值内存。 | `src/core/export/DataExporter.cpp`、`tests/unit/TestDataExporter.cpp` | 已完成。`exportToFile()` 复用 `exportToDevice()` 流式核心，两次线性扫描完成计数和逐条写入；TXT、CSV、HTML、JSON、XML、Binary、HexDump 都按匹配记录分块写入，不再复制过滤全集或拼完整文件内容。 |
| `DataWindow` 补字符数裁剪 | 主接收页已处理无换行超长流，数据分窗如果只靠最大 block 数，长行可能单 block 膨胀。 | `src/ui/widgets/DataWindow.cpp`、`tests/unit/TestDataWindow.cpp` | 已完成。分窗文本区按字符上限裁剪头部，保留尾部最新数据；清空时同步清理 `QTextDocument` undo 栈；导出只包含裁剪后的可见内容。 |
| `DataTableWidget::clearAll()` 后 `squeeze()` | 清空记录但保留历史峰值容量，用户清空后内存不一定回落。 | `src/ui/widgets/DataTableWidget.cpp`、`tests/unit/TestMemoryAwareUiBehavior.cpp` | 已完成。清空后释放 `m_records` 和 `m_pendingRecords` 容量，并替换空表格模型以释放模型内部历史分配。 |
| 调试/帧/Modbus 等模式的详情文本框关闭 undo | 这些输出框多为只读历史显示，保留 undo 栈没有使用价值，长时间接收会增加文档内部缓存。 | `src/ui/modes/FrameModeWidget.cpp`、`src/ui/modes/DebugModeWidget.cpp`、`src/ui/widgets/ModbusAnalyzerWidget.cpp` | 已完成。只读详情区已关闭 undo；清空时清理 undo 栈，避免切换/清空后保留无意义历史。 |
| 暂停显示或批量刷新队列继续有界化 | 主接收区已限制暂停缓存，但其它模式仍可能存在待刷新 `QVector/QByteArray` 清空后容量不回落或暂停期间增长的问题。 | `src/ui/modes/DebugModeWidget.cpp`、`src/ui/modes/FrameModeWidget.cpp`、`src/ui/widgets/WaterfallWidget.cpp` | 已完成。Frame/Debug/Modbus/Waterfall 清空时释放 pending、记录和接收缓冲容量；Frame/Debug 记录队列使用 `QVector` 以便明确 `squeeze()`。 |

## 其它可优化点

| 项目 | 建议 | 关闭标准 |
|------|------|----------|
| `YModemG` | 已完成。标准协议下拉框已显示 YMODEM-G；核心发送端使用 `G` 握手并按 0ms 定时泵逐包发送，不等待逐数据包 ACK；接收端使用 `G` 握手，成功数据包不 ACK，EOT 后 ACK+G 请求下一文件，错误包发送 CAN 并失败退出。 | `tests/unit/TestFileTransfer.cpp` 覆盖发送无逐包 ACK、接收 G 握手、错误包中止和 UI 协议入口。 |
| 裸流/OTA 接收 | 当前明确提示未实现，不算暗坑；文档继续保持“仅发送”即可。 | 文档和 UI 始终明确第一版仅发送。 |
| `ExportDialog::updateStatistics()` | 已完成。过滤后数量现在调用 `DataExporter::filteredRecordCount()`，与真实导出过滤规则一致。 | `tests/unit/TestExportDialog.cpp` 覆盖内容过滤后统计从 3 条变为 2 条。 |
| 旧 `LuaEngine` | 已完成。旧源码保留但不进入主程序或测试目标；脚本编辑器、Lua 协议解析和开发文档均以 `LuaSandbox` 为生产入口。 | `tests/unit/TestReleaseMetadata.cpp` 验证旧 `LuaEngine.cpp` 未进入构建清单。 |
| `ConfigManager::saveConfig(filePath)` | 已完成。显式路径保存使用临时 `QSettings` 写入目标文件，不切换当前配置对象，也不依赖原 `m_settings` 路径。 | `tests/unit/TestConfigManager.cpp` 覆盖显式路径真实写入且当前设置路径保持不变。 |
| 自定义 OTA Magic | 已完成。内部 `OtaTransferOptions::magic` 改为 `quint32`；UI 同时接受旧版 4 字节 ASCII 和 `0x12345678UL` 十六进制常量，启动前校验格式并按小端写入头包前 4 字节。 | `tests/unit/TestFileTransfer.cpp` 覆盖 ASCII 兼容、`uint32_t` magic、越界拒绝和 UI 不截断十六进制输入。 |

## 执行顺序

| 顺序 | 任务 | 验证重点 |
|------|------|----------|
| 1 | 修复 `SendDispatcher` 部分写入处理 | 已完成。多次部分写入不丢尾包；部分写入后失败只重发剩余数据；0 字节写入不会误判成功或死循环。 |
| 2 | 限制 TCP Server / UDP / HID 影子缓存 | 已完成。长时间信号消费不积累隐藏缓存；旧 `readAll()` 调用只保留 `bufferSize()` 上限内的最近数据；关闭、清空或读取旧缓存后释放容量。 |
| 3 | 处理 YMODEM 接收承诺 | 已完成。UI、内置帮助、核心状态机和测试对 YMODEM 接收能力一致。 |
| 4 | 接入或降级导出功能 | 已完成。主菜单导出能力与文档一致；导出历史有界且清空释放容量；`DataExporter` 真实文件导出已改为流式过滤和逐条写入。 |
| 5 | X/YMODEM 流式读写 | 已完成发送侧和 XMODEM 接收侧：大文件传输内存峰值下降，协议行为保持兼容。 |
| 6 | `DataWindow` / `DataTableWidget` 内存回落 | 已完成。`DataWindow` 按字符数裁剪无换行长流；`DataTableWidget::clearAll()` 释放记录容器和表格模型历史容量。 |
| 7 | 清理遗留模块 | 已完成。旧 `IAPUpgrader`、`AutoSaveManager`、`LuaEngine` 不再进入主程序和测试构建目标；文档说明真实入口和废弃边界。 |
| 8 | 补齐 YMODEM-G 能力露出 | 已完成。核心、UI、帮助文档和测试均承认 YMODEM-G 为标准协议选项，并明确其适合低误码链路。 |

## 已完成修复记录

| 日期 | 修复项 | 完整实现范围 | 回归测试 |
|------|--------|--------------|----------|
| 2026-06-03 | `SendDispatcher` 部分写入处理 | 队列记录队首写入偏移；调度器只写未完成尾部；成功完成前强制校验整包已写完；失败后保留进度以便只重试剩余数据；0 字节写入按停滞失败处理；底层返回超过剩余长度时拒绝推进。 | `tests/unit/TestSendQueue.cpp` 覆盖队首写入进度、多次部分写入、部分写入后失败恢复、0 字节停滞和超量返回异常。 |
| 2026-06-03 | TCP Server / UDP / HID 影子接收缓存 | `dataReceived`/`clientDataReceived`/`datagramReceived` 仍发送完整数据；`readAll()` 兼容缓存通过 `ICommunication` 统一 helper 只保留 `bufferSize()` 限制内的尾部最近数据；`setBufferSize()` 调小后立即裁剪已有缓存；`bufferSize() <= 0` 保持“不限制”兼容语义；修正 UDP 无 pending datagram 时 `bytesAvailable()` 误减 1 的统计问题。 | `tests/unit/TestCommunicationReceiveBuffers.cpp` 覆盖 TCP、UDP、HID 三后端的完整信号接收、有界 `readAll()` 兼容缓存、运行中调小缓存立即裁剪。 |
| 2026-06-03 | 帧模式校验与清空内存回落 | 帧模式接收按 `帧头 + payload + checksum + 帧尾` 校验 XOR/SUM/CRC16；CRC16 使用 Modbus CRC16 且低字节在前；“带帧头尾发送”会按当前配置追加校验字节；普通发送保持原始 HEX；空帧头/帧尾不会覆盖当前有效配置，异常空配置解析入口会丢弃半包防止 UI 卡死；Frame/Debug/Modbus 只读详情区关闭 undo，Frame/Debug/Modbus/Waterfall 清空时释放记录、pending 和接收缓冲容量；TCP Server/UDP/HID 的 `readAll()`、`clearBuffer()`、`close()` 释放兼容缓存容量，TCP Server 关闭后同步清空客户端引用。 | `tests/unit/TestFrameModeWidget.cpp` 覆盖坏校验无效、好校验有效、带帧头尾发送追加校验、空帧头/帧尾拒绝、清空释放容量；`tests/unit/TestCommunicationReceiveBuffers.cpp` 覆盖 TCP/UDP/HID 清空/关闭释放兼容缓存容量和 TCP Server 关闭后清空客户端列表。 |
| 2026-06-03 | YMODEM 接收入口与核心实现不一致 | `YModemTransfer` 接收模式现在主动发送 `C` 握手，解析 0 号文件头，按声明文件大小写入目标文件，处理数据包 ACK、坏包 NAK、重复包补 ACK、双 EOT 收尾和空 0 号头批次结束；对端文件名会拦截绝对路径、目录分隔符、盘符和 `..`，避免写出保存目录。 | `tests/unit/TestFileTransfer.cpp` 覆盖单文件批次完整接收、坏头包 NAK 后重试、路径穿越文件名拒绝。 |
| 2026-06-03 | 增强导出主入口未接入 | `文件 -> 导出数据` 现在打开 `ExportDialog`，使用主窗口最近有界结构化收发历史；普通发送、TCP Server 定向/广播、UDP 指定目标、HID Output/Feature 和接收数据都会进入导出历史；历史按 10000 条和 16MB payload 双上限裁剪，清空当前数据时释放容量；`ExportDialog` 过滤统计和预览复用 `DataExporter` 过滤规则；串口/TCP 接收导出按完整文本行合并 readyRead 分片，无换行长流达到 64KB 上限后提交当前片段，UDP/HID 保持报文粒度。 | `tests/unit/TestMainWindowExportIntegration.cpp` 覆盖主窗口 RX/TX 后导出入口打开增强对话框并显示预览，以及 32/32/4 分片的一条串口日志只导出为一条 RX 记录；`tests/unit/TestExportDialog.cpp` 覆盖过滤统计真实反映当前条件。 |
| 2026-06-03 | 清屏只清界面、断开后历史不回落 | 主窗口清屏现在走 `clearAllUserDataCaches()`，会清除已创建的串口显示模式、网络/HID 工作台日志、增强导出历史、数据表格、数据分窗、绘图数据、绘图行缓冲、协议运行期缓存和通信兼容缓存，并调用工作集整理。断开连接不清空可见屏幕，但会把增强导出历史裁剪到最近 256KB/256 条并释放 `ExportDialog` 等可重建副本；导出对话框和数据表格窗口关闭后也会销毁对象，避免隐藏窗口继续持有记录副本或表格模型。 | `tests/unit/TestMainWindowExportIntegration.cpp` 覆盖断开连接后导出历史裁剪到 256KB/256 条、导出对话框关闭后销毁并释放复制记录；`tests/unit/TestMainWindowLazyLoading.cpp` 覆盖数据表格窗口关闭后销毁。 |
| 2026-06-03 | XMODEM/YMODEM 标准协议发送整文件缓存 | XMODEM 发送启动后只记录文件大小和分包数，按当前 128/1024 字节块读取；YMODEM 头包阶段只读取文件元数据，数据包按 1K 读取；两者都只缓存当前待 ACK 协议包用于 NAK/超时重发，EOT 超时只重发 EOT，完成、取消、失败、接收方取消和批量切换文件时释放文件句柄。 | `tests/unit/TestFileTransfer.cpp` 覆盖 XMODEM/YMODEM 发送不缓存整文件、XMODEM EOT 超时重发 EOT、YMODEM NAK 重发当前包、每包 ACK 后重置重试计数。 |
| 2026-06-03 | 裸流/OTA 文件发送完成提示早于串口硬件发送结束 | Raw/OTA 文件传输从主窗口发送入口改走专用异步排空链路：串口后端等待 Qt/系统写缓冲清空，再按当前数据位、校验位、停止位和波特率重新等待完整线路发送时间；进度条每块在排空后才推进，最后一块排空后才显示完成。标准 X/YMODEM 仍由协议 ACK/NAK/G 握手驱动，避免破坏 YMODEM-G 连续发送。 | `tests/unit/TestMainWindowCommunicationController.cpp` 覆盖 drain 未完成不回调成功、drain 失败回传错误；`tests/unit/TestSerialPortTransmitDrain.cpp` 覆盖写缓冲清空后仍等待完整线路时间。 |
| 2026-06-03 | XMODEM 接收整文件累计到内存 | XMODEM 接收完整包校验通过后立即写入目标文件，半包会保留在 `m_receiveBuffer` 等待后续字节补齐；EOT 阶段只 ACK、flush 和关闭文件；发送方 CAN、校验失败超限和主动取消都会释放文件句柄；由于标准 XMODEM 无真实文件大小字段，最后一包 `CPMEOF` 填充不默认裁剪。 | `tests/unit/TestFileTransfer.cpp` 覆盖接收不累计 `m_fileData`、半包保留、发送方取消释放文件、校验失败释放文件。 |
| 2026-06-03 | `DataExporter` 文件导出复制过滤记录并拼完整内容 | `exportToFile()` 现在打开目标设备后走 `exportToDeviceInternal()`：第一次扫描只计算匹配数量和总字节数，第二次按过滤结果逐条写入。文本格式通过 `QTextCodec` 编码后分块写入并统计真实字节数；二进制直接逐条写 payload；JSON 不再构造完整 `QJsonArray`。`exportToString()`/`exportToBytes()` 保留给预览和旧接口。 | `tests/unit/TestDataExporter.cpp` 覆盖文本导出分块写入、内容过滤一致性、二进制逐条写入和进度、非 UTF-8 编码文件大小统计。 |
| 2026-06-03 | `DataWindow` 无换行长行单 block 膨胀 | 数据分窗现在除 `maximumBlockCount` 外增加字符总量裁剪，追加数据后按 `m_maxTextChars` 删除最早文本并保留最新内容；清空时清理 `QTextDocument` undo 栈；分窗导出只输出裁剪后的当前可见内容。 | `tests/unit/TestDataWindow.cpp` 覆盖无换行长流裁剪、保留最新标记、删除最早标记，以及导出文件与裁剪后内容一致。 |
| 2026-06-03 | `DataTableWidget::clearAll()` 清空后容量不回落 | 数据表格清空现在同时释放已落屏记录和待刷新记录的 `QVector` 容量，并用全新的空 `QStandardItemModel` 替换旧模型，释放旧表格行和单元格对象的历史分配；清空后序号从 1 重新开始，后续接收、过滤、排序和导出仍复用同一视图链路。 | `tests/unit/TestMemoryAwareUiBehavior.cpp` 覆盖已落屏记录容量、pending 记录容量、模型行数清空，以及释放容量后继续接收新数据。 |
| 2026-06-03 | `ConfigManager::saveConfig(filePath)` 显式路径未真实写入 | `saveConfig(filePath)` 现在把写入逻辑集中到 `writeConfigToSettings(QSettings&)`，默认保存继续使用当前配置对象，显式路径保存则创建临时 `QSettings` 写入目标文件，不改变当前配置文件位置；测试专用 `resetForTest()` 保证单例状态可隔离验证。 | `tests/unit/TestConfigManager.cpp` 覆盖目标路径真实生成、内容包含应用设置，以及当前配置路径不被显式保存切换。 |
| 2026-06-03 | 遗留模块仍编译进构建目标 | 主程序和测试目标已移除 `AutoSaveManager.cpp`、旧 `LuaEngine.cpp` 和 `IAPUpgrader.cpp`；保留源码只作历史参考，禁止新增生产入口。IAP 菜单继续存在，但真实链路走 `FileTransferDialog::setIAPMode(true)` 和文件传输中心，不再依赖旧升级器。 | `tests/unit/TestReleaseMetadata.cpp` 覆盖根 CMake 与测试 CMake 均不包含三个遗留源文件。 |
| 2026-06-03 | `YModemG` 枚举存在但 UI 和核心行为不完整 | YMODEM-G 已在标准协议下拉框中可选；发送端使用 `G` 启动，头包后收到 `ACK+G` 进入 0ms 定时流式发送，逐包从文件读取，不等待数据包 ACK，EOT 后等待 `ACK+G` 再发空头结束；接收端用 `G` 握手，不对数据包逐包 ACK，EOT 后 `ACK+G`，校验或序号错误时发送多字节 CAN 并释放文件句柄。 | `tests/unit/TestFileTransfer.cpp` 覆盖 G 模式发送、接收、错误中止和 UI 下拉框入口。 |
| 2026-06-03 | 接收区智能暂停滚动仍被新数据拉到底部 | 主接收区追加文本时不再移动 `QPlainTextEdit` 自身可见光标，而是用独立 `QTextCursor` 写入文档尾部；智能暂停状态下保存并恢复滚动条值，裁剪历史后按新范围夹紧，并屏蔽恢复信号避免误判为用户滚动到底部。 | `tests/unit/TestTabbedReceiveWidget.cpp` 覆盖用户滚到顶部后继续接收新数据仍保留阅读位置。 |
| 2026-06-03 | 自定义 OTA Magic 仍按字符串截断 | `OtaTransferOptions::magic` 改为 `quint32`，默认值为 ASCII `OTA1` 对应的小端数值 `0x3141544F`；`parseMagicText()` 支持 `OTA1` 和 `0x474F5441UL`，拒绝空值、ASCII 超 4 字节和越界数值；UI Magic 输入框放宽长度并在启动前校验。 | `tests/unit/TestFileTransfer.cpp` 覆盖 `uint32_t` 包头、文本解析、UI 输入框不截断。 |
