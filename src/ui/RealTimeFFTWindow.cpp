/**
 * @file RealTimeFFTWindow.cpp
 * @brief 实时FFT分析窗口实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "RealTimeFFTWindow.h"
#include "widgets/WaterfallWidget.h"
#include "core/utils/Logger.h"

#include <QToolBar>
#include <QVBoxLayout>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <algorithm>
#include <cmath>

namespace ComAssistant {

RealTimeFFTWindow::RealTimeFFTWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_timePlot(nullptr)
    , m_freqPlot(nullptr)
    , m_infoLabel(nullptr)
    , m_updateTimer(nullptr)
{
    setWindowTitle(tr("实时 FFT 分析"));
    resize(1000, 800);

    setupUi();
    setupToolBar();
    setupPlots();

    // 初始化定时器
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &RealTimeFFTWindow::onUpdateTimer);
    m_updateTimer->start(m_updateInterval);

    // 初始化默认配置
    m_fftConfig.sampleRate = 1000.0;
    m_fftConfig.fftSize = 1024;
    m_fftConfig.windowType = WindowType::Hanning;

    LOG_INFO("RealTimeFFTWindow created");
}

RealTimeFFTWindow::~RealTimeFFTWindow()
{
    LOG_INFO("RealTimeFFTWindow destroyed");
}

void RealTimeFFTWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    // 创建分隔器
    QSplitter* splitter = new QSplitter(Qt::Vertical);

    // 时域图
    m_timePlot = new QCustomPlot;
    splitter->addWidget(m_timePlot);

    // 频域图
    m_freqPlot = new QCustomPlot;
    splitter->addWidget(m_freqPlot);

    // 瀑布图（默认隐藏）
    m_waterfall = new WaterfallWidget;
    m_waterfall->setVisible(false);
    splitter->addWidget(m_waterfall);

    // 设置初始分隔比例
    splitter->setSizes({300, 300, 200});

    mainLayout->addWidget(splitter);

    // 信息标签
    m_infoLabel = new QLabel;
    m_infoLabel->setStyleSheet(
        "QLabel { background-color: #2c3e50; color: #ecf0f1; "
        "padding: 5px; font-family: Consolas; font-size: 11px; }");
    mainLayout->addWidget(m_infoLabel);

    setCentralWidget(centralWidget);
}

void RealTimeFFTWindow::setupToolBar()
{
    QToolBar* toolBar = addToolBar(tr("FFT工具栏"));
    toolBar->setMovable(false);

    // FFT点数选择
    m_fftSizeLabel = new QLabel(tr(" FFT点数: "));
    toolBar->addWidget(m_fftSizeLabel);
    m_fftSizeCombo = new QComboBox;
    m_fftSizeCombo->addItem("256", 256);
    m_fftSizeCombo->addItem("512", 512);
    m_fftSizeCombo->addItem("1024", 1024);
    m_fftSizeCombo->addItem("2048", 2048);
    m_fftSizeCombo->addItem("4096", 4096);
    m_fftSizeCombo->addItem("8192", 8192);
    m_fftSizeCombo->setCurrentIndex(2);  // 默认1024
    connect(m_fftSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RealTimeFFTWindow::onFFTSizeChanged);
    toolBar->addWidget(m_fftSizeCombo);

    toolBar->addSeparator();

    // 窗口函数选择
    m_windowLabel = new QLabel(tr(" 窗口: "));
    toolBar->addWidget(m_windowLabel);
    m_windowTypeCombo = new QComboBox;
    m_windowTypeCombo->addItem(tr("矩形"), static_cast<int>(WindowType::Rectangular));
    m_windowTypeCombo->addItem(tr("汉宁"), static_cast<int>(WindowType::Hanning));
    m_windowTypeCombo->addItem(tr("汉明"), static_cast<int>(WindowType::Hamming));
    m_windowTypeCombo->addItem(tr("布莱克曼"), static_cast<int>(WindowType::Blackman));
    m_windowTypeCombo->addItem(tr("凯泽"), static_cast<int>(WindowType::Kaiser));
    m_windowTypeCombo->addItem(tr("平顶"), static_cast<int>(WindowType::FlatTop));
    m_windowTypeCombo->setCurrentIndex(1);  // 默认汉宁
    connect(m_windowTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RealTimeFFTWindow::onWindowTypeChanged);
    toolBar->addWidget(m_windowTypeCombo);

    toolBar->addSeparator();

    // 采样率
    m_sampleRateLabel = new QLabel(tr(" 采样率: "));
    toolBar->addWidget(m_sampleRateLabel);
    m_sampleRateSpin = new QDoubleSpinBox;
    m_sampleRateSpin->setRange(1, 10000000);
    m_sampleRateSpin->setValue(1000);
    m_sampleRateSpin->setSuffix(" Hz");
    m_sampleRateSpin->setDecimals(0);
    connect(m_sampleRateSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RealTimeFFTWindow::onSampleRateChanged);
    toolBar->addWidget(m_sampleRateSpin);

    toolBar->addSeparator();

    // 暂停按钮
    m_pauseAction = toolBar->addAction(tr("暂停"));
    m_pauseAction->setCheckable(true);
    connect(m_pauseAction, &QAction::toggled, this, &RealTimeFFTWindow::onPauseToggled);

    // 清空按钮
    m_clearAction = toolBar->addAction(tr("清空"), this, &RealTimeFFTWindow::onClearClicked);

    toolBar->addSeparator();

    // 显示选项
    m_logScaleCheck = new QCheckBox(tr("对数坐标"));
    m_logScaleCheck->setChecked(true);
    connect(m_logScaleCheck, &QCheckBox::toggled, this, &RealTimeFFTWindow::onLogScaleToggled);
    toolBar->addWidget(m_logScaleCheck);

    m_maxHoldCheck = new QCheckBox(tr("峰值保持"));
    connect(m_maxHoldCheck, &QCheckBox::toggled, this, &RealTimeFFTWindow::onMaxHoldToggled);
    toolBar->addWidget(m_maxHoldCheck);

    m_averagingCheck = new QCheckBox(tr("频谱平均"));
    connect(m_averagingCheck, &QCheckBox::toggled, this, &RealTimeFFTWindow::onAveragingToggled);
    toolBar->addWidget(m_averagingCheck);

    m_peakDetectionCheck = new QCheckBox(tr("峰值检测"));
    m_peakDetectionCheck->setChecked(true);
    connect(m_peakDetectionCheck, &QCheckBox::toggled, this, &RealTimeFFTWindow::onPeakDetectionToggled);
    toolBar->addWidget(m_peakDetectionCheck);

    m_waterfallCheck = new QCheckBox(tr("瀑布图"));
    connect(m_waterfallCheck, &QCheckBox::toggled, this, &RealTimeFFTWindow::onWaterfallToggled);
    toolBar->addWidget(m_waterfallCheck);

    toolBar->addSeparator();

    // 导出按钮
    m_exportAction = toolBar->addAction(tr("导出..."), this, &RealTimeFFTWindow::onExportClicked);
}

void RealTimeFFTWindow::setupPlots()
{
    // ========== 时域图设置 ==========
    m_timePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_timePlot->xAxis->setLabel(tr("样本点"));
    m_timePlot->yAxis->setLabel(tr("幅度"));
    m_timePlot->xAxis->grid()->setVisible(true);
    m_timePlot->yAxis->grid()->setVisible(true);
    m_timePlot->legend->setVisible(false);

    // 时域曲线
    QCPGraph* timeGraph = m_timePlot->addGraph();
    timeGraph->setName(tr("时域信号"));
    timeGraph->setPen(QPen(QColor(31, 119, 180), 1.5));

    // 时域光标
    m_timeCursor = new QCPItemLine(m_timePlot);
    m_timeCursor->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    m_timeCursor->setVisible(false);

    m_timeCursorText = new QCPItemText(m_timePlot);
    m_timeCursorText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_timeCursorText->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_timeCursorText->position->setCoords(0.02, 0.02);
    m_timeCursorText->setFont(QFont("Consolas", 9));
    m_timeCursorText->setPadding(QMargins(5, 5, 5, 5));
    m_timeCursorText->setBrush(QBrush(QColor(255, 255, 255, 200)));
    m_timeCursorText->setVisible(false);

    m_timePlot->setMouseTracking(true);
    connect(m_timePlot, &QCustomPlot::mouseMove, this, &RealTimeFFTWindow::onTimeDomainMouseMove);

    // ========== 频域图设置 ==========
    m_freqPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_freqPlot->xAxis->setLabel(tr("频率 (Hz)"));
    m_freqPlot->yAxis->setLabel(tr("幅度 (dB)"));
    m_freqPlot->xAxis->grid()->setVisible(true);
    m_freqPlot->yAxis->grid()->setVisible(true);
    m_freqPlot->legend->setVisible(true);
    m_freqPlot->legend->setBrush(QBrush(QColor(255, 255, 255, 180)));

    // 当前频谱曲线
    QCPGraph* specGraph = m_freqPlot->addGraph();
    specGraph->setName(tr("频谱"));
    specGraph->setPen(QPen(QColor(31, 119, 180), 1.5));

    // 峰值保持曲线
    QCPGraph* maxHoldGraph = m_freqPlot->addGraph();
    maxHoldGraph->setName(tr("峰值保持"));
    maxHoldGraph->setPen(QPen(QColor(255, 127, 14), 1));
    maxHoldGraph->setVisible(false);

    // 频域光标
    m_freqCursor = new QCPItemLine(m_freqPlot);
    m_freqCursor->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    m_freqCursor->setVisible(false);

    m_freqCursorText = new QCPItemText(m_freqPlot);
    m_freqCursorText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_freqCursorText->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_freqCursorText->position->setCoords(0.02, 0.02);
    m_freqCursorText->setFont(QFont("Consolas", 9));
    m_freqCursorText->setPadding(QMargins(5, 5, 5, 5));
    m_freqCursorText->setBrush(QBrush(QColor(255, 255, 255, 200)));
    m_freqCursorText->setVisible(false);

    m_freqPlot->setMouseTracking(true);
    connect(m_freqPlot, &QCustomPlot::mouseMove, this, &RealTimeFFTWindow::onFreqDomainMouseMove);

    // 初始范围
    m_timePlot->xAxis->setRange(0, m_bufferSize);
    m_timePlot->yAxis->setRange(-1, 1);
    m_freqPlot->xAxis->setRange(0, m_fftConfig.sampleRate / 2);
    m_freqPlot->yAxis->setRange(-100, 0);
}

void RealTimeFFTWindow::setFFTConfig(const FFTConfig& config)
{
    m_fftConfig = config;

    // 更新UI控件
    int sizeIndex = m_fftSizeCombo->findData(config.fftSize);
    if (sizeIndex >= 0) m_fftSizeCombo->setCurrentIndex(sizeIndex);

    int windowIndex = m_windowTypeCombo->findData(static_cast<int>(config.windowType));
    if (windowIndex >= 0) m_windowTypeCombo->setCurrentIndex(windowIndex);

    m_sampleRateSpin->setValue(config.sampleRate);

    // 更新缓冲区大小
    m_bufferSize = config.fftSize > 0 ? config.fftSize : 1024;
    m_timePlot->xAxis->setRange(0, m_bufferSize);
    m_freqPlot->xAxis->setRange(0, config.sampleRate / 2);

    // 更新瀑布图频率范围
    m_waterfall->setFrequencyRange(0, config.sampleRate / 2);
}

void RealTimeFFTWindow::appendData(double value)
{
    if (m_paused) return;
    if (!std::isfinite(value)) {
        return;
    }

    m_timeData.append(value);

    // 限制缓冲区大小
    const int overflow = m_timeData.size() - m_bufferSize;
    if (overflow > 0) {
        m_timeData.remove(0, overflow);
    }

    m_needUpdate = true;
}

void RealTimeFFTWindow::appendData(const QVector<double>& values)
{
    if (m_paused) return;

    /*
     * 实时 FFT 来自绘图曲线，仍需过滤 NaN/Inf，防止异常采样污染频谱。
     * 批量追加后一次性裁剪旧数据，避免 removeFirst 循环造成 O(n²)。
     */
    const int oldSize = m_timeData.size();
    for (double value : values) {
        if (std::isfinite(value)) {
            m_timeData.append(value);
        }
    }

    if (m_timeData.size() == oldSize) {
        return;
    }

    // 限制缓冲区大小
    const int overflow = m_timeData.size() - m_bufferSize;
    if (overflow > 0) {
        m_timeData.remove(0, overflow);
    }

    m_needUpdate = true;
}

