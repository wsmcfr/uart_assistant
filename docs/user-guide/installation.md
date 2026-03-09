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

1. 运行新版本安装程序
2. 安装程序会自动检测并卸载旧版本
3. 用户配置会被保留

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
