/**
 * @file RealTimeFFTWindow.h
 * @brief 实时FFT分析窗口
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef REALTIMEFFTWINDOW_H
#define REALTIMEFFTWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QVector>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include "qcustomplot/qcustomplot.h"
#include "core/utils/FFTUtils.h"

namespace ComAssistant {

// 前向声明
class WaterfallWidget;

/**
 * @brief 实时FFT分析窗口
 *
 * 提供时域和频域的双视图实时显示：
 * - 上半部分显示时域波形
 * - 下半部分显示频谱
 * - 支持峰值保持、频谱平均、峰值检测
 */
class RealTimeFFTWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RealTimeFFTWindow(QWidget* parent = nullptr);
    ~RealTimeFFTWindow() override;

    /**
     * @brief 设置FFT配置
     */
    void setFFTConfig(const FFTConfig& config);

    /**
     * @brief 获取FFT配置
     */
    FFTConfig fftConfig() const { return m_fftConfig; }

    /**
     * @brief 添加时域数据点
     * @param value 数据值
     */
    void appendData(double value);

    /**
     * @brief 添加批量时域数据
     * @param values 数据数组
     */
    void appendData(const QVector<double>& values);

    /**
     * @brief 清空数据
     */
    void clearData();

    /**
     * @brief 设置采样率
     */
    void setSampleRate(double rate);

    /**
     * @brief 获取采样率
     */
    double sampleRate() const { return m_fftConfig.sampleRate; }

    /**
     * @brief 设置FFT点数
     */
    void setFFTSize(int size);

    /**
     * @brief 设置是否启用峰值保持
     */
    void setMaxHoldEnabled(bool enabled);

    /**
     * @brief 设置是否启用频谱平均
     */
    void setAveragingEnabled(bool enabled);

    /**
     * @brief 设置是否暂停
     */
    void setPaused(bool paused);

    /**
     * @brief 获取是否暂停
     */
    bool isPaused() const { return m_paused; }

signals:
    /**
     * @brief 窗口关闭信号
     */
    void windowClosed();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onUpdateTimer();
    void onFFTSizeChanged(int index);
    void onWindowTypeChanged(int index);
    void onSampleRateChanged(double value);
    void onPauseToggled(bool checked);
    void onClearClicked();
    void onMaxHoldToggled(bool checked);
    void onAveragingToggled(bool checked);
    void onLogScaleToggled(bool checked);
    void onPeakDetectionToggled(bool checked);
    void onWaterfallToggled(bool checked);
    void onExportClicked();
    void onTimeDomainMouseMove(QMouseEvent* event);
    void onFreqDomainMouseMove(QMouseEvent* event);

private:
    void setupUi();
    void setupToolBar();
    void setupPlots();
    void retranslateUi();
    void updateTimeDomainPlot();
    void updateFrequencyDomainPlot();
    void updateWaterfall();
    void performFFT();
    void updateInfoLabel();

private:
    // UI控件
    QCustomPlot* m_timePlot;           ///< 时域图
    QCustomPlot* m_freqPlot;           ///< 频域图
    WaterfallWidget* m_waterfall;      ///< 瀑布图
    QLabel* m_infoLabel;               ///< 信息标签

    // 工具栏控件
    QComboBox* m_fftSizeCombo;
    QComboBox* m_windowTypeCombo;
    QDoubleSpinBox* m_sampleRateSpin;
    QAction* m_pauseAction;
    QCheckBox* m_maxHoldCheck;
    QCheckBox* m_averagingCheck;
    QCheckBox* m_logScaleCheck;
    QCheckBox* m_peakDetectionCheck;
    QCheckBox* m_waterfallCheck;

    // 需要国际化的工具栏标签
    QLabel* m_fftSizeLabel = nullptr;
    QLabel* m_windowLabel = nullptr;
    QLabel* m_sampleRateLabel = nullptr;
    QAction* m_clearAction = nullptr;
    QAction* m_exportAction = nullptr;

    // 数据
    QVector<double> m_timeData;        ///< 时域数据缓冲
    QVector<double> m_maxHoldData;     ///< 峰值保持数据
    QVector<FFTResult> m_avgBuffer;    ///< 平均缓冲
    FFTResult m_lastResult;            ///< 最新FFT结果

    // 配置
    FFTConfig m_fftConfig;
    int m_bufferSize = 2048;           ///< 时域数据缓冲大小
    int m_avgCount = 4;                ///< 平均次数
    bool m_paused = false;
    bool m_maxHoldEnabled = false;
    bool m_averagingEnabled = false;
    bool m_logScale = true;
    bool m_peakDetection = true;
    bool m_waterfallEnabled = false;
    bool m_needUpdate = false;

    // 定时器
    QTimer* m_updateTimer;
    int m_updateInterval = 50;         ///< 更新间隔(ms)

    // 光标
    QCPItemLine* m_timeCursor = nullptr;
    QCPItemLine* m_freqCursor = nullptr;
    QCPItemText* m_timeCursorText = nullptr;
    QCPItemText* m_freqCursorText = nullptr;
};

} // namespace ComAssistant

#endif // REALTIMEFFTWINDOW_H