void RealTimeFFTWindow::clearData()
{
    m_timeData.clear();
    m_maxHoldData.clear();
    m_avgBuffer.clear();

    m_timePlot->graph(0)->data()->clear();
    m_freqPlot->graph(0)->data()->clear();
    m_freqPlot->graph(1)->data()->clear();

    // 清除峰值标记
    for (int i = m_freqPlot->itemCount() - 1; i >= 0; --i) {
        QCPAbstractItem* item = m_freqPlot->item(i);
        if (item != m_freqCursor && item != m_freqCursorText) {
            m_freqPlot->removeItem(item);
        }
    }

    // 清除瀑布图
    m_waterfall->clearData();

    m_timePlot->replot(QCustomPlot::rpQueuedReplot);
    m_freqPlot->replot(QCustomPlot::rpQueuedReplot);

    updateInfoLabel();
}

void RealTimeFFTWindow::setSampleRate(double rate)
{
    /*
     * 采样率可能来自外部配置，必须防止 0、负数或无穷进入坐标轴。
     */
    const double safeRate = (std::isfinite(rate) && rate > 0.0) ? rate : 1000.0;
    m_fftConfig.sampleRate = safeRate;
    m_sampleRateSpin->setValue(safeRate);
    m_freqPlot->xAxis->setRange(0, safeRate / 2);
}

