/**
 * @file FFTSettingsDialog.h
 * @brief FFT 参数设置对话框
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef FFTSETTINGSDIALOG_H
#define FFTSETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include "core/utils/FFTUtils.h"

namespace ComAssistant {

/**
 * @brief FFT 参数设置对话框
 *
 * 允许用户配置FFT分析参数：
 * - FFT点数
 * - 采样率
 * - 窗口函数
 * - 零填充
 * - 频谱平均次数
 */
class FFTSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FFTSettingsDialog(QWidget* parent = nullptr);
    ~FFTSettingsDialog() override = default;

    /**
     * @brief 获取FFT配置
     */
    FFTConfig getConfig() const;

    /**
     * @brief 设置FFT配置
     */
    void setConfig(const FFTConfig& config);

    /**
     * @brief 获取是否使用对数刻度显示
     */
    bool useLogScale() const;

    /**
     * @brief 设置是否使用对数刻度
     */
    void setUseLogScale(bool enabled);

    /**
     * @brief 获取是否启用峰值检测
     */
    bool peakDetectionEnabled() const;

    /**
     * @brief 设置是否启用峰值检测
     */
    void setPeakDetectionEnabled(bool enabled);

    /**
     * @brief 获取最大峰值数量
     */
    int maxPeaks() const;

    /**
     * @brief 设置最大峰值数量
     */
    void setMaxPeaks(int count);

    /**
     * @brief 获取是否启用峰值保持
     */
    bool maxHoldEnabled() const;

    /**
     * @brief 设置是否启用峰值保持
     */
    void setMaxHoldEnabled(bool enabled);

private slots:
    void onWindowTypeChanged(int index);
    void onFFTSizeChanged(int index);
    void updateFrequencyResolution();

private:
    void setupUi();
    void setupConnections();

private:
    // FFT基本设置
    QComboBox* m_fftSizeCombo;          ///< FFT点数选择
    QDoubleSpinBox* m_sampleRateSpin;   ///< 采样率输入
    QComboBox* m_windowTypeCombo;       ///< 窗口函数选择
    QCheckBox* m_zeroPaddingCheck;      ///< 零填充选项
    QSpinBox* m_averageCountSpin;       ///< 频谱平均次数

    // 窗口函数参数
    QDoubleSpinBox* m_kaiserBetaSpin;   ///< 凯泽窗Beta参数
    QDoubleSpinBox* m_gaussianSigmaSpin;///< 高斯窗Sigma参数
    QWidget* m_kaiserParamWidget;       ///< 凯泽窗参数面板
    QWidget* m_gaussianParamWidget;     ///< 高斯窗参数面板

    // 显示设置
    QCheckBox* m_logScaleCheck;         ///< 对数刻度显示
    QCheckBox* m_peakDetectionCheck;    ///< 峰值检测
    QSpinBox* m_maxPeaksSpin;           ///< 最大峰值数量
    QCheckBox* m_maxHoldCheck;          ///< 峰值保持

    // 信息显示
    QLabel* m_freqResolutionLabel;      ///< 频率分辨率显示
    QLabel* m_windowDescLabel;          ///< 窗口函数描述
};

} // namespace ComAssistant

#endif // FFTSETTINGSDIALOG_H
