/**
 * @file main.cpp
 * @brief 测试主入口
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include <QApplication>
#include <QTest>
#include <QDebug>

// 包含测试头文件
#include "unit/TestChecksumUtils.h"
#include "unit/TestConversionUtils.h"
#include "unit/TestEncodingUtils.h"
#include "unit/TestAnsiParser.h"
#include "unit/TestFFTUtils.h"
#include "unit/TestFilterUtils.h"
#include "unit/TestDisplaySettingsPolicy.h"
#include "unit/TestPlotRenderQuality.h"
#include "unit/TestPlotterManagerLifecycle.h"
#include "unit/TestTabbedReceiveWidget.h"
#include "unit/TestTerminalModeWidget.h"
#include "unit/TestTranslationCompleteness.h"

int main(int argc, char *argv[])
{
    /*
     * 部分回归测试需要真实构造 QWidget（例如终端模式的发送/接收显示边界），
     * 因此测试入口使用 QApplication。纯工具类测试仍可在同一进程内运行。
     */
    QApplication app(argc, argv);
    app.setApplicationName("ComAssistant_tests");

    int status = 0;

    // 过滤掉 -o 参数，因为它会导致每个测试类覆盖输出文件
    // 让所有测试结果输出到控制台
    QStringList filteredArgs;
    filteredArgs << QString::fromLocal8Bit(argv[0]);
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "-o") {
            // 跳过 -o 及其参数
            if (i + 1 < argc) ++i;
            continue;
        }
        filteredArgs << arg;
    }

    qDebug() << "========================================";
    qDebug() << "ComAssistant Unit Tests";
    qDebug() << "========================================";

    // 运行所有测试
    {
        qDebug() << "\n[TEST] ChecksumUtils";
        TestChecksumUtils test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] ConversionUtils";
        TestConversionUtils test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] EncodingUtils";
        TestEncodingUtils test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] AnsiParser";
        TestAnsiParser test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] FFTUtils";
        TestFFTUtils test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] FilterUtils";
        TestFilterUtils test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] DisplaySettingsPolicy";
        TestDisplaySettingsPolicy test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] PlotRenderQuality";
        TestPlotRenderQuality test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] PlotterManagerLifecycle";
        TestPlotterManagerLifecycle test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] TabbedReceiveWidget";
        TestTabbedReceiveWidget test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] TerminalModeWidget";
        TestTerminalModeWidget test;
        status |= QTest::qExec(&test, filteredArgs);
    }
    {
        qDebug() << "\n[TEST] TranslationCompleteness";
        TestTranslationCompleteness test;
        status |= QTest::qExec(&test, filteredArgs);
    }

    qDebug() << "\n========================================";
    if (status == 0) {
        qDebug() << "All tests PASSED!";
    } else {
        qDebug() << "Some tests FAILED!";
    }
    qDebug() << "========================================";

    return status;
}
