/**
 * @file TestPlotRenderQuality.h
 * @brief 绘图渲染质量策略单元测试头文件
 * @author ComAssistant Team
 * @date 2026-03-09
 */

#ifndef TESTPLOTRENDERQUALITY_H
#define TESTPLOTRENDERQUALITY_H

#include <QObject>
#include <QTest>

class TestPlotRenderQuality : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testDefaultBackendProfile();
    void testHighQualityProfile();
    void testHighPerformanceProfile();
    void testModeDifference();
    void testBackendProfileIndependentFromQualityMode();
};

#endif // TESTPLOTRENDERQUALITY_H