void RealTimeFFTWindow::setFFTSize(int size)
{
    const int safeSize = qBound(256, FFTUtils::nextPowerOfTwo(size), 8192);
    m_fftConfig.fftSize = safeSize;
    m_bufferSize = safeSize;

    int index = m_fftSizeCombo->findData(safeSize);
    if (index >= 0) m_fftSizeCombo->setCurrentIndex(index);

    m_timePlot->xAxis->setRange(0, m_bufferSize);
}

void RealTimeFFTWindow::setMaxHoldEnabled(bool enabled)
{
    m_maxHoldEnabled = enabled;
    m_maxHoldCheck->setChecked(enabled);
    m_freqPlot->graph(1)->setVisible(enabled);

    if (!enabled) {
        m_maxHoldData.clear();
        m_freqPlot->graph(1)->data()->clear();
    }
}

void RealTimeFFTWindow::setAveragingEnabled(bool enabled)
{
    m_averagingEnabled = enabled;
    m_averagingCheck->setChecked(enabled);

    if (!enabled) {
        m_avgBuffer.clear();
    }
}

void RealTimeFFTWindow::setPaused(bool paused)
{
    m_paused = paused;
    m_pauseAction->setChecked(paused);
}

void RealTimeFFTWindow::closeEvent(QCloseEvent* event)
{
    emit windowClosed();
    QMainWindow::closeEvent(event);
}

