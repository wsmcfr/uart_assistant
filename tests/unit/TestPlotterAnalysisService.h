/**
 * @file TestPlotterAnalysisService.h
 * @brief 绘图分析服务单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTPLOTTERANALYSISSERVICE_H
#define TESTPLOTTERANALYSISSERVICE_H

#include <QObject>
#include <QTest>

class TestPlotterAnalysisService : public QObject
{
    Q_OBJECT

private slots:
    void testExtractYValuesAndBasicStatistics();
    void testAdvancedStatisticsUsesGraphKeys();
    void testPrepareFftInputRejectsShortCurves();
    void testPidMetricsDelegatesToCalculator();
};

#endif // TESTPLOTTERANALYSISSERVICE_H
