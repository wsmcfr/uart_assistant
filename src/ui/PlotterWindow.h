/**
 * @file PlotterWindow.h
 * @brief 绘图窗口类
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef PLOTTERWINDOW_H
#define PLOTTERWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QColor>
#include <QTimer>
#include <QShortcut>
#include <QMenu>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QScrollBar>
#include <QEvent>

#include "qcustomplot/qcustomplot.h"
#include "core/utils/FilterUtils.h"
#include "PlotRenderQuality.h"

namespace ComAssistant {

// 前向声明
class SpectrumWindow;
class RealTimeFFTWindow;
class PlotControlPanel;

/**
 * @brief 数据抽稀比例枚举
 */
enum class DecimationRatio {
    None = 1,    ///< 无抽稀 (1:1)
    Half = 2,    ///< 1:2
    Fifth = 5,   ///< 1:5
    Tenth = 10   ///< 1:10
};

/**
 * @brief 绘图视图模式枚举
 */
enum class PlotViewMode {
    TimeSeries = 0,  ///< 时间序列（波形）模式
    Histogram = 1,   ///< 直方图模式
    XY = 2           ///< XY模式（李萨如图形等）
};

/**
 * @brief 曲线统计信息结构体（嵌入式常用指标）
 */
struct CurveStatistics {
    // 基本统计
    double minValue = 0;           ///< 最小值
    double maxValue = 0;           ///< 最大值
    double average = 0;            ///< 平均值
    double stdDev = 0;             ///< 标准差
    double median = 0;             ///< 中位数

    // 嵌入式常用指标
    double peakToPeak = 0;         ///< 峰峰值 (max - min)
    double rms = 0;                ///< 均方根值 (RMS)
    double crestFactor = 0;        ///< 波峰因子 (peak / RMS)

    // 数据信息
    int dataCount = 0;             ///< 数据点数
    double sampleRate = 0;         ///< 估算采样率 (Hz)
    double duration = 0;           ///< 数据时长 (s)

    // 选区信息
    double rangeStart = 0;         ///< 统计区间起点
    double rangeEnd = 0;           ///< 统计区间终点
    bool isRangeStats = false;     ///< 是否为选区统计
};

/**
 * @brief 数据报警配置
 */
struct AlarmConfig {
    bool enabled = false;          ///< 是否启用报警
    int sourceCurve = 0;           ///< 报警源曲线

    // 上下限报警
    bool upperLimitEnabled = false;
    double upperLimit = 100;       ///< 上限值
    bool lowerLimitEnabled = false;
    double lowerLimit = 0;         ///< 下限值

    // 变化率报警
    bool rateAlarmEnabled = false;
    double rateLimit = 10;         ///< 变化率限制 (单位/采样点)

    // 报警动作
    bool soundAlert = true;        ///< 声音提示
    bool visualAlert = true;       ///< 视觉提示
    bool autoPause = false;        ///< 自动暂停
    bool logToFile = false;        ///< 记录到日志
};

/**
 * @brief 数据标记结构体
 */
struct DataMarker {
    double xPos = 0;               ///< X位置
    double yPos = 0;               ///< Y位置
    int curveIndex = 0;            ///< 所属曲线
    QString label;                 ///< 标记文本
    QColor color = Qt::red;        ///< 标记颜色
    QCPItemTracer* tracer = nullptr;  ///< 追踪点
    QCPItemText* text = nullptr;   ///< 文本项
};

/**
 * @brief 嵌入式场景预设类型
 */
enum class EmbeddedSceneType {
    PIDControl,        ///< PID 调参场景
    MotorControl,      ///< 电机控制场景
    SensorMonitor,     ///< 传感器监测场景
    ADCSampling,       ///< ADC 采样场景
    PowerMonitor,      ///< 电源监测场景
    TemperatureControl ///< 温度控制场景
};

/**
 * @brief 绘图窗口类
 *
 * 基于QCustomPlot的实时数据绘图窗口，支持多曲线、自动缩放、OpenGL加速等功能。
 */
class PlotterWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param windowId 窗口唯一标识
     * @param parent 父窗口指针
     */
    explicit PlotterWindow(const QString& windowId, QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~PlotterWindow() override;

    /**
     * @brief 获取窗口ID
     * @return 窗口ID字符串
     */
    QString windowId() const { return m_windowId; }

    /**
     * @brief 添加曲线
     * @param curveName 曲线名称
     * @param color 曲线颜色（可选，自动分配）
     */
    void addCurve(const QString& curveName, const QColor& color = QColor());

    /**
     * @brief 获取曲线数量
     * @return 当前曲线数量
     */
    int curveCount() const { return m_plot ? m_plot->graphCount() : 0; }

    /**
     * @brief 获取曲线名称
     * @param curveIndex 曲线索引
     * @return 曲线名称，索引无效时返回空字符串
     */
    QString curveName(int curveIndex) const;

    /**
     * @brief 获取曲线可见性
     * @param curveIndex 曲线索引
     * @return 曲线是否可见
     */
    bool isCurveVisible(int curveIndex) const;

    /**
     * @brief 追加单点数据（X轴自动递增）
     * @param curveIndex 曲线索引
     * @param y Y轴值
     */
    void appendData(int curveIndex, double y);

    /**
     * @brief 追加单点数据（指定X值）
     * @param curveIndex 曲线索引
     * @param x X轴值
     * @param y Y轴值
     */
    void appendData(int curveIndex, double x, double y);

    /**
     * @brief 追加多曲线数据（X轴自动递增）
     * @param values 各曲线的Y值列表
     */
    void appendMultiData(const QVector<double>& values);

    /**
     * @brief 追加多曲线数据（指定X值）
     * @param x X轴值
     * @param values 各曲线的Y值列表
     */
    void appendMultiData(double x, const QVector<double>& values);

    /**
     * @brief 清空指定曲线数据
     * @param curveIndex 曲线索引
     */
    void clearCurve(int curveIndex);

    /**
     * @brief 清空所有数据
     */
    void clearAll();

    /**
     * @brief 设置X轴范围
     * @param min 最小值
     * @param max 最大值
     */
    void setXAxisRange(double min, double max);

    /**
     * @brief 设置Y轴范围
     * @param min 最小值
     * @param max 最大值
     */
    void setYAxisRange(double min, double max);

    /**
     * @brief 设置自动缩放
     * @param enabled 是否启用
     */
    void setAutoScale(bool enabled);

    /**
     * @brief 设置显示网格
     * @param show 是否显示
     */
    void setShowGrid(bool show);

    /**
     * @brief 设置显示图例
     * @param show 是否显示
     */
    void setShowLegend(bool show);

    /**
     * @brief 设置最大数据点数
     * @param maxPoints 最大点数
     */
    void setMaxDataPoints(int maxPoints);

    /**
     * @brief 设置暂停状态
     * @param paused 是否暂停
     */
    void setPaused(bool paused);

    /**
     * @brief 获取暂停状态
     * @return 是否暂停
     */
    bool isPaused() const { return m_paused; }

    /**
     * @brief 设置OpenGL加速
     * @param enabled 是否启用
     */
    void setOpenGLEnabled(bool enabled);

    /**
     * @brief 获取OpenGL是否启用
     * @return 是否启用
     */
    bool isOpenGLEnabled() const { return m_openGLEnabled; }

    /**
     * @brief 设置渲染质量模式
     * @param mode 渲染质量模式
     */
    void setRenderQualityMode(RenderQualityMode mode);

    /**
     * @brief 获取渲染质量模式
     * @return 当前渲染质量模式
     */
    RenderQualityMode renderQualityMode() const { return m_renderQualityMode; }

    /**
     * @brief 设置数据抽稀比例
     * @param ratio 抽稀比例
     */
    void setDecimationRatio(DecimationRatio ratio);

    /**
     * @brief 获取数据抽稀比例
     * @return 抽稀比例
     */
    DecimationRatio decimationRatio() const { return m_decimationRatio; }

    /**
     * @brief 设置曲线线宽
     * @param curveIndex 曲线索引
     * @param width 线宽
     */
    void setCurveLineWidth(int curveIndex, double width);

    /**
     * @brief 设置曲线线型
     * @param curveIndex 曲线索引
     * @param style 线型
     */
    void setCurveLineStyle(int curveIndex, Qt::PenStyle style);

    /**
     * @brief 设置曲线点型
     * @param curveIndex 曲线索引
     * @param shape 点型
     */
    void setCurveScatterStyle(int curveIndex, QCPScatterStyle::ScatterShape shape);

    /**
     * @brief 导出图片
     * @param filename 文件名
     * @return 是否成功
     */
    bool exportImage(const QString& filename);

    /**
     * @brief 导出数据为CSV
     * @param filename 文件名
     * @return 是否成功
     */
    bool exportData(const QString& filename);

    /**
     * @brief 重命名曲线
     * @param curveIndex 曲线索引
     * @param name 新名称
     */
    void renameCurve(int curveIndex, const QString& name);

    /**
     * @brief 设置参考线（设定值线）
     * @param value Y轴参考值
     * @param label 标签文本
     */
    void setReferenceLine(double value, const QString& label = QString());

    /**
     * @brief 移除参考线
     */
    void removeReferenceLine();

    /**
     * @brief 计算曲线统计信息
     * @param curveIndex 曲线索引
     * @param min 输出最小值
     * @param max 输出最大值
     * @param avg 输出平均值
     * @param stddev 输出标准差
     */
    void calculateStatistics(int curveIndex, double& min, double& max, double& avg, double& stddev);

    /**
     * @brief 计算PID性能指标
     * @param curveIndex 曲线索引（响应曲线）
     * @param setpoint 设定值
     * @param overshoot 输出超调量（%）
     * @param settlingTime 输出稳定时间（采样点数）
     * @param riseTime 输出上升时间（采样点数）
     * @param steadyError 输出稳态误差
     */
    void calculatePIDMetrics(int curveIndex, double setpoint,
                             double& overshoot, int& settlingTime,
                             int& riseTime, double& steadyError);

    /**
     * @brief 计算增强统计信息（嵌入式常用指标）
     * @param curveIndex 曲线索引
     * @param rangeStart X轴起点（-1表示全部）
     * @param rangeEnd X轴终点（-1表示全部）
     * @return 统计信息结构体
     */
    CurveStatistics calculateAdvancedStatistics(int curveIndex, double rangeStart = -1, double rangeEnd = -1);

    /**
     * @brief 设置曲线使用的Y轴
     * @param curveIndex 曲线索引
     * @param useRightAxis true使用右轴，false使用左轴
     */
    void setCurveYAxis(int curveIndex, bool useRightAxis);

    /**
     * @brief 获取曲线使用的Y轴
     * @param curveIndex 曲线索引
     * @return true使用右轴，false使用左轴
     */
    bool curveUsesRightAxis(int curveIndex) const;

    /**
     * @brief 设置右Y轴范围
     * @param min 最小值
     * @param max 最大值
     */
    void setRightYAxisRange(double min, double max);

    /**
     * @brief 设置右Y轴标签
     * @param label 标签文本
     */
    void setRightYAxisLabel(const QString& label);

    /**
     * @brief 设置右Y轴是否可见
     * @param visible 是否可见
     */
    void setRightYAxisVisible(bool visible);

signals:
    /**
     * @brief 窗口关闭信号
     * @param windowId 窗口ID
     */
    void windowClosed(const QString& windowId);

    /**
     * @brief 数据量变化信号
     * @param totalPoints 总数据点数
     */
    void dataSizeChanged(int totalPoints);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onPauseToggled();
    void onClearClicked();
    void onAutoScaleClicked();
    void onExportImageClicked();
    void onExportDataClicked();
    void onGridToggled(bool checked);
    void onLegendToggled(bool checked);
    void onOpenGLToggled(bool checked);
    void onDecimationChanged(int index);
    void onLineWidthChanged(double value);
    void onShowSettingsDialog();
    void onFFTAnalysisClicked();         ///< FFT 频谱分析
    void onRealTimeFFTClicked();         ///< 实时 FFT 分析
    void onScrollModeToggled(bool checked);  ///< 滚动模式切换
    void onScrollBarChanged(int value);      ///< 滚动条值改变
    void updatePlot();
    void updateValuePanel();                 ///< 更新数值显示
    void onMouseMove(QMouseEvent* event);    ///< 鼠标移动事件
    void onRenameCurveClicked();             ///< 重命名曲线
    void onSetReferenceLineClicked();        ///< 设置参考线
    void onPIDAnalysisClicked();             ///< PID性能分析
    void onShowDifferenceClicked();          ///< 显示曲线差值
    void onConfigureYAxisClicked();          ///< 配置Y轴
    void updateCursorInfo(double x, bool doReplot = true);  ///< 更新光标位置信息
    void onMeasureCursorClicked();           ///< 测量游标设置
    void updateMeasureCursors();             ///< 更新测量游标显示
    void onFilterCurveClicked();             ///< 曲线滤波
    void onTriggerCaptureClicked();          ///< 触发捕获设置
    void checkTriggerCondition(int curveIndex, double value);  ///< 检查触发条件
    void onPeakAnnotationClicked();          ///< 峰值自动标注设置
    void updatePeakAnnotations();            ///< 更新峰值标注
    void onAdvancedStatsClicked();           ///< 增强统计分析
    void onAlarmConfigClicked();             ///< 报警配置
    void checkAlarmCondition(int curveIndex, double value);  ///< 检查报警条件
    void onAddMarkerClicked();               ///< 添加数据标记
    void clearAllMarkers();                  ///< 清除所有标记
    void updateAlarmDisplay();               ///< 更新报警显示
    void onApplyScenePresetClicked();        ///< 应用场景预设
    void applyScenePreset(EmbeddedSceneType sceneType);  ///< 应用指定场景预设
    void onHistogramViewClicked();           ///< 切换直方图视图
    void updateHistogramView();              ///< 更新直方图显示
    void onXYViewClicked();                  ///< 切换XY视图模式
    void updateXYView();                     ///< 更新XY视图显示
    void onXYChannelChanged();               ///< XY通道选择改变

private:
    void setupUi();
    void setupToolBar();
    void setupMenuBar();
    void setupShortcuts();
    void setupPlot();
    void retranslateUi();
    QColor getDefaultColor(int index);
    void ensureCurveExists(int index);
    void trimData();
    bool shouldDecimate();  ///< 检查是否应该抽稀当前数据点
    void applyRenderQualityMode();  ///< 应用渲染质量策略

private:
    QString m_windowId;                    ///< 窗口ID
    QCustomPlot* m_plot;                   ///< 绑图控件
    QTimer* m_updateTimer;                 ///< 更新定时器

    // 工具栏控件
    QAction* m_pauseAction;
    QAction* m_gridAction;
    QAction* m_legendAction;
    QAction* m_openGLAction;
    QAction* m_scrollModeAction;           ///< 滚动模式开关
    QAction* m_renderQualityHighAction = nullptr;      ///< 高质量渲染动作
    QAction* m_renderQualityPerformanceAction = nullptr;  ///< 高性能渲染动作
    QComboBox* m_renderQualityCombo = nullptr;         ///< 工具栏渲染质量切换
    QLabel* m_renderQualityLabel = nullptr;            ///< 工具栏渲染标签
    QLabel* m_visiblePointsLabel = nullptr;            ///< 工具栏显示点数标签
    QSpinBox* m_visiblePointsSpin = nullptr;           ///< 工具栏显示点数输入
    PlotControlPanel* m_controlPanel = nullptr;  ///< 控制面板

    // 数据管理
    int m_dataIndex = 0;                   ///< 自动递增的X轴索引
    int m_maxDataPoints = 5000;            ///< 最大数据点数（降低以提升性能）
    bool m_autoScale = true;               ///< 自动缩放
    bool m_paused = false;                 ///< 暂停状态
    bool m_needReplot = false;             ///< 需要重绘标志
    bool m_openGLEnabled = true;           ///< OpenGL加速（默认开启）
    RenderQualityMode m_renderQualityMode = RenderQualityMode::HighQuality;  ///< 渲染质量模式
    DecimationRatio m_decimationRatio = DecimationRatio::None;  ///< 数据抽稀比例
    int m_decimationCounter = 0;           ///< 抽稀计数器
    bool m_throttleAutoRangeUpdates = false;  ///< 是否节流Y轴范围更新
    int m_autoRangeUpdateCounter = 0;      ///< 自动范围更新节流计数器（高性能模式）
    double m_sampleRate = 1000.0;          ///< 采样率 (Hz)，用于FFT分析

    // 滚动模式
    bool m_scrollMode = true;              ///< 滚动模式（固定宽度）
    int m_visiblePoints = 300;             ///< 可见数据点数（默认300，更宽松）
    QScrollBar* m_scrollBar = nullptr;     ///< 水平滚动条

    // 数值显示
    QWidget* m_valuePanel = nullptr;       ///< 数值显示面板
    QVector<QLabel*> m_valueLabels;        ///< 各曲线数值标签
    QVector<double> m_latestValues;        ///< 各曲线最新值
    int m_lastValueCount = 0;              ///< 上次显示的数值数量

    // 鼠标光标跟踪
    QCPItemLine* m_cursorLine = nullptr;   ///< 垂直光标线
    QCPItemText* m_cursorText = nullptr;   ///< 光标数值文本
    QVector<QCPItemTracer*> m_tracers;     ///< 各曲线追踪点
    bool m_cursorEnabled = true;           ///< 是否启用光标
    int m_lastCursorPixelX = 0;            ///< 上次光标像素X位置
    bool m_cursorInPlot = false;           ///< 光标是否在绘图区域内

    // 参考线（设定值）
    QCPItemLine* m_referenceLine = nullptr;  ///< 水平参考线
    QCPItemText* m_referenceLabel = nullptr; ///< 参考线标签
    double m_referenceValue = 0;            ///< 参考值

    // 差值曲线（支持多条）
    struct DiffCurveInfo {
        QCPGraph* graph = nullptr;   ///< 差值曲线图形
        int curve1 = -1;             ///< 源曲线1索引
        int curve2 = -1;             ///< 源曲线2索引
        QString name;                ///< 差值曲线名称
    };
    QVector<DiffCurveInfo> m_diffCurves;  ///< 所有差值曲线
    QSet<QCPGraph*> m_staticCurves;       ///< 静态曲线（仅历史对比用，不参与实时裁剪）

    // 实时滤波配置
    struct RealTimeFilterInfo {
        QCPGraph* filterGraph = nullptr;     ///< 滤波曲线
        int sourceCurveIndex = -1;           ///< 源曲线索引
        FilterConfig config;                 ///< 滤波配置
        FilterState state;                   ///< 滤波状态（用于实时滤波）
    };
    QVector<RealTimeFilterInfo> m_realTimeFilters;  ///< 实时滤波列表

    // 统计信息显示
    QWidget* m_statsPanel = nullptr;        ///< 统计信息面板
    QVector<QLabel*> m_statsLabels;         ///< 统计标签
    bool m_showStats = true;                ///< 是否显示统计

    // 多Y轴支持
    QCPAxis* m_rightYAxis = nullptr;        ///< 右侧Y轴
    QSet<int> m_rightAxisCurves;            ///< 使用右轴的曲线索引集合
    bool m_rightAxisVisible = false;        ///< 右轴是否可见

    // 测量游标
    struct MeasureCursor {
        QCPItemLine* line = nullptr;        ///< 垂直线
        QCPItemText* label = nullptr;       ///< 标签
        double xPos = 0;                    ///< X位置
        bool active = false;                ///< 是否激活
    };
    MeasureCursor m_measureCursor1;         ///< 测量游标1
    MeasureCursor m_measureCursor2;         ///< 测量游标2
    QCPItemText* m_measureDeltaText = nullptr;  ///< 差值显示文本
    bool m_measureModeEnabled = false;      ///< 测量模式是否启用
    int m_measureCursorToMove = 0;          ///< 正在移动的游标（0=无，1=游标1，2=游标2）

    // 触发捕获
    enum class TriggerType {
        RisingEdge,    ///< 上升沿触发
        FallingEdge,   ///< 下降沿触发
        LevelHigh,     ///< 高电平触发
        LevelLow       ///< 低电平触发
    };

    struct TriggerConfig {
        bool enabled = false;              ///< 是否启用触发
        int sourceCurve = 0;               ///< 触发源曲线
        TriggerType type = TriggerType::RisingEdge;  ///< 触发类型
        double level = 0;                  ///< 触发电平
        int preTriggerPoints = 100;        ///< 触发前数据点数
        int postTriggerPoints = 400;       ///< 触发后数据点数
        bool singleShot = false;           ///< 单次触发模式
    };

    TriggerConfig m_triggerConfig;         ///< 触发配置
    bool m_triggerArmed = false;           ///< 触发已准备
    bool m_triggerFired = false;           ///< 触发已触发
    double m_lastTriggerValue = 0;         ///< 上次触发源值
    int m_triggerPosition = 0;             ///< 触发位置
    QVector<QVector<double>> m_preTriggerBuffer;  ///< 触发前缓冲
    QCPItemLine* m_triggerLevelLine = nullptr;    ///< 触发电平线
    QCPItemText* m_triggerStatusText = nullptr;   ///< 触发状态文本

    // 峰值自动标注
    struct PeakAnnotationConfig {
        bool enabled = false;              ///< 是否启用峰值标注
        int targetCurve = 0;               ///< 目标曲线
        double threshold = 0.03;           ///< 峰值阈值（相对于振幅，降低到3%更容易检测）
        int minDistance = 3;               ///< 相邻峰值最小间距（点数，降低到3更灵敏）
        bool showMaxPeaks = true;          ///< 显示极大值
        bool showMinPeaks = false;         ///< 显示极小值
        int maxPeakCount = 20;             ///< 最大标注峰值数量
    };

    PeakAnnotationConfig m_peakConfig;     ///< 峰值标注配置
    QVector<QCPAbstractItem*> m_peakMarkers; ///< 峰值标记（椭圆）
    QVector<QCPItemText*> m_peakLabels;    ///< 峰值标签

    // 数据报警
    AlarmConfig m_alarmConfig;             ///< 报警配置
    bool m_alarmActive = false;            ///< 报警是否激活
    double m_lastAlarmValue = 0;           ///< 上次报警检测值
    QCPItemLine* m_upperLimitLine = nullptr;   ///< 上限线
    QCPItemLine* m_lowerLimitLine = nullptr;   ///< 下限线
    QCPItemText* m_alarmStatusText = nullptr;  ///< 报警状态文本
    QTimer* m_alarmBlinkTimer = nullptr;   ///< 报警闪烁定时器
    int m_alarmCount = 0;                  ///< 报警计数

    // 数据标记
    QVector<DataMarker> m_dataMarkers;     ///< 数据标记列表

    // 采样率计算
    qint64 m_lastDataTime = 0;             ///< 上次数据时间
    int m_dataCountForRate = 0;            ///< 用于计算采样率的数据计数
    double m_calculatedSampleRate = 0;     ///< 计算得到的采样率

    // 实时FFT窗口连接
    struct RealTimeFFTConnection {
        RealTimeFFTWindow* window = nullptr;  ///< 实时FFT窗口指针
        int curveIndex = -1;                  ///< 关联的曲线索引
    };
    QVector<RealTimeFFTConnection> m_realTimeFFTWindows;  ///< 实时FFT窗口列表
    void updateRealTimeFFT(int curveIndex, double value);  ///< 更新实时FFT数据
    void cleanupClosedFFTWindows();                        ///< 清理已关闭的FFT窗口

    // 直方图视图
    PlotViewMode m_viewMode = PlotViewMode::TimeSeries;  ///< 当前视图模式
    QCPBars* m_histogramBars = nullptr;                  ///< 直方图柱状图
    int m_histogramCurveIndex = 0;                       ///< 直方图目标曲线索引
    int m_histogramBinCount = 50;                        ///< 直方图分箱数量
    QAction* m_histogramAction = nullptr;                ///< 直方图切换动作
    qint64 m_lastHistogramUpdateMs = 0;                  ///< 直方图上次刷新时间
    bool m_forceHistogramRefresh = false;                ///< 强制刷新直方图一次

    // XY视图模式
    QAction* m_xyViewAction = nullptr;                   ///< XY视图切换动作
    QCPCurve* m_xyCurve = nullptr;                       ///< XY曲线对象
    int m_xyChannelX = 0;                                ///< X轴数据通道索引
    int m_xyChannelY = 1;                                ///< Y轴数据通道索引
    QComboBox* m_xyChannelXCombo = nullptr;              ///< X通道选择下拉框
    QComboBox* m_xyChannelYCombo = nullptr;              ///< Y通道选择下拉框
    QWidget* m_xyControlWidget = nullptr;                ///< XY控制面板容器
    qint64 m_lastXYUpdateMs = 0;                         ///< XY视图上次刷新时间
    bool m_forceXYRefresh = false;                       ///< 强制刷新XY视图一次

    // 默认颜色列表
    static const QVector<QColor> s_defaultColors;
};

} // namespace ComAssistant

#endif // PLOTTERWINDOW_H