void RealTimeFFTWindow::onUpdateTimer()
{
    if (!m_needUpdate || m_paused) return;

    updateTimeDomainPlot();
    performFFT();
    updateFrequencyDomainPlot();
    updateWaterfall();
    updateInfoLabel();

    m_needUpdate = false;
}

void RealTimeFFTWindow::onFFTSizeChanged(int index)
{
    int size = m_fftSizeCombo->itemData(index).toInt();
    const int safeSize = qBound(256, FFTUtils::nextPowerOfTwo(size), 8192);
    m_fftConfig.fftSize = safeSize;
    m_bufferSize = safeSize;
    m_timePlot->xAxis->setRange(0, m_bufferSize);

    const int overflow = m_timeData.size() - m_bufferSize;
    if (overflow > 0) {
        m_timeData.remove(0, overflow);
    }

    // 清空峰值保持和平均缓冲
    m_maxHoldData.clear();
    m_avgBuffer.clear();
}

void RealTimeFFTWindow::onWindowTypeChanged(int index)
{
    m_fftConfig.windowType = static_cast<WindowType>(m_windowTypeCombo->itemData(index).toInt());
}

void RealTimeFFTWindow::onSampleRateChanged(double value)
{
    const double safeValue = (std::isfinite(value) && value > 0.0) ? value : 1000.0;
    m_fftConfig.sampleRate = safeValue;
    m_freqPlot->xAxis->setRange(0, safeValue / 2);
    m_waterfall->setFrequencyRange(0, safeValue / 2);
}

