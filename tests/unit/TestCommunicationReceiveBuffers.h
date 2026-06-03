/**
 * @file TestCommunicationReceiveBuffers.h
 * @brief 通信接收兼容缓存内存边界回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTCOMMUNICATIONRECEIVEBUFFERS_H
#define TESTCOMMUNICATIONRECEIVEBUFFERS_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证各通信后端的 readAll() 兼容缓存不会在信号消费路径下无限增长。
 *
 * 主窗口主要通过 dataReceived 信号消费接收数据，TCP Server、UDP、HID 仍需保留
 * readAll() 旧接口兼容缓存。本测试确保信号收到完整数据，而兼容缓存只保留
 * bufferSize() 指定的尾部数据，避免长期运行时出现隐藏内存增长。
 */
class TestCommunicationReceiveBuffers : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证 TCP Server 的信号接收完整，同时 readAll() 兼容缓存受 bufferSize 限制。
     */
    void testTcpServerReceiveBufferIsBoundedWhileSignalGetsFullPayload();

    /**
     * @brief 验证 UDP 的信号接收完整，同时 readAll() 兼容缓存受 bufferSize 限制。
     */
    void testUdpReceiveBufferIsBoundedWhileSignalGetsFullDatagram();

    /**
     * @brief 验证 HID 的输入报告接收完整，同时 readAll() 兼容缓存受 bufferSize 限制。
     */
    void testHidReceiveBufferIsBoundedWhileSignalGetsFullReports();

    /**
     * @brief 验证运行中调小 bufferSize 会立即裁剪已有 readAll() 兼容缓存。
     */
    void testSetBufferSizeTrimsExistingReceiveBuffers();

    /**
     * @brief 验证 clearBuffer()/readAll()/close() 会释放兼容缓存历史容量。
     */
    void testClearingAndClosingReleaseReceiveBufferCapacity();

    /**
     * @brief 验证 TCP Server 关闭后立即断开并清空客户端对象引用。
     */
    void testTcpServerCloseClearsClientListSynchronously();
};

#endif // TESTCOMMUNICATIONRECEIVEBUFFERS_H
