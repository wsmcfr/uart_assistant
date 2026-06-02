/**
 * @file TestMainWindowLazyLoading.cpp
 * @brief 主窗口重型组件懒加载回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMainWindowLazyLoading.h"

#include "ui/MainWindow.h"
#include "ui/modes/DebugModeWidget.h"
#include "ui/modes/FrameModeWidget.h"
#include "ui/modes/SerialModeWidget.h"
#include "ui/modes/TerminalModeWidget.h"
#include "ui/widgets/HidReportWorkspaceWidget.h"
#include "ui/widgets/TcpClientWorkspaceWidget.h"
#include "ui/widgets/UdpWorkspaceWidget.h"

#include <QComboBox>
#include <QEvent>
#include <QPointer>

using namespace ComAssistant;

namespace {

/**
 * @brief 按 objectName 查找子控件，并在缺失时让测试立即失败。
 * @param root 主窗口。
 * @param objectName 子控件 objectName。
 * @return 找到的子控件指针。
 */
template <typename T>
T* requireChild(QWidget& root, const char* objectName)
{
    T* child = root.findChild<T*>(QString::fromLatin1(objectName));
    if (!child) {
        qFatal("Missing child widget: %s", objectName);
    }
    return child;
}

} // namespace

void TestMainWindowLazyLoading::testMainWindowCreatesHeavyWidgetsOnDemand()
{
    MainWindow window;
    window.resize(1200, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.findChild<SerialModeWidget*>() != nullptr);
    QVERIFY2(window.findChild<TerminalModeWidget*>() == nullptr,
             "终端模式应在首次切换时再创建。");
    QVERIFY2(window.findChild<FrameModeWidget*>() == nullptr,
             "帧模式应在首次切换时再创建。");
    QVERIFY2(window.findChild<DebugModeWidget*>() == nullptr,
             "调试模式应在首次切换时再创建。");
    QVERIFY2(window.findChild<TcpClientWorkspaceWidget*>() == nullptr,
             "TCP Client 工作台应在首次切换通信类型时再创建。");
    QVERIFY2(window.findChild<UdpWorkspaceWidget*>() == nullptr,
             "UDP 工作台应在首次切换通信类型时再创建。");
    QVERIFY2(window.findChild<HidReportWorkspaceWidget*>() == nullptr,
             "HID 工作台应在首次切换通信类型时再创建。");

    QComboBox* displayModeCombo = requireChild<QComboBox>(window, "displayModeCombo");
    displayModeCombo->setCurrentIndex(displayModeCombo->findData(static_cast<int>(DisplayMode::Terminal)));
    QTest::qWait(20);
    QVERIFY2(window.findChild<TerminalModeWidget*>() != nullptr,
             "切换到终端模式后应创建终端组件。");

    QComboBox* commTypeCombo = requireChild<QComboBox>(window, "commTypeCombo");
    commTypeCombo->setCurrentIndex(commTypeCombo->findData(static_cast<int>(CommType::TcpClient)));
    QTest::qWait(20);
    QVERIFY2(window.findChild<TcpClientWorkspaceWidget*>() != nullptr,
             "切换到 TCP Client 后应创建 TCP Client 工作台。");
}

void TestMainWindowLazyLoading::testMainWindowReleasesUnusedHeavyWidgetsAfterSwitchingAway()
{
    MainWindow window;
    window.resize(1200, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QComboBox* displayModeCombo = requireChild<QComboBox>(window, "displayModeCombo");
    displayModeCombo->setCurrentIndex(displayModeCombo->findData(static_cast<int>(DisplayMode::Terminal)));
    QTest::qWait(30);

    QPointer<TerminalModeWidget> terminalProbe(window.findChild<TerminalModeWidget*>());
    QVERIFY2(!terminalProbe.isNull(), "切换到终端模式后应先创建终端组件。");

    displayModeCombo->setCurrentIndex(displayModeCombo->findData(static_cast<int>(DisplayMode::Serial)));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(30);
    QVERIFY2(terminalProbe.isNull(),
             "离开终端模式后应销毁终端组件，释放终端缓冲、主题和工具栏对象。");
    QVERIFY2(window.findChild<TerminalModeWidget*>() == nullptr,
             "离开终端模式后主窗口不应再持有终端组件。");

    QComboBox* commTypeCombo = requireChild<QComboBox>(window, "commTypeCombo");
    commTypeCombo->setCurrentIndex(commTypeCombo->findData(static_cast<int>(CommType::TcpClient)));
    QTest::qWait(30);

    QPointer<TcpClientWorkspaceWidget> tcpClientProbe(window.findChild<TcpClientWorkspaceWidget*>());
    QVERIFY2(!tcpClientProbe.isNull(), "切换到 TCP Client 后应先创建专用工作台。");

    commTypeCombo->setCurrentIndex(commTypeCombo->findData(static_cast<int>(CommType::Serial)));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(30);
    QVERIFY2(tcpClientProbe.isNull(),
             "离开 TCP Client 后应销毁专用工作台，释放日志区和表单缓存。");
    QVERIFY2(window.findChild<TcpClientWorkspaceWidget*>() == nullptr,
             "离开 TCP Client 后主窗口不应再持有 TCP Client 工作台。");
}

void TestMainWindowLazyLoading::testMainWindowReleasesSerialAdvancedModeWhenLeavingSerialWorkspace()
{
    MainWindow window;
    window.resize(1200, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QComboBox* displayModeCombo = requireChild<QComboBox>(window, "displayModeCombo");
    displayModeCombo->setCurrentIndex(displayModeCombo->findData(static_cast<int>(DisplayMode::Terminal)));
    QTest::qWait(30);

    QPointer<TerminalModeWidget> terminalProbe(window.findChild<TerminalModeWidget*>());
    QVERIFY2(!terminalProbe.isNull(), "测试前应先创建终端模式组件。");

    /*
     * 切到非串口工作台后，串口高级模式即使被隐藏也不应继续常驻。
     * 这对应用户“用到什么加载什么，不用就不要加载”的内存优先要求。
     */
    QComboBox* commTypeCombo = requireChild<QComboBox>(window, "commTypeCombo");
    commTypeCombo->setCurrentIndex(commTypeCombo->findData(static_cast<int>(CommType::TcpClient)));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(30);

    QVERIFY2(terminalProbe.isNull(),
             "离开串口工作台后应释放隐藏的终端组件。");
    QVERIFY2(window.findChild<TerminalModeWidget*>() == nullptr,
             "切到 TCP Client 后主窗口不应继续持有终端模式组件。");
}

void TestMainWindowLazyLoading::testLeavingSerialWorkspaceKeepsLastSerialDisplaySelection()
{
    MainWindow window;
    window.resize(1200, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QComboBox* displayModeCombo = requireChild<QComboBox>(window, "displayModeCombo");
    displayModeCombo->setCurrentIndex(displayModeCombo->findData(static_cast<int>(DisplayMode::Terminal)));
    QTest::qWait(30);

    QPointer<TerminalModeWidget> terminalProbe(window.findChild<TerminalModeWidget*>());
    QVERIFY2(!terminalProbe.isNull(), "测试前应先创建终端模式组件。");

    QComboBox* commTypeCombo = requireChild<QComboBox>(window, "commTypeCombo");
    commTypeCombo->setCurrentIndex(commTypeCombo->findData(static_cast<int>(CommType::TcpClient)));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(30);

    QVERIFY2(terminalProbe.isNull(),
             "离开串口工作台后仍应释放终端模式组件。");
    QCOMPARE(displayModeCombo->currentData().toInt(),
             static_cast<int>(DisplayMode::Terminal));
}