void RealTimeFFTWindow::onPauseToggled(bool checked)
{
    m_paused = checked;
    m_pauseAction->setText(checked ? tr("继续") : tr("暂停"));
}

void RealTimeFFTWindow::onClearClicked()
{
    clearData();
}

void RealTimeFFTWindow::onMaxHoldToggled(bool checked)
{
    m_maxHoldEnabled = checked;
    m_freqPlot->graph(1)->setVisible(checked);

    if (!checked) {
        m_maxHoldData.clear();
        m_freqPlot->graph(1)->data()->clear();
    }

    m_freqPlot->replot(QCustomPlot::rpQueuedReplot);
}

void RealTimeFFTWindow::onAveragingToggled(bool checked)
{
    m_averagingEnabled = checked;
    if (!checked) {
        m_avgBuffer.clear();
    }
}

void RealTimeFFTWindow::onLogScaleToggled(bool checked)
{
    m_logScale = checked;
    m_freqPlot->yAxis->setLabel(checked ? tr("幅度 (dB)") : tr("幅度"));

    if (checked) {
        m_freqPlot->yAxis->setRange(-100, 0);
    } else {
        m_freqPlot->yAxis->rescale();
    }
}

void RealTimeFFTWindow::onPeakDetectionToggled(bool checked)
{
    m_peakDetection = checked;
}

void RealTimeFFTWindow::onWaterfallToggled(bool checked)
{
    m_waterfallEnabled = checked;
    m_waterfall->setVisible(checked);

    if (checked) {
        // 设置瀑布图的频率范围和幅度范围
        m_waterfall->setFrequencyRange(0, m_fftConfig.sampleRate / 2);
        m_waterfall->setAmplitudeRange(-100, 0);  // 确保正确的dB范围
        m_waterfall->clearData();
    }
}

void RealTimeFFTWindow::onExportClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("导出频谱数据"),
        "spectrum_data.csv",
        tr("CSV文件 (*.csv);;所有文件 (*.*)"));

    if (filename.isEmpty()) return;

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法创建文件"));
        return;
    }

    QTextStream out(&file);
    out << "Frequency(Hz),Magnitude,Magnitude(dB),Phase(deg)\n";

    for (int i = 0; i < m_lastResult.frequencies.size(); ++i) {
        double db = (m_lastResult.magnitudes[i] > 1e-20) ?
            20 * std::log10(m_lastResult.magnitudes[i]) : -200;
        out << QString::number(m_lastResult.frequencies[i], 'f', 4) << ","
            << QString::number(m_lastResult.magnitudes[i], 'g', 6) << ","
            << QString::number(db, 'f', 2) << ","
            << QString::number(m_lastResult.phases[i], 'f', 2) << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("导出成功"), tr("频谱数据已保存到: %1").arg(filename));
}

