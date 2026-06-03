/**
 * @file TestFrameModeWidget.h
 * @brief 帧模式校验与内存回收回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTFRAMEMODEWIDGET_H
#define TESTFRAMEMODEWIDGET_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证帧模式已经真正执行校验，并在清空时释放历史缓存容量。
 *
 * 帧模式 UI 已经向用户暴露 XOR、SUM、CRC16 校验选项；本测试类覆盖
 * 接收校验、发送构帧和清空释放三个用户可见/内存相关行为。
 */
class TestFrameModeWidget : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief XOR/SUM/CRC16 错误校验帧必须标记为无效帧。
     */
    void testChecksumFailuresAreMarkedInvalid();

    /**
     * @brief XOR/SUM/CRC16 正确校验帧必须标记为有效帧。
     */
    void testChecksumSuccessesAreMarkedValid();

    /**
     * @brief 带帧头尾发送时应按当前校验配置追加校验字节。
     */
    void testSendWithHeaderAppendsConfiguredChecksum();

    /**
     * @brief 空帧头或空帧尾配置不能覆盖当前有效配置，避免接收解析死循环。
     */
    void testEmptyFrameMarkersAreRejected();

    /**
     * @brief 清空后应释放帧列表、待刷新队列和接收缓冲的历史容量。
     */
    void testClearReleasesFrameBuffers();
};

#endif // TESTFRAMEMODEWIDGET_H
