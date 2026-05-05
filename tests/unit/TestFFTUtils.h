/**
 * @file TestFFTUtils.h
 * @brief FFT 工具边界可靠性测试头文件
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#ifndef TESTFFTUTILS_H
#define TESTFFTUTILS_H

#include <QObject>
#include <QTest>

class TestFFTUtils : public QObject {
    Q_OBJECT

private slots:
    void testAnalyzeWithNonPowerOfTwoSize();
    void testAnalyzeFiltersNonFiniteSamples();
    void testAnalyzeClampsInvalidSampleRate();
};

#endif // TESTFFTUTILS_H
