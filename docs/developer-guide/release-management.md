# 发布与版本管理规范

## 目标

- 让普通用户在仓库页面可以直接下载可运行软件
- 让维护者可以标准化发布多个历史版本
- 让客户端能够自动检查最新版本

## 版本命名

- 版本号采用 `SemVer`：`MAJOR.MINOR.PATCH`
- Git 标签必须使用 `v` 前缀：`v1.2.3`
- `src/version.h` 中的 `APP_VERSION` 必须与标签版本一致

## Release 资产约定

- 主资产命名：`ComAssistant_<tag>_Windows_x64_Portable.zip`
- 同时发布 `SHA256SUMS.txt` 供校验完整性
- Release 描述必须说明：
  - 新增功能
  - 修复问题
  - 兼容性变更
  - 升级注意事项
- Release 正文来源于 `CHANGELOG.md` 对应版本段落（由 CI 自动提取）

## 自动构建发布

项目内已提供 GitHub Actions 工作流：

- 文件：`.github/workflows/release.yml`
- 触发条件：推送标签 `v*.*.*`
- 行为：
  - Windows Release 构建
  - `windeployqt` 依赖打包
  - 生成 ZIP 便携包与 SHA256 清单
  - 自动创建/更新 GitHub Release 并上传资产

## 客户端更新检查

应用通过 GitHub API 检查最新发布：

- API：`/repos/<owner>/<repo>/releases/latest`
- 默认策略：每日自动检查一次
- 用户入口：
  - `帮助 -> 检查更新...`
  - `帮助 -> 启动时自动检查更新`
- 忽略策略：支持“忽略此版本”提醒

## 标准发布流程

1. 完成功能与测试
2. 更新 `src/version.h`、`CHANGELOG.md`
3. 提交并合并到主分支
4. 创建标签：`git tag vX.Y.Z`
5. 推送标签：`git push origin vX.Y.Z`
6. 等待 GitHub Actions 完成发布
7. 在 Release 页面核对资产可下载性

## CHANGELOG 编写模板（每个版本必填）

建议按以下结构维护对应版本段落，便于用户快速判断是否需要升级：

```markdown
## [X.Y.Z] - YYYY-MM-DD

### 新增与恢复
- ...

### 性能优化
- ...

### 修复
- ...

### 升级说明（可选）
- ...
```

注意：
- 标题必须是 `## [X.Y.Z] - YYYY-MM-DD` 格式，`X.Y.Z` 需与标签 `vX.Y.Z` 一致。
- 若未找到对应段落，CI 会在 Release 中提示“未同步 CHANGELOG 说明”。
