/**
 * @file AppConfig.h
 * @brief 应用配置数据结构定义
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_APPCONFIG_H
#define COMASSISTANT_APPCONFIG_H

#include <QString>
#include <QFont>
#include <QColor>
#include <QKeySequence>
#include <QSerialPort>

namespace ComAssistant {

//=============================================================================
// 通信相关枚举
//=============================================================================

/**
 * @brief 停止位枚举
 */
enum class StopBits {
    One = 0,           ///< 1位停止位
    OnePointFive = 1,  ///< 1.5位停止位
    Two = 2            ///< 2位停止位
};

/**
 * @brief 校验位枚举
 */
enum class Parity {
    None = 0,    ///< 无校验
    Odd = 1,     ///< 奇校验
    Even = 2,    ///< 偶校验
    Mark = 3,    ///< 标记校验
    Space = 4    ///< 空格校验
};

/**
 * @brief 流控枚举
 */
enum class FlowControl {
    None = 0,        ///< 无流控
    Hardware = 1,    ///< 硬件流控 (RTS/CTS)
    Software = 2     ///< 软件流控 (XON/XOFF)
};

/**
 * @brief 数据位枚举
 */
enum class DataBits {
    Five = 5,
    Six = 6,
    Seven = 7,
    Eight = 8
};

//=============================================================================
// 串口配置
//=============================================================================

/**
 * @brief 串口配置结构
 */
struct SerialConfig {
    QString portName;                           ///< COM端口名称
    int baudRate = 115200;                      ///< 波特率
    DataBits dataBits = DataBits::Eight;        ///< 数据位
    StopBits stopBits = StopBits::One;          ///< 停止位
    Parity parity = Parity::None;               ///< 校验位
    FlowControl flowControl = FlowControl::None; ///< 流控
    int readBufferSize = 65536;                 ///< 读缓冲区大小
    int readTimeout = 100;                      ///< 读超时(ms)
    int writeTimeout = 100;                     ///< 写超时(ms)
};

//=============================================================================
// 网络配置
//=============================================================================

/**
 * @brief 网络模式枚举
 */
enum class NetworkMode {
    TcpServer,
    TcpClient,
    Udp
};

/**
 * @brief 网络配置结构
 */
struct NetworkConfig {
    NetworkMode mode = NetworkMode::TcpClient;  ///< 连接模式
    QString serverIp = "127.0.0.1";             ///< 服务器IP
    int serverPort = 8080;                      ///< 服务器端口
    int listenPort = 8080;                      ///< 监听端口
    QString remoteIp;                           ///< 远程IP(UDP)
    int remotePort = 0;                         ///< 远程端口(UDP)
    int connectTimeout = 5000;                  ///< 连接超时(ms)
    int maxConnections = 10;                    ///< 最大连接数
};

//=============================================================================
// HID 配置
//=============================================================================

/**
 * @brief HID设备配置结构
 */
struct HidConfig {
    QString name;                   ///< 设备名称
    quint16 vendorId = 0;           ///< 厂商ID
    quint16 productId = 0;          ///< 产品ID
    quint8 interfaceNumber = 0;     ///< 接口号
    quint8 usagePage = 0;           ///< 使用页
    quint8 usage = 0;               ///< 使用
    bool firstDataIsLength = false; ///< 首字节为长度
    quint8 outReportId = 0;         ///< 输出报告ID
    bool removeInReportId = false;  ///< 移除输入报告ID
};

//=============================================================================
// 全局配置
//=============================================================================

/**
 * @brief 全局配置结构
 */
struct GlobalConfig {
    QString language = "zh_CN";                 ///< 语言
    int themeIndex = 0;                         ///< 主题索引 (0:明亮, 1:暗黑)
    QString windowTitleSuffix;                  ///< 窗口标题后缀
    QKeySequence popUpHotKey;                   ///< 弹出热键
    QFont guiFont;                              ///< GUI字体
    QColor backgroundColor;                     ///< 背景色
    bool highDpiScaling = true;                 ///< 高分屏适配
    bool firstRun = true;                       ///< 首次运行
    QString lastActivatedTab = "serial";        ///< 上次激活的标签页
};

//=============================================================================
// 配置文件头
//=============================================================================

/**
 * @brief 配置文件头结构
 */
struct ConfigFileHeader {
    char magic[4] = {'C', 'A', 'C', 'F'};  ///< 文件标识
    quint16 majorVersion = 1;              ///< 主版本号
    quint16 minorVersion = 0;              ///< 次版本号
};

} // namespace ComAssistant

#endif // COMASSISTANT_APPCONFIG_H
