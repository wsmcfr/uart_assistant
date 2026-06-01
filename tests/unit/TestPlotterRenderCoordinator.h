/**
 * @file TestPlotterRenderCoordinator.h
 * @brief 绘图渲染协调器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTPLOTTERRENDERCOORDINATOR_H
#define TESTPLOTTERRENDERCOORDINATOR_H

#include <QObject>
#include <QTest>

class TestPlotterRenderCoordinator : public QObject
{
    Q_OBJECT

private slots:
    void testApplyHighPerformanceProfileToPlot();
    void testApplyHighQualityProfileToExistingGraphs();
    void testOpenGlProfileKeepsBackendAndCompatibleHints();
    void testApplyCurveDefaultsFollowsQualityMode();
};

#endif // TESTPLOTTERRENDERCOORDINATOR_H
