/**
 * @file WaterfallWidget.cpp
 * @brief 瀑布图/频谱图显示组件实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "WaterfallWidget.h"
#include "core/utils/Logger.h"

#include <QVBoxLayout>
#include <cmath>

namespace ComAssistant {

WaterfallWidget::WaterfallWidget(QWidget* parent)
    : QWidget(parent)
    , m_plot(nullptr)
    , m_colorMap(nullptr)
    , m_colorScale(nullptr)
{
    setupUi();
}

WaterfallWidget::~WaterfallWidget()
{
}

void WaterfallWidget::setupUi()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_plot = new QCustomPlot(this);
    layout->addWidget(m_plot);

    // 创建颜色图
    m_colorMap = new QCPColorMap(m_plot->xAxis, m_plot->yAxis);

    // 设置数据大小
    m_colorMap->data()->setSize(m_freqBins, m_timeDepth);
    m_colorMap->data()->setRange(
        QCPRange(m_minFreq, m_maxFreq),  // X: 频率
        QCPRange(0, m_timeDepth)          // Y: 时间
    );

    // 初始化为最小值
    for (int x = 0; x < m_freqBins; ++x) {
        for (int y = 0; y < m_timeDepth; ++y) {
            m_colorMap->data()->setCell(x, y, m_minDb);
        }
    }

    // 创建颜色标尺
    m_colorScale = new QCPColorScale(m_plot);
    m_plot->plotLayout()->addElement(0, 1, m_colorScale);
    m_colorScale->setType(QCPAxis::atRight);
    m_colorMap->setColorScale(m_colorScale);
    m_colorScale->axis()->setLabel(tr("幅度 (dB)"));

    // 默认颜色映射
    setupColorMap(ColorMap::Jet);

    // 设置坐标轴
    m_plot->xAxis->setLabel(tr("频率 (Hz)"));
    m_plot->yAxis->setLabel(tr("时间"));
    m_plot->xAxis->setRange(m_minFreq, m_maxFreq);
    m_plot->yAxis->setRange(0, m_timeDepth);

    // Y轴反向（新数据在顶部）
    m_plot->yAxis->setRangeReversed(true);

    // 设置交互
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // 边距
    QCPMarginGroup* marginGroup = new QCPMarginGroup(m_plot);
    m_plot->axisRect()->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);
    m_colorScale->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);

    // 鼠标跟踪
    m_plot->setMouseTracking(true);
    connect(m_plot, &QCustomPlot::mouseMove, this, [this](QMouseEvent* event) {
        double freq = m_plot->xAxis->pixelToCoord(event->pos().x());
        double timeIdx = m_plot->yAxis->pixelToCoord(event->pos().y());

        int xi = static_cast<int>((freq - m_minFreq) / (m_maxFreq - m_minFreq) * m_freqBins);
        int yi = static_cast<int>(timeIdx);

        if (xi >= 0 && xi < m_freqBins && yi >= 0 && yi < m_timeDepth) {
            double amp = m_colorMap->data()->cell(xi, yi);
            emit cursorMoved(freq, yi, amp);
        }
    });

    // 显式设置颜色数据范围（不使用rescaleDataRange，因为初始数据都是m_minDb）
    m_colorMap->setDataRange(QCPRange(m_minDb, m_maxDb));
    m_plot->replot();
}

void WaterfallWidget::setupColorMap(ColorMap type)
{
    QCPColorGradient gradient;

    switch (type) {
    case ColorMap::Jet:
        gradient = QCPColorGradient::gpJet;
        break;
    case ColorMap::Hot:
        gradient = QCPColorGradient::gpHot;
        break;
    case ColorMap::Cold:
        gradient = QCPColorGradient::gpCold;
        break;
    case ColorMap::Grayscale:
        gradient = QCPColorGradient::gpGrayscale;
        break;
    case ColorMap::Spectrum:
        // 自定义频谱色
        gradient.clearColorStops();
        gradient.setColorStopAt(0.0, QColor(0, 0, 128));     // 深蓝
        gradient.setColorStopAt(0.2, QColor(0, 0, 255));     // 蓝
        gradient.setColorStopAt(0.4, QColor(0, 255, 255));   // 青
        gradient.setColorStopAt(0.6, QColor(0, 255, 0));     // 绿
        gradient.setColorStopAt(0.8, QColor(255, 255, 0));   // 黄
        gradient.setColorStopAt(1.0, QColor(255, 0, 0));     // 红
        break;
    }

    m_colorMap->setGradient(gradient);
    m_colorScale->setGradient(gradient);
}

void WaterfallWidget::setFrequencyRange(double minFreq, double maxFreq)
{
    m_minFreq = minFreq;
    m_maxFreq = maxFreq;

    m_colorMap->data()->setRange(
        QCPRange(m_minFreq, m_maxFreq),
        QCPRange(0, m_timeDepth)
    );
    m_plot->xAxis->setRange(m_minFreq, m_maxFreq);
    m_plot->replot();
}

void WaterfallWidget::setTimeDepth(int depth)
{
    if (depth == m_timeDepth) return;

    m_timeDepth = depth;
    m_currentRow = 0;

    m_colorMap->data()->setSize(m_freqBins, m_timeDepth);
    m_colorMap->data()->setRange(
        QCPRange(m_minFreq, m_maxFreq),
        QCPRange(0, m_timeDepth)
    );

    // 初始化为最小值
    for (int x = 0; x < m_freqBins; ++x) {
        for (int y = 0; y < m_timeDepth; ++y) {
            m_colorMap->data()->setCell(x, y, m_minDb);
        }
    }

    m_plot->yAxis->setRange(0, m_timeDepth);
    m_plot->replot();
}

void WaterfallWidget::setAmplitudeRange(double minDb, double maxDb)
{
    m_minDb = minDb;
    m_maxDb = maxDb;
    m_colorMap->setDataRange(QCPRange(m_minDb, m_maxDb));
    m_plot->replot();
}

void WaterfallWidget::setColorMap(ColorMap colorMap)
{
    setupColorMap(colorMap);
    m_plot->replot();
}

void WaterfallWidget::shiftDataUp()
{
    // 将所有数据向上移动一行
    for (int y = m_timeDepth - 1; y > 0; --y) {
        for (int x = 0; x < m_freqBins; ++x) {
            m_colorMap->data()->setCell(x, y, m_colorMap->data()->cell(x, y - 1));
        }
    }
}

void WaterfallWidget::addSpectrumLine(const QVector<double>& spectrum)
{
    if (m_paused) return;

    // 将数据上移
    shiftDataUp();

    // 重采样到目标频率点数
    int srcSize = spectrum.size();
    for (int x = 0; x < m_freqBins; ++x) {
        int srcIdx = static_cast<int>(x * srcSize / static_cast<double>(m_freqBins));
        if (srcIdx >= srcSize) srcIdx = srcSize - 1;

        double value = spectrum[srcIdx];
        // 限制范围
        value = qBound(m_minDb, value, m_maxDb);
        m_colorMap->data()->setCell(x, 0, value);
    }

    m_plot->replot(QCustomPlot::rpImmediateRefresh);
}

void WaterfallWidget::addMagnitudeLine(const QVector<double>& magnitudes)
{
    if (m_paused) return;

    // 转换为dB
    QVector<double> dbSpectrum;
    dbSpectrum.reserve(magnitudes.size());

    double maxMag = *std::max_element(magnitudes.begin(), magnitudes.end());
    if (maxMag < 1e-20) maxMag = 1e-20;

    for (double mag : magnitudes) {
        double db = (mag > 1e-20) ? 20 * std::log10(mag / maxMag) : -200;
        dbSpectrum.append(db);
    }

    addSpectrumLine(dbSpectrum);
}

void WaterfallWidget::clearData()
{
    m_currentRow = 0;

    for (int x = 0; x < m_freqBins; ++x) {
        for (int y = 0; y < m_timeDepth; ++y) {
            m_colorMap->data()->setCell(x, y, m_minDb);
        }
    }

    m_plot->replot();
}

bool WaterfallWidget::exportImage(const QString& filename)
{
    QString ext = QFileInfo(filename).suffix().toLower();

    if (ext == "png") {
        return m_plot->savePng(filename, 0, 0, 2.0);
    } else if (ext == "jpg" || ext == "jpeg") {
        return m_plot->saveJpg(filename, 0, 0, 2.0);
    } else if (ext == "pdf") {
        return m_plot->savePdf(filename);
    }

    return m_plot->savePng(filename + ".png", 0, 0, 2.0);
}

} // namespace ComAssistant
