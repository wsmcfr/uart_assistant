/**
 * @file TestPlotterManagerLifecycle.h
 * @brief 绘图窗口生命周期与关闭重建行为回归测试头文件
 * @author ComAssistant Team
 * @date 2026-05-06
 */

#ifndef TESTPLOTTERMANAGERLIFECYCLE_H
#define TESTPLOTTERMANAGERLIFECYCLE_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证绘图窗口关闭后会真正销毁，而不是隐藏后继续占用资源。
 *
 * 用户允许“同一个绘图窗口在后续数据到来时再次自动弹出”，
 * 但前提是上一个窗口对象已经完成销毁和资源释放。
 */
class TestPlotterManagerLifecycle : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 用户关闭窗口后，应销毁旧对象；后续新数据到来时应创建新对象。
     */
    void testClosedWindowIsDestroyedBeforeSameIdIsRecreated();
};

#endif // TESTPLOTTERMANAGERLIFECYCLE_H
