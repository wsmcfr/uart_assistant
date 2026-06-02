# 4.5 Lua 安全沙箱设计

## 背景

4.1 到 4.4 已经把协议平台化的事实源打牢：协议身份来自 `ProtocolDescriptor`，配置来自 `ProtocolConfigSchema`，排障快照来自 `ProtocolDiagnosticsBuilder`。下一步如果直接让 Lua 脚本参与协议扩展或调试自动化，当前脚本能力存在两个明显边界问题。

| 现状 | 风险 |
|---|---|
| `LuaEngine` 初始化时调用 `luaL_openlibs()` 打开标准库 | `io`、`os`、`package`、`debug` 等能力过宽，不适合执行未知脚本 |
| `ScriptEditorDialog` 当前使用正则模拟执行 `ui.log`、`serial.send` 和 `serial.sendHex` | 用户界面表现像 Lua，但没有真正的 Lua 运行时语义，也无法复用协议平台化能力 |
| 脚本文档已提示“取消、沙箱隔离和权限控制属于后续升级方向” | 安全边界已经是明确待办，后续协议脚本化必须先解决 |

4.5 的目标是先建立可测试的 Lua 沙箱核心基座。第一版只解决脚本执行安全边界，不完整替换脚本编辑器，也不注册 Lua 协议。

## 目标与非目标

| 类型 | 内容 |
|---|---|
| 目标 | 新增独立 `LuaSandbox` 核心执行器，和历史 `LuaEngine` 解耦 |
| 目标 | 默认禁用文件、进程、网络、动态加载、调试库和外部模块加载能力 |
| 目标 | 通过选项控制超时、内存预算、最大输出行数和通信 API 是否启用 |
| 目标 | 通过 Lua hook 支持超时中断和外部取消标记 |
| 目标 | 通过自定义 Lua allocator 支持内存预算限制 |
| 目标 | 重写 `print(...)`，把输出收集到执行结果，不直接依赖 UI |
| 目标 | 提供稳定 `LuaSandboxResult`，让 UI、测试和后续协议脚本化都能判断失败原因 |
| 目标 | 更新脚本文档，说明沙箱能力与当前脚本编辑器的阶段性关系 |
| 非目标 | 不在 4.5 内完整替换 `ScriptEditorDialog::executeSimpleScript()` |
| 非目标 | 不删除 `LuaEngine`，不迁移所有历史脚本 API |
| 非目标 | 不注册 Lua 协议，不实现脚本协议配置 UI |
| 非目标 | 不实现 DLL 插件加载、插件 ABI、热加载或卸载 |
| 非目标 | 不承诺强隔离恶意 native 代码；本阶段沙箱只约束 Lua 层能力和资源预算 |

## 推荐方案

采用“新核心沙箱 + 旧入口保留 + 后续接入预留”的方案。

| 层 | 职责 |
|---|---|
| `LuaSandbox` | 每次执行创建独立 Lua state，装载安全白名单库、注册受限 API、执行脚本并返回结构化结果 |
| `LuaSandboxOptions` | 描述资源限制和能力开关，例如超时、内存、输出行数、是否允许通信 API |
| `LuaSandboxResult` | 描述执行成败、错误类型、输出行、耗时和资源限制命中情况 |
| `ScriptEditorDialog` | 4.5 暂不迁移，只在后续阶段改为调用 `LuaSandbox` |
| 协议平台 | 4.5 只定义后续接入方式：Lua 协议仍通过 descriptor/schema/diagnostics 暴露事实源 |

这种方案避免把 UI 改造、脚本协议化和安全底座混在一次改动里。核心沙箱有单元测试后，后续 4.6 可以安全地接入脚本编辑器，4.7 再考虑 Lua 协议注册。

## 核心数据结构

新增 `src/core/script/LuaSandbox.h/.cpp`。

