/**
 * @file WaterfallWidget.h
 * @brief 瀑布图/频谱图显示组件
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef WATERFALLWIDGET_H
#define WATERFALLWIDGET_H

#include <QWidget>
#include <QVector>
#include <QTimer>
#include "qcustomplot/qcustomplot.h"

namespace ComAssistant {

/**
 * @brief 瀑布图显示组件
 *
 * 使用QCPColorMap显示频谱随时间变化的瀑布图（频谱图）
 * - X轴：频率
 * - Y轴：时间
 * - 颜色：幅度/功率
 */
class WaterfallWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 颜色映射类型
     */
    enum class ColorMap {
        Jet,        ///< 彩虹色
        Hot,        ///< 热图
        Cold,       ///< 冷色调
        Grayscale,  ///< 灰度
        Spectrum    ///< 频谱色（蓝-青-绿-黄-红）
    };

    explicit WaterfallWidget(QWidget* parent = nullptr);
    ~WaterfallWidget() override;

    /**
     * @brief 设置频率范围
     * @param minFreq 最小频率 (Hz)
     * @param maxFreq 最大频率 (Hz)
     */
    void setFrequencyRange(double minFreq, double maxFreq);

    /**
     * @brief 设置时间深度
     * @param depth 时间行数
     */
    void setTimeDepth(int depth);

    /**
     * @brief 获取时间深度
     */
    int timeDepth() const { return m_timeDepth; }

    /**
     * @brief 设置幅度范围
     * @param minDb 最小值 (dB)
     * @param maxDb 最大值 (dB)
     */
    void setAmplitudeRange(double minDb, double maxDb);

    /**
     * @brief 设置颜色映射
     * @param colorMap 颜色映射类型
     */
    void setColorMap(ColorMap colorMap);

    /**
     * @brief 添加一行频谱数据
     * @param spectrum 频谱幅度数据（dB）
     */
    void addSpectrumLine(const QVector<double>& spectrum);

    /**
     * @brief 添加一行频谱数据（线性幅度，内部转换为dB）
     * @param magnitudes 线性幅度数据
     */
    void addMagnitudeLine(const QVector<double>& magnitudes);

    /**
     * @brief 清空数据
     */
    void clearData();

    /**
     * @brief 设置是否暂停更新
     */
    void setPaused(bool paused);

    /**
     * @brief 获取是否暂停
     */
    bool isPaused() const { return m_paused; }

    /**
     * @brief 导出图像
     * @param filename 文件名
     * @return 是否成功
     */
    bool exportImage(const QString& filename);

signals:
    /**
     * @brief 鼠标位置变化信号
     * @param freq 频率
     * @param time 时间索引
     * @param amplitude 幅度
     */
    void cursorMoved(double freq, int time, double amplitude);

private:
    void setupUi();
    void setupColorMap(ColorMap type);
    void shiftDataUp(int rows);
    QVector<double> normalizeSpectrumLine(const QVector<double>& spectrum) const;
    void scheduleSpectrumFlush();
    void flushPendingSpectra();

private:
    QCustomPlot* m_plot;
    QCPColorMap* m_colorMap;
    QCPColorScale* m_colorScale;

    int m_timeDepth = 200;          ///< 时间深度（行数）
    int m_freqBins = 512;           ///< 频率点数
    double m_minFreq = 0;           ///< 最小频率
    double m_maxFreq = 500;         ///< 最大频率
    double m_minDb = -100;          ///< 最小dB
    double m_maxDb = 0;             ///< 最大dB
    int m_currentRow = 0;           ///< 当前行
    bool m_paused = false;
    QVector<QVector<double>> m_pendingSpectra; ///< 待批量写入颜色图的频谱行，保持原始时间顺序
    QTimer* m_spectrumFlushTimer = nullptr;    ///< 瀑布图批量刷新定时器，用于合并高频重绘
    int m_spectrumFlushIntervalMs = 33;        ///< 瀑布图刷新间隔，约 30fps
    int m_spectrumFlushBatchSize = 16;         ///< 单次最多合并的频谱行数，避免连续数据重复搬移整张色图
};

} // namespace ComAssistant

#endif // WATERFALLWIDGET_H
