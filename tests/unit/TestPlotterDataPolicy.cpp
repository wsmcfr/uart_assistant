/**
 * @file TestPlotterDataPolicy.cpp
 * @brief 绘图数据策略单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestPlotterDataPolicy.h"

#include "ui/PlotterDataPolicy.h"

using namespace ComAssistant;

void TestPlotterDataPolicy::testNoDecimationKeepsEveryPoint()
{
    PlotterDataPolicy policy;
    policy.setDecimationRatio(DecimationRatio::None);

    QVERIFY(!policy.shouldSkipNextPoint());
    QVERIFY(!policy.shouldSkipNextPoint());
    QVERIFY(!policy.shouldSkipNextPoint());
}

void TestPlotterDataPolicy::testDecimationKeepsNthPoint()
{
    PlotterDataPolicy policy;
    policy.setDecimationRatio(DecimationRatio::Fifth);

    QVERIFY(policy.shouldSkipNextPoint());
    QVERIFY(policy.shouldSkipNextPoint());
    QVERIFY(policy.shouldSkipNextPoint());
    QVERIFY(policy.shouldSkipNextPoint());
    QVERIFY(!policy.shouldSkipNextPoint());
    QVERIFY(policy.shouldSkipNextPoint());
}

void TestPlotterDataPolicy::testResetDecimationClearsCounter()
{
    PlotterDataPolicy policy;
    policy.setDecimationRatio(DecimationRatio::Half);

    QVERIFY(policy.shouldSkipNextPoint());
    policy.setDecimationRatio(DecimationRatio::Half);
    QVERIFY(policy.shouldSkipNextPoint());
    QVERIFY(!policy.shouldSkipNextPoint());
}

void TestPlotterDataPolicy::testTrimPlanKeepsNinetyPercentOfLimit()
{
    const PlotterDataPolicy::TrimPlan noTrim =
        PlotterDataPolicy::makeTrimPlan(5000, 5000);
    QVERIFY(!noTrim.shouldTrim);
    QCOMPARE(noTrim.removeCount, 0);
    QCOMPARE(noTrim.targetSize, 5000);

    const PlotterDataPolicy::TrimPlan trim =
        PlotterDataPolicy::makeTrimPlan(5200, 5000);
    QVERIFY(trim.shouldTrim);
    QCOMPARE(trim.targetSize, 4500);
    QCOMPARE(trim.removeCount, 700);

    const PlotterDataPolicy::TrimPlan invalidLimit =
        PlotterDataPolicy::makeTrimPlan(5200, 0);
    QVERIFY(!invalidLimit.shouldTrim);
    QCOMPARE(invalidLimit.removeCount, 0);
}
