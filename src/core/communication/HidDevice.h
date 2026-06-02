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
#include "HidWorker.h"

#include <QList>
#include <QThread>
#include <functional>

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
 * 主要流程：open() 根据 path 或 VID/PID 打开设备；HidWorker 在线程中
 * 轮询输入报告并发回主对象；write() 根据配置补齐 Report ID 和报告长度
 * 后投递给 worker 写入设备。
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
     * @brief 发送 Feature Report。
     *
     * @param report 已包含 Report ID 的完整 Feature Report。
     * @return true 表示 hidapi 已接受该报告。
     */
    bool sendFeatureReport(const QByteArray& report);

    /**
     * @brief 读取 Feature Report。
     *
     * @param requestReport 首字节为 Report ID 的接收缓冲区，长度决定读取上限。
     * @return 读取到的完整 Feature Report；失败时返回空数组并写入 lastError。
     */
    QByteArray getFeatureReport(const QByteArray& requestReport);

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

    /**
     * @brief 测试辅助：获取 HID worker 所在线程。
     * @return worker 线程指针；生产代码不应依赖该对象生命周期。
     */
    QThread* workerThreadForTest() const { return m_workerThread; }

private slots:
    /**
     * @brief 处理 worker 线程读到的原始输入报告。
     * @param report HID 原始输入报告。
     */
    void handleInputReport(const QByteArray& report);

    /**
     * @brief 处理 worker 线程报告的错误。
     * @param errorMessage 错误文本。
     */
    void handleWorkerError(const QString& errorMessage);

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

    /**
     * @brief 创建并启动 HID worker 线程。
     *
     * 构造函数调用该函数，让 open/write/Feature Report 可以统一通过
     * worker 串行执行。即使设备未打开，线程也保持就绪，便于测试生命周期。
     */
    void startWorkerThread();

    /**
     * @brief 停止 HID worker 线程并释放 worker 对象。
     *
     * 关闭时先通过阻塞调用让 worker 在自己的线程内释放设备句柄，再退出
     * 线程，避免应用退出时仍有 hidapi 读写悬挂。
     */
    void stopWorkerThread();

    /**
     * @brief 在 worker 线程同步调用无返回值函数。
     * @param operation 要在 worker 线程执行的操作。
     */
    void invokeWorkerBlocking(const std::function<void()>& operation) const;

    HidConfig m_config;              ///< 当前 HID 配置
    QThread* m_workerThread = nullptr; ///< HID 后台线程
    HidWorker* m_worker = nullptr;    ///< HID 后台工作器
    QByteArray m_readBuffer;         ///< 未被 readAll() 消费的接收缓存
    bool m_open = false;             ///< UI 线程可读的连接状态缓存
};

} // namespace ComAssistant

#endif // COMASSISTANT_HIDDEVICE_H
