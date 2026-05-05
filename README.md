# ComAssistant 串口调试助手

`ComAssistant` 是一个面向调试与开发场景的串口/网络通信工具，支持数据收发、协议解析、绘图分析、脚本扩展等功能。

## 技术栈

- **Qt 5.12.9** (MinGW 7.3.0 64-bit)
- **C++17**
- **CMake** 构建系统
- **QCustomPlot** 绘图引擎（支持 OpenGL 加速）
- **Lua 5.4** 脚本引擎
- **spdlog** 日志库

## 快速下载

- 最新稳定版下载页：<https://github.com/wsmcfr/uart_assistant/releases/latest>
- 所有历史版本：<https://github.com/wsmcfr/uart_assistant/releases>

<!-- LATEST_RELEASE:START -->
- 当前最新稳定版：v1.3.0（发布于 2026-05-05）
- 直链下载：<https://github.com/wsmcfr/uart_assistant/releases/download/v1.3.0/ComAssistant_v1.3.0_Win64.zip>
<!-- LATEST_RELEASE:END -->

下载时优先选择发布资产中的 `ComAssistant_*_Windows_x64_Portable.zip`，解压后可直接运行。
每个版本的更新内容、修复项与升级说明可在对应 Release 正文中查看。

## 版本策略

- 采用 `SemVer` 语义化版本：`主版本.次版本.修订号`（例如 `v1.2.0`）
- Git 标签格式固定为：`vX.Y.Z`
- 每个版本都在 GitHub Releases 中发布可执行资产，便于用户按需下载不同版本

## 自动更新

- 应用启动后默认每日自动检查一次 GitHub 最新发布版本
- 可在菜单 `帮助 -> 启动时自动检查更新` 开启/关闭
- 手动检查入口：`帮助 -> 检查更新...`

## 发布规范（维护者）

1. 同步更新版本号：
   - `src/version.h`
   - `CMakeLists.txt`（`project(... VERSION X.Y.Z)`）
2. 更新 `CHANGELOG.md`
3. 提交代码并创建标签：`git tag vX.Y.Z`
4. 推送标签：`git push origin vX.Y.Z`
5. GitHub Actions 自动构建并创建 Release 资产
6. CI 自动从 `CHANGELOG.md` 提取本版本说明写入 Release 正文

详细规则见：

- [发布与版本管理规范](docs/developer-guide/release-management.md)
