/**
 * @file SpectrumWindow.cpp
 * @brief 频谱分析窗口实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "SpectrumWindow.h"
#include "core/utils/Logger.h"

#include <QToolBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>

namespace ComAssistant {

SpectrumWindow::SpectrumWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_magnitudePlot(nullptr)
    , m_phasePlot(nullptr)
    , m_powerPlot(nullptr)
    , m_infoPanel(nullptr)
    , m_harmonicsTable(nullptr)
{
    setupUi();
    setupToolBar();

    setWindowTitle(tr("FFT 频谱分析"));
    resize(1000, 700);

    LOG_INFO("SpectrumWindow created");
}

SpectrumWindow::~SpectrumWindow()
{
    LOG_INFO("SpectrumWindow destroyed");
}

void SpectrumWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(5, 5, 5, 5);

    m_tabWidget = new QTabWidget;
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &SpectrumWindow::onTabChanged);

    // 幅度谱标签页
    m_magnitudePlot = new QCustomPlot;
    m_magnitudePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_magnitudePlot->xAxis->setLabel(tr("频率 (Hz)"));
    m_magnitudePlot->yAxis->setLabel(tr("幅度"));
    m_magnitudePlot->legend->setVisible(true);
    m_magnitudePlot->xAxis->grid()->setVisible(true);
    m_magnitudePlot->yAxis->grid()->setVisible(true);
    m_tabWidget->addTab(m_magnitudePlot, tr("幅度谱"));

    // 相位谱标签页
    m_phasePlot = new QCustomPlot;
    m_phasePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_phasePlot->xAxis->setLabel(tr("频率 (Hz)"));
    m_phasePlot->yAxis->setLabel(tr("相位 (°)"));
    m_phasePlot->legend->setVisible(true);
    m_phasePlot->xAxis->grid()->setVisible(true);
    m_phasePlot->yAxis->grid()->setVisible(true);
    m_tabWidget->addTab(m_phasePlot, tr("相位谱"));

    // 功率谱标签页
    m_powerPlot = new QCustomPlot;
    m_powerPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    m_powerPlot->xAxis->setLabel(tr("频率 (Hz)"));
    m_powerPlot->yAxis->setLabel(tr("功率 (dB)"));
    m_powerPlot->legend->setVisible(true);
    m_powerPlot->xAxis->grid()->setVisible(true);
    m_powerPlot->yAxis->grid()->setVisible(true);
    m_tabWidget->addTab(m_powerPlot, tr("功率谱"));

    // 谐波分析标签页
    m_harmonicsTable = new QTableWidget;
    m_harmonicsTable->setColumnCount(5);
    m_harmonicsTable->setHorizontalHeaderLabels({
        tr("谐波次数"), tr("频率 (Hz)"), tr("幅度"), tr("相对幅度 (%)"), tr("相位 (°)")
    });
    m_harmonicsTable->horizontalHeader()->setStretchLastSection(true);
    m_harmonicsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_harmonicsTable->setAlternatingRowColors(true);
    m_harmonicsTable->setStyleSheet(
        "QTableWidget { background-color: #1c2833; color: #d5d8dc; gridline-color: #566573; }"
        "QTableWidget::item { padding: 5px; }"
        "QHeaderView::section { background-color: #2874a6; color: white; padding: 8px; }"
    );
    m_tabWidget->addTab(m_harmonicsTable, tr("谐波分析"));

    // 分析信息标签页
    m_infoPanel = new QTextEdit;
    m_infoPanel->setReadOnly(true);
    m_infoPanel->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1c2833;"
        "  color: #e0e0e0;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  padding: 10px;"
        "}"
    );
    m_tabWidget->addTab(m_infoPanel, tr("分析结果"));

    layout->addWidget(m_tabWidget);
    setCentralWidget(centralWidget);
}

void SpectrumWindow::setupToolBar()
{
    QToolBar* toolBar = addToolBar(tr("频谱工具栏"));
    toolBar->setMovable(false);

    toolBar->addAction(tr("导出图片"), this, &SpectrumWindow::onExportClicked);
    toolBar->addAction(tr("自动缩放"), this, [this]() {
        if (m_tabWidget->currentIndex() == 0) {
            m_magnitudePlot->rescaleAxes();
            m_magnitudePlot->replot();
        } else if (m_tabWidget->currentIndex() == 1) {
            m_phasePlot->rescaleAxes();
            m_phasePlot->replot();
        } else if (m_tabWidget->currentIndex() == 2) {
            m_powerPlot->rescaleAxes();
            m_powerPlot->replot();
        }
    });
}

void SpectrumWindow::setFFTResult(const FFTResult& result, const QString& curveName)
{
    m_fftResult = result;
    m_curveName = curveName;

    updateMagnitudePlot();
    updatePhasePlot();
    updatePowerPlot();
    updateHarmonicsTable();
    updateInfoPanel();

    setWindowTitle(tr("FFT 频谱分析 - %1 [%2]").arg(curveName).arg(result.waveformName));
}

void SpectrumWindow::updateMagnitudePlot()
{
    m_magnitudePlot->clearGraphs();
    m_magnitudePlot->clearItems();

    if (m_fftResult.frequencies.isEmpty()) return;

    QCPGraph* graph = m_magnitudePlot->addGraph();
    graph->setName(m_curveName);
    graph->setPen(QPen(QColor(31, 119, 180), 1.5));
    graph->setData(m_fftResult.frequencies, m_fftResult.magnitudes);

    // 标记主频率
    if (m_fftResult.dominantFrequency > 0) {
        QCPItemLine* line = new QCPItemLine(m_magnitudePlot);
        line->start->setCoords(m_fftResult.dominantFrequency, 0);
        line->end->setCoords(m_fftResult.dominantFrequency, m_fftResult.dominantMagnitude);
        line->setPen(QPen(Qt::red, 1, Qt::DashLine));

        QCPItemText* label = new QCPItemText(m_magnitudePlot);
        label->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
        label->position->setCoords(m_fftResult.dominantFrequency, m_fftResult.dominantMagnitude * 1.1);
        label->setText(QString("%1 Hz\n%2").arg(m_fftResult.dominantFrequency, 0, 'f', 2)
            .arg(m_fftResult.waveformName));
        label->setColor(Qt::red);
        label->setFont(QFont("Sans", 9, QFont::Bold));
    }

    // 标记谐波
    for (int i = 1; i < qMin(5, m_fftResult.harmonics.size()); ++i) {
        const auto& h = m_fftResult.harmonics[i];
        if (h.relativeMag > 5) {  // 只标记大于5%的谐波
            QCPItemLine* hLine = new QCPItemLine(m_magnitudePlot);
            hLine->start->setCoords(h.frequency, 0);
            hLine->end->setCoords(h.frequency, h.magnitude);
            hLine->setPen(QPen(QColor(255, 127, 14), 1, Qt::DotLine));
        }
    }

    m_magnitudePlot->rescaleAxes();
    m_magnitudePlot->replot();
}

void SpectrumWindow::updatePhasePlot()
{
    m_phasePlot->clearGraphs();

    if (m_fftResult.frequencies.isEmpty()) return;

    QCPGraph* graph = m_phasePlot->addGraph();
    graph->setName(m_curveName);
    graph->setPen(QPen(QColor(44, 160, 44), 1.5));
    graph->setData(m_fftResult.frequencies, m_fftResult.phases);

    m_phasePlot->rescaleAxes();
    m_phasePlot->yAxis->setRange(-180, 180);
    m_phasePlot->replot();
}

void SpectrumWindow::updatePowerPlot()
{
    m_powerPlot->clearGraphs();

    if (m_fftResult.frequencies.isEmpty()) return;

    // 转换为dB刻度
    QVector<double> powerDB;
    double maxPower = *std::max_element(m_fftResult.powerSpectrum.begin(), m_fftResult.powerSpectrum.end());
    if (maxPower < 1e-20) maxPower = 1e-20;

    for (double p : m_fftResult.powerSpectrum) {
        double db = (p > 1e-20) ? 10 * std::log10(p / maxPower) : -100;
        powerDB.append(db);
    }

    QCPGraph* graph = m_powerPlot->addGraph();
    graph->setName(m_curveName);
    graph->setPen(QPen(QColor(255, 127, 14), 1.5));
    graph->setData(m_fftResult.frequencies, powerDB);

    m_powerPlot->rescaleAxes();
    m_powerPlot->yAxis->setRange(-80, 5);
    m_powerPlot->replot();
}

void SpectrumWindow::updateHarmonicsTable()
{
    m_harmonicsTable->setRowCount(0);

    for (const auto& h : m_fftResult.harmonics) {
        int row = m_harmonicsTable->rowCount();
        m_harmonicsTable->insertRow(row);

        QString orderStr = (h.order == 1) ? tr("基波 (1次)") : tr("%1次").arg(h.order);

        m_harmonicsTable->setItem(row, 0, new QTableWidgetItem(orderStr));
        m_harmonicsTable->setItem(row, 1, new QTableWidgetItem(QString::number(h.frequency, 'f', 4)));
        m_harmonicsTable->setItem(row, 2, new QTableWidgetItem(QString::number(h.magnitude, 'f', 6)));
        m_harmonicsTable->setItem(row, 3, new QTableWidgetItem(QString::number(h.relativeMag, 'f', 2)));
        m_harmonicsTable->setItem(row, 4, new QTableWidgetItem(QString::number(h.phase, 'f', 2)));

        // 基波行高亮
        if (h.order == 1) {
            for (int col = 0; col < 5; ++col) {
                m_harmonicsTable->item(row, col)->setBackground(QColor(39, 174, 96, 100));
            }
        }
    }
}

void SpectrumWindow::updateInfoPanel()
{
    QString info;

    // 波形类型（大标题）
    info += QString("<div style='text-align:center; margin-bottom:20px;'>");
    info += QString("<h1 style='color:#f39c12; font-size:28px;'>%1</h1>").arg(m_fftResult.waveformName);
    info += QString("</div>");

    info += QString("<hr style='border-color:#3498db;'>");

    // 基本信息
    info += QString("<h2 style='color:#5dade2;'>%1</h2>").arg(tr("基本信息"));
    info += QString("<table style='color:#d5d8dc; margin-left:20px; font-size:14px;'>");
    info += QString("<tr><td style='padding:5px; width:150px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("曲线名称")).arg(m_curveName);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("数据点数")).arg(m_fftResult.validPoints);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2 Hz</td></tr>")
        .arg(tr("采样率")).arg(m_sampleRate, 0, 'f', 2);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2 Hz</td></tr>")
        .arg(tr("频率分辨率")).arg(m_fftResult.frequencies.size() > 1 ?
            m_fftResult.frequencies[1] - m_fftResult.frequencies[0] : 0, 0, 'f', 4);
    info += QString("</table>");

    // 频域特征
    info += QString("<h2 style='color:#5dade2;'>%1</h2>").arg(tr("频域特征"));
    info += QString("<table style='color:#d5d8dc; margin-left:20px; font-size:14px;'>");
    info += QString("<tr><td style='padding:5px; width:150px;'>%1:</td><td style='color:#f39c12; font-weight:bold; font-size:16px;'>%2 Hz</td></tr>")
        .arg(tr("基波频率")).arg(m_fftResult.fundamentalFreq, 0, 'f', 4);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#f39c12;'>%2 s (%3 ms)</td></tr>")
        .arg(tr("基波周期")).arg(m_fftResult.fundamentalPeriod, 0, 'f', 6)
        .arg(m_fftResult.fundamentalPeriod * 1000, 0, 'f', 3);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("基波幅度")).arg(m_fftResult.dominantMagnitude, 0, 'f', 6);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("直流分量 (DC)")).arg(m_fftResult.dcComponent, 0, 'f', 6);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("有效谐波数")).arg(m_fftResult.harmonicCount);
    info += QString("</table>");

    // 信号质量
    info += QString("<h2 style='color:#5dade2;'>%1</h2>").arg(tr("信号质量"));
    info += QString("<table style='color:#d5d8dc; margin-left:20px; font-size:14px;'>");

    // THD颜色：<5%绿色，5-10%黄色，>10%红色
    QString thdColor = (m_fftResult.thd < 5) ? "#58d68d" : (m_fftResult.thd < 10) ? "#f4d03f" : "#e74c3c";
    info += QString("<tr><td style='padding:5px; width:150px;'>%1:</td><td style='color:%3; font-weight:bold;'>%2 %%</td></tr>")
        .arg(tr("THD (总谐波失真)")).arg(m_fftResult.thd, 0, 'f', 2).arg(thdColor);

    // SNR颜色：>40dB绿色，20-40dB黄色，<20dB红色
    QString snrColor = (m_fftResult.snr > 40) ? "#58d68d" : (m_fftResult.snr > 20) ? "#f4d03f" : "#e74c3c";
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:%3; font-weight:bold;'>%2 dB</td></tr>")
        .arg(tr("SNR (信噪比)")).arg(m_fftResult.snr, 0, 'f', 2).arg(snrColor);

    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("总功率")).arg(m_fftResult.totalPower, 0, 'f', 6);
    info += QString("</table>");

    // 时域参数
    info += QString("<h2 style='color:#5dade2;'>%1</h2>").arg(tr("时域参数"));
    info += QString("<table style='color:#d5d8dc; margin-left:20px; font-size:14px;'>");
    info += QString("<tr><td style='padding:5px; width:150px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("峰峰值")).arg(m_fftResult.peakToPeak, 0, 'f', 4);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("最大值")).arg(m_fftResult.maxValue, 0, 'f', 4);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("最小值")).arg(m_fftResult.minValue, 0, 'f', 4);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("平均值")).arg(m_fftResult.avgValue, 0, 'f', 4);
    info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2</td></tr>")
        .arg(tr("有效值 (RMS)")).arg(m_fftResult.rmsValue, 0, 'f', 4);

    if (m_fftResult.waveformType == WaveformType::Square ||
        m_fftResult.waveformType == WaveformType::Pulse) {
        info += QString("<tr><td style='padding:5px;'>%1:</td><td style='color:#58d68d;'>%2 %%</td></tr>")
            .arg(tr("占空比")).arg(m_fftResult.dutyCycle, 0, 'f', 1);
    }
    info += QString("</table>");

    // 波形特征说明
    info += QString("<h2 style='color:#5dade2;'>%1</h2>").arg(tr("波形特征说明"));
    info += QString("<div style='color:#aab7b8; margin-left:20px; font-size:13px; line-height:1.6;'>");

    switch (m_fftResult.waveformType) {
    case WaveformType::Sine:
        info += tr("检测到<b style='color:#f39c12;'>正弦波</b>信号。"
                  "正弦波是最纯净的波形，只包含单一频率分量，THD接近0%。"
                  "该信号的谐波含量极低，表明是高质量的正弦波信号。");
        break;
    case WaveformType::Square:
        info += tr("检测到<b style='color:#f39c12;'>方波</b>信号。"
                  "方波只包含奇次谐波（1次、3次、5次...），幅度按1/n衰减。"
                  "理论THD约为48.3%。方波常用于数字信号和时钟信号。");
        break;
    case WaveformType::Triangle:
        info += tr("检测到<b style='color:#f39c12;'>三角波</b>信号。"
                  "三角波只包含奇次谐波，但幅度按1/n²快速衰减，因此THD较低（约12%）。"
                  "三角波常用于线性扫描和调制信号。");
        break;
    case WaveformType::Sawtooth:
        info += tr("检测到<b style='color:#f39c12;'>锯齿波</b>信号。"
                  "锯齿波包含所有谐波（奇次和偶次），幅度按1/n衰减。"
                  "锯齿波常用于扫描电路和合成器波形。");
        break;
    case WaveformType::Noise:
        info += tr("检测到<b style='color:#f39c12;'>噪声</b>信号。"
                  "信号没有明显的周期性特征，频谱较为平坦。"
                  "可能是随机噪声或复杂的非周期信号。");
        break;
    case WaveformType::DC:
        info += tr("检测到<b style='color:#f39c12;'>直流</b>信号。"
                  "信号主要是直流分量，没有明显的交流成分。");
        break;
    case WaveformType::Composite:
        info += tr("检测到<b style='color:#f39c12;'>复合波</b>信号。"
                  "信号包含多个频率分量，可能是多个信号叠加或复杂调制信号。"
                  "请参考谐波分析表查看详细的频率成分。");
        break;
    default:
        info += tr("无法确定波形类型，请检查信号质量或增加采样点数。");
        break;
    }

    info += QString("</div>");

    m_infoPanel->setHtml(info);
}

void SpectrumWindow::onExportClicked()
{
    QCustomPlot* currentPlot = nullptr;
    QString defaultName;

    switch (m_tabWidget->currentIndex()) {
    case 0:
        currentPlot = m_magnitudePlot;
        defaultName = "magnitude_spectrum.png";
        break;
    case 1:
        currentPlot = m_phasePlot;
        defaultName = "phase_spectrum.png";
        break;
    case 2:
        currentPlot = m_powerPlot;
        defaultName = "power_spectrum.png";
        break;
    default:
        QMessageBox::information(this, tr("提示"), tr("请选择频谱图标签页"));
        return;
    }

    QString filename = QFileDialog::getSaveFileName(this,
        tr("保存频谱图"),
        defaultName,
        tr("PNG图片 (*.png);;JPEG图片 (*.jpg);;PDF文档 (*.pdf)"));

    if (!filename.isEmpty()) {
        bool success = false;
        QString ext = QFileInfo(filename).suffix().toLower();

        if (ext == "png") {
            success = currentPlot->savePng(filename, 0, 0, 2.0);
        } else if (ext == "jpg" || ext == "jpeg") {
            success = currentPlot->saveJpg(filename, 0, 0, 2.0);
        } else if (ext == "pdf") {
            success = currentPlot->savePdf(filename);
        } else {
            success = currentPlot->savePng(filename + ".png", 0, 0, 2.0);
        }

        if (success) {
            QMessageBox::information(this, tr("导出成功"), tr("频谱图已保存到: %1").arg(filename));
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法保存频谱图"));
        }
    }
}

void SpectrumWindow::onTabChanged(int index)
{
    Q_UNUSED(index);
}

void SpectrumWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void SpectrumWindow::retranslateUi()
{
    setWindowTitle(tr("FFT 频谱分析 - %1 [%2]").arg(m_curveName).arg(m_fftResult.waveformName));

    // 标签页
    if (m_tabWidget) {
        m_tabWidget->setTabText(0, tr("幅度谱"));
        m_tabWidget->setTabText(1, tr("相位谱"));
        m_tabWidget->setTabText(2, tr("功率谱"));
        m_tabWidget->setTabText(3, tr("谐波分析"));
        m_tabWidget->setTabText(4, tr("分析结果"));
    }

    // 图表轴标签
    if (m_magnitudePlot) {
        m_magnitudePlot->xAxis->setLabel(tr("频率 (Hz)"));
        m_magnitudePlot->yAxis->setLabel(tr("幅度"));
        m_magnitudePlot->replot();
    }
    if (m_phasePlot) {
        m_phasePlot->xAxis->setLabel(tr("频率 (Hz)"));
        m_phasePlot->yAxis->setLabel(tr("相位 (°)"));
        m_phasePlot->replot();
    }
    if (m_powerPlot) {
        m_powerPlot->xAxis->setLabel(tr("频率 (Hz)"));
        m_powerPlot->yAxis->setLabel(tr("功率 (dB)"));
        m_powerPlot->replot();
    }

    // 谐波表头
    if (m_harmonicsTable) {
        m_harmonicsTable->setHorizontalHeaderLabels({
            tr("谐波次数"), tr("频率 (Hz)"), tr("幅度"), tr("相对幅度 (%)"), tr("相位 (°)")
        });
    }

    // 重新更新信息面板以应用翻译
    updateInfoPanel();
}

} // namespace ComAssistant
