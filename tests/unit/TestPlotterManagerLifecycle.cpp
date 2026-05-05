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
    QVERIFY2(recreatedWindow != firstWindow,
             "后续同 ID 数据到来时，应创建新的窗口对象，而不是复用已关闭对象");

    manager->closeAllWindows();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}
