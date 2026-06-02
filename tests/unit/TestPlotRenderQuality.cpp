/**
 * @file TestPlotRenderQuality.cpp
 * @brief 绘图渲染质量策略单元测试
 * @author ComAssistant Team
 * @date 2026-03-09
 */

#include "TestPlotRenderQuality.h"
#include "ui/PlotRenderQuality.h"

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QTextStream>

using namespace ComAssistant;

void TestPlotRenderQuality::initTestCase()
{
    qDebug() << "Starting PlotRenderQuality tests...";
}

void TestPlotRenderQuality::cleanupTestCase()
{
    qDebug() << "PlotRenderQuality tests completed.";
}

void TestPlotRenderQuality::testDefaultBackendProfile()
{
    const PlotBackendProfile profile = makeDefaultPlotBackendProfile();

    /*
     * 绘图窗口默认不直接启用 OpenGL，避免新窗口刚打开就分配大体积 FBO。
     * 同时保留 4x 多重采样，保证用户手动开启硬件加速时仍具备平滑边缘质量。
     */
    QCOMPARE(profile.openGlEnabledByDefault, false);
    QCOMPARE(profile.openGlMultisamples, 4);
}

void TestPlotRenderQuality::testHighQualityProfile()
{
    const RenderQualityProfile profile = makeRenderQualityProfile(RenderQualityMode::HighQuality);

    QCOMPARE(profile.updateIntervalMs, 33);
    QCOMPARE(profile.noAntialiasingOnDrag, false);
    QCOMPARE(profile.useFastPolylines, false);
    QCOMPARE(profile.antialiasPlottables, true);
    QCOMPARE(profile.throttleAutoRangeUpdates, false);
    QCOMPARE(profile.valuePanelUpdateEvery, 2);
}

void TestPlotRenderQuality::testHighPerformanceProfile()
{
    const RenderQualityProfile profile = makeRenderQualityProfile(RenderQualityMode::HighPerformance);

    QCOMPARE(profile.updateIntervalMs, 25);
    QCOMPARE(profile.noAntialiasingOnDrag, true);
    QCOMPARE(profile.useFastPolylines, true);
    QCOMPARE(profile.antialiasPlottables, false);
    QCOMPARE(profile.throttleAutoRangeUpdates, true);
    QCOMPARE(profile.valuePanelUpdateEvery, 4);
}

void TestPlotRenderQuality::testModeDifference()
{
    const RenderQualityProfile quality = makeRenderQualityProfile(RenderQualityMode::HighQuality);
    const RenderQualityProfile performance = makeRenderQualityProfile(RenderQualityMode::HighPerformance);

    QVERIFY(quality.updateIntervalMs > performance.updateIntervalMs);
    QVERIFY(quality.antialiasPlottables != performance.antialiasPlottables);
    QVERIFY(quality.useFastPolylines != performance.useFastPolylines);
    QVERIFY(quality.throttleAutoRangeUpdates != performance.throttleAutoRangeUpdates);
}

void TestPlotRenderQuality::testBackendProfileIndependentFromQualityMode()
{
    const PlotBackendProfile backend = makeDefaultPlotBackendProfile();
    const RenderQualityProfile quality = makeRenderQualityProfile(RenderQualityMode::HighQuality);
    const RenderQualityProfile performance = makeRenderQualityProfile(RenderQualityMode::HighPerformance);

    /*
     * 这条断言用于防止后续维护时把“渲染质量档位”和“默认后端选择”重新耦合。
     * 内存优化只应影响默认后端，不应偷偷篡改已有质量档位的刷新节流参数。
     */
    QCOMPARE(backend.openGlEnabledByDefault, false);
    QCOMPARE(backend.openGlMultisamples, 4);
    QCOMPARE(quality.updateIntervalMs, 33);
    QCOMPARE(performance.updateIntervalMs, 25);
}

void TestPlotRenderQuality::testBlankPlotFrameDetection()
{
    /*
     * 纯白截图等价于用户反馈的 OpenGL 白屏：QCustomPlot 已经切换到 OpenGL，
     * 但实际帧里没有坐标轴、网格、文字或曲线等有效像素。
     */
    QImage blankFrame(320, 180, QImage::Format_ARGB32_Premultiplied);
    blankFrame.fill(Qt::white);

    const PlotFrameInkAnalysis analysis = analyzePlotFrameInk(blankFrame, QColor(Qt::white), 0.001);

    QVERIFY(analysis.valid);
    QVERIFY(analysis.likelyBlank);
    QCOMPARE(analysis.inkPixels, 0);
}