| 结构 | 字段 | 说明 |
|---|---|---|
| `LuaSandboxOptions` | `timeoutMs` | 最大执行时间，默认 1000ms；小于等于 0 表示不启用时间限制 |
| `LuaSandboxOptions` | `memoryLimitKb` | Lua state 可用内存预算，默认 1024KB；小于等于 0 表示不启用内存限制 |
| `LuaSandboxOptions` | `maxOutputLines` | `print` 最多收集的输出行数，默认 200 |
| `LuaSandboxOptions` | `allowCommunicationApi` | 是否注册通信 API，默认 `false` |
| `LuaSandboxResult` | `success` | 脚本是否成功执行完毕 |
| `LuaSandboxResult` | `timedOut` | 是否因超时中断 |
| `LuaSandboxResult` | `interrupted` | 是否因外部取消中断 |
| `LuaSandboxResult` | `memoryExceeded` | 是否因内存预算失败 |
| `LuaSandboxResult` | `errorMessage` | 失败原因或 Lua 错误信息 |
| `LuaSandboxResult` | `outputLines` | `print(...)` 收集到的输出 |
| `LuaSandboxResult` | `elapsedMs` | 本次执行耗时 |

## 标准库白名单

第一版不调用 `luaL_openlibs()`。沙箱只打开或手动注册必要能力。

| Lua 能力 | 状态 | 原因 |
|---|---|---|
| `print` | 允许，重写 | 输出进入 `LuaSandboxResult::outputLines` |
| 基础表达式、函数、表、循环 | 允许 | Lua 基础语言能力必须保留 |
| `string` | 允许 | 字符串处理是协议脚本的核心需求 |
| `table` | 允许 | 表处理是 Lua 基础能力 |
| `math` | 允许 | 校验、换算、协议构帧常用 |
| `utf8` | 可允许 | Lua 5.4 标准库，纯计算能力 |
| `io` | 禁用 | 防止读写任意文件 |
| `os` | 禁用 | 防止执行命令、读环境、退出进程 |
| `package`、`require` | 禁用 | 防止加载外部模块绕过沙箱 |
| `debug` | 禁用 | 防止破坏 hook、环境和调用栈边界 |
| `dofile`、`loadfile` | 禁用 | 防止脚本自行读取并执行外部文件 |
| `load` | 暂禁用 | 简化第一版执行边界，避免二次动态编译绕过检查 |

## 超时与取消

`LuaSandbox` 在执行前设置 Lua hook。hook 每隔固定指令数运行一次，检查：

| 检查项 | 命中行为 |
|---|---|
| `QElapsedTimer` 超过 `timeoutMs` | 标记 `timedOut`，通过 `luaL_error` 终止脚本 |
| 外部取消标记为 true | 标记 `interrupted`，通过 `luaL_error` 终止脚本 |

第一版以同步执行为主，不创建后台线程。这样单元测试可控，后续脚本编辑器接入时再决定是否把执行放入 worker。

## 内存限制

Lua state 通过 `lua_newstate(customAllocator, userdata)` 创建。allocator 持有当前占用字节数和最大字节数：

| 场景 | 行为 |
|---|---|
| 分配后未超过预算 | 更新当前占用并返回新内存 |
| 分配后超过预算 | 标记 `memoryExceeded` 并返回 `nullptr` |
| 释放内存 | 递减当前占用 |

Lua 报出内存错误后，`LuaSandboxResult` 应同时包含 `memoryExceeded=true` 和清晰错误消息。

## API 暴露策略

4.5 只内置纯函数和输出能力，通信 API 默认关闭。

| API | 4.5 状态 | 说明 |
|---|---|---|
| `print(...)` | 启用 | 写入结果输出 |
| `hexToBytes(hex)` | 可启用 | 纯转换，不触碰外部资源 |
| `bytesToHex(data)` | 可启用 | 纯转换，不触碰外部资源 |
| `crc16(data)` | 可启用 | 纯计算 |
| `crc32(data)` | 可启用 | 纯计算 |
| `serial.send`、`serial.receive` | 默认不启用 | 后续接入脚本编辑器或协议脚本时通过受控回调启用 |

## 错误处理

