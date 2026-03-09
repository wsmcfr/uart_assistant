/**
 * @file FFTSettingsDialog.cpp
 * @brief FFT 参数设置对话框实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "FFTSettingsDialog.h"
#include "core/utils/Logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QPushButton>

namespace ComAssistant {

FFTSettingsDialog::FFTSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("FFT 参数设置"));
    setMinimumWidth(450);
    setupUi();
    setupConnections();

    // 初始化显示
    onWindowTypeChanged(m_windowTypeCombo->currentIndex());
    updateFrequencyResolution();
}

void FFTSettingsDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);

    // ==================== FFT基本设置组 ====================
    QGroupBox* fftGroup = new QGroupBox(tr("FFT 设置"));
    QFormLayout* fftLayout = new QFormLayout(fftGroup);
    fftLayout->setSpacing(10);

    // FFT点数
    m_fftSizeCombo = new QComboBox;
    m_fftSizeCombo->addItem(tr("自动 (使用数据长度)"), 0);
    m_fftSizeCombo->addItem("256", 256);
    m_fftSizeCombo->addItem("512", 512);
    m_fftSizeCombo->addItem("1024", 1024);
    m_fftSizeCombo->addItem("2048", 2048);
    m_fftSizeCombo->addItem("4096", 4096);
    m_fftSizeCombo->addItem("8192", 8192);
    m_fftSizeCombo->addItem("16384", 16384);
    m_fftSizeCombo->setCurrentIndex(0);
    fftLayout->addRow(tr("FFT 点数:"), m_fftSizeCombo);

    // 采样率
    m_sampleRateSpin = new QDoubleSpinBox;
    m_sampleRateSpin->setRange(0.001, 10000000);
    m_sampleRateSpin->setValue(1000);
    m_sampleRateSpin->setDecimals(2);
    m_sampleRateSpin->setSuffix(" Hz");
    m_sampleRateSpin->setToolTip(tr("数据采样率，影响频率轴刻度"));
    fftLayout->addRow(tr("采样率:"), m_sampleRateSpin);

    // 频率分辨率显示
    m_freqResolutionLabel = new QLabel(tr("-- Hz"));
    m_freqResolutionLabel->setStyleSheet("color: #3498db; font-weight: bold;");
    fftLayout->addRow(tr("频率分辨率:"), m_freqResolutionLabel);

    // 零填充
    m_zeroPaddingCheck = new QCheckBox(tr("启用零填充 (扩展到2的幂次)"));
    m_zeroPaddingCheck->setChecked(true);
    m_zeroPaddingCheck->setToolTip(tr("将数据长度扩展到最近的2的幂次，提高FFT效率"));
    fftLayout->addRow("", m_zeroPaddingCheck);

    // 频谱平均次数
    m_averageCountSpin = new QSpinBox;
    m_averageCountSpin->setRange(1, 100);
    m_averageCountSpin->setValue(1);
    m_averageCountSpin->setToolTip(tr("多次FFT结果取平均，降低噪声"));
    fftLayout->addRow(tr("频谱平均次数:"), m_averageCountSpin);

    mainLayout->addWidget(fftGroup);

    // ==================== 窗口函数设置组 ====================
    QGroupBox* windowGroup = new QGroupBox(tr("窗口函数"));
    QVBoxLayout* windowLayout = new QVBoxLayout(windowGroup);

    // 窗口函数选择
    QHBoxLayout* windowSelectLayout = new QHBoxLayout;
    QLabel* windowLabel = new QLabel(tr("类型:"));
    m_windowTypeCombo = new QComboBox;
    m_windowTypeCombo->addItem(tr("矩形窗 (无窗)"), static_cast<int>(WindowType::Rectangular));
    m_windowTypeCombo->addItem(tr("汉宁窗 (Hanning)"), static_cast<int>(WindowType::Hanning));
    m_windowTypeCombo->addItem(tr("汉明窗 (Hamming)"), static_cast<int>(WindowType::Hamming));
    m_windowTypeCombo->addItem(tr("布莱克曼窗 (Blackman)"), static_cast<int>(WindowType::Blackman));
    m_windowTypeCombo->addItem(tr("布莱克曼-哈里斯窗"), static_cast<int>(WindowType::BlackmanHarris));
    m_windowTypeCombo->addItem(tr("凯泽窗 (Kaiser)"), static_cast<int>(WindowType::Kaiser));
    m_windowTypeCombo->addItem(tr("平顶窗 (Flat Top)"), static_cast<int>(WindowType::FlatTop));
    m_windowTypeCombo->addItem(tr("高斯窗 (Gaussian)"), static_cast<int>(WindowType::Gaussian));
    m_windowTypeCombo->setCurrentIndex(1);  // 默认汉宁窗
    windowSelectLayout->addWidget(windowLabel);
    windowSelectLayout->addWidget(m_windowTypeCombo, 1);
    windowLayout->addLayout(windowSelectLayout);

    // 窗口函数描述
    m_windowDescLabel = new QLabel;
    m_windowDescLabel->setWordWrap(true);
    m_windowDescLabel->setStyleSheet("color: #7f8c8d; font-size: 11px; padding: 5px;");
    windowLayout->addWidget(m_windowDescLabel);

    // 凯泽窗参数
    m_kaiserParamWidget = new QWidget;
    QHBoxLayout* kaiserLayout = new QHBoxLayout(m_kaiserParamWidget);
    kaiserLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* betaLabel = new QLabel(tr("Beta 参数:"));
    m_kaiserBetaSpin = new QDoubleSpinBox;
    m_kaiserBetaSpin->setRange(0, 20);
    m_kaiserBetaSpin->setValue(5.0);
    m_kaiserBetaSpin->setDecimals(1);
    m_kaiserBetaSpin->setToolTip(tr("Beta越大，主瓣越宽，旁瓣越小"));
    kaiserLayout->addWidget(betaLabel);
    kaiserLayout->addWidget(m_kaiserBetaSpin);
    kaiserLayout->addStretch();
    m_kaiserParamWidget->hide();
    windowLayout->addWidget(m_kaiserParamWidget);

    // 高斯窗参数
    m_gaussianParamWidget = new QWidget;
    QHBoxLayout* gaussianLayout = new QHBoxLayout(m_gaussianParamWidget);
    gaussianLayout->setContentsMargins(0, 0, 0, 0);
    QLabel* sigmaLabel = new QLabel(tr("Sigma 参数:"));
    m_gaussianSigmaSpin = new QDoubleSpinBox;
    m_gaussianSigmaSpin->setRange(0.1, 1.0);
    m_gaussianSigmaSpin->setValue(0.4);
    m_gaussianSigmaSpin->setDecimals(2);
    m_gaussianSigmaSpin->setSingleStep(0.05);
    m_gaussianSigmaSpin->setToolTip(tr("Sigma越小，窗口越窄"));
    gaussianLayout->addWidget(sigmaLabel);
    gaussianLayout->addWidget(m_gaussianSigmaSpin);
    gaussianLayout->addStretch();
    m_gaussianParamWidget->hide();
    windowLayout->addWidget(m_gaussianParamWidget);

    mainLayout->addWidget(windowGroup);

    // ==================== 显示设置组 ====================
    QGroupBox* displayGroup = new QGroupBox(tr("显示设置"));
    QVBoxLayout* displayLayout = new QVBoxLayout(displayGroup);

    // 对数刻度
    m_logScaleCheck = new QCheckBox(tr("使用对数刻度 (dB)"));
    m_logScaleCheck->setChecked(true);
    displayLayout->addWidget(m_logScaleCheck);

    // 峰值检测
    QHBoxLayout* peakLayout = new QHBoxLayout;
    m_peakDetectionCheck = new QCheckBox(tr("启用峰值检测"));
    m_peakDetectionCheck->setChecked(true);
    peakLayout->addWidget(m_peakDetectionCheck);
    QLabel* maxPeaksLabel = new QLabel(tr("最大峰值数:"));
    m_maxPeaksSpin = new QSpinBox;
    m_maxPeaksSpin->setRange(1, 50);
    m_maxPeaksSpin->setValue(10);
    peakLayout->addWidget(maxPeaksLabel);
    peakLayout->addWidget(m_maxPeaksSpin);
    peakLayout->addStretch();
    displayLayout->addLayout(peakLayout);

    // 峰值保持
    m_maxHoldCheck = new QCheckBox(tr("启用峰值保持 (Max Hold)"));
    m_maxHoldCheck->setChecked(false);
    displayLayout->addWidget(m_maxHoldCheck);

    mainLayout->addWidget(displayGroup);

    // ==================== 按钮 ====================
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this]() {
        FFTConfig defaultConfig;
        setConfig(defaultConfig);
        setUseLogScale(true);
        setPeakDetectionEnabled(true);
        setMaxPeaks(10);
        setMaxHoldEnabled(false);
    });

    mainLayout->addWidget(buttonBox);
}

void FFTSettingsDialog::setupConnections()
{
    connect(m_windowTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FFTSettingsDialog::onWindowTypeChanged);
    connect(m_fftSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FFTSettingsDialog::onFFTSizeChanged);
    connect(m_sampleRateSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &FFTSettingsDialog::updateFrequencyResolution);
    connect(m_peakDetectionCheck, &QCheckBox::toggled, m_maxPeaksSpin, &QSpinBox::setEnabled);
}

void FFTSettingsDialog::onWindowTypeChanged(int index)
{
    WindowType type = static_cast<WindowType>(m_windowTypeCombo->itemData(index).toInt());

    // 隐藏所有参数面板
    m_kaiserParamWidget->hide();
    m_gaussianParamWidget->hide();

    // 根据窗口类型显示相应参数和描述
    QString desc;
    switch (type) {
    case WindowType::Rectangular:
        desc = tr("不加窗处理。频率分辨率最高，但旁瓣泄漏最严重。适用于瞬态信号分析。");
        break;
    case WindowType::Hanning:
        desc = tr("最常用的窗函数。在频率分辨率和旁瓣抑制之间有良好平衡。适用于大多数周期信号分析。");
        break;
    case WindowType::Hamming:
        desc = tr("类似汉宁窗，但旁瓣衰减更快。适用于窄带信号分析。");
        break;
    case WindowType::Blackman:
        desc = tr("旁瓣抑制优秀（-58dB），但主瓣较宽。适用于需要高动态范围的分析。");
        break;
    case WindowType::BlackmanHarris:
        desc = tr("极低旁瓣（-92dB），主瓣更宽。适用于测量弱信号附近的强信号。");
        break;
    case WindowType::Kaiser:
        desc = tr("可调参数窗口。Beta参数控制主瓣宽度和旁瓣抑制的权衡。Beta=0时接近矩形窗，Beta越大旁瓣越小。");
        m_kaiserParamWidget->show();
        break;
    case WindowType::FlatTop:
        desc = tr("幅度测量精度最高（<0.01%误差）。适用于精确测量信号幅度，但频率分辨率较差。");
        break;
    case WindowType::Gaussian:
        desc = tr("时频域都是高斯形状，无旁瓣。Sigma参数控制窗口宽度。适用于时频分析。");
        m_gaussianParamWidget->show();
        break;
    }
    m_windowDescLabel->setText(desc);
}

void FFTSettingsDialog::onFFTSizeChanged(int index)
{
    Q_UNUSED(index);
    updateFrequencyResolution();
}

void FFTSettingsDialog::updateFrequencyResolution()
{
    int fftSize = m_fftSizeCombo->currentData().toInt();
    double sampleRate = m_sampleRateSpin->value();

    if (fftSize == 0) {
        m_freqResolutionLabel->setText(tr("取决于数据长度"));
    } else {
        double resolution = sampleRate / fftSize;
        m_freqResolutionLabel->setText(QString("%1 Hz").arg(resolution, 0, 'f', 4));
    }
}

FFTConfig FFTSettingsDialog::getConfig() const
{
    FFTConfig config;
    config.fftSize = m_fftSizeCombo->currentData().toInt();
    config.sampleRate = m_sampleRateSpin->value();
    config.windowType = static_cast<WindowType>(m_windowTypeCombo->currentData().toInt());
    config.kaiserBeta = m_kaiserBetaSpin->value();
    config.gaussianSigma = m_gaussianSigmaSpin->value();
    config.zeroPadding = m_zeroPaddingCheck->isChecked();
    config.averageCount = m_averageCountSpin->value();
    return config;
}

void FFTSettingsDialog::setConfig(const FFTConfig& config)
{
    // FFT点数
    int sizeIndex = m_fftSizeCombo->findData(config.fftSize);
    if (sizeIndex >= 0) {
        m_fftSizeCombo->setCurrentIndex(sizeIndex);
    }

    // 采样率
    m_sampleRateSpin->setValue(config.sampleRate);

    // 窗口函数
    int windowIndex = m_windowTypeCombo->findData(static_cast<int>(config.windowType));
    if (windowIndex >= 0) {
        m_windowTypeCombo->setCurrentIndex(windowIndex);
    }

    // 窗口参数
    m_kaiserBetaSpin->setValue(config.kaiserBeta);
    m_gaussianSigmaSpin->setValue(config.gaussianSigma);

    // 其他选项
    m_zeroPaddingCheck->setChecked(config.zeroPadding);
    m_averageCountSpin->setValue(config.averageCount);
}

bool FFTSettingsDialog::useLogScale() const
{
    return m_logScaleCheck->isChecked();
}

void FFTSettingsDialog::setUseLogScale(bool enabled)
{
    m_logScaleCheck->setChecked(enabled);
}

bool FFTSettingsDialog::peakDetectionEnabled() const
{
    return m_peakDetectionCheck->isChecked();
}

void FFTSettingsDialog::setPeakDetectionEnabled(bool enabled)
{
    m_peakDetectionCheck->setChecked(enabled);
}

int FFTSettingsDialog::maxPeaks() const
{
    return m_maxPeaksSpin->value();
}

void FFTSettingsDialog::setMaxPeaks(int count)
{
    m_maxPeaksSpin->setValue(count);
}

bool FFTSettingsDialog::maxHoldEnabled() const
{
    return m_maxHoldCheck->isChecked();
}

void FFTSettingsDialog::setMaxHoldEnabled(bool enabled)
{
    m_maxHoldCheck->setChecked(enabled);
}

} // namespace ComAssistant
