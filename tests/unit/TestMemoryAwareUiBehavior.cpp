/**
 * @file TestMemoryAwareUiBehavior.cpp
 * @brief 内存友好 UI 行为回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMemoryAwareUiBehavior.h"

#include "ui/PlotterWindow.h"
#include "ui/controls/controls/SliderControl.h"
#include "ui/widgets/PlotControlPanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSizePolicy>

using namespace ComAssistant;

void TestMemoryAwareUiBehavior::testPlotControlPanelExposesCurveRenameAction()
{
    PlotterWindow window(QStringLiteral("rename_ui_test"));
    window.resize(1000, 700);
    window.addCurve(QStringLiteral("原始曲线"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    PlotControlPanel* panel = window.findChild<PlotControlPanel*>(QStringLiteral("plotControlPanel"));
    QVERIFY2(panel != nullptr, "绘图窗口右侧控制面板应存在。");

    QListWidget* curveList = panel->findChild<QListWidget*>(QStringLiteral("plotCurveList"));
    QVERIFY2(curveList != nullptr, "曲线列表应有稳定 objectName，方便回归测试和后续维护。");
    QVERIFY(curveList->count() > 0);
    curveList->setCurrentRow(0);

    QPushButton* renameButton = panel->findChild<QPushButton*>(QStringLiteral("plotRenameCurveButton"));
    QVERIFY2(renameButton != nullptr, "曲线管理区应提供“重命名”按钮。");
    QVERIFY2(renameButton->isEnabled(), "选择曲线后重命名按钮应可点击。");

    window.renameCurve(0, QStringLiteral("用户自定义曲线"));
    panel->updateCurveList();
    QCOMPARE(window.curveName(0), QStringLiteral("用户自定义曲线"));
    QCOMPARE(curveList->item(0)->text(), QStringLiteral("用户自定义曲线"));
}

void TestMemoryAwareUiBehavior::testSliderControlLayoutAvoidsTextOverlap()
{
    SliderControl control;
    control.resize(220, 120);
    control.setRange(-12345.67, 98765.43);
    control.setDecimals(2);
    control.setUnit(QStringLiteral("rpm"));
    control.show();
    QVERIFY(QTest::qWaitForWindowExposed(&control));

    QVERIFY2(control.minimumHeight() >= 110,
             "滑动条控件不能再固定为 80px，否则名称、滑条、数值和范围文本容易互相遮挡。");
    QCOMPARE(control.maximumHeight(), QWIDGETSIZE_MAX);

    QLineEdit* valueEdit = control.findChild<QLineEdit*>(QStringLiteral("sliderValueEdit"));
    QLabel* unitLabel = control.findChild<QLabel*>(QStringLiteral("sliderUnitLabel"));
    QLabel* rangeLabel = control.findChild<QLabel*>(QStringLiteral("sliderRangeLabel"));

    QVERIFY(valueEdit != nullptr);
    QVERIFY(unitLabel != nullptr);
    QVERIFY(rangeLabel != nullptr);

    QVERIFY2(valueEdit->minimumWidth() >= 88,
             "数值输入框需要能容纳负号、小数和较大的调参值。");
    QVERIFY2(rangeLabel->wordWrap(),
             "范围提示允许换行，窄窗口下不应挤压数值输入框。");
    QVERIFY2(rangeLabel->sizePolicy().horizontalPolicy() == QSizePolicy::Expanding,
             "范围提示应随剩余空间伸缩，而不是固定宽度抢占其它控件。");
}