void RealTimeFFTWindow::onTimeDomainMouseMove(QMouseEvent* event)
{
    double x = m_timePlot->xAxis->pixelToCoord(event->pos().x());
    double yMin = m_timePlot->yAxis->range().lower;
    double yMax = m_timePlot->yAxis->range().upper;

    m_timeCursor->start->setCoords(x, yMin);
    m_timeCursor->end->setCoords(x, yMax);
    m_timeCursor->setVisible(true);

    int index = static_cast<int>(x);
    if (index >= 0 && index < m_timeData.size()) {
        double value = m_timeData[index];
        m_timeCursorText->setText(QString("Sample: %1\nValue: %2").arg(index).arg(value, 0, 'f', 4));
        m_timeCursorText->setVisible(true);
    } else {
        m_timeCursorText->setVisible(false);
    }

    m_timePlot->replot(QCustomPlot::rpQueuedReplot);
}

void RealTimeFFTWindow::onFreqDomainMouseMove(QMouseEvent* event)
{
    double freq = m_freqPlot->xAxis->pixelToCoord(event->pos().x());
    double yMin = m_freqPlot->yAxis->range().lower;
    double yMax = m_freqPlot->yAxis->range().upper;

    m_freqCursor->start->setCoords(freq, yMin);
    m_freqCursor->end->setCoords(freq, yMax);
    m_freqCursor->setVisible(true);

    // 查找最近的频率点
    if (!m_lastResult.frequencies.isEmpty()) {
        int nearestIdx = 0;
        double minDist = std::abs(m_lastResult.frequencies[0] - freq);
        for (int i = 1; i < m_lastResult.frequencies.size(); ++i) {
            double dist = std::abs(m_lastResult.frequencies[i] - freq);
            if (dist < minDist) {
                minDist = dist;
                nearestIdx = i;
            }
        }

        double mag = m_lastResult.magnitudes[nearestIdx];
        double db = (mag > 1e-20) ? 20 * std::log10(mag) : -200;
        m_freqCursorText->setText(QString("Freq: %1 Hz\nMag: %2 dB")
            .arg(m_lastResult.frequencies[nearestIdx], 0, 'f', 2)
            .arg(db, 0, 'f', 1));
        m_freqCursorText->setVisible(true);
    }

    m_freqPlot->replot(QCustomPlot::rpQueuedReplot);
}

void RealTimeFFTWindow::updateTimeDomainPlot()
{
    if (m_timeData.isEmpty()) return;

    QVector<double> xData(m_timeData.size());
    for (int i = 0; i < m_timeData.size(); ++i) {
        xData[i] = i;
    }

    m_timePlot->graph(0)->setData(xData, m_timeData);
    m_timePlot->rescaleAxes();
    // 周期刷新路径使用队列重绘，让 Qt 合并同一事件循环内的多次 repaint。
    m_timePlot->replot(QCustomPlot::rpQueuedReplot);
}

