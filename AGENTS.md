# ComAssistant 项目开发规范

## 项目基本信息

- **项目名称**：ComAssistant（串口调试助手）
- **开发语言**：C++17
- **UI 框架**：Qt 5.12.9（MinGW 7.3 64-bit）
- **构建系统**：CMake 3.14+
- **依赖管理**：vcpkg（spdlog、nlohmann-json、zlib）
- **代码仓库**：https://github.com/wsmcfr/uart_assistant

## 文档同步要求（每次代码/功能/UI 改动必须执行）

- 每次修改代码、功能行为、界面文案、操作流程、配置项、通信方式、协议能力、快捷操作或用户可见行为时，必须同步检查并按需更新内置帮助文档 `resources/help/quickstart.html`。
- 如果改动会影响用户理解、上手步骤、按钮名称、菜单入口、模式说明、协议说明、示例流程或风险提示，必须在同一次改动中更新帮助文档，不能只改代码。
- 如果确认本次改动不需要更新帮助文档，必须在最终说明中明确写出“不需要更新帮助文档”的理由，避免遗漏。
- 发布信息相关改动还必须同时遵守下方“更新说明文档”流程，保证 `CHANGELOG.md`、`README.md` 与 `resources/help/quickstart.html` 的版本信息一致。

## 发布流程（每次版本升级必须执行）

### 1. 版本号更新（3个文件）

版本号遵循 SemVer 语义化版本：`主版本.次版本.修订号`

需要修改的文件：

| 文件 | 修改内容 |
|------|---------|
| `CMakeLists.txt` | `VERSION x.y.z` |
| `src/version.h` | `APP_VERSION`、`APP_VERSION_MAJOR/MINOR/PATCH/BUILD`、`APP_VERSION_STRING`、`APP_BUILD_DATE` |
| `CHANGELOG.md` | 新增对应版本段落，作为 GitHub Release 正文来源 |

版本号递增规则：
- **主版本（Major）**：不兼容的 API 变更、重大架构调整
- **次版本（Minor）**：新增功能、新增协议支持、UI 重构
- **修订号（Patch）**：Bug 修复、性能优化、文档更新

### 2. 更新说明文档（2个文件 + 1 个必填版本说明）

| 文件 | 标记段 | 说明 |
|------|--------|------|
| `CHANGELOG.md` | `## [x.y.z] - YYYY-MM-DD` | 本次版本更新说明，CI 会自动提取到 GitHub Release 正文 |
| `README.md` | `<!-- LATEST_RELEASE:START -->` ~ `<!-- LATEST_RELEASE:END -->` | 最新版本号 + 下载链接 |
| `resources/help/quickstart.html` | `<!-- LATEST_RELEASE_HELP:START -->` ~ `<!-- LATEST_RELEASE_HELP:END -->` | 内嵌帮助中的版本信息 |

> 注：
> 1. `CHANGELOG.md` 是每次发布前的必填项，不能省略，否则 GitHub Release 页面只会显示“未同步到 CHANGELOG”的占位文案。
> 2. CI 工作流会在 Release 成功后自动更新 `README.md` 与 `resources/help/quickstart.html`，但手动发布时需自行更新。

### 3. Git 提交与打 Tag

```bash
# 1. 提交所有变更
git add -A
git commit -m "feat: vx.y.z - 简要描述主要变更"

# 2. 推送代码
git push origin main

# 3. 创建 Tag（触发 CI 自动构建和发布）
git tag vx.y.z
git push origin vx.y.z
```

### 4. GitHub Release Notes 规范

Release Notes 必须包含以下结构：

```markdown
## vx.y.z Release (YYYY-MM-DD)

### 新增功能
- **功能名称**：简要描述

### 优化改进
- 具体优化内容

### Bug 修复
- 修复的具体问题

### 升级说明（可选但推荐）
- 配置迁移、兼容性、已知限制或用户需要注意的事项

### 下载
- Windows x64 便携版：`ComAssistant_vx.y.z_Windows_x64_Portable.zip`
- 完整性校验：`SHA256SUMS.txt`

### 反馈
- 问题反馈与建议：https://github.com/wsmcfr/uart_assistant/issues
```

### 5. CI 自动化流程

推送 Tag 后，`.github/workflows/release.yml` 自动执行：

1. 安装 Qt 5.12.9（win64_msvc2017_64）
2. vcpkg 安装依赖（spdlog、nlohmann-json、zlib）
3. CMake 配置 + MSVC 编译
4. windeployqt 部署 Qt 运行时
5. 打包便携版 ZIP + SHA256 校验
6. 上传到 GitHub Release
7. 自动更新 README.md 和 quickstart.html 中的版本信息
8. 提交版本信息更新到 main 分支

### 6. 发布前检查补充要求

在执行 `git tag vx.y.z` 之前，必须额外确认以下内容：

1. `CHANGELOG.md` 已新增 `## [x.y.z] - YYYY-MM-DD` 段落
2. 版本说明至少包含“新增/优化/修复”中的实际变更项，不能只写占位文字
3. 如果本次版本已推送但 Release 正文不完整，需要补充 `CHANGELOG.md` 后手动更新 GitHub Release 描述

## 构建命令

```bash
# 本地 Release 构建
cmake -B build_release -S . -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_release --config Release

# CI Release 构建（MSVC）
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT" -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --target ComAssistant --parallel
```

## 代码规范

- 大括号独占一行并对齐
- 所有代码必须写详细中文注释
- 使用 Qt 5.12 兼容 API（避免 Qt 5.15+ 的新特性）
- `QVector::resize()` 不支持填充值参数（Qt 5.12 限制），需用构造函数替代
- 文件路径使用反斜杠（Windows 环境）