void TestPlotRenderQuality::testPlotFrameDetectionKeepsVisibleInk()
{
    /*
     * 正常绘图即使背景接近白色，也一定会有坐标轴、网格或曲线留下非背景像素。
     * 这条用一条深色曲线模拟“有内容”的帧，避免回退逻辑误杀可用的 OpenGL。
     */
    QImage plotFrame(320, 180, QImage::Format_ARGB32_Premultiplied);
    plotFrame.fill(Qt::white);

    QPainter painter(&plotFrame);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(40, 90, 190), 2));
    painter.drawLine(20, 150, 300, 30);
    painter.end();

    const PlotFrameInkAnalysis analysis = analyzePlotFrameInk(plotFrame, QColor(Qt::white), 0.001);

    QVERIFY(analysis.valid);
    QVERIFY(!analysis.likelyBlank);
    QVERIFY(analysis.inkPixels > 0);
}

void TestPlotRenderQuality::testPlotterWindowFallsBackWhenOpenGlFrameIsBlank()
{
    /*
     * 用户机器已经出现“QCustomPlot::openGl() 返回 true，但实际窗口仍全白”的情况。
     * 因此窗口层不能只相信 OpenGL 后端状态，还必须检查实际帧，并在空白时切回软件绘制。
     */
    QFile sourceFile(QStringLiteral(COMASSISTANT_SOURCE_DIR)
                     + QStringLiteral("/src/ui/PlotterWindow.cpp"));
    QVERIFY2(sourceFile.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("无法读取 PlotterWindow.cpp: %1").arg(sourceFile.errorString())));

    QTextStream stream(&sourceFile);
    const QString source = stream.readAll();
    const QRegularExpression openGlFunctionPattern(
        QStringLiteral("bool PlotterWindow::trySetOpenGLEnabled\\(bool enabled\\)\\s*\\{(?<body>.*?)\\n\\}"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = openGlFunctionPattern.match(source);

    QVERIFY2(match.hasMatch(), "未找到 PlotterWindow::trySetOpenGLEnabled 实现。");
    const QString body = match.captured(QStringLiteral("body"));
    QVERIFY(body.contains(QStringLiteral("validateOpenGlRenderedFrame()")));
    QVERIFY(body.contains(QStringLiteral("m_plot->setOpenGl(false)")));
    QVERIFY(body.contains(QStringLiteral("m_openGLAvailable = false")));

    /*
     * 白屏问题发生在用户实际看到的 QWidget 上，所以验证函数必须优先抓取控件画面，
     * 不能只依赖 toPixmap 导出路径；导出路径只能作为不可抓取时的兜底诊断。
     */
    QVERIFY(source.contains(QStringLiteral("m_plot->grab()")));
    QVERIFY(source.contains(QStringLiteral("m_plot->toPixmap(frameWidth, frameHeight, 1.0)")));
}

void TestPlotRenderQuality::testQCustomPlotFboDrawMakesContextCurrentBeforeReadback()
{
    /*
     * QCustomPlot 的 OpenGL FBO 缓冲最终通过 QOpenGLFramebufferObject::toImage()
     * 读回并合成到 QWidget。部分 Windows/Qt 5.12/OpenGL 驱动组合要求读回前
     * 当前线程必须重新 makeCurrent 到创建该 FBO 的 context，否则会得到空白图像。
     * 这里用源码级回归测试锁住该补丁，避免后续升级第三方文件时丢失。
     */
    QFile sourceFile(QStringLiteral(COMASSISTANT_SOURCE_DIR)
                     + QStringLiteral("/src/third_party/qcustomplot/qcustomplot.cpp"));
    QVERIFY2(sourceFile.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("无法读取 qcustomplot.cpp: %1").arg(sourceFile.errorString())));

    QTextStream stream(&sourceFile);
    const QString source = stream.readAll();
    const QRegularExpression drawFunctionPattern(
        QStringLiteral("void QCPPaintBufferGlFbo::draw\\(QCPPainter \\*painter\\) const\\s*\\{(?<body>.*?)\\n\\}"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = drawFunctionPattern.match(source);

    QVERIFY2(match.hasMatch(), "未找到 QCPPaintBufferGlFbo::draw 实现。");
    const QString body = match.captured(QStringLiteral("body"));
    QVERIFY(body.contains(QStringLiteral("mGlContext.toStrongRef()")));
    QVERIFY(body.contains(QStringLiteral("context->makeCurrent(context->surface())")));
    QVERIFY(body.indexOf(QStringLiteral("context->makeCurrent(context->surface())"))
            < body.indexOf(QStringLiteral("mGlFrameBuffer->toImage()")));
}
