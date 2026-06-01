/**
 * @file TestPlotterManagerLifecycle.cpp
 * @brief 绘图窗口生命周期与关闭重建行为回归测试实现
 * @author ComAssistant Team
 * @date 2026-05-06
 */

#include "TestPlotterManagerLifecycle.h"

#include "ui/PlotterManager.h"
#include "ui/PlotterWindow.h"

#include <QPointer>

using namespace ComAssistant;

void TestPlotterManagerLifecycle::testClosedWindowIsDestroyedBeforeSameIdIsRecreated()
{
    PlotterManager* manager = PlotterManager::instance();
    manager->closeAllWindows();
    QCoreApplication::processEvents();

    PlotterWindow* firstWindow = manager->createWindow(QStringLiteral("lifecycle_test"));
    QVERIFY(firstWindow != nullptr);
    QVERIFY(manager->hasWindow(QStringLiteral("lifecycle_test")));

    QPointer<PlotterWindow> destroyedProbe(firstWindow);

    /*
     * 模拟用户点击窗口关闭按钮。
     * 期望结果不是“窗口隐藏后对象还在”，而是旧对象被真正销毁，
     * 这样后续自动重建同 ID 窗口时，旧窗口占用的内存才能释放。
     */
    firstWindow->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QVERIFY2(destroyedProbe.isNull(),
             "关闭绘图窗口后，旧窗口对象应被销毁，而不是继续存活");
    QVERIFY2(!manager->hasWindow(QStringLiteral("lifecycle_test")),
             "旧绘图窗口关闭后，管理器不应再保留该窗口映射");

    PlotterWindow* recreatedWindow = manager->createWindow(QStringLiteral("lifecycle_test"));
    QVERIFY(recreatedWindow != nullptr);
    /*
     * 旧对象销毁后，内存分配器可能把新窗口放回同一个地址，因此不能
     * 用裸指针地址是否相同判断“是否复用旧对象”。这里改为验证旧
     * QPointer 已经失效，并且管理器已经重新登记同 ID 窗口。
     */
    QVERIFY2(destroyedProbe.isNull(),
             "后续同 ID 数据到来时，旧窗口指针仍应保持失效状态");
    QVERIFY2(manager->hasWindow(QStringLiteral("lifecycle_test")),
             "后续同 ID 数据到来时，管理器应登记新创建的窗口");
    QCOMPARE(manager->getWindow(QStringLiteral("lifecycle_test")), recreatedWindow);

    manager->closeAllWindows();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}