void RealTimeFFTWindow::performFFT()
{
    if (m_timeData.size() < 8) return;

    // 执行FFT
    m_lastResult = FFTUtils::analyzeWithConfig(m_timeData, m_fftConfig);
    if (m_lastResult.frequencies.isEmpty() || m_lastResult.magnitudes.isEmpty()) {
        return;
    }

    // 频谱平均
    if (m_averagingEnabled) {
        m_avgBuffer.append(m_lastResult);
        const int overflow = m_avgBuffer.size() - m_avgCount;
        if (overflow > 0) {
            // 平均窗口只保留最近 N 帧，一次性裁剪旧帧避免高频路径重复搬移。
            m_avgBuffer.remove(0, overflow);
        }

        if (m_avgBuffer.size() > 1) {
            m_lastResult = FFTUtils::averageSpectrum(m_avgBuffer);
        }
    }

    // 峰值保持
    if (m_maxHoldEnabled) {
        FFTUtils::updateMaxHold(m_lastResult, m_maxHoldData);
    }

    // 峰值检测
    if (m_peakDetection) {
        m_lastResult.peaks = FFTUtils::detectPeaks(m_lastResult, 5);
    }
}

void RealTimeFFTWindow::updateFrequencyDomainPlot()
{
    if (m_lastResult.frequencies.isEmpty()) return;

    // 准备显示数据
    QVector<double> displayData;
    if (m_logScale) {
        // dB刻度
        double maxMag = *std::max_element(m_lastResult.magnitudes.begin(), m_lastResult.magnitudes.end());
        if (maxMag < 1e-20) maxMag = 1e-20;

        for (double mag : m_lastResult.magnitudes) {
            double db = (mag > 1e-20) ? 20 * std::log10(mag / maxMag) : -200;
            displayData.append(qBound(-100.0, db, 0.0));
        }
    } else {
        displayData = m_lastResult.magnitudes;
    }

    m_freqPlot->graph(0)->setData(m_lastResult.frequencies, displayData);

    // 峰值保持曲线
    if (m_maxHoldEnabled && !m_maxHoldData.isEmpty()) {
        QVector<double> maxHoldDisplay;
        if (m_logScale) {
            double maxMag = *std::max_element(m_maxHoldData.begin(), m_maxHoldData.end());
            if (maxMag < 1e-20) maxMag = 1e-20;

            for (double mag : m_maxHoldData) {
                double db = (mag > 1e-20) ? 20 * std::log10(mag / maxMag) : -200;
                maxHoldDisplay.append(qBound(-100.0, db, 0.0));
            }
        } else {
            maxHoldDisplay = m_maxHoldData;
        }
        m_freqPlot->graph(1)->setData(m_lastResult.frequencies, maxHoldDisplay);
    }

    // 清除旧的峰值标记
    for (int i = m_freqPlot->itemCount() - 1; i >= 0; --i) {
        QCPAbstractItem* item = m_freqPlot->item(i);
        if (item != m_freqCursor && item != m_freqCursorText) {
            m_freqPlot->removeItem(item);
        }
    }

    // 添加峰值标记
    if (m_peakDetection && !m_lastResult.peaks.isEmpty()) {
        for (const auto& peak : m_lastResult.peaks) {
            if (peak.index < displayData.size()) {
                double y = displayData[peak.index];

                // 峰值点
                QCPItemTracer* tracer = new QCPItemTracer(m_freqPlot);
                tracer->setGraph(m_freqPlot->graph(0));
                tracer->setGraphKey(peak.frequency);
                tracer->setInterpolating(true);
                tracer->setStyle(QCPItemTracer::tsCircle);
                tracer->setPen(QPen(Qt::red));
                tracer->setBrush(Qt::red);
                tracer->setSize(6);

                // 峰值标签
                QCPItemText* label = new QCPItemText(m_freqPlot);
                label->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
                label->position->setCoords(peak.frequency, y + 3);
                label->setText(QString("%1 Hz").arg(peak.frequency, 0, 'f', 1));
                label->setFont(QFont("Sans", 8));
                label->setColor(Qt::red);
            }
        }
    }

    // 周期刷新路径使用队列重绘，保持频谱点和标注质量不变，仅合并 repaint。
    m_freqPlot->replot(QCustomPlot::rpQueuedReplot);
}

void RealTimeFFTWindow::updateWaterfall()
{
    if (!m_waterfallEnabled || m_lastResult.magnitudes.isEmpty()) return;

    // 将频谱数据添加到瀑布图
    m_waterfall->addMagnitudeLine(m_lastResult.magnitudes);
}

