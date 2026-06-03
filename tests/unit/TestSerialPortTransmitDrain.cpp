/**
 * @file TestSerialPortTransmitDrain.cpp
 * @brief 串口发送排空等待语义回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestSerialPortTransmitDrain.h"

#include <QSignalSpy>
#include <QTimer>

/*
 * 本测试只验证 SerialPort 内部 drain 状态机的时间语义，不访问真实串口。
 * 通过在当前测试编译单元暴露 private，可以直接模拟“Qt 写缓冲刚清空”
 * 这一内部边界，避免测试依赖具体机器上的 COM 口。
 */
#define private public
#include "core/communication/SerialPort.h"
#undef private

using namespace ComAssistant;

void TestSerialPortTransmitDrain::testLineDelayIsFullAfterWriteBufferBecomesEmpty()
{
    /*
     * 选择 9600 8N1 和 960 字节，是因为完整线路时间正好约 1000ms。
     * 这样即使测试调度有少量误差，也能稳定区分“完整等待”和“扣减后提前完成”。
     */
    SerialConfig config;
    config.baudRate = 9600;
    config.dataBits = DataBits::Eight;
    config.parity = Parity::None;
    config.stopBits = StopBits::One;

    SerialPort port(config);
    QSignalSpy drainedSpy(&port, &SerialPort::transmitDrained);
    QVERIFY(drainedSpy.isValid());

    port.m_waitingTransmitDrain = true;
    port.m_pendingTransmitDrainBytes = 960;

    /*
     * 模拟 Qt/系统写缓冲在 drain 请求 20ms 之后才清空。正确行为是从这一刻
     * 起再等待完整线路时间；如果把这 20ms 扣掉，定时器间隔会小于估算值。
     */
    QTest::qWait(20);
    const int estimatedMs = port.estimateTransmitTimeMs(port.m_pendingTransmitDrainBytes);
    port.startTransmitLineDelay();

    QVERIFY2(port.m_transmitLineTimer->isActive(),
             "写缓冲清空后必须启动线路等待定时器，不能直接判定发送完成。");
    QVERIFY2(drainedSpy.isEmpty(),
             "线路等待定时器启动前不应发出发送完成信号。");
    QCOMPARE(port.m_transmitLineTimer->interval(), estimatedMs);
}
