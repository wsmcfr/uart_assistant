/**
 * @file TestMacroRecorderMemoryPolicy.h
 * @brief 宏录制内存策略回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTMACRORECORDERMEMORYPOLICY_H
#define TESTMACRORECORDERMEMORYPOLICY_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证宏录制不会因长时间接收数据而无限增长。
 */
class TestMacroRecorderMemoryPolicy : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 录制事件超过上限后，应删除最早事件并保留最新事件。
     */
    void testRecorderDropsOldestEventsAfterLimit();

    /**
     * @brief 录制数据超过字节上限后，应按最早事件开始裁剪。
     */
    void testRecorderDropsOldestPayloadAfterByteLimit();

    /**
     * @brief 单条接收数据超过字节上限时，不应保存超过上限的事件。
     */
    void testRecorderSkipsSingleOversizedPayload();
};

#endif // TESTMACRORECORDERMEMORYPOLICY_H