void RealTimeFFTWindow::updateInfoLabel()
{
    if (m_lastResult.frequencies.isEmpty()) {
        m_infoLabel->setText(tr("等待数据..."));
        return;
    }

    QString info = QString(tr("数据点: %1 | FFT点数: %2 | 频率分辨率: %3 Hz | 主频: %4 Hz | THD: %5%"))
        .arg(m_timeData.size())
        .arg(m_lastResult.fftSize)
        .arg(m_lastResult.frequencyResolution, 0, 'f', 3)
        .arg(m_lastResult.dominantFrequency, 0, 'f', 2)
        .arg(m_lastResult.thd, 0, 'f', 2);

    if (!m_lastResult.peaks.isEmpty()) {
        info += QString(tr(" | 峰值: %1")).arg(m_lastResult.peaks.size());
    }

    m_infoLabel->setText(info);
}

void RealTimeFFTWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void RealTimeFFTWindow::retranslateUi()
{
    setWindowTitle(tr("实时 FFT 分析"));

    // 工具栏标签
    if (m_fftSizeLabel) m_fftSizeLabel->setText(tr(" FFT点数: "));
    if (m_windowLabel) m_windowLabel->setText(tr(" 窗口: "));
    if (m_sampleRateLabel) m_sampleRateLabel->setText(tr(" 采样率: "));

    // 窗口函数选项
    if (m_windowTypeCombo) {
        m_windowTypeCombo->setItemText(0, tr("矩形"));
        m_windowTypeCombo->setItemText(1, tr("汉宁"));
        m_windowTypeCombo->setItemText(2, tr("汉明"));
        m_windowTypeCombo->setItemText(3, tr("布莱克曼"));
        m_windowTypeCombo->setItemText(4, tr("凯泽"));
        m_windowTypeCombo->setItemText(5, tr("平顶"));
    }

    // 暂停/继续按钮
    if (m_pauseAction) {
        m_pauseAction->setText(m_paused ? tr("继续") : tr("暂停"));
    }

    // 清空和导出按钮
    if (m_clearAction) m_clearAction->setText(tr("清空"));
    if (m_exportAction) m_exportAction->setText(tr("导出..."));

    // 复选框
    if (m_logScaleCheck) m_logScaleCheck->setText(tr("对数坐标"));
    if (m_maxHoldCheck) m_maxHoldCheck->setText(tr("峰值保持"));
    if (m_averagingCheck) m_averagingCheck->setText(tr("频谱平均"));
    if (m_peakDetectionCheck) m_peakDetectionCheck->setText(tr("峰值检测"));
    if (m_waterfallCheck) m_waterfallCheck->setText(tr("瀑布图"));

    // 图表轴标签
    if (m_timePlot) {
        m_timePlot->xAxis->setLabel(tr("样本点"));
        m_timePlot->yAxis->setLabel(tr("幅度"));
        if (m_timePlot->graphCount() > 0) {
            m_timePlot->graph(0)->setName(tr("时域信号"));
        }
        m_timePlot->replot(QCustomPlot::rpQueuedReplot);
    }

    if (m_freqPlot) {
        m_freqPlot->xAxis->setLabel(tr("频率 (Hz)"));
        m_freqPlot->yAxis->setLabel(m_logScale ? tr("幅度 (dB)") : tr("幅度"));
        if (m_freqPlot->graphCount() > 0) {
            m_freqPlot->graph(0)->setName(tr("频谱"));
        }
        if (m_freqPlot->graphCount() > 1) {
            m_freqPlot->graph(1)->setName(tr("峰值保持"));
        }
        m_freqPlot->replot(QCustomPlot::rpQueuedReplot);
    }

    // 更新信息标签
    updateInfoLabel();
}

} // namespace ComAssistant
