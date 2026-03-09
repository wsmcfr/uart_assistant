/**
 * @file SpectrumWindow.h
 * @brief 频谱分析窗口
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef SPECTRUMWINDOW_H
#define SPECTRUMWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QTableWidget>
#include <QEvent>
#include "qcustomplot/qcustomplot.h"
#include "core/utils/FFTUtils.h"

namespace ComAssistant {

/**
 * @brief 频谱分析窗口类
 *
 * 显示 FFT 频谱分析结果，包括幅度谱、相位谱、功率谱、谐波分析
 */
class SpectrumWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SpectrumWindow(QWidget* parent = nullptr);
    ~SpectrumWindow() override;

    /**
     * @brief 设置 FFT 分析结果并显示
     * @param result FFT 分析结果
     * @param curveName 曲线名称
     */
    void setFFTResult(const FFTResult& result, const QString& curveName);

    /**
     * @brief 设置采样率
     * @param rate 采样率 (Hz)
     */
    void setSampleRate(double rate) { m_sampleRate = rate; }

private slots:
    void onExportClicked();
    void onTabChanged(int index);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupToolBar();
    void retranslateUi();
    void updateMagnitudePlot();
    void updatePhasePlot();
    void updatePowerPlot();
    void updateHarmonicsTable();
    void updateInfoPanel();

private:
    QTabWidget* m_tabWidget;
    QCustomPlot* m_magnitudePlot;    ///< 幅度谱
    QCustomPlot* m_phasePlot;        ///< 相位谱
    QCustomPlot* m_powerPlot;        ///< 功率谱
    QTableWidget* m_harmonicsTable;  ///< 谐波分析表
    QTextEdit* m_infoPanel;          ///< 信息面板

    FFTResult m_fftResult;
    QString m_curveName;
    double m_sampleRate = 1000;      ///< 默认采样率
};

} // namespace ComAssistant

#endif // SPECTRUMWINDOW_H
