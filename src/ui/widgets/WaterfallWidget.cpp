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

    m_spectrumFlushTimer = new QTimer(this);
    m_spectrumFlushTimer->setSingleShot(true);
    connect(m_spectrumFlushTimer, &QTimer::timeout, this, &WaterfallWidget::flushPendingSpectra);
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
    m_plot->replot(QCustomPlot::rpQueuedReplot);
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
    /*
     * 频率范围可能来自恢复配置或用户输入，非法范围会让鼠标定位和颜色图
     * 坐标换算出现 NaN。这里保持当前有效范围，避免把瀑布图切到坏状态。
     */
    if (!std::isfinite(minFreq) || !std::isfinite(maxFreq) || maxFreq <= minFreq) {
        return;
    }

    m_minFreq = minFreq;
    m_maxFreq = maxFreq;

    m_colorMap->data()->setRange(
        QCPRange(m_minFreq, m_maxFreq),
        QCPRange(0, m_timeDepth)
    );
    m_plot->xAxis->setRange(m_minFreq, m_maxFreq);
    flushPendingSpectra();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WaterfallWidget::setTimeDepth(int depth)
{
    /*
     * 时间深度直接决定 QCPColorMap 的二维缓存大小，必须限制到正数，
     * 防止恢复异常配置时创建空图或过大的 UI 缓冲。
     */
    depth = qBound(1, depth, 2000);
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
    flushPendingSpectra();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WaterfallWidget::setAmplitudeRange(double minDb, double maxDb)
{
    /*
     * 幅度范围用于颜色映射，非法范围会让色带退化。无效输入直接忽略，
     * 保留上一次可用配置，保证实时绘图不中断。
     */
    if (!std::isfinite(minDb) || !std::isfinite(maxDb) || maxDb <= minDb) {
        return;
    }

    m_minDb = minDb;
    m_maxDb = maxDb;
    m_colorMap->setDataRange(QCPRange(m_minDb, m_maxDb));
    flushPendingSpectra();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WaterfallWidget::setColorMap(ColorMap colorMap)
{
    setupColorMap(colorMap);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WaterfallWidget::shiftDataUp(int rows)
{
    /*
     * 批量追加频谱线时一次性移动旧数据。原实现每条频谱线都搬移整张
     * m_freqBins*m_timeDepth 色图，高频 FFT 下 CPU 消耗会线性放大。
     * 这里按批次移动 rows 行，数据内容和显示顺序保持不变，只减少重复搬移。
     */
    rows = qBound(0, rows, m_timeDepth);
    if (rows <= 0) {
        return;
    }

    for (int y = m_timeDepth - 1; y >= rows; --y) {
        for (int x = 0; x < m_freqBins; ++x) {
            m_colorMap->data()->setCell(x, y, m_colorMap->data()->cell(x, y - rows));
        }
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < m_freqBins; ++x) {
            m_colorMap->data()->setCell(x, y, m_minDb);
        }
    }
}

QVector<double> WaterfallWidget::normalizeSpectrumLine(const QVector<double>& spectrum) const
{
    QVector<double> normalized;
    if (spectrum.isEmpty()) {
        return normalized;
    }

    // 重采样到目标频率点数，保持和原瀑布图一致的横向显示质量。
    normalized.resize(m_freqBins);
    int srcSize = spectrum.size();
    for (int x = 0; x < m_freqBins; ++x) {
        int srcIdx = static_cast<int>(x * srcSize / static_cast<double>(m_freqBins));
        if (srcIdx >= srcSize) srcIdx = srcSize - 1;

        double value = spectrum[srcIdx];
        if (!std::isfinite(value)) {
            value = m_minDb;
        }
        // 限制范围，避免异常脚本或非法 FFT 输出污染颜色映射。
        normalized[x] = qBound(m_minDb, value, m_maxDb);
    }

    return normalized;
}

void WaterfallWidget::scheduleSpectrumFlush()
{
    /*
     * 高频频谱线先进入队列，定时批量写入颜色图。这样既不丢线，也不会
     * 每条线都触发整张色图搬移和 replot。
     */
    if (m_pendingSpectra.size() >= m_spectrumFlushBatchSize) {
        flushPendingSpectra();
        return;
    }

    if (m_spectrumFlushTimer && !m_spectrumFlushTimer->isActive()) {
        m_spectrumFlushTimer->start(m_spectrumFlushIntervalMs);
    }
}

void WaterfallWidget::flushPendingSpectra()
{
    if (m_spectrumFlushTimer) {
        m_spectrumFlushTimer->stop();
    }
    if (!m_colorMap || m_pendingSpectra.isEmpty()) {
        return;
    }

    /*
     * 单次最多写入 timeDepth 行。若后台短时间产生了更多频谱线，旧的
     * 超出部分在原逐行实现中也会被挤出可视时间深度；这里直接保留最新
     * timeDepth 行，显示结果与逐条追加后的最终状态一致。
     */
    if (m_pendingSpectra.size() > m_timeDepth) {
        const int overflow = m_pendingSpectra.size() - m_timeDepth;
        m_pendingSpectra.erase(m_pendingSpectra.begin(), m_pendingSpectra.begin() + overflow);
    }

    const int count = qMin(m_pendingSpectra.size(), m_spectrumFlushBatchSize);
    shiftDataUp(count);

    /*
     * 颜色图 Y 轴反向，新数据位于视觉顶部。flush 时 m_pendingSpectra
     * 仍按接收先后排列，因此最新行写到 row 0，较早行依次写到 row 1。
     */
    for (int row = 0; row < count; ++row) {
        const QVector<double>& line = m_pendingSpectra.at(count - 1 - row);
        for (int x = 0; x < qMin(m_freqBins, line.size()); ++x) {
            m_colorMap->data()->setCell(x, row, line.at(x));
        }
    }

    m_pendingSpectra.erase(m_pendingSpectra.begin(), m_pendingSpectra.begin() + count);
    if (!m_pendingSpectra.isEmpty()) {
        scheduleSpectrumFlush();
    }

    // 瀑布图实时刷新使用排队重绘，合并同一事件循环内的 repaint。
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WaterfallWidget::addSpectrumLine(const QVector<double>& spectrum)
{
    if (m_paused) return;

    QVector<double> normalized = normalizeSpectrumLine(spectrum);
    if (normalized.isEmpty()) {
        return;
    }

    m_pendingSpectra.append(normalized);
    scheduleSpectrumFlush();
}

void WaterfallWidget::addMagnitudeLine(const QVector<double>& magnitudes)
{
    if (m_paused) return;
    if (magnitudes.isEmpty()) {
        return;
    }

    // 转换为dB
    QVector<double> dbSpectrum;
    dbSpectrum.reserve(magnitudes.size());

    /*
     * FFT 边界保护会过滤 NaN/Inf，但瀑布图也可能被脚本或后续模块直接调用。
     * 这里只用有限且为正的幅度计算参考值，防止 max_element 得到 NaN 后污染整行。
     */
    double maxMag = 0.0;
    for (double mag : magnitudes) {
        if (std::isfinite(mag) && mag > maxMag) {
            maxMag = mag;
        }
    }
    if (maxMag < 1e-20) maxMag = 1e-20;

    for (double mag : magnitudes) {
        double db = (std::isfinite(mag) && mag > 1e-20) ? 20 * std::log10(mag / maxMag) : -200;
        dbSpectrum.append(db);
    }

    addSpectrumLine(dbSpectrum);
}

void WaterfallWidget::clearData()
{
    if (m_spectrumFlushTimer) {
        m_spectrumFlushTimer->stop();
    }
    m_pendingSpectra.clear();
    m_currentRow = 0;

    for (int x = 0; x < m_freqBins; ++x) {
        for (int y = 0; y < m_timeDepth; ++y) {
            m_colorMap->data()->setCell(x, y, m_minDb);
        }
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WaterfallWidget::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }

    m_paused = paused;
    if (m_paused) {
        /*
         * 暂停时把已经收到但尚未写屏的数据先落到颜色图，避免用户按下暂停后
         * 看到的最后一帧落后于实际接收状态；暂停期间的新数据仍会被忽略。
         */
        flushPendingSpectra();
    }
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
