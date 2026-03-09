# 开发者指南

本文档面向希望参与 ComAssistant 开发的开发者。

## 架构概述

ComAssistant 采用模块化架构设计：

```
src/
├── core/                   # 核心模块
│   ├── communication/      # 通信模块（串口、TCP、UDP）
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
- CMake 3.20+
- Qt 6.5+
- Visual Studio 2019+ 或 MSVC 工具集
- vcpkg（包管理器）

### 可选工具
- NSIS（安装包制作）
- 7-Zip（便携版打包）

## 构建步骤

### 1. 克隆仓库

```bash
git clone https://github.com/comassistant/comassistant.git
cd comassistant
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

## 模块说明

### 通信模块 (communication)

基于接口设计：
```cpp
class ICommunication {
public:
    virtual bool open(const QVariantMap& params) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray& data) = 0;
signals:
    void dataReceived(const QByteArray& data);
    void errorOccurred(const QString& error);
};
```

### 协议模块 (protocol)

支持多种协议解析：
- Raw：原始数据
- Ascii：ASCII 文本
- Hex：十六进制
- Modbus：Modbus RTU
- Text/Stamp/Csv：绘图协议

### 脚本模块 (script)

Lua 脚本引擎封装：
```cpp
class LuaEngine {
public:
    bool executeScript(const QString& script);
    void registerFunction(const QString& name, lua_CFunction func);
};
```

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

1. 更新版本号 (`src/version.h`)
2. 更新 CHANGELOG.md
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
