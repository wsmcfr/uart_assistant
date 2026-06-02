/**
 * @file HidWorker.h
 * @brief HID 设备后台工作器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_HIDWORKER_H
#define COMASSISTANT_HIDWORKER_H

#include "config/AppConfig.h"

#include <QByteArray>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <memory>

namespace ComAssistant {

/**
 * @brief HID worker 后端接口。
 *
 * 该接口把 hidapi 或测试替身隔离在 worker 之后。生产环境使用 hidapi
 * 后端，单元测试可以注入 fake 后端验证互斥、关闭和失败路径。
 */
class HidWorkerBackend
{
public:
    virtual ~HidWorkerBackend() = default;

    /**
     * @brief 打开 HID 设备。
     * @param config HID 设备配置。
     * @param errorMessage 失败时写入可读错误。
     * @return 打开成功返回 true。
     */
    virtual bool open(const HidConfig& config, QString& errorMessage) = 0;

    /**
     * @brief 关闭 HID 设备并释放句柄。
     */
    virtual void close() = 0;

    /**
     * @brief 判断后端是否已持有打开的设备句柄。
     * @return 已打开返回 true。
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief 写入完整 Output Report。
     * @param report 已包含 Report ID 和填充字节的完整报告。
     * @param errorMessage 失败时写入可读错误。
     * @return hidapi 接受的字节数，失败返回负数。
     */
    virtual int writeOutputReport(const QByteArray& report, QString& errorMessage) = 0;

    /**
     * @brief 发送 Feature Report。
     * @param report 已包含 Report ID 的完整 Feature Report。
     * @param errorMessage 失败时写入可读错误。
     * @return hidapi 接受的字节数，失败返回负数。
     */
    virtual int sendFeatureReport(const QByteArray& report, QString& errorMessage) = 0;

    /**
     * @brief 读取 Feature Report。
     * @param requestReport 首字节为 Report ID、长度为读取上限的请求缓冲。
     * @param errorMessage 失败时写入可读错误。
     * @return 读取到的完整 Feature Report，失败返回空数组。
     */
    virtual QByteArray getFeatureReport(const QByteArray& requestReport, QString& errorMessage) = 0;

    /**
     * @brief 非阻塞读取当前可用的 Input Report。
     * @param maxReportLength 单个报告最大读取长度。
     * @param errorMessage 失败时写入可读错误；没有数据时保持为空。
     * @return 当前轮询周期读到的报告列表。
     */
    virtual QList<QByteArray> readInputReports(int maxReportLength, QString& errorMessage) = 0;
};

/**
 * @brief HID 后台工作器。
 *
 * 主要流程：HidDevice 把该对象移动到独立 QThread；open() 后由 worker
 * 自己的 QTimer 轮询 Input Report；Output/Feature/close 操作都通过
 * 同一把互斥锁进入后端，保证 hidapi 句柄不会被并发读写。
 */
class HidWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造 HID worker。
     * @param backend 可注入后端；为空时创建生产 hidapi 后端。
     * @param parent Qt 父对象。
     */
    explicit HidWorker(std::unique_ptr<HidWorkerBackend> backend = nullptr,
                       QObject* parent = nullptr);

    /**
     * @brief 析构时停止轮询并关闭设备。
     */
    ~HidWorker() override;

    /**
     * @brief 打开 HID 设备并启动输入报告轮询。
     * @param config HID 设备配置。
     * @param errorMessage 失败时输出错误。
     * @return 打开成功返回 true。
     */
    bool open(const HidConfig& config, QString& errorMessage);

    /**
     * @brief 停止轮询并关闭 HID 设备。
     */
    void close();

    /**
     * @brief 判断 HID 设备是否打开。
     * @return 已打开返回 true。
     */
    bool isOpen() const;

    /**
     * @brief 写入完整 Output Report。
     * @param report 已编码好的 Output Report。
     * @param errorMessage 失败时输出错误。
     * @return 成功写入的报告字节数，失败返回 -1。
     */
    int writeOutputReport(const QByteArray& report, QString& errorMessage);

    /**
     * @brief 发送 Feature Report。
     * @param report 已编码好的 Feature Report。
     * @param errorMessage 失败时输出错误。
     * @return 发送成功返回 true。
     */
    bool sendFeatureReport(const QByteArray& report, QString& errorMessage);

    /**
     * @brief 读取 Feature Report。
     * @param requestReport 已包含 Report ID 和目标长度的请求缓冲。
     * @param errorMessage 失败时输出错误。
     * @return 读取到的完整 Feature Report。
     */
    QByteArray getFeatureReport(const QByteArray& requestReport, QString& errorMessage);

signals:
    /**
     * @brief worker 线程读取到一帧原始 Input Report。
     */
    void inputReportReady(const QByteArray& report);

    /**
     * @brief worker 线程发生 HID 错误。
     */
    void errorOccurred(const QString& errorMessage);

private slots:
    /**
     * @brief 非阻塞轮询 Input Report。
     */
    void pollInputReports();

private:
    /**
     * @brief 创建默认生产后端。
     * @return hidapi 后端；未启用 hidapi 时返回会明确失败的空后端。
     */
    static std::unique_ptr<HidWorkerBackend> createDefaultBackend();

private:
    mutable QMutex m_mutex;                   ///< 串行化 hidapi 句柄访问
    std::unique_ptr<HidWorkerBackend> m_backend; ///< 实际 HID 后端
    QTimer* m_pollTimer = nullptr;            ///< worker 线程内的输入报告轮询定时器
    HidConfig m_config;                       ///< 当前打开设备使用的配置
};

} // namespace ComAssistant

#endif // COMASSISTANT_HIDWORKER_H
