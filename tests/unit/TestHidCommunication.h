/**
 * @file TestHidCommunication.h
 * @brief HID 通信工厂与会话配置回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef TESTHIDCOMMUNICATION_H
#define TESTHIDCOMMUNICATION_H

#include <QObject>
#include <QTest>

class TestHidCommunication : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证通信工厂把 HID 纳入受支持类型，并能创建 HID 通信实例。
     */
    void testFactoryCreatesHidCommunication();

    /**
     * @brief 验证缺少设备标识时 HID 打开失败，并返回可读的错误信息。
     */
    void testOpenWithoutDeviceIdentityFailsClearly();

    /**
     * @brief 验证会话文件能保存并恢复 HID 配置，避免重新打开会话后丢失设备选择。
     */
    void testSessionPersistsHidConfig();

    /**
     * @brief 验证输出报告会按 Report ID、长度字节和固定报告长度构造。
     */
    void testOutputReportCodecBuildsPaddedReport();

    /**
     * @brief 验证输入报告归一化会按配置移除输入 Report ID。
     */
    void testInputReportCodecRemovesReportId();

    /**
     * @brief 验证 Feature Report 使用独立的 Report ID 和报告长度。
     */
    void testFeatureReportCodecUsesFeatureSettings();

    /**
     * @brief 验证 HID 通信对象拥有独立 worker 线程并能在析构时安全停止。
     */
    void testHidDeviceOwnsAndStopsWorkerThread();

    /**
     * @brief 验证 Feature Report 请求会在 worker 线程中串行执行。
     */
    void testHidWorkerSerializesConcurrentFeatureReports();
};

#endif // TESTHIDCOMMUNICATION_H
