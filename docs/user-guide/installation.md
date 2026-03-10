# 安装指南

## 系统要求

- **操作系统**: Windows 10 (1809) 或更高版本
- **处理器**: x64 架构，1 GHz 或更快
- **内存**: 512 MB RAM（推荐 1 GB）
- **硬盘空间**: 100 MB 可用空间
- **显示器**: 1280 x 720 或更高分辨率
- **运行时**: Visual C++ 2019 Redistributable

## 安装步骤

### 安装版

1. 下载 `ComAssistant_x.x.x_Setup.exe` 安装包
2. 双击运行安装程序
3. 按照安装向导完成安装
4. 安装完成后，可从开始菜单或桌面启动程序

### 便携版

1. 下载 `ComAssistant_x.x.x_Portable.zip` 压缩包
2. 解压到任意目录
3. 双击 `ComAssistant.exe` 运行

便携版不会在系统中写入注册表，可放在 U 盘中随身携带。

## 首次运行

首次运行时，程序会：
1. 在用户数据目录创建配置文件
2. 初始化日志系统
3. 加载默认设置

配置文件位置：`%APPDATA%\ComAssistant\`

## 升级安装

### 从旧版本升级

1. 打开 GitHub Releases 页面下载新版本便携包：`ComAssistant_*_Windows_x64_Portable.zip`
2. 退出正在运行的软件，解压到新目录
3. 若希望覆盖升级，可将新文件覆盖旧目录（建议先备份）
4. 用户配置默认保存在 `%APPDATA%\\ComAssistant\\`，不会因替换程序目录而丢失

### 应用内检查更新

- 手动检查：`帮助 -> 检查更新...`
- 自动检查：`帮助 -> 启动时自动检查更新`（默认每日检查一次）
- 检测到新版本后，可直接打开对应 Release 下载页

### 保留配置

升级前，可备份以下文件：
- `%APPDATA%\ComAssistant\config.ini` - 用户配置
- `%APPDATA%\ComAssistant\quicksend.json` - 快捷发送配置

## 卸载

### 通过控制面板

1. 打开 **设置 → 应用**
2. 找到 "串口调试助手"
3. 点击 "卸载"

### 通过开始菜单

1. 打开开始菜单
2. 找到 "串口调试助手" 文件夹
3. 点击 "卸载 串口调试助手"

### 清理残留数据

卸载后，如需完全清除数据，请手动删除：
- `%APPDATA%\ComAssistant\` 目录

## 常见安装问题

### 缺少 VCRUNTIME140.dll

**解决方案**: 安装 [Visual C++ 2019 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)

### 安装程序被杀毒软件拦截

**解决方案**: 临时关闭杀毒软件，或将安装程序添加到白名单

### 程序无法启动

**检查步骤**:
1. 确认系统满足最低要求
2. 以管理员身份运行
3. 检查日志文件 `%APPDATA%\ComAssistant\logs\`

## 静默安装

对于企业部署，支持静默安装：

```batch
ComAssistant_1.0.0_Setup.exe /S /D=C:\Program Files\ComAssistant
```

参数说明：
- `/S` - 静默安装
- `/D=<path>` - 指定安装目录
