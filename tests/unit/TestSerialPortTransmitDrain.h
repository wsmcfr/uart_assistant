/**
 * @file TestSerialPortTransmitDrain.h
 * @brief 串口发送排空等待语义回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTSERIALPORTTRANSMITDRAIN_H
#define TESTSERIALPORTTRANSMITDRAIN_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证串口文件发送排空等待不会早于线路真实发送时间。
 *
 * 文件传输进度条依赖该语义判断“最后一个字节是否已经离开 USB-串口芯片”。
 * 这里不打开真实串口，只测试 drain 状态机在 Qt 写缓冲清空后的保守等待策略。
 */
class TestSerialPortTransmitDrain : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief Qt 写缓冲清空后仍应等待完整线路发送时间。
     *
     * 主要流程：模拟 drain 请求已经等待过一段时间，再触发“写缓冲清空”。
     * 期望串口对象重新按当前块字节数启动完整线路定时器，而不是把之前等待
     * Qt/系统缓冲的时间扣掉后立刻发出 transmitDrained。
     */
    void testLineDelayIsFullAfterWriteBufferBecomesEmpty();
};

#endif // TESTSERIALPORTTRANSMITDRAIN_H