| 错误类型 | 结果字段 |
|---|---|
| Lua 语法错误 | `success=false`，`errorMessage` 为 Lua 错误 |
| 运行时错误 | `success=false`，`errorMessage` 为 Lua 错误 |
| 超时 | `success=false`，`timedOut=true` |
| 外部取消 | `success=false`，`interrupted=true` |
| 内存超限 | `success=false`，`memoryExceeded=true` |
| 输出超过行数 | 截断输出并追加提示，不让脚本无限刷 UI |

每次执行使用新的 Lua state，避免一次脚本污染下一次脚本的全局变量、hook 或库表。

## 与协议平台的后续接入

4.5 不注册 Lua 协议，但后续接入规则必须提前约束。

| 接入点 | 后续规则 |
|---|---|
| `ProtocolDescriptor` | Lua 协议必须提供稳定 ID、显示名、分类和能力标志 |
| `ProtocolConfigSchema` | Lua 协议配置必须先声明 Schema，再允许 UI 编辑和会话持久化 |
| `ProtocolDiagnosticsBuilder` | Lua 协议诊断必须导出脚本来源、沙箱选项、配置校验结果和最近错误 |
| `ProtocolRegistry` | Lua 协议注册不能覆盖内置协议 ID，必须遵循 4.1 的增量幂等注册规则 |

## 文档影响

| 文件 | 动作 |
|---|---|
| `docs/user-guide/scripting.md` | 更新安全说明，说明 4.5 沙箱默认禁用文件/系统/模块加载，并说明脚本编辑器完整迁移仍属于后续阶段 |
| `resources/help/protocols.html` | 如提到后续 Lua 协议接入，补一句“Lua 协议会复用沙箱、descriptor/schema/diagnostics” |
| `resources/help/quickstart.html` | 预计不更新，因为 4.5 不改变快速连接、发送、接收、会话保存或脚本编辑器主流程；实现完成后复核 |

## 测试计划

| 测试 | 覆盖点 |
|---|---|
| `TestLuaSandbox::allowsSafeMathStringAndTableUsage` | 安全白名单库可用 |
| `TestLuaSandbox::blocksUnsafeLibrariesAndLoaders` | `os`、`io`、`package`、`debug`、`require`、`dofile`、`loadfile` 不可用 |
| `TestLuaSandbox::capturesPrintOutput` | `print` 输出进入结果 |
| `TestLuaSandbox::reportsRuntimeErrors` | 运行时错误返回结构化失败 |
| `TestLuaSandbox::timesOutInfiniteLoop` | 死循环会被 hook 超时终止 |
| `TestLuaSandbox::limitsMemoryUsage` | 大表或大字符串触发内存预算 |
| `TestLuaSandbox::isolatesExecutions` | 多次执行之间全局变量不污染 |
| `TestLuaSandbox::truncatesExcessiveOutput` | 输出行数超过上限后截断 |

## 风险与缓解

| 风险 | 缓解 |
|---|---|
| Lua C API 长跳转错误处理复杂 | 执行路径统一走 `luaL_loadstring` + `lua_pcall`，hook 中只通过 `luaL_error` 返回 |
| 内存 allocator 统计不准导致误判 | 只承诺 Lua 层预算，不把它描述为进程级内存隔离 |
| 同步执行仍可能短时间阻塞 UI | 4.5 只做核心，UI 接入阶段再 worker 化 |
| 历史 `LuaEngine` 与新 `LuaSandbox` 并存产生理解成本 | 文档和代码注释明确 `LuaSandbox` 是新安全入口，`LuaEngine` 后续迁移 |

## 验证命令

| 命令 | 目的 |
|---|---|
| `cmake --build build_release --config Release --target ComAssistant_tests --parallel` | 构建测试目标 |
| `build_release\\tests\\ComAssistant_tests.exe -o -,txt` | 运行全部 Qt 单元测试 |
| `ctest --test-dir build_release --output-on-failure` | CTest 汇总验证 |
| `cmake --build build_release --config Release --target ComAssistant --parallel` | 构建正式可运行程序 |
| `git diff --check` | 检查空白错误 |
| `python C:\\Users\\caofengrui\\.agents\\skills\\recallloom\\scripts\\validate_context.py .` | 校验 RecallLoom 记忆文件 |
