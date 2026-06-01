/**
 * @file HidDevice.h
 * @brief USB HID 通信实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_HIDDEVICE_H
#define COMASSISTANT_HIDDEVICE_H

#include "ICommunication.h"
#include "config/AppConfig.h"

#include <QList>
#include <QTimer>

#ifdef COMASSISTANT_ENABLE_HIDAPI
#include <hidapi/hidapi.h>
#endif

namespace ComAssistant {

/**
 * @brief HID 设备信息。
 *
 * 该结构只保存 UI 和工厂需要的轻量信息，避免上层直接依赖 hidapi 的
 * C 结构体生命周期。
 */
struct HidDeviceInfo {
    QString name;              ///< 设备显示名称
    QString path;              ///< hidapi 设备路径
    quint16 vendorId = 0;      ///< 厂商 ID
    quint16 productId = 0;     ///< 产品 ID
    quint8 interfaceNumber = 0;///< 接口号
    quint16 usagePage = 0;     ///< 使用页
    quint16 usage = 0;         ///< 使用
};

/**
 * @brief USB HID 通信类。
 *
 * 主要流程：open() 根据 path 或 VID/PID 打开设备；QTimer 定时用
 * hid_read_timeout() 拉取输入报告并发出 dataReceived；write() 根据配置
 * 补齐 Report ID 和报告长度后写入设备。
 */
class HidDevice : public ICommunication {
    Q_OBJECT

public:
    /**
     * @brief 构造 HID 通信实例。
     * @param config HID 设备配置。
     * @param parent Qt 父对象。
     */
    explicit HidDevice(const HidConfig& config, QObject* parent = nullptr);

    /**
     * @brief 析构时确保设备句柄被释放。
     */
    ~HidDevice() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;

    qint64 write(const QByteArray& data) override;
    QByteArray readAll() override;
    qint64 bytesAvailable() const override;

    void setBufferSize(int size) override;
    int bufferSize() const override;
    void clearBuffer() override;

    void setReadTimeout(int ms) override;
    int readTimeout() const override;
    void setWriteTimeout(int ms) override;
    int writeTimeout() const override;

    CommType type() const override { return CommType::Hid; }
    QString typeName() const override { return QStringLiteral("HID"); }
    QString statusString() const override;
    QString lastError() const override { return m_lastError; }

    /**
     * @brief 更新 HID 配置。
     * @param config 新配置；设备已打开时不会热切换，需关闭后重新打开。
     */
    void setConfig(const HidConfig& config);

    /**
     * @brief 获取当前 HID 配置。
     * @return 当前配置副本。
     */
    HidConfig config() const;

    /**
     * @brief 枚举当前系统可见 HID 设备。
     * @return 设备信息列表；未启用 hidapi 时返回空列表。
     */
    static QList<HidDeviceInfo> availableDevices();

    /**
     * @brief 判断当前构建是否启用了 hidapi 后端。
     * @return true 表示可访问真实 HID 设备。
     */
    static bool backendAvailable();

private slots:
    /**
     * @brief 轮询输入报告并派发接收信号。
     */
    void pollInputReport();

private:
    /**
     * @brief 根据配置构造写入报告。
     * @param data 用户要发送的原始数据。
     * @return 可直接传给 hid_write 的报告数据。
     */
    QByteArray buildOutputReport(const QByteArray& data) const;

    /**
     * @brief 将输入报告转换为上层接收数据。
     * @param report hidapi 读出的完整输入报告。
     * @return 发送给 UI/协议层的数据。
     */
    QByteArray normalizeInputReport(const QByteArray& report) const;

    /**
     * @brief 生成设备显示名称。
     * @return 用于状态栏和下拉框的短文本。
     */
    QString displayName() const;

#ifdef COMASSISTANT_ENABLE_HIDAPI
    /**
     * @brief 从 hidapi 返回的宽字符串构造 QString。
     * @param text hidapi 宽字符串指针，可为空。
     * @return 转换后的 QString。
     */
    static QString fromWideString(const wchar_t* text);

    hid_device* m_device = nullptr;  ///< hidapi 设备句柄
#endif

    HidConfig m_config;              ///< 当前 HID 配置
    QTimer* m_pollTimer = nullptr;   ///< 输入报告轮询定时器
    QByteArray m_readBuffer;         ///< 未被 readAll() 消费的接收缓存
};

} // namespace ComAssistant

#endif // COMASSISTANT_HIDDEVICE_H
