/**
 * @file PlotterWindow.cpp
 * @brief 绘图窗口类实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "PlotterWindow.h"
#include "PlotterManager.h"
#include "PlotRenderQuality.h"
#include "SpectrumWindow.h"
#include "RealTimeFFTWindow.h"
#include "dialogs/FFTSettingsDialog.h"
#include "widgets/PlotControlPanel.h"
#include "core/utils/Logger.h"
#include "core/utils/FFTUtils.h"
#include "core/utils/FilterUtils.h"

#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QKeyEvent>
#include <QDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QActionGroup>
#include <QLabel>
#include <QSignalBlocker>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QListWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QColorDialog>
#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QStyle>
#include <QPalette>
#include <QLinearGradient>
#include <cmath>
#include <limits>

namespace ComAssistant {

namespace {

QPen createCurvePen(const QColor& color, double width, Qt::PenStyle style = Qt::SolidLine)
{
    QPen pen(color, width, style);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

const QCPGraph* findReferenceGraphForScroll(const QCustomPlot* plot)
{
    if (!plot) {
        return nullptr;
    }

    const QCPGraph* reference = nullptr;
    double latestKey = std::numeric_limits<double>::lowest();
    int bestSize = 0;

    for (int i = 0; i < plot->graphCount(); ++i) {
        QCPGraph* graph = plot->graph(i);
        if (!graph || !graph->visible() || graph->data()->isEmpty()) {
            continue;
        }

        auto lastIt = graph->data()->constEnd();
        --lastIt;
        const double graphLastKey = lastIt->key;
        const int graphSize = graph->data()->size();

        if (!reference || graphLastKey > latestKey ||
            (qFuzzyCompare(graphLastKey + 1.0, latestKey + 1.0) && graphSize > bestSize)) {
            reference = graph;
            latestKey = graphLastKey;
            bestSize = graphSize;
        }
    }

    return reference;
}

bool resolveKeyRangeByPointWindow(const QCPGraph* graph,
                                  int startIndex,
                                  int pointCount,
                                  double& keyMin,
                                  double& keyMax)
{
    keyMin = 0.0;
    keyMax = 0.0;

    if (!graph || pointCount <= 0 || graph->data()->isEmpty()) {
        return false;
    }

    const int dataSize = graph->data()->size();
    const int boundedStart = qBound(0, startIndex, qMax(0, dataSize - 1));
    const int boundedEnd = qBound(boundedStart, boundedStart + pointCount - 1, dataSize - 1);

    // O(1) 随机访问：QCPDataContainer 底层为 QVector，支持迭代器算术
    auto startIt = graph->data()->constBegin() + boundedStart;
    keyMin = startIt->key;

    auto endIt = graph->data()->constBegin() + boundedEnd;
    keyMax = endIt->key;

    if (keyMax < keyMin) {
        qSwap(keyMin, keyMax);
    }
    if (qFuzzyCompare(keyMin + 1.0, keyMax + 1.0)) {
        keyMax = keyMin + 1.0;
    }

    return true;
}

bool calculateAxisValueRange(const QCustomPlot* plot,
                             const QCPAxis* valueAxis,
                             const QCPRange& keyRange,
                             double& minValue,
                             double& maxValue)
{
    minValue = std::numeric_limits<double>::max();
    maxValue = std::numeric_limits<double>::lowest();
    bool foundAny = false;

    if (!plot || !valueAxis) {
        return false;
    }

    for (int i = 0; i < plot->graphCount(); ++i) {
        QCPGraph* graph = plot->graph(i);
        if (!graph || graph->valueAxis() != valueAxis || !graph->visible() || graph->data()->isEmpty()) {
            continue;
        }

        bool foundRange = false;
        const QCPRange graphRange = graph->getValueRange(foundRange, QCP::sdBoth, keyRange);
        if (!foundRange) {
            continue;
        }

        minValue = qMin(minValue, graphRange.lower);
        maxValue = qMax(maxValue, graphRange.upper);
        foundAny = true;
    }

    return foundAny;
}

void applySmoothedAxisRange(QCPAxis* axis, double minValue, double maxValue)
{
    if (!axis) {
        return;
    }

    if (minValue > maxValue) {
        qSwap(minValue, maxValue);
    }

    if (qFuzzyCompare(minValue + 1.0, maxValue + 1.0)) {
        minValue -= 0.5;
        maxValue += 0.5;
    }

    const double span = qMax(maxValue - minValue, 1e-6);
    const double padding = qMax(span * 0.08, 0.002);
    const double targetLower = minValue - padding;
    const double targetUpper = maxValue + padding;

    const QCPRange currentRange = axis->range();
    if (!std::isfinite(currentRange.lower) || !std::isfinite(currentRange.upper) || currentRange.size() <= 0) {
        axis->setRange(targetLower, targetUpper);
        return;
    }

    // 平滑过渡，减少实时波形在自动缩放时的“抖动感”。
    const double smoothing = 0.24;
    double lower = currentRange.lower + (targetLower - currentRange.lower) * smoothing;
    double upper = currentRange.upper + (targetUpper - currentRange.upper) * smoothing;

    if (upper - lower < 1e-6) {
        lower -= 0.5;
        upper += 0.5;
    }

    axis->setRange(lower, upper);
}

}  // namespace

// 默认颜色列表
const QVector<QColor> PlotterWindow::s_defaultColors = {
    QColor(31, 119, 180),   // 蓝色
    QColor(255, 127, 14),   // 橙色
    QColor(44, 160, 44),    // 绿色
    QColor(214, 39, 40),    // 红色
    QColor(148, 103, 189),  // 紫色
    QColor(140, 86, 75),    // 棕色
    QColor(227, 119, 194),  // 粉色
    QColor(127, 127, 127),  // 灰色
    QColor(188, 189, 34),   // 黄绿色
    QColor(23, 190, 207),   // 青色
    QColor(255, 187, 120),  // 浅橙色
    QColor(152, 223, 138),  // 浅绿色
    QColor(255, 152, 150),  // 浅红色
    QColor(197, 176, 213),  // 浅紫色
    QColor(196, 156, 148),  // 浅棕色
    QColor(247, 182, 210)   // 浅粉色
};

PlotterWindow::PlotterWindow(const QString& windowId, QWidget* parent)
    : QMainWindow(parent)
    , m_windowId(windowId)
    , m_plot(nullptr)
    , m_updateTimer(nullptr)
    , m_pauseAction(nullptr)
    , m_gridAction(nullptr)
    , m_legendAction(nullptr)
    , m_openGLAction(nullptr)
{
    setObjectName("plotterWindow");

    setupUi();
    setupToolBar();
    setupMenuBar();
    setupShortcuts();
    setupPlot();

    setWindowTitle(tr("绘图窗口 - %1").arg(windowId));
    resize(980, 620);

    // 启动更新定时器（默认30fps，具体由渲染质量模式再调整）
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &PlotterWindow::updatePlot);
    m_updateTimer->start(33);
    applyRenderQualityMode();

    // 创建控制面板
    m_controlPanel = new PlotControlPanel(this, this);
    addDockWidget(Qt::RightDockWidgetArea, m_controlPanel);

    // 连接控制面板信号
    connect(m_controlPanel, &PlotControlPanel::curveVisibilityChanged, this, [this](int index, bool visible) {
        if (index >= 0 && index < m_plot->graphCount()) {
            m_plot->graph(index)->setVisible(visible);
            m_plot->replot(QCustomPlot::rpQueuedReplot);
        }
    });
    connect(m_controlPanel, &PlotControlPanel::curveColorChanged, this, [this](int index, const QColor& color) {
        if (!color.isValid() || index < 0 || index >= m_plot->graphCount()) {
            return;
        }

        QCPGraph* graph = m_plot->graph(index);
        if (!graph) {
            return;
        }

        QPen pen = graph->pen();
        pen.setColor(color);
        graph->setPen(pen);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });
    connect(m_controlPanel, &PlotControlPanel::xRangeChanged, this, &PlotterWindow::setXAxisRange);
    connect(m_controlPanel, &PlotControlPanel::yRangeChanged, this, &PlotterWindow::setYAxisRange);
    connect(m_controlPanel, &PlotControlPanel::autoScaleChanged, this, &PlotterWindow::setAutoScale);
    connect(m_controlPanel, &PlotControlPanel::gridVisibleChanged, this, [this](bool visible) {
        m_plot->xAxis->grid()->setVisible(visible);
        m_plot->yAxis->grid()->setVisible(visible);
        m_gridAction->setChecked(visible);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });
    connect(m_controlPanel, &PlotControlPanel::legendVisibleChanged, this, [this](bool visible) {
        m_plot->legend->setVisible(visible);
        m_legendAction->setChecked(visible);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });
    connect(m_controlPanel, &PlotControlPanel::maxDataPointsChanged, this, [this](int points) {
        m_maxDataPoints = points;
    });
    connect(m_controlPanel, &PlotControlPanel::differenceCurveRequested,
            this, &PlotterWindow::onShowDifferenceClicked);
    m_controlPanel->updateCurveList();

    LOG_INFO(QString("PlotterWindow created: %1").arg(windowId));
}

PlotterWindow::~PlotterWindow()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    LOG_INFO(QString("PlotterWindow destroyed: %1").arg(m_windowId));
}

void PlotterWindow::setupUi()
{
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("plotterCentralWidget");
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // 绑图控件
    m_plot = new QCustomPlot(centralWidget);
    m_plot->setObjectName("plotCanvas");
    mainLayout->addWidget(m_plot, 1);

    // 水平滚动条
    m_scrollBar = new QScrollBar(Qt::Horizontal, centralWidget);
    m_scrollBar->setObjectName("plotScrollBar");
    m_scrollBar->setRange(0, 0);
    m_scrollBar->setPageStep(m_visiblePoints);
    connect(m_scrollBar, &QScrollBar::valueChanged, this, &PlotterWindow::onScrollBarChanged);
    mainLayout->addWidget(m_scrollBar);

    // 数值显示面板
    m_valuePanel = new QWidget(centralWidget);
    m_valuePanel->setObjectName("plotValuePanel");
    QVBoxLayout* valuePanelLayout = new QVBoxLayout(m_valuePanel);
    valuePanelLayout->setContentsMargins(12, 8, 12, 8);
    valuePanelLayout->setSpacing(4);
    mainLayout->addWidget(m_valuePanel);

    setCentralWidget(centralWidget);
}

void PlotterWindow::setupToolBar()
{
    QToolBar* toolBar = addToolBar(tr("绘图工具栏"));
    toolBar->setObjectName("plotToolBar");
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(18, 18));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // 暂停/继续
    m_pauseAction = toolBar->addAction(style()->standardIcon(QStyle::SP_MediaPause), tr("暂停"));
    m_pauseAction->setCheckable(true);
    m_pauseAction->setShortcut(QKeySequence(Qt::Key_Space));
    connect(m_pauseAction, &QAction::triggered, this, &PlotterWindow::onPauseToggled);

    // 清空
    QAction* clearAction = toolBar->addAction(style()->standardIcon(QStyle::SP_DialogResetButton),
                                              tr("清空"),
                                              this,
                                              &PlotterWindow::onClearClicked);
    clearAction->setShortcut(QKeySequence("Ctrl+Shift+C"));

    // 自动缩放
    QAction* autoScaleAction = toolBar->addAction(style()->standardIcon(QStyle::SP_BrowserReload),
                                                  tr("自动缩放"),
                                                  this,
                                                  &PlotterWindow::onAutoScaleClicked);
    autoScaleAction->setShortcut(QKeySequence("Ctrl+A"));

    toolBar->addSeparator();

    // 滚动模式
    m_scrollModeAction = toolBar->addAction(tr("滚动模式"));
    m_scrollModeAction->setCheckable(true);
    m_scrollModeAction->setChecked(m_scrollMode);
    m_scrollModeAction->setToolTip(tr("固定宽度，新数据从右侧进入"));
    connect(m_scrollModeAction, &QAction::toggled, this, &PlotterWindow::onScrollModeToggled);

    m_renderQualityLabel = new QLabel(tr("  渲染:"));
    toolBar->addWidget(m_renderQualityLabel);

    m_renderQualityCombo = new QComboBox(toolBar);
    m_renderQualityCombo->setObjectName("plotRenderQualityCombo");
    m_renderQualityCombo->setToolTip(tr("切换波形渲染质量档位"));
    m_renderQualityCombo->addItem(tr("高质量"), static_cast<int>(RenderQualityMode::HighQuality));
    m_renderQualityCombo->addItem(tr("高性能"), static_cast<int>(RenderQualityMode::HighPerformance));
    m_renderQualityCombo->setCurrentIndex(m_renderQualityMode == RenderQualityMode::HighQuality ? 0 : 1);
    connect(m_renderQualityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) {
            return;
        }
        setRenderQualityMode(static_cast<RenderQualityMode>(m_renderQualityCombo->itemData(index).toInt()));
    });
    toolBar->addWidget(m_renderQualityCombo);

    // 可见点数设置
    m_visiblePointsLabel = new QLabel(tr("  显示点数:"));
    toolBar->addWidget(m_visiblePointsLabel);

    m_visiblePointsSpin = new QSpinBox;
    m_visiblePointsSpin->setRange(50, 5000);
    m_visiblePointsSpin->setSingleStep(50);
    m_visiblePointsSpin->setValue(m_visiblePoints);
    m_visiblePointsSpin->setToolTip(tr("设置可见的数据点数量"));
    connect(m_visiblePointsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        m_visiblePoints = value;
        if (m_scrollBar) {
            m_scrollBar->setPageStep(value);
        }
        m_needReplot = true;
    });
    toolBar->addWidget(m_visiblePointsSpin);

    toolBar->addSeparator();

    // 网格
    m_gridAction = toolBar->addAction(tr("网格"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(true);
    m_gridAction->setShortcut(QKeySequence("Ctrl+G"));
    connect(m_gridAction, &QAction::toggled, this, &PlotterWindow::onGridToggled);

    // 图例
    m_legendAction = toolBar->addAction(tr("图例"));
    m_legendAction->setCheckable(true);
    m_legendAction->setChecked(true);
    m_legendAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(m_legendAction, &QAction::toggled, this, &PlotterWindow::onLegendToggled);

    toolBar->addSeparator();

    QAction* differenceAction = toolBar->addAction(tr("差值曲线"));
    differenceAction->setToolTip(tr("管理两条曲线的差值波形"));
    differenceAction->setShortcut(QKeySequence("Ctrl+D"));
    connect(differenceAction, &QAction::triggered, this, &PlotterWindow::onShowDifferenceClicked);

    toolBar->addSeparator();

    // 导出图片
    QAction* exportImageAction = toolBar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                                                    tr("保存图片"),
                                                    this,
                                                    &PlotterWindow::onExportImageClicked);
    exportImageAction->setShortcut(QKeySequence("Ctrl+S"));

    // 导出数据
    QAction* exportDataAction = toolBar->addAction(style()->standardIcon(QStyle::SP_FileIcon),
                                                   tr("导出数据"),
                                                   this,
                                                   &PlotterWindow::onExportDataClicked);
    exportDataAction->setShortcut(QKeySequence("Ctrl+E"));

    toolBar->addSeparator();
    m_perfDiagLabel = new QLabel(tr("性能: 等待数据..."), toolBar);
    m_perfDiagLabel->setObjectName("plotPerfDiagLabel");
    m_perfDiagLabel->setMinimumWidth(260);
    m_perfDiagLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_perfDiagLabel->setVisible(m_showPerfDiag);
    toolBar->addWidget(m_perfDiagLabel);
}

void PlotterWindow::setupMenuBar()
{
    QMenuBar* menuBar = this->menuBar();

    // 文件菜单
    QMenu* fileMenu = menuBar->addMenu(tr("文件(&F)"));
    fileMenu->addAction(tr("保存图片(&S)..."), this, &PlotterWindow::onExportImageClicked, QKeySequence("Ctrl+S"));
    fileMenu->addAction(tr("导出数据(&E)..."), this, &PlotterWindow::onExportDataClicked, QKeySequence("Ctrl+E"));
    fileMenu->addSeparator();
    fileMenu->addAction(tr("关闭(&C)"), this, &QWidget::close, QKeySequence("Ctrl+W"));

    // 视图菜单
    QMenu* viewMenu = menuBar->addMenu(tr("视图(&V)"));
    viewMenu->addAction(m_gridAction);
    viewMenu->addAction(m_legendAction);
    viewMenu->addSeparator();
    viewMenu->addAction(tr("自动缩放(&A)"), this, &PlotterWindow::onAutoScaleClicked, QKeySequence("Ctrl+A"));
    viewMenu->addSeparator();

    // 直方图视图
    m_histogramAction = viewMenu->addAction(tr("直方图视图(&H)"));
    m_histogramAction->setCheckable(true);
    m_histogramAction->setChecked(false);
    m_histogramAction->setShortcut(QKeySequence("Ctrl+H"));
    connect(m_histogramAction, &QAction::triggered, this, &PlotterWindow::onHistogramViewClicked);

    // XY视图
    m_xyViewAction = viewMenu->addAction(tr("XY视图(&Y)"));
    m_xyViewAction->setCheckable(true);
    m_xyViewAction->setChecked(false);
    m_xyViewAction->setShortcut(QKeySequence("Ctrl+Y"));
    connect(m_xyViewAction, &QAction::triggered, this, &PlotterWindow::onXYViewClicked);

    // 设置菜单
    QMenu* settingsMenu = menuBar->addMenu(tr("设置(&S)"));

    // OpenGL 加速
    m_openGLAction = settingsMenu->addAction(tr("启用 OpenGL 加速(&O)"));
    m_openGLAction->setCheckable(true);
    m_openGLAction->setChecked(m_openGLEnabled);
    connect(m_openGLAction, &QAction::toggled, this, &PlotterWindow::onOpenGLToggled);

    settingsMenu->addSeparator();

    m_diffRealtimeAction = settingsMenu->addAction(tr("差值曲线实时更新"));
    m_diffRealtimeAction->setCheckable(true);
    m_diffRealtimeAction->setChecked(m_diffRealtimeEnabled);
    connect(m_diffRealtimeAction, &QAction::toggled, this, [this](bool checked) {
        m_diffRealtimeEnabled = checked;
    });

    m_filterRealtimeAction = settingsMenu->addAction(tr("滤波曲线实时更新"));
    m_filterRealtimeAction->setCheckable(true);
    m_filterRealtimeAction->setChecked(m_filterRealtimeEnabled);
    connect(m_filterRealtimeAction, &QAction::toggled, this, [this](bool checked) {
        m_filterRealtimeEnabled = checked;
    });

    m_perfDiagAction = settingsMenu->addAction(tr("显示性能诊断"));
    m_perfDiagAction->setCheckable(true);
    m_perfDiagAction->setChecked(m_showPerfDiag);
    connect(m_perfDiagAction, &QAction::toggled, this, [this](bool checked) {
        m_showPerfDiag = checked;
        if (m_perfDiagLabel) {
            m_perfDiagLabel->setVisible(checked);
            if (checked && m_perfDiagLabel->text().isEmpty()) {
                m_perfDiagLabel->setText(tr("性能: 等待数据..."));
            }
        }
    });

    settingsMenu->addSeparator();

    // 波形渲染质量模式
    QMenu* renderQualityMenu = settingsMenu->addMenu(tr("波形渲染质量(&R)"));
    QActionGroup* renderQualityGroup = new QActionGroup(this);
    renderQualityGroup->setExclusive(true);

    m_renderQualityHighAction = renderQualityMenu->addAction(tr("高质量（细腻）"));
    m_renderQualityHighAction->setCheckable(true);
    m_renderQualityHighAction->setData(static_cast<int>(RenderQualityMode::HighQuality));
    renderQualityGroup->addAction(m_renderQualityHighAction);

    m_renderQualityPerformanceAction = renderQualityMenu->addAction(tr("高性能（流畅）"));
    m_renderQualityPerformanceAction->setCheckable(true);
    m_renderQualityPerformanceAction->setData(static_cast<int>(RenderQualityMode::HighPerformance));
    renderQualityGroup->addAction(m_renderQualityPerformanceAction);

    if (m_renderQualityMode == RenderQualityMode::HighQuality) {
        m_renderQualityHighAction->setChecked(true);
    } else {
        m_renderQualityPerformanceAction->setChecked(true);
    }

    connect(renderQualityGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        if (!action) {
            return;
        }
        setRenderQualityMode(static_cast<RenderQualityMode>(action->data().toInt()));
    });

    settingsMenu->addSeparator();

    // 数据抽稀子菜单
    QMenu* decimationMenu = settingsMenu->addMenu(tr("数据抽稀(&D)"));
    QActionGroup* decimationGroup = new QActionGroup(this);

    QAction* dec1 = decimationMenu->addAction(tr("1:1 (无抽稀)"));
    dec1->setCheckable(true);
    dec1->setChecked(true);
    dec1->setData(1);
    decimationGroup->addAction(dec1);

    QAction* dec2 = decimationMenu->addAction(tr("1:2"));
    dec2->setCheckable(true);
    dec2->setData(2);
    decimationGroup->addAction(dec2);

    QAction* dec5 = decimationMenu->addAction(tr("1:5"));
    dec5->setCheckable(true);
    dec5->setData(5);
    decimationGroup->addAction(dec5);

    QAction* dec10 = decimationMenu->addAction(tr("1:10"));
    dec10->setCheckable(true);
    dec10->setData(10);
    decimationGroup->addAction(dec10);

    connect(decimationGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        int ratio = action->data().toInt();
        setDecimationRatio(static_cast<DecimationRatio>(ratio));
    });

    settingsMenu->addSeparator();

    // 最大数据点数
    settingsMenu->addAction(tr("设置最大数据点数(&M)..."), this, &PlotterWindow::onShowSettingsDialog);

    // 曲线菜单
    QMenu* curveMenu = menuBar->addMenu(tr("曲线(&C)"));
    curveMenu->addAction(tr("重命名曲线(&R)..."), this, &PlotterWindow::onRenameCurveClicked);
    curveMenu->addAction(tr("曲线样式(&L)..."), this, [this]() {
        if (m_plot->graphCount() == 0) {
            QMessageBox::information(this, tr("提示"), tr("当前没有曲线"));
            return;
        }

        QDialog dialog(this);
        dialog.setWindowTitle(tr("曲线样式设置"));
        QFormLayout* layout = new QFormLayout(&dialog);

        // 曲线选择
        QComboBox* curveCombo = new QComboBox;
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            curveCombo->addItem(m_plot->graph(i)->name(), i);
        }
        layout->addRow(tr("选择曲线:"), curveCombo);

        // 线宽
        QDoubleSpinBox* lineWidthSpin = new QDoubleSpinBox;
        lineWidthSpin->setRange(0.5, 10.0);
        lineWidthSpin->setSingleStep(0.5);
        lineWidthSpin->setValue(m_plot->graph(0)->pen().widthF());
        layout->addRow(tr("线宽:"), lineWidthSpin);

        // 线型
        QComboBox* lineStyleCombo = new QComboBox;
        lineStyleCombo->addItem(tr("实线"), static_cast<int>(Qt::SolidLine));
        lineStyleCombo->addItem(tr("虚线"), static_cast<int>(Qt::DashLine));
        lineStyleCombo->addItem(tr("点线"), static_cast<int>(Qt::DotLine));
        lineStyleCombo->addItem(tr("点划线"), static_cast<int>(Qt::DashDotLine));
        layout->addRow(tr("线型:"), lineStyleCombo);

        // 点型
        QComboBox* scatterCombo = new QComboBox;
        scatterCombo->addItem(tr("无"), static_cast<int>(QCPScatterStyle::ssNone));
        scatterCombo->addItem(tr("圆点"), static_cast<int>(QCPScatterStyle::ssCircle));
        scatterCombo->addItem(tr("方块"), static_cast<int>(QCPScatterStyle::ssSquare));
        scatterCombo->addItem(tr("菱形"), static_cast<int>(QCPScatterStyle::ssDiamond));
        scatterCombo->addItem(tr("三角形"), static_cast<int>(QCPScatterStyle::ssTriangle));
        scatterCombo->addItem(tr("十字"), static_cast<int>(QCPScatterStyle::ssCross));
        scatterCombo->addItem(tr("加号"), static_cast<int>(QCPScatterStyle::ssPlus));
        layout->addRow(tr("点型:"), scatterCombo);

        // 更新显示当前曲线的设置
        auto updateCurrentCurve = [&]() {
            int idx = curveCombo->currentData().toInt();
            if (idx >= 0 && idx < m_plot->graphCount()) {
                QCPGraph* graph = m_plot->graph(idx);
                lineWidthSpin->setValue(graph->pen().widthF());

                int styleIdx = lineStyleCombo->findData(static_cast<int>(graph->pen().style()));
                if (styleIdx >= 0) lineStyleCombo->setCurrentIndex(styleIdx);

                int scatterIdx = scatterCombo->findData(static_cast<int>(graph->scatterStyle().shape()));
                if (scatterIdx >= 0) scatterCombo->setCurrentIndex(scatterIdx);
            }
        };
        connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), updateCurrentCurve);
        updateCurrentCurve();

        QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        layout->addRow(buttonBox);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() == QDialog::Accepted) {
            int idx = curveCombo->currentData().toInt();
            setCurveLineWidth(idx, lineWidthSpin->value());
            setCurveLineStyle(idx, static_cast<Qt::PenStyle>(lineStyleCombo->currentData().toInt()));
            setCurveScatterStyle(idx, static_cast<QCPScatterStyle::ScatterShape>(scatterCombo->currentData().toInt()));
            m_plot->replot(QCustomPlot::rpQueuedReplot);
        }
    });
    curveMenu->addSeparator();
    curveMenu->addAction(tr("显示曲线差值(&D)..."), this, &PlotterWindow::onShowDifferenceClicked);
    curveMenu->addSeparator();
    curveMenu->addAction(tr("配置Y轴(&Y)..."), this, &PlotterWindow::onConfigureYAxisClicked);

    // 分析菜单
    QMenu* analysisMenu = menuBar->addMenu(tr("分析(&A)"));
    analysisMenu->addAction(tr("FFT 频谱分析(&F)..."), this, &PlotterWindow::onFFTAnalysisClicked, QKeySequence("Ctrl+F"));
    analysisMenu->addAction(tr("测量游标(&M)..."), this, &PlotterWindow::onMeasureCursorClicked, QKeySequence("Ctrl+M"));
    analysisMenu->addAction(tr("数字滤波(&D)..."), this, &PlotterWindow::onFilterCurveClicked);
    analysisMenu->addAction(tr("触发捕获(&T)..."), this, &PlotterWindow::onTriggerCaptureClicked, QKeySequence("Ctrl+T"));
    analysisMenu->addAction(tr("峰值标注(&K)..."), this, &PlotterWindow::onPeakAnnotationClicked, QKeySequence("Ctrl+K"));
    analysisMenu->addSeparator();
    analysisMenu->addAction(tr("增强统计分析(&S)..."), this, &PlotterWindow::onAdvancedStatsClicked);
    analysisMenu->addAction(tr("数据报警配置(&L)..."), this, &PlotterWindow::onAlarmConfigClicked);
    analysisMenu->addSeparator();
    analysisMenu->addAction(tr("添加数据标记(&B)..."), this, &PlotterWindow::onAddMarkerClicked);
    analysisMenu->addAction(tr("清除所有标记"), this, &PlotterWindow::clearAllMarkers);
    analysisMenu->addSeparator();
    analysisMenu->addAction(tr("设置参考线(&R)..."), this, &PlotterWindow::onSetReferenceLineClicked);
    analysisMenu->addAction(tr("PID 性能分析(&P)..."), this, &PlotterWindow::onPIDAnalysisClicked);
    analysisMenu->addSeparator();
    analysisMenu->addAction(tr("嵌入式场景预设(&E)..."), this, &PlotterWindow::onApplyScenePresetClicked);
}

void PlotterWindow::setupShortcuts()
{
    // 快捷键已在 setupToolBar 和 setupMenuBar 中设置
    // 这里可以添加额外的全局快捷键
}

void PlotterWindow::keyPressEvent(QKeyEvent* event)
{
    // 空格键暂停/继续
    if (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier) {
        m_pauseAction->toggle();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void PlotterWindow::setupPlot()
{
    // 设置交互（添加框选放大）
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectAxes);
    m_plot->setSelectionRectMode(QCP::srmZoom);  // 框选放大模式
    m_plot->setSelectionTolerance(8);

    // 默认抗锯齿策略，后续由渲染质量模式进一步覆盖
    m_plot->setNotAntialiasedElements(QCP::aeGrid | QCP::aeAxes | QCP::aeLegend | QCP::aeLegendItems);
    m_plot->setAntialiasedElements(QCP::aePlottables | QCP::aeItems);

    // 默认绘图提示，后续由渲染质量模式进一步覆盖
    m_plot->setNoAntialiasingOnDrag(false);
    m_plot->setPlottingHints(QCP::phCacheLabels);

    // 默认启用 OpenGL 加速。实际启用前会做上下文能力探测，失败时自动回退软件绘制。
    if (m_openGLEnabled) {
        trySetOpenGLEnabled(true);
    }

    const QColor baseColor = palette().color(QPalette::Base);
    QLinearGradient plotBg(0, 0, 0, 1);
    plotBg.setCoordinateMode(QGradient::ObjectBoundingMode);
    plotBg.setColorAt(0.0, baseColor.lighter(106));
    plotBg.setColorAt(1.0, baseColor.darker(104));
    m_plot->axisRect()->setBackground(QBrush(plotBg));

    // 设置坐标轴标签
    m_plot->xAxis->setLabel(tr("X"));
    m_plot->yAxis->setLabel(tr("Y"));
    const QColor axisColor = palette().color(QPalette::Mid).darker(110);
    const QPen axisPen(axisColor, 1.0);
    m_plot->xAxis->setBasePen(axisPen);
    m_plot->yAxis->setBasePen(axisPen);
    m_plot->xAxis->setTickPen(axisPen);
    m_plot->yAxis->setTickPen(axisPen);
    m_plot->xAxis->setSubTickPen(axisPen);
    m_plot->yAxis->setSubTickPen(axisPen);
    m_plot->xAxis->setTickLabelColor(palette().color(QPalette::Text));
    m_plot->yAxis->setTickLabelColor(palette().color(QPalette::Text));
    m_plot->xAxis->setLabelColor(palette().color(QPalette::Text));
    m_plot->yAxis->setLabelColor(palette().color(QPalette::Text));
    m_plot->xAxis->setLabelFont(QFont("Microsoft YaHei", 9, QFont::DemiBold));
    m_plot->yAxis->setLabelFont(QFont("Microsoft YaHei", 9, QFont::DemiBold));
    m_plot->xAxis->setTickLabelFont(QFont("Consolas", 9));
    m_plot->yAxis->setTickLabelFont(QFont("Consolas", 9));

    // 设置网格
    m_plot->xAxis->grid()->setVisible(true);
    m_plot->yAxis->grid()->setVisible(true);
    m_plot->xAxis->grid()->setSubGridVisible(true);
    m_plot->yAxis->grid()->setSubGridVisible(true);
    QPen majorGridPen(axisColor);
    majorGridPen.setStyle(Qt::DashLine);
    majorGridPen.setWidthF(1.0);
    majorGridPen.setColor(QColor(axisColor.red(), axisColor.green(), axisColor.blue(), 90));
    QPen minorGridPen(axisColor);
    minorGridPen.setStyle(Qt::DotLine);
    minorGridPen.setWidthF(1.0);
    minorGridPen.setColor(QColor(axisColor.red(), axisColor.green(), axisColor.blue(), 45));
    m_plot->xAxis->grid()->setPen(majorGridPen);
    m_plot->yAxis->grid()->setPen(majorGridPen);
    m_plot->xAxis->grid()->setSubGridPen(minorGridPen);
    m_plot->yAxis->grid()->setSubGridPen(minorGridPen);

    // 设置图例
    m_plot->legend->setVisible(true);
    QColor legendBg = palette().color(QPalette::Base);
    legendBg.setAlpha(220);
    m_plot->legend->setBrush(QBrush(legendBg));
    m_plot->legend->setBorderPen(QPen(axisColor, 1.0));
    m_plot->legend->setTextColor(palette().color(QPalette::Text));
    m_plot->legend->setFont(QFont("Microsoft YaHei", 9));
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

    // 初始范围
    m_plot->xAxis->setRange(0, 100);
    m_plot->yAxis->setRange(-10, 10);

    // 设置鼠标跟踪
    m_plot->setMouseTracking(true);
    connect(m_plot, &QCustomPlot::mouseMove, this, &PlotterWindow::onMouseMove);

    // 创建垂直光标线
    m_cursorLine = new QCPItemLine(m_plot);
    m_cursorLine->setPen(QPen(Qt::gray, 1, Qt::DashLine));
    m_cursorLine->setVisible(false);

    // 创建光标数值文本
    m_cursorText = new QCPItemText(m_plot);
    m_cursorText->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_cursorText->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_cursorText->position->setCoords(0.02, 0.02);
    m_cursorText->setFont(QFont("Consolas", 9));
    m_cursorText->setPadding(QMargins(5, 5, 5, 5));
    QColor cursorBg = palette().color(QPalette::Base);
    cursorBg.setAlpha(220);
    m_cursorText->setBrush(QBrush(cursorBg));
    m_cursorText->setPen(QPen(axisColor));
    m_cursorText->setColor(palette().color(QPalette::Text));
    m_cursorText->setVisible(false);

    // 双击恢复自动缩放
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, [this](QMouseEvent* event) {
        Q_UNUSED(event);
        m_plot->rescaleAxes();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    });

    // 初始化右侧Y轴
    m_rightYAxis = m_plot->axisRect()->addAxis(QCPAxis::atRight);
    m_rightYAxis->setLabel(tr("Y2"));
    m_rightYAxis->setVisible(false);  // 默认隐藏
    m_rightYAxis->grid()->setVisible(false);
    m_rightYAxis->setBasePen(axisPen);
    m_rightYAxis->setTickPen(axisPen);
    m_rightYAxis->setSubTickPen(axisPen);
    m_rightYAxis->setTickLabelColor(palette().color(QPalette::Text));
    m_rightYAxis->setLabelColor(palette().color(QPalette::Text));

    // 右键菜单
    m_plot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_plot, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(tr("自动缩放"), this, [this]() {
            m_plot->rescaleAxes();
            m_plot->replot(QCustomPlot::rpQueuedReplot);
        });
        menu.addSeparator();
        menu.addAction(tr("暂停/继续"), this, &PlotterWindow::onPauseToggled);
        menu.addAction(tr("清空数据"), this, &PlotterWindow::onClearClicked);
        menu.addSeparator();
        menu.addAction(tr("配置Y轴..."), this, &PlotterWindow::onConfigureYAxisClicked);
        menu.addAction(tr("测量游标..."), this, &PlotterWindow::onMeasureCursorClicked);
        menu.addAction(tr("差值曲线..."), this, &PlotterWindow::onShowDifferenceClicked);
        menu.addSeparator();
        menu.addAction(tr("FFT 分析"), this, &PlotterWindow::onFFTAnalysisClicked);
        menu.addAction(tr("实时 FFT"), this, &PlotterWindow::onRealTimeFFTClicked);
        menu.addSeparator();
        menu.addAction(tr("导出图片..."), this, &PlotterWindow::onExportImageClicked);
        menu.addAction(tr("导出数据..."), this, &PlotterWindow::onExportDataClicked);
        menu.exec(m_plot->mapToGlobal(pos));
    });

    applyRenderQualityMode();
}

QColor PlotterWindow::getDefaultColor(int index)
{
    return s_defaultColors[index % s_defaultColors.size()];
}

void PlotterWindow::ensureCurveExists(int index)
{
    bool curveAdded = false;
    while (m_plot->graphCount() <= index) {
        int newIndex = m_plot->graphCount();
        const bool highQuality = (m_renderQualityMode == RenderQualityMode::HighQuality);
        QCPGraph* graph = m_plot->addGraph();
        graph->setName(tr("曲线 %1").arg(newIndex + 1));
        graph->setPen(createCurvePen(getDefaultColor(newIndex), 1.7));
        graph->setAntialiased(highQuality);
        graph->setAdaptiveSampling(true);  // 启用自适应采样（自动降采样远处数据）
        curveAdded = true;
    }

    if (curveAdded && m_controlPanel) {
        m_controlPanel->updateCurveList();
    }
}

void PlotterWindow::addCurve(const QString& curveName, const QColor& color)
{
    const bool highQuality = (m_renderQualityMode == RenderQualityMode::HighQuality);
    QCPGraph* graph = m_plot->addGraph();
    graph->setName(curveName);

    QColor curveColor = color.isValid() ? color : getDefaultColor(m_plot->graphCount() - 1);
    graph->setPen(createCurvePen(curveColor, 1.7));
    graph->setAntialiased(highQuality);
    graph->setAdaptiveSampling(true);  // 启用自适应采样
    if (m_controlPanel) {
        m_controlPanel->updateCurveList();
    }

    LOG_INFO(QString("Added curve '%1' to window '%2'").arg(curveName, m_windowId));
}

QString PlotterWindow::curveName(int curveIndex) const
{
    if (!m_plot || curveIndex < 0 || curveIndex >= m_plot->graphCount()) {
        return QString();
    }

    QCPGraph* graph = m_plot->graph(curveIndex);
    return graph ? graph->name() : QString();
}

bool PlotterWindow::isCurveVisible(int curveIndex) const
{
    if (!m_plot || curveIndex < 0 || curveIndex >= m_plot->graphCount()) {
        return false;
    }

    QCPGraph* graph = m_plot->graph(curveIndex);
    return graph && graph->visible();
}

void PlotterWindow::appendData(int curveIndex, double y)
{
    if (m_paused) return;

    // 数据抽稀检查（与 appendMultiData 保持一致）
    if (shouldDecimate()) {
        m_dataIndex++;
        return;  // 跳过这个数据点
    }

    ensureCurveExists(curveIndex);
    const double x = static_cast<double>(m_dataIndex);
    m_plot->graph(curveIndex)->addData(x, y);

    if (curveIndex >= m_latestValues.size()) {
        m_latestValues.resize(curveIndex + 1);
    }
    m_latestValues[curveIndex] = y;

    checkTriggerCondition(curveIndex, y);
    checkAlarmCondition(curveIndex, y);
    updateRealTimeFFT(curveIndex, y);

    if (m_diffRealtimeEnabled) {
        for (auto& diffInfo : m_diffCurves) {
            if (!diffInfo.graph) {
                continue;
            }
            if (curveIndex != diffInfo.curve1 && curveIndex != diffInfo.curve2) {
                continue;
            }
            if (diffInfo.curve1 < 0 || diffInfo.curve2 < 0 ||
                diffInfo.curve1 >= m_latestValues.size() || diffInfo.curve2 >= m_latestValues.size()) {
                continue;
            }

            const double diff = m_latestValues[diffInfo.curve1] - m_latestValues[diffInfo.curve2];
            diffInfo.graph->addData(x, diff);
        }
    }

    if (m_filterRealtimeEnabled) {
        for (auto& filterInfo : m_realTimeFilters) {
            if (!filterInfo.filterGraph || filterInfo.sourceCurveIndex != curveIndex) {
                continue;
            }
            const double filteredValue = FilterUtils::filterPoint(y, filterInfo.config, filterInfo.state);
            filterInfo.filterGraph->addData(x, filteredValue);
        }
    }

    ++m_dataIndex;
    if (m_dataIndex % 100 == 0) {
        trimData();
    }
    m_needReplot = true;
}

void PlotterWindow::appendData(int curveIndex, double x, double y)
{
    if (m_paused) return;

    // 数据抽稀检查（与 appendMultiData 保持一致）
    if (shouldDecimate()) {
        m_dataIndex++;
        return;  // 跳过这个数据点
    }

    ensureCurveExists(curveIndex);
    m_plot->graph(curveIndex)->addData(x, y);

    if (curveIndex >= m_latestValues.size()) {
        m_latestValues.resize(curveIndex + 1);
    }
    m_latestValues[curveIndex] = y;

    checkTriggerCondition(curveIndex, y);
    checkAlarmCondition(curveIndex, y);
    updateRealTimeFFT(curveIndex, y);

    if (m_diffRealtimeEnabled) {
        for (auto& diffInfo : m_diffCurves) {
            if (!diffInfo.graph) {
                continue;
            }
            if (curveIndex != diffInfo.curve1 && curveIndex != diffInfo.curve2) {
                continue;
            }
            if (diffInfo.curve1 < 0 || diffInfo.curve2 < 0 ||
                diffInfo.curve1 >= m_latestValues.size() || diffInfo.curve2 >= m_latestValues.size()) {
                continue;
            }

            const double diff = m_latestValues[diffInfo.curve1] - m_latestValues[diffInfo.curve2];
            diffInfo.graph->addData(x, diff);
        }
    }

    if (m_filterRealtimeEnabled) {
        for (auto& filterInfo : m_realTimeFilters) {
            if (!filterInfo.filterGraph || filterInfo.sourceCurveIndex != curveIndex) {
                continue;
            }
            const double filteredValue = FilterUtils::filterPoint(y, filterInfo.config, filterInfo.state);
            filterInfo.filterGraph->addData(x, filteredValue);
        }
    }

    ++m_dataIndex;
    if (m_dataIndex % 100 == 0) {
        trimData();
    }
    m_needReplot = true;
}

void PlotterWindow::appendMultiData(const QVector<double>& values)
{
    if (m_paused) return;

    // 数据抽稀检查
    if (shouldDecimate()) {
        m_dataIndex++;
        return;  // 跳过这个数据点
    }

    // 预先确保所有曲线存在（避免内循环中逐个检查）
    for (int i = 0; i < values.size(); ++i) {
        ensureCurveExists(i);
    }

    for (int i = 0; i < values.size(); ++i) {
        m_plot->graph(i)->addData(m_dataIndex, values[i]);

        // 触发条件检测（内部已有 enabled 快速退出）
        checkTriggerCondition(i, values[i]);

        // 报警条件检测（内部已有 enabled 快速退出）
        checkAlarmCondition(i, values[i]);
    }

    // 实时FFT更新（仅在有FFT窗口时才处理）
    if (!m_realTimeFFTWindows.isEmpty()) {
        for (int i = 0; i < values.size(); ++i) {
            updateRealTimeFFT(i, values[i]);
        }
    }

    // 实时更新所有差值曲线
    if (m_diffRealtimeEnabled) {
        for (auto& diffInfo : m_diffCurves) {
            if (diffInfo.graph && diffInfo.curve1 >= 0 && diffInfo.curve2 >= 0 &&
                diffInfo.curve1 < values.size() && diffInfo.curve2 < values.size()) {
                double diff = values[diffInfo.curve1] - values[diffInfo.curve2];
                diffInfo.graph->addData(m_dataIndex, diff);
            }
        }
    }

    // 实时滤波更新
    if (m_filterRealtimeEnabled) {
        for (auto& filterInfo : m_realTimeFilters) {
            if (filterInfo.filterGraph && filterInfo.sourceCurveIndex >= 0 &&
                filterInfo.sourceCurveIndex < values.size()) {
                double rawValue = values[filterInfo.sourceCurveIndex];
                double filteredValue = FilterUtils::filterPoint(rawValue, filterInfo.config, filterInfo.state);
                filterInfo.filterGraph->addData(m_dataIndex, filteredValue);
            }
        }
    }

    // 保存最新值
    m_latestValues = values;

    m_dataIndex++;
    m_needReplot = true;

    // 更新缓存的参考图和总点数（避免 updatePlot 每帧遍历所有图）
    if (m_plot->graphCount() > 0) {
        m_cachedRefGraph = m_plot->graph(0);
        m_cachedTotalPoints = m_cachedRefGraph->data()->size();
    }
    m_totalDataPoints += values.size();

    // 优化：每100个点才检查一次数据裁剪
    if (m_dataIndex % 100 == 0) {
        trimData();
    }
    // 优化：updateValuePanel 移到 updatePlot() 中统一处理
}

void PlotterWindow::appendMultiData(double x, const QVector<double>& values)
{
    if (m_paused) return;

    // 数据抽稀检查
    if (shouldDecimate()) {
        return;  // 跳过这个数据点
    }

    // 预先确保所有曲线存在
    for (int i = 0; i < values.size(); ++i) {
        ensureCurveExists(i);
    }

    for (int i = 0; i < values.size(); ++i) {
        m_plot->graph(i)->addData(x, values[i]);

        // 触发条件检测（内部已有 enabled 快速退出）
        checkTriggerCondition(i, values[i]);

        // 报警条件检测（内部已有 enabled 快速退出）
        checkAlarmCondition(i, values[i]);
    }

    // 实时FFT更新（仅在有FFT窗口时才处理）
    if (!m_realTimeFFTWindows.isEmpty()) {
        for (int i = 0; i < values.size(); ++i) {
            updateRealTimeFFT(i, values[i]);
        }
    }

    // 实时更新所有差值曲线
    if (m_diffRealtimeEnabled) {
        for (auto& diffInfo : m_diffCurves) {
            if (diffInfo.graph && diffInfo.curve1 >= 0 && diffInfo.curve2 >= 0 &&
                diffInfo.curve1 < values.size() && diffInfo.curve2 < values.size()) {
                double diff = values[diffInfo.curve1] - values[diffInfo.curve2];
                diffInfo.graph->addData(x, diff);
            }
        }
    }

    // 实时滤波更新
    if (m_filterRealtimeEnabled) {
        for (auto& filterInfo : m_realTimeFilters) {
            if (filterInfo.filterGraph && filterInfo.sourceCurveIndex >= 0 &&
                filterInfo.sourceCurveIndex < values.size()) {
                double rawValue = values[filterInfo.sourceCurveIndex];
                double filteredValue = FilterUtils::filterPoint(rawValue, filterInfo.config, filterInfo.state);
                filterInfo.filterGraph->addData(x, filteredValue);
            }
        }
    }

    // 保存最新值
    m_latestValues = values;

    ++m_dataIndex;
    m_needReplot = true;

    // 更新缓存的参考图和总点数
    if (m_plot->graphCount() > 0) {
        m_cachedRefGraph = m_plot->graph(0);
        m_cachedTotalPoints = m_cachedRefGraph->data()->size();
    }
    m_totalDataPoints += values.size();

    // 优化：每100个点才检查一次数据裁剪
    if (m_dataIndex % 100 == 0) {
        trimData();
    }
    // 优化：updateValuePanel 移到 updatePlot() 中统一处理
}

void PlotterWindow::trimData()
{
    // 限制数据点数量（保留真正静态曲线，其余实时曲线都参与裁剪）
    QSet<QCPGraph*> realtimeFilterGraphs;
    for (const auto& filterInfo : m_realTimeFilters) {
        if (filterInfo.filterGraph) {
            realtimeFilterGraphs.insert(filterInfo.filterGraph);
        }
    }

    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);

        // 仅跳过”真正静态曲线”，实时滤波曲线也要参与裁剪
        if (m_staticCurves.contains(graph) && !realtimeFilterGraphs.contains(graph)) {
            continue;
        }

        int dataSize = graph->data()->size();

        if (dataSize > m_maxDataPoints) {
            // 移除最早的数据，保留 90% 的最大点数以避免频繁裁剪
            int targetSize = m_maxDataPoints * 9 / 10;  // 保留90%
            int removeCount = dataSize - targetSize;

            if (removeCount >= dataSize) {
                m_totalDataPoints -= dataSize;
                graph->data()->clear();
                continue;
            }

            // 使用真实边界 key 进行裁剪，避免非等步长 X 轴时的删除误差
            auto boundaryIt = graph->data()->constBegin();
            for (int step = 0; step < removeCount && boundaryIt != graph->data()->constEnd(); ++step) {
                ++boundaryIt;
            }

            if (boundaryIt != graph->data()->constEnd()) {
                graph->data()->removeBefore(boundaryIt->key);
                m_totalDataPoints -= removeCount;
            } else {
                m_totalDataPoints -= dataSize;
                graph->data()->clear();
            }
        }
    }
}

void PlotterWindow::clearCurve(int curveIndex)
{
    if (curveIndex >= 0 && curveIndex < m_plot->graphCount()) {
        m_plot->graph(curveIndex)->data()->clear();
        m_needReplot = true;
    }
}

void PlotterWindow::clearAll()
{
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        m_plot->graph(i)->data()->clear();
    }
    m_dataIndex = 0;
    m_needReplot = true;
    m_totalDataPoints = 0;
    m_cachedRefGraph = nullptr;
    m_cachedTotalPoints = 0;

    // 重置所有实时滤波状态
    for (auto& filterInfo : m_realTimeFilters) {
        FilterUtils::resetState(filterInfo.state);
    }

    m_perfWindowStartMs = 0;
    m_perfFrameCount = 0;
    m_perfTotalFrameMs = 0.0;
    if (m_perfDiagLabel) {
        m_perfDiagLabel->setText(tr("性能: 等待数据..."));
    }

    LOG_INFO(QString("Cleared all data in window '%1'").arg(m_windowId));
}

void PlotterWindow::setXAxisRange(double min, double max)
{
    m_plot->xAxis->setRange(min, max);
    m_autoScale = false;
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::setYAxisRange(double min, double max)
{
    m_plot->yAxis->setRange(min, max);
    m_autoScale = false;
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::setAutoScale(bool enabled)
{
    m_autoScale = enabled;
    m_needReplot = true;
}

void PlotterWindow::setShowGrid(bool show)
{
    m_plot->xAxis->grid()->setVisible(show);
    m_plot->yAxis->grid()->setVisible(show);
    m_gridAction->setChecked(show);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::setShowLegend(bool show)
{
    m_plot->legend->setVisible(show);
    m_legendAction->setChecked(show);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::setMaxDataPoints(int maxPoints)
{
    m_maxDataPoints = qMax(100, maxPoints);
}

void PlotterWindow::setPaused(bool paused)
{
    m_paused = paused;
    m_pauseAction->setChecked(paused);
    m_pauseAction->setText(paused ? tr("继续") : tr("暂停"));
}

bool PlotterWindow::exportImage(const QString& filename)
{
    QString ext = QFileInfo(filename).suffix().toLower();

    bool success = false;
    if (ext == "png") {
        success = m_plot->savePng(filename, 0, 0, 2.0);
    } else if (ext == "jpg" || ext == "jpeg") {
        success = m_plot->saveJpg(filename, 0, 0, 2.0);
    } else if (ext == "pdf") {
        success = m_plot->savePdf(filename);
    } else if (ext == "bmp") {
        success = m_plot->saveBmp(filename);
    } else {
        // 默认保存为PNG
        success = m_plot->savePng(filename + ".png", 0, 0, 2.0);
    }

    if (success) {
        LOG_INFO(QString("Exported image to: %1").arg(filename));
    } else {
        LOG_ERROR(QString("Failed to export image to: %1").arg(filename));
    }

    return success;
}

bool PlotterWindow::exportData(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString("Failed to open file for writing: %1").arg(filename));
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    // 写入表头
    stream << "X";
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        stream << "," << m_plot->graph(i)->name();
    }
    stream << "\n";

    // 收集所有X值
    QSet<double> xValuesSet;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        auto data = m_plot->graph(i)->data();
        for (auto it = data->begin(); it != data->end(); ++it) {
            xValuesSet.insert(it->key);
        }
    }

    // 排序X值
    QVector<double> xValues = xValuesSet.toList().toVector();
    std::sort(xValues.begin(), xValues.end());

    // 写入数据
    for (double x : xValues) {
        stream << x;
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            auto data = m_plot->graph(i)->data();
            auto it = data->findBegin(x, false);
            if (it != data->end() && qFuzzyCompare(it->key, x)) {
                stream << "," << it->value;
            } else {
                stream << ",";
            }
        }
        stream << "\n";
    }

    file.close();
    LOG_INFO(QString("Exported data to: %1").arg(filename));
    return true;
}

void PlotterWindow::closeEvent(QCloseEvent* event)
{
    emit windowClosed(m_windowId);
    event->accept();
}

void PlotterWindow::onPauseToggled()
{
    // 切换暂停状态（而非读取 action 的 checked 状态，因为右键菜单也会调用此函数）
    m_paused = !m_paused;

    // 同步工具栏按钮状态和图标
    if (m_pauseAction) {
        m_pauseAction->setChecked(m_paused);
        m_pauseAction->setIcon(style()->standardIcon(m_paused ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
        m_pauseAction->setText(m_paused ? tr("继续") : tr("暂停"));
    }
    LOG_INFO(QString("Window '%1' %2").arg(m_windowId, m_paused ? "paused" : "resumed"));

    // 多窗口同步暂停状态
    PlotterManager::instance()->syncPausedState(m_windowId, m_paused);
}

void PlotterWindow::onClearClicked()
{
    clearAll();
}

void PlotterWindow::onAutoScaleClicked()
{
    m_plot->rescaleAxes();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::onExportImageClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("保存图片"),
        QString(),
        tr("PNG图片 (*.png);;JPEG图片 (*.jpg);;PDF文档 (*.pdf);;BMP图片 (*.bmp)"));

    if (!filename.isEmpty()) {
        if (exportImage(filename)) {
            QMessageBox::information(this, tr("导出成功"), tr("图片已保存到: %1").arg(filename));
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法保存图片"));
        }
    }
}

void PlotterWindow::onExportDataClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("导出数据"),
        QString(),
        tr("CSV文件 (*.csv);;所有文件 (*)"));

    if (!filename.isEmpty()) {
        if (exportData(filename)) {
            QMessageBox::information(this, tr("导出成功"), tr("数据已保存到: %1").arg(filename));
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("无法保存数据"));
        }
    }
}

void PlotterWindow::onGridToggled(bool checked)
{
    m_plot->xAxis->grid()->setVisible(checked);
    m_plot->yAxis->grid()->setVisible(checked);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::onLegendToggled(bool checked)
{
    m_plot->legend->setVisible(checked);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::updatePlot()
{
    if (!m_needReplot) return;
    m_needReplot = false;

    QElapsedTimer frameTimer;
    frameTimer.start();

    auto finalizePerfDiagnostics = [this, &frameTimer]() {
        const double frameMs = frameTimer.nsecsElapsed() / 1000000.0;
        m_perfTotalFrameMs += frameMs;
        ++m_perfFrameCount;

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (m_perfWindowStartMs == 0) {
            m_perfWindowStartMs = nowMs;
        }

        const qint64 elapsedMs = nowMs - m_perfWindowStartMs;
        if (elapsedMs < 1000) {
            if (m_perfDiagLabel) {
                m_perfDiagLabel->setVisible(m_showPerfDiag);
            }
            return;
        }

        const int totalPoints = m_totalDataPoints;

        const double fps = elapsedMs > 0
            ? (static_cast<double>(m_perfFrameCount) * 1000.0 / static_cast<double>(elapsedMs))
            : 0.0;
        const double avgFrameMs = m_perfFrameCount > 0
            ? (m_perfTotalFrameMs / static_cast<double>(m_perfFrameCount))
            : 0.0;

        if (m_perfDiagLabel) {
            m_perfDiagLabel->setVisible(m_showPerfDiag);
            if (m_showPerfDiag) {
                m_perfDiagLabel->setText(
                    tr("性能  FPS:%1  平均:%2ms  点数:%3")
                        .arg(QString::number(fps, 'f', 1))
                        .arg(QString::number(avgFrameMs, 'f', 2))
                        .arg(totalPoints));
            }
        }

        m_perfWindowStartMs = nowMs;
        m_perfFrameCount = 0;
        m_perfTotalFrameMs = 0.0;
    };

    // 根据视图模式选择不同的更新逻辑
    if (m_viewMode == PlotViewMode::Histogram) {
        // 直方图模式：更新直方图数据
        updateHistogramView();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        updateValuePanel();
        finalizePerfDiagnostics();
        return;
    }

    if (m_viewMode == PlotViewMode::XY) {
        // XY模式：更新XY曲线
        updateXYView();
        updateValuePanel();
        finalizePerfDiagnostics();
        return;
    }

    // 时间序列模式（默认）
    if (m_plot->graphCount() > 0) {
        // 快速判断：使用缓存的参考图或第一个图检查是否有数据
        bool hasData = (m_cachedRefGraph && !m_cachedRefGraph->data()->isEmpty())
                       || (m_plot->graph(0) && !m_plot->graph(0)->data()->isEmpty());

        if (hasData) {
            if (m_scrollMode) {
                // 滚动模式：使用缓存的参考图（避免每帧遍历所有图）
                const QCPGraph* referenceGraph = m_cachedRefGraph
                    ? m_cachedRefGraph : findReferenceGraphForScroll(m_plot);
                if (referenceGraph) {
                    const int totalPoints = referenceGraph->data()->size();
                    const int scrollMax = qMax(0, totalPoints - m_visiblePoints);
                    int startIndex = scrollMax;

                    if (m_scrollBar) {
                        const bool followLatest = (m_scrollBar->value() >= m_scrollBar->maximum());
                        startIndex = followLatest ? scrollMax : qBound(0, m_scrollBar->value(), scrollMax);
                        m_scrollBar->blockSignals(true);
                        m_scrollBar->setRange(0, scrollMax);
                        m_scrollBar->setPageStep(m_visiblePoints);
                        m_scrollBar->setValue(startIndex);
                        m_scrollBar->blockSignals(false);
                    }

                    double xMin = 0.0;
                    double xMax = 0.0;
                    if (resolveKeyRangeByPointWindow(referenceGraph, startIndex, m_visiblePoints, xMin, xMax)) {
                        m_plot->xAxis->setRange(xMin, xMax);

                        bool shouldUpdateAxisRange = true;
                        if (m_throttleAutoRangeUpdates) {
                            ++m_autoRangeUpdateCounter;
                            shouldUpdateAxisRange = (m_autoRangeUpdateCounter % 2 == 0);
                        } else {
                            m_autoRangeUpdateCounter = 0;
                        }

                        if (shouldUpdateAxisRange) {
                            const QCPRange visibleKeyRange(xMin, xMax);
                            double minValue = 0.0;
                            double maxValue = 0.0;
                            if (calculateAxisValueRange(m_plot, m_plot->yAxis, visibleKeyRange, minValue, maxValue)) {
                                applySmoothedAxisRange(m_plot->yAxis, minValue, maxValue);
                            }

                            if (m_rightAxisVisible && m_rightYAxis &&
                                calculateAxisValueRange(m_plot, m_rightYAxis, visibleKeyRange, minValue, maxValue)) {
                                applySmoothedAxisRange(m_rightYAxis, minValue, maxValue);
                            }
                        }
                    }
                }
            } else if (m_autoScale) {
                // 自动缩放模式
                m_plot->rescaleAxes();

                const QCPRange yRange = m_plot->yAxis->range();
                const double yPadding = yRange.size() * 0.05;
                m_plot->yAxis->setRange(yRange.lower - yPadding, yRange.upper + yPadding);

                if (m_rightYAxis && m_rightYAxis->visible()) {
                    const QCPRange rightRange = m_rightYAxis->range();
                    const double rightPadding = rightRange.size() * 0.05;
                    m_rightYAxis->setRange(rightRange.lower - rightPadding, rightRange.upper + rightPadding);
                }
            }
        }
    }

    // 更新峰值标注（仅在启用时才检测，每20帧一次）
    if (m_peakConfig.enabled) {
        static int peakUpdateCounter = 0;
        if (++peakUpdateCounter >= 20) {
            peakUpdateCounter = 0;
            updatePeakAnnotations();
        }
    }

    // 更新测量游标（Y轴范围可能变化了）
    if (m_measureModeEnabled) {
        updateMeasureCursors();
    }

    // 性能优化：使用 rpQueuedReplot 避免阻塞
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    // 更新数值面板按渲染质量档位节流，避免 QLabel 文本刷新抢占绘图时间。
    if (++m_valuePanelUpdateCounter >= m_valuePanelUpdateEvery) {
        m_valuePanelUpdateCounter = 0;
        updateValuePanel();
    }

    // 发送数据量变化信号（优化：每10次更新才计算一次）
    // 发送数据量变化信号（使用增量维护的总数，避免遍历）
    static int signalCounter = 0;
    if (++signalCounter >= 10) {
        signalCounter = 0;
        emit dataSizeChanged(m_totalDataPoints);
    }

    // 如果光标在图表区域内，根据像素位置实时刷新光标的X坐标和数值
    if (m_cursorEnabled && m_cursorInPlot) {
        // 根据保存的像素位置重新计算当前X坐标（因为图表可能滚动了）
        double currentX = m_plot->xAxis->pixelToCoord(m_lastCursorPixelX);
        updateCursorInfo(currentX, false);
    }

    finalizePerfDiagnostics();
}

void PlotterWindow::onOpenGLToggled(bool checked)
{
    setOpenGLEnabled(checked);
}

void PlotterWindow::onDecimationChanged(int index)
{
    Q_UNUSED(index);
    // 由 QActionGroup::triggered 处理
}

void PlotterWindow::onLineWidthChanged(double value)
{
    Q_UNUSED(value);
    // 由曲线样式对话框处理
}

void PlotterWindow::onShowSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("绘图设置"));
    QFormLayout* layout = new QFormLayout(&dialog);

    // 最大数据点数
    QSpinBox* maxPointsSpin = new QSpinBox;
    maxPointsSpin->setRange(1000, 100000);
    maxPointsSpin->setSingleStep(1000);
    maxPointsSpin->setValue(m_maxDataPoints);
    layout->addRow(tr("最大数据点数:"), maxPointsSpin);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        setMaxDataPoints(maxPointsSpin->value());
        LOG_INFO(QString("Max data points set to: %1").arg(m_maxDataPoints));
    }
}

void PlotterWindow::setOpenGLEnabled(bool enabled)
{
    if (m_openGLEnabled == enabled && (!enabled || m_openGLAvailable)) {
        return;
    }

    const bool actualEnabled = trySetOpenGLEnabled(enabled);

    if (m_openGLAction) {
        QSignalBlocker blocker(m_openGLAction);
        m_openGLAction->setChecked(actualEnabled);
        m_openGLAction->setEnabled(m_openGLAvailable || !enabled);
    }

    applyRenderQualityMode();
    LOG_INFO(QString("OpenGL %1 for window '%2'")
        .arg(actualEnabled ? "enabled" : "disabled")
        .arg(m_windowId));
}

bool PlotterWindow::trySetOpenGLEnabled(bool enabled)
{
    /*
     * QCustomPlot 的 OpenGL 后端依赖当前机器能创建有效 OpenGL 上下文。
     * 某些 Windows 远程桌面、虚拟机或驱动异常环境下 setOpenGl(true) 可能失败；
     * 这里先做能力探测，失败时回退到软件绘制，保证绘图质量和功能不受影响。
     */
    if (!m_plot) {
        m_openGLEnabled = false;
        return false;
    }

    if (!enabled) {
        m_plot->setOpenGl(false);
        m_openGLEnabled = false;
        m_openGLAvailable = true;
        return false;
    }

#ifdef QCUSTOMPLOT_USE_OPENGL
    QOpenGLContext probeContext;
    if (!probeContext.create()) {
        m_plot->setOpenGl(false);
        m_openGLEnabled = false;
        m_openGLAvailable = false;
        LOG_WARN(QString("OpenGL context creation failed for plot window '%1', fallback to software rendering")
                 .arg(m_windowId));
        return false;
    }

    m_plot->setOpenGl(true);
    if (!m_plot->openGl()) {
        m_plot->setOpenGl(false);
        m_openGLEnabled = false;
        m_openGLAvailable = false;
        LOG_WARN(QString("QCustomPlot OpenGL backend unavailable for window '%1', fallback to software rendering")
                 .arg(m_windowId));
        return false;
    }

    m_openGLEnabled = true;
    m_openGLAvailable = true;
    return true;
#else
    m_plot->setOpenGl(false);
    m_openGLEnabled = false;
    m_openGLAvailable = false;
    return false;
#endif
}

void PlotterWindow::setRenderQualityMode(RenderQualityMode mode)
{
    if (m_renderQualityMode == mode) {
        return;
    }

    m_renderQualityMode = mode;
    m_autoRangeUpdateCounter = 0;
    applyRenderQualityMode();

    if (m_renderQualityHighAction) {
        m_renderQualityHighAction->setChecked(mode == RenderQualityMode::HighQuality);
    }
    if (m_renderQualityPerformanceAction) {
        m_renderQualityPerformanceAction->setChecked(mode == RenderQualityMode::HighPerformance);
    }
    if (m_renderQualityCombo) {
        QSignalBlocker blocker(m_renderQualityCombo);
        m_renderQualityCombo->setCurrentIndex(mode == RenderQualityMode::HighQuality ? 0 : 1);
    }

    LOG_INFO(QString("Render quality mode changed to '%1' in window '%2'")
        .arg(mode == RenderQualityMode::HighQuality ? "HighQuality" : "HighPerformance")
        .arg(m_windowId));
}

void PlotterWindow::applyRenderQualityMode()
{
    if (!m_plot) {
        return;
    }

    const RenderQualityProfile profile = makeRenderQualityProfile(m_renderQualityMode);
    const bool highQuality = profile.antialiasPlottables;

    m_throttleAutoRangeUpdates = profile.throttleAutoRangeUpdates;
    m_valuePanelUpdateEvery = qMax(1, profile.valuePanelUpdateEvery);

    QCP::AntialiasedElements notAntialiased = QCP::aeGrid | QCP::aeAxes | QCP::aeLegend | QCP::aeLegendItems;
    QCP::AntialiasedElements antialiased = QCP::aeItems;
    if (profile.antialiasPlottables) {
        antialiased = antialiased | QCP::aePlottables;
    } else {
        notAntialiased = notAntialiased | QCP::aePlottables;
    }

    QCP::PlottingHints hints = QCP::phCacheLabels;
    if (profile.useFastPolylines) {
        hints = hints | QCP::phFastPolylines;
    }

    m_plot->setNotAntialiasedElements(notAntialiased);
    m_plot->setAntialiasedElements(antialiased);
    m_plot->setNoAntialiasingOnDrag(profile.noAntialiasingOnDrag);
    m_plot->setPlottingHints(hints);
    if (m_updateTimer) {
        m_updateTimer->setInterval(profile.updateIntervalMs);
    }

    if (m_openGLEnabled) {
        trySetOpenGLEnabled(true);
    } else {
        m_plot->setOpenGl(false);
    }

    for (int i = 0; i < m_plot->graphCount(); ++i) {
        if (QCPGraph* graph = m_plot->graph(i)) {
            graph->setAntialiased(highQuality);
            graph->setAdaptiveSampling(true);
        }
    }

    if (m_xyCurve) {
        m_xyCurve->setAntialiased(highQuality);
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::setDecimationRatio(DecimationRatio ratio)
{
    m_decimationRatio = ratio;
    m_decimationCounter = 0;
    LOG_INFO(QString("Decimation ratio set to 1:%1 for window '%2'")
        .arg(static_cast<int>(ratio)).arg(m_windowId));
}

bool PlotterWindow::shouldDecimate()
{
    if (m_decimationRatio == DecimationRatio::None) {
        return false;  // 不抽稀，保留所有数据
    }

    m_decimationCounter++;
    if (m_decimationCounter >= static_cast<int>(m_decimationRatio)) {
        m_decimationCounter = 0;
        return false;  // 保留这个点
    }
    return true;  // 抽稀这个点
}

void PlotterWindow::setCurveLineWidth(int curveIndex, double width)
{
    if (curveIndex >= 0 && curveIndex < m_plot->graphCount()) {
        QPen pen = m_plot->graph(curveIndex)->pen();
        pen.setWidthF(width);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        m_plot->graph(curveIndex)->setPen(pen);
    }
}

void PlotterWindow::setCurveLineStyle(int curveIndex, Qt::PenStyle style)
{
    if (curveIndex >= 0 && curveIndex < m_plot->graphCount()) {
        QPen pen = m_plot->graph(curveIndex)->pen();
        pen.setStyle(style);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        m_plot->graph(curveIndex)->setPen(pen);
    }
}

void PlotterWindow::setCurveScatterStyle(int curveIndex, QCPScatterStyle::ScatterShape shape)
{
    if (curveIndex >= 0 && curveIndex < m_plot->graphCount()) {
        QCPScatterStyle scatterStyle;
        scatterStyle.setShape(shape);
        scatterStyle.setSize(6);
        scatterStyle.setPen(m_plot->graph(curveIndex)->pen());
        m_plot->graph(curveIndex)->setScatterStyle(scatterStyle);
    }
}

void PlotterWindow::onFFTAnalysisClicked()
{
    if (m_plot->graphCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有曲线数据"));
        return;
    }

    // 创建主对话框
    QDialog mainDialog(this);
    mainDialog.setWindowTitle(tr("FFT 频谱分析"));
    mainDialog.setMinimumWidth(500);
    QVBoxLayout* mainLayout = new QVBoxLayout(&mainDialog);

    // ========== 曲线选择区域 ==========
    QGroupBox* curveGroup = new QGroupBox(tr("选择曲线"));
    QFormLayout* curveLayout = new QFormLayout(curveGroup);

    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QString name = m_plot->graph(i)->name();
        int dataCount = m_plot->graph(i)->data()->size();
        curveCombo->addItem(QString("%1 (%2 点)").arg(name).arg(dataCount), i);
    }
    curveLayout->addRow(tr("曲线:"), curveCombo);
    mainLayout->addWidget(curveGroup);

    // ========== FFT设置对话框内嵌 ==========
    // 创建设置对话框并获取其配置
    FFTSettingsDialog* settingsDialog = new FFTSettingsDialog(&mainDialog);
    // 设置默认采样率
    FFTConfig defaultConfig;
    defaultConfig.sampleRate = m_sampleRate;
    settingsDialog->setConfig(defaultConfig);

    // 将设置对话框的内容嵌入到主对话框中
    // 由于FFTSettingsDialog是完整的对话框，这里我们用按钮打开它
    QPushButton* settingsBtn = new QPushButton(tr("FFT 参数设置..."));
    connect(settingsBtn, &QPushButton::clicked, settingsDialog, [settingsDialog]() {
        settingsDialog->exec();
    });
    mainLayout->addWidget(settingsBtn);

    // 显示当前设置摘要
    QLabel* settingsSummary = new QLabel;
    settingsSummary->setObjectName("plotSettingsSummary");
    auto updateSummary = [settingsSummary, settingsDialog]() {
        FFTConfig cfg = settingsDialog->getConfig();
        QString windowName = FFTUtils::getWindowName(cfg.windowType);
        QString sizeStr = cfg.fftSize == 0 ? tr("自动") : QString::number(cfg.fftSize);
        settingsSummary->setText(tr("当前设置: FFT点数=%1, 采样率=%2Hz, 窗口=%3")
            .arg(sizeStr).arg(cfg.sampleRate, 0, 'f', 1).arg(windowName));
    };
    updateSummary();
    connect(settingsDialog, &QDialog::accepted, settingsSummary, updateSummary);
    mainLayout->addWidget(settingsSummary);

    // ========== 按钮 ==========
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &mainDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &mainDialog, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    if (mainDialog.exec() != QDialog::Accepted) {
        return;
    }

    // 获取选择的曲线和配置
    int curveIndex = curveCombo->currentData().toInt();
    QString curveName = m_plot->graph(curveIndex)->name();
    FFTConfig config = settingsDialog->getConfig();

    // 获取曲线数据
    QCPGraph* graph = m_plot->graph(curveIndex);
    if (graph->data()->isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("所选曲线没有数据"));
        return;
    }

    // 提取Y值数据
    QVector<double> yData;
    auto dataContainer = graph->data();
    for (auto it = dataContainer->constBegin(); it != dataContainer->constEnd(); ++it) {
        yData.append(it->value);
    }

    if (yData.size() < 8) {
        QMessageBox::warning(this, tr("错误"), tr("数据点数太少（至少需要8个点）"));
        return;
    }

    // 使用配置执行 FFT 分析
    FFTResult result = FFTUtils::analyzeWithConfig(yData, config);

    // 如果启用峰值检测，执行峰值检测
    if (settingsDialog->peakDetectionEnabled()) {
        result.peaks = FFTUtils::detectPeaks(result, settingsDialog->maxPeaks());
    }

    // 创建并显示频谱窗口
    SpectrumWindow* spectrumWin = new SpectrumWindow(this);
    spectrumWin->setAttribute(Qt::WA_DeleteOnClose);
    spectrumWin->setSampleRate(config.sampleRate);
    spectrumWin->setFFTResult(result, curveName);
    spectrumWin->show();

    // 更新本窗口的采样率设置
    m_sampleRate = config.sampleRate;

    LOG_INFO(QString("FFT analysis performed on curve '%1' with %2 points, FFT size %3, sample rate %4 Hz, window %5")
        .arg(curveName).arg(yData.size()).arg(result.fftSize).arg(config.sampleRate)
        .arg(FFTUtils::getWindowName(config.windowType)));
}

void PlotterWindow::onScrollModeToggled(bool checked)
{
    m_scrollMode = checked;

    // 显示/隐藏滚动条
    if (m_scrollBar) {
        m_scrollBar->setVisible(checked);
    }

    if (checked) {
        // 切换到滚动模式，自动跟随最新数据
        if (m_scrollBar) {
            m_scrollBar->setValue(m_scrollBar->maximum());
        }
        m_needReplot = true;
    } else {
        // 切换到自动缩放模式
        m_autoScale = true;
        m_plot->rescaleAxes();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }

    LOG_INFO(QString("Scroll mode %1 for window '%2'")
        .arg(checked ? "enabled" : "disabled").arg(m_windowId));
}

void PlotterWindow::onScrollBarChanged(int value)
{
    if (!m_scrollMode) return;

    const QCPGraph* referenceGraph = findReferenceGraphForScroll(m_plot);
    if (!referenceGraph) {
        return;
    }

    const int totalPoints = referenceGraph->data()->size();
    const int maxStart = qMax(0, totalPoints - m_visiblePoints);
    const int startIndex = qBound(0, value, maxStart);

    double xMin = 0.0;
    double xMax = 0.0;
    if (!resolveKeyRangeByPointWindow(referenceGraph, startIndex, m_visiblePoints, xMin, xMax)) {
        return;
    }

    m_plot->xAxis->setRange(xMin, xMax);

    const QCPRange visibleKeyRange(xMin, xMax);
    double minValue = 0.0;
    double maxValue = 0.0;
    if (calculateAxisValueRange(m_plot, m_plot->yAxis, visibleKeyRange, minValue, maxValue)) {
        applySmoothedAxisRange(m_plot->yAxis, minValue, maxValue);
    }

    if (m_rightAxisVisible && m_rightYAxis &&
        calculateAxisValueRange(m_plot, m_rightYAxis, visibleKeyRange, minValue, maxValue)) {
        applySmoothedAxisRange(m_rightYAxis, minValue, maxValue);
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::updateValuePanel()
{
    if (!m_valuePanel) return;

    // 收集所有要显示的数据
    struct ValueInfo {
        QString name;
        double value;
        QColor color;
    };
    QVector<ValueInfo> allValues;

    // 原始曲线数据
    for (int i = 0; i < m_latestValues.size(); ++i) {
        ValueInfo info;
        info.value = m_latestValues[i];
        info.color = getDefaultColor(i);

        if (i < m_plot->graphCount()) {
            info.name = m_plot->graph(i)->name();
            info.color = m_plot->graph(i)->pen().color();
        } else {
            info.name = tr("曲线 %1").arg(i + 1);
        }
        allValues.append(info);
    }

    // 差值曲线数据
    for (const auto& diffInfo : m_diffCurves) {
        ValueInfo info;
        info.name = diffInfo.name;
        info.color = diffInfo.graph ? diffInfo.graph->pen().color() : Qt::magenta;

        if (diffInfo.curve1 >= 0 && diffInfo.curve1 < m_latestValues.size() &&
            diffInfo.curve2 >= 0 && diffInfo.curve2 < m_latestValues.size()) {
            info.value = m_latestValues[diffInfo.curve1] - m_latestValues[diffInfo.curve2];
        } else {
            info.value = 0;
        }
        allValues.append(info);
    }

    // 检查是否需要重建布局（数量变化）
    bool needRebuild = (allValues.size() != m_lastValueCount) || m_valueLabels.isEmpty();

    if (!needRebuild) {
        // 高频路径：仅更新文本，避免每帧 setStyleSheet 触发额外样式重算
        for (int i = 0; i < qMin(allValues.size(), m_valueLabels.size()); ++i) {
            const auto& info = allValues[i];
            m_valueLabels[i]->setText(QString("%1: %2").arg(info.name).arg(info.value, 0, 'f', 2));
        }
        return;
    }

    // 需要重建布局
    m_lastValueCount = allValues.size();

    // 获取或创建布局
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(m_valuePanel->layout());
    if (!mainLayout) return;

    // 清空现有内容
    QLayoutItem* item;
    while ((item = mainLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
    m_valueLabels.clear();

    if (allValues.isEmpty()) {
        QLabel* emptyLabel = new QLabel(tr("当前值: 无数据"), m_valuePanel);
        emptyLabel->setObjectName("plotValueEmpty");
        mainLayout->addWidget(emptyLabel);
        return;
    }

    // 计算可用宽度
    int availableWidth = m_valuePanel->width() - 30;
    if (availableWidth < 200) availableWidth = 800;

    const int labelSpacing = 20;
    const int avgCharWidth = 9;

    // 创建带标题的第一行
    QWidget* currentRow = new QWidget(m_valuePanel);
    QHBoxLayout* currentRowLayout = new QHBoxLayout(currentRow);
    currentRowLayout->setContentsMargins(0, 0, 0, 0);
    currentRowLayout->setSpacing(15);
    mainLayout->addWidget(currentRow);

    // 添加标题
    QLabel* titleLabel = new QLabel(tr("当前值:"), currentRow);
    titleLabel->setObjectName("plotValueTitle");
    currentRowLayout->addWidget(titleLabel);

    int currentRowWidth = 60;

    for (int i = 0; i < allValues.size(); ++i) {
        const auto& info = allValues[i];

        // 估计标签宽度
        QString labelText = QString("%1: %2").arg(info.name).arg(info.value, 0, 'f', 2);
        int estimatedWidth = labelText.length() * avgCharWidth + labelSpacing;

        // 检查是否需要换行
        if (currentRowWidth + estimatedWidth > availableWidth && currentRowWidth > 80) {
            // 为当前行添加弹性空间
            currentRowLayout->addStretch();

            // 创建新行
            currentRow = new QWidget(m_valuePanel);
            currentRowLayout = new QHBoxLayout(currentRow);
            currentRowLayout->setContentsMargins(60, 0, 0, 0);
            currentRowLayout->setSpacing(15);
            mainLayout->addWidget(currentRow);
            currentRowWidth = 60;
        }

        // 创建标签
        QLabel* label = new QLabel(labelText, currentRow);
        label->setStyleSheet(QString("color: %1; font-weight: bold;")
            .arg(info.color.name()));

        currentRowLayout->addWidget(label);
        m_valueLabels.append(label);
        currentRowWidth += estimatedWidth;
    }

    // 为最后一行添加弹性空间
    if (currentRowLayout) {
        currentRowLayout->addStretch();
    }
}

void PlotterWindow::onMouseMove(QMouseEvent* event)
{
    // 检查鼠标是否在绑图区域内
    QRect plotRect = m_plot->axisRect()->rect();
    m_cursorInPlot = plotRect.contains(event->pos());

    if (!m_cursorEnabled || m_plot->graphCount() == 0 || !m_cursorInPlot) {
        if (m_cursorLine) m_cursorLine->setVisible(false);
        if (m_cursorText) m_cursorText->setVisible(false);
        m_cursorInPlot = false;
        return;
    }

    // 保存鼠标的像素X位置
    m_lastCursorPixelX = event->pos().x();

    // 根据像素位置计算当前X坐标
    double x = m_plot->xAxis->pixelToCoord(m_lastCursorPixelX);

    // 性能优化：使用节流，减少鼠标移动时的 replot 频率
    static qint64 lastReplotTime = 0;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    bool shouldReplot = (currentTime - lastReplotTime) > 30;  // 每30ms最多replot一次

    updateCursorInfo(x, shouldReplot);

    if (shouldReplot) {
        lastReplotTime = currentTime;
    }
}

void PlotterWindow::updateCursorInfo(double x, bool doReplot)
{
    if (!m_cursorLine || !m_cursorText) return;

    // 更新垂直光标线位置
    m_cursorLine->start->setCoords(x, m_plot->yAxis->range().lower);
    m_cursorLine->end->setCoords(x, m_plot->yAxis->range().upper);
    m_cursorLine->setVisible(true);

    // 构建数值显示文本（纯文本格式）
    QString info = QString("X: %1\n").arg(x, 0, 'f', 1);

    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        if (graph->data()->isEmpty()) continue;

        // 查找最接近x的数据点
        auto it = graph->data()->findBegin(x, false);
        if (it != graph->data()->end()) {
            double value = it->value;
            info += QString("%1: %2\n")
                .arg(graph->name())
                .arg(value, 0, 'f', 2);
        }
    }

    m_cursorText->setText(info.trimmed());
    m_cursorText->setVisible(true);

    if (doReplot) {
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void PlotterWindow::onRenameCurveClicked()
{
    if (m_plot->graphCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有曲线"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("重命名曲线"));
    QFormLayout* layout = new QFormLayout(&dialog);

    // 曲线选择
    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        curveCombo->addItem(m_plot->graph(i)->name(), i);
    }
    layout->addRow(tr("选择曲线:"), curveCombo);

    // 新名称输入
    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setText(m_plot->graph(0)->name());
    layout->addRow(tr("新名称:"), nameEdit);

    // 当选择曲线变化时更新名称
    connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
        if (index >= 0 && index < m_plot->graphCount()) {
            nameEdit->setText(m_plot->graph(index)->name());
        }
    });

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        int idx = curveCombo->currentData().toInt();
        QString newName = nameEdit->text().trimmed();
        if (!newName.isEmpty()) {
            renameCurve(idx, newName);
        }
    }
}

void PlotterWindow::renameCurve(int curveIndex, const QString& name)
{
    if (curveIndex >= 0 && curveIndex < m_plot->graphCount()) {
        m_plot->graph(curveIndex)->setName(name);
        if (m_controlPanel) {
            m_controlPanel->updateCurveList();
        }
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        updateValuePanel();
        LOG_INFO(QString("Curve %1 renamed to '%2'").arg(curveIndex).arg(name));
    }
}

void PlotterWindow::onSetReferenceLineClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("设置参考线"));
    QFormLayout* layout = new QFormLayout(&dialog);

    // 参考值
    QDoubleSpinBox* valueSpin = new QDoubleSpinBox;
    valueSpin->setRange(-1e9, 1e9);
    valueSpin->setDecimals(3);
    valueSpin->setValue(m_referenceValue);
    layout->addRow(tr("参考值（设定值）:"), valueSpin);

    // 标签
    QLineEdit* labelEdit = new QLineEdit;
    labelEdit->setText(tr("设定值"));
    labelEdit->setPlaceholderText(tr("可选"));
    layout->addRow(tr("标签:"), labelEdit);

    // 删除按钮
    QPushButton* removeBtn = new QPushButton(tr("移除参考线"));
    removeBtn->setEnabled(m_referenceLine != nullptr);
    connect(removeBtn, &QPushButton::clicked, [&]() {
        removeReferenceLine();
        dialog.reject();
    });
    layout->addRow(removeBtn);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        setReferenceLine(valueSpin->value(), labelEdit->text());
    }
}

void PlotterWindow::setReferenceLine(double value, const QString& label)
{
    m_referenceValue = value;

    // 创建或更新参考线
    if (!m_referenceLine) {
        m_referenceLine = new QCPItemLine(m_plot);
        m_referenceLine->setPen(QPen(Qt::red, 2, Qt::DashLine));
    }

    m_referenceLine->start->setTypeX(QCPItemPosition::ptAxisRectRatio);
    m_referenceLine->start->setTypeY(QCPItemPosition::ptPlotCoords);
    m_referenceLine->start->setCoords(0, value);
    m_referenceLine->end->setTypeX(QCPItemPosition::ptAxisRectRatio);
    m_referenceLine->end->setTypeY(QCPItemPosition::ptPlotCoords);
    m_referenceLine->end->setCoords(1, value);
    m_referenceLine->setVisible(true);

    // 创建或更新标签
    if (!m_referenceLabel) {
        m_referenceLabel = new QCPItemText(m_plot);
        m_referenceLabel->setColor(Qt::red);
        m_referenceLabel->setFont(QFont("", 9));
    }

    QString displayLabel = label.isEmpty() ? QString::number(value, 'f', 2) : label;
    m_referenceLabel->setText(displayLabel);
    m_referenceLabel->position->setTypeX(QCPItemPosition::ptAxisRectRatio);
    m_referenceLabel->position->setTypeY(QCPItemPosition::ptPlotCoords);
    m_referenceLabel->position->setCoords(0.01, value);
    m_referenceLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
    m_referenceLabel->setVisible(true);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
    LOG_INFO(QString("Reference line set to %1").arg(value));
}

void PlotterWindow::removeReferenceLine()
{
    if (m_referenceLine) {
        m_plot->removeItem(m_referenceLine);
        m_referenceLine = nullptr;
    }
    if (m_referenceLabel) {
        m_plot->removeItem(m_referenceLabel);
        m_referenceLabel = nullptr;
    }
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    LOG_INFO("Reference line removed");
}

void PlotterWindow::calculateStatistics(int curveIndex, double& minVal, double& maxVal, double& avg, double& stddev)
{
    minVal = maxVal = avg = stddev = 0;

    if (curveIndex < 0 || curveIndex >= m_plot->graphCount()) return;

    QCPGraph* graph = m_plot->graph(curveIndex);
    if (graph->data()->isEmpty()) return;

    double sum = 0;
    double sumSq = 0;
    int count = 0;
    bool first = true;

    for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
        double v = it->value;
        if (first) {
            minVal = maxVal = v;
            first = false;
        } else {
            minVal = qMin(minVal, v);
            maxVal = qMax(maxVal, v);
        }
        sum += v;
        sumSq += v * v;
        count++;
    }

    if (count > 0) {
        avg = sum / count;
        double variance = (sumSq / count) - (avg * avg);
        stddev = std::sqrt(qMax(0.0, variance));
    }
}

void PlotterWindow::calculatePIDMetrics(int curveIndex, double setpoint,
                                         double& overshoot, int& settlingTime,
                                         int& riseTime, double& steadyError)
{
    overshoot = 0;
    settlingTime = 0;
    riseTime = 0;
    steadyError = 0;

    if (curveIndex < 0 || curveIndex >= m_plot->graphCount()) return;

    QCPGraph* graph = m_plot->graph(curveIndex);
    if (graph->data()->size() < 10) return;

    // 收集数据
    QVector<double> values;
    for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
        values.append(it->value);
    }

    // 假设初始值为第一个数据点
    double initialValue = values.first();
    double responseRange = setpoint - initialValue;

    if (qAbs(responseRange) < 0.001) {
        steadyError = 0;
        return;
    }

    // 计算上升时间 (10% -> 90%)
    double thresh10 = initialValue + responseRange * 0.1;
    double thresh90 = initialValue + responseRange * 0.9;
    int idx10 = -1, idx90 = -1;

    for (int i = 0; i < values.size(); ++i) {
        if (idx10 < 0 && ((responseRange > 0 && values[i] >= thresh10) ||
                          (responseRange < 0 && values[i] <= thresh10))) {
            idx10 = i;
        }
        if (idx90 < 0 && ((responseRange > 0 && values[i] >= thresh90) ||
                          (responseRange < 0 && values[i] <= thresh90))) {
            idx90 = i;
            break;
        }
    }

    if (idx10 >= 0 && idx90 >= 0) {
        riseTime = idx90 - idx10;
    }

    // 计算超调量
    double peakValue = values.first();
    for (double v : values) {
        if (responseRange > 0) {
            peakValue = qMax(peakValue, v);
        } else {
            peakValue = qMin(peakValue, v);
        }
    }

    if (qAbs(responseRange) > 0.001) {
        overshoot = qAbs(peakValue - setpoint) / qAbs(responseRange) * 100.0;
        if ((responseRange > 0 && peakValue <= setpoint) ||
            (responseRange < 0 && peakValue >= setpoint)) {
            overshoot = 0;  // 没有超调
        }
    }

    // 计算稳态误差（使用最后10%数据的平均值）
    int steadyStart = values.size() * 9 / 10;
    double steadySum = 0;
    int steadyCount = 0;
    for (int i = steadyStart; i < values.size(); ++i) {
        steadySum += values[i];
        steadyCount++;
    }
    double steadyAvg = steadyCount > 0 ? steadySum / steadyCount : values.last();
    steadyError = setpoint - steadyAvg;

    // 计算调节时间（±2%范围内稳定）
    double tolerance = qAbs(responseRange) * 0.02;
    settlingTime = values.size() - 1;

    for (int i = values.size() - 1; i >= 0; --i) {
        if (qAbs(values[i] - setpoint) > tolerance) {
            settlingTime = i + 1;
            break;
        }
    }
}

void PlotterWindow::onPIDAnalysisClicked()
{
    if (m_plot->graphCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有曲线数据"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("PID 性能分析"));
    dialog.resize(450, 400);
    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // 参数设置区
    QFormLayout* paramLayout = new QFormLayout;

    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        curveCombo->addItem(m_plot->graph(i)->name(), i);
    }
    paramLayout->addRow(tr("响应曲线:"), curveCombo);

    QDoubleSpinBox* setpointSpin = new QDoubleSpinBox;
    setpointSpin->setRange(-1e9, 1e9);
    setpointSpin->setDecimals(3);
    setpointSpin->setValue(m_referenceValue);
    paramLayout->addRow(tr("设定值:"), setpointSpin);

    mainLayout->addLayout(paramLayout);

    // 结果显示区
    QGroupBox* resultGroup = new QGroupBox(tr("分析结果"));
    QFormLayout* resultLayout = new QFormLayout(resultGroup);

    QLabel* statsLabel = new QLabel;
    statsLabel->setObjectName("plotStatsLabel");
    resultLayout->addRow(statsLabel);

    mainLayout->addWidget(resultGroup);

    // 分析按钮
    QPushButton* analyzeBtn = new QPushButton(tr("开始分析"));
    mainLayout->addWidget(analyzeBtn);

    connect(analyzeBtn, &QPushButton::clicked, [&]() {
        int idx = curveCombo->currentData().toInt();
        double setpoint = setpointSpin->value();

        // 计算统计信息
        double minVal, maxVal, avg, stddev;
        calculateStatistics(idx, minVal, maxVal, avg, stddev);

        // 计算PID性能指标
        double overshoot;
        int settlingTime, riseTime;
        double steadyError;
        calculatePIDMetrics(idx, setpoint, overshoot, settlingTime, riseTime, steadyError);

        // 显示结果
        QString result;
        result += tr("【统计信息】\n");
        result += tr("  最小值: %1\n").arg(minVal, 0, 'f', 3);
        result += tr("  最大值: %1\n").arg(maxVal, 0, 'f', 3);
        result += tr("  平均值: %1\n").arg(avg, 0, 'f', 3);
        result += tr("  标准差: %1\n").arg(stddev, 0, 'f', 3);
        result += tr("\n【PID性能指标】\n");
        result += tr("  设定值: %1\n").arg(setpoint, 0, 'f', 3);
        result += tr("  超调量: %1%\n").arg(overshoot, 0, 'f', 1);
        result += tr("  上升时间: %1 点\n").arg(riseTime);
        result += tr("  调节时间: %1 点\n").arg(settlingTime);
        result += tr("  稳态误差: %1\n").arg(steadyError, 0, 'f', 3);

        statsLabel->setText(result);
    });

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void PlotterWindow::onShowDifferenceClicked()
{
    if (m_plot->graphCount() < 2) {
        QMessageBox::information(this, tr("提示"), tr("至少需要2条曲线才能计算差值"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("管理差值曲线"));
    dialog.resize(450, 350);
    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // 已有差值曲线列表
    QGroupBox* existingGroup = new QGroupBox(tr("已有差值曲线"));
    QVBoxLayout* existingLayout = new QVBoxLayout(existingGroup);

    QListWidget* diffList = new QListWidget;
    for (int i = 0; i < m_diffCurves.size(); ++i) {
        auto& info = m_diffCurves[i];
        QString itemText = QString("%1: %2 - %3")
            .arg(info.name)
            .arg(info.curve1 < m_plot->graphCount() ? m_plot->graph(info.curve1)->name() : "?")
            .arg(info.curve2 < m_plot->graphCount() ? m_plot->graph(info.curve2)->name() : "?");
        diffList->addItem(itemText);
    }
    existingLayout->addWidget(diffList);

    // 删除选中按钮
    QPushButton* removeBtn = new QPushButton(tr("删除选中"));
    removeBtn->setEnabled(!m_diffCurves.isEmpty());
    connect(removeBtn, &QPushButton::clicked, [&]() {
        int row = diffList->currentRow();
        if (row >= 0 && row < m_diffCurves.size()) {
            if (m_diffCurves[row].graph) {
                m_plot->removeGraph(m_diffCurves[row].graph);
            }
            m_diffCurves.removeAt(row);
            delete diffList->takeItem(row);
            if (m_controlPanel) {
                m_controlPanel->updateCurveList();
            }
            m_plot->replot(QCustomPlot::rpQueuedReplot);
            removeBtn->setEnabled(!m_diffCurves.isEmpty());
        }
    });
    existingLayout->addWidget(removeBtn);
    mainLayout->addWidget(existingGroup);

    // 添加新差值曲线
    QGroupBox* addGroup = new QGroupBox(tr("添加新差值曲线"));
    QFormLayout* addLayout = new QFormLayout(addGroup);

    // 差值曲线名称
    QLineEdit* nameEdit = new QLineEdit;
    nameEdit->setText(tr("差值 %1").arg(m_diffCurves.size() + 1));
    addLayout->addRow(tr("名称:"), nameEdit);

    // 曲线1选择（排除差值曲线）
    QComboBox* curve1Combo = new QComboBox;
    QComboBox* curve2Combo = new QComboBox;

    for (int i = 0; i < m_plot->graphCount(); ++i) {
        // 检查是否是差值曲线
        bool isDiff = false;
        for (const auto& info : m_diffCurves) {
            if (m_plot->graph(i) == info.graph) {
                isDiff = true;
                break;
            }
        }
        if (!isDiff) {
            curve1Combo->addItem(m_plot->graph(i)->name(), i);
            curve2Combo->addItem(m_plot->graph(i)->name(), i);
        }
    }

    if (curve2Combo->count() > 1) {
        curve2Combo->setCurrentIndex(1);
    }

    addLayout->addRow(tr("曲线1 (被减数):"), curve1Combo);
    addLayout->addRow(tr("曲线2 (减数):"), curve2Combo);

    mainLayout->addWidget(addGroup);

    // 按钮区
    QHBoxLayout* btnLayout = new QHBoxLayout;
    QPushButton* addBtn = new QPushButton(tr("添加差值曲线"));
    QPushButton* closeBtn = new QPushButton(tr("关闭"));

    connect(addBtn, &QPushButton::clicked, [&]() {
        int idx1 = curve1Combo->currentData().toInt();
        int idx2 = curve2Combo->currentData().toInt();

        if (idx1 == idx2) {
            QMessageBox::warning(this, tr("错误"), tr("请选择两条不同的曲线"));
            return;
        }

        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            name = tr("差值 %1").arg(m_diffCurves.size() + 1);
        }

        // 创建新差值曲线
        DiffCurveInfo info;
        info.curve1 = idx1;
        info.curve2 = idx2;
        info.name = name;
        info.graph = m_plot->addGraph();
        info.graph->setName(name);

        // 使用不同颜色
        static const QVector<QColor> diffColors = {
            Qt::magenta, Qt::cyan, QColor(255, 128, 0), QColor(128, 0, 255)
        };
        QColor color = diffColors[m_diffCurves.size() % diffColors.size()];
        info.graph->setPen(createCurvePen(color, 1.7, Qt::DashLine));
        info.graph->setAntialiased(m_renderQualityMode == RenderQualityMode::HighQuality);
        info.graph->setAdaptiveSampling(true);

        // 计算历史数据的差值
        QCPGraph* g1 = m_plot->graph(idx1);
        QCPGraph* g2 = m_plot->graph(idx2);

        for (auto it = g1->data()->constBegin(); it != g1->data()->constEnd(); ++it) {
            double x = it->key;
            double y1 = it->value;
            auto it2 = g2->data()->findBegin(x, false);
            if (it2 != g2->data()->end()) {
                double y2 = it2->value;
                info.graph->addData(x, y1 - y2);
            }
        }

        m_diffCurves.append(info);
        if (m_controlPanel) {
            m_controlPanel->updateCurveList();
        }

        // 更新列表
        QString itemText = QString("%1: %2 - %3")
            .arg(name).arg(g1->name()).arg(g2->name());
        diffList->addItem(itemText);
        removeBtn->setEnabled(true);

        // 更新名称建议
        nameEdit->setText(tr("差值 %1").arg(m_diffCurves.size() + 1));

        m_plot->replot(QCustomPlot::rpQueuedReplot);
        LOG_INFO(QString("Difference curve created: %1 = %2 - %3")
            .arg(name).arg(g1->name()).arg(g2->name()));
    });

    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    dialog.exec();
}

void PlotterWindow::onRealTimeFFTClicked()
{
    if (m_plot->graphCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有曲线数据"));
        return;
    }

    // 选择要分析的曲线
    QDialog selectDialog(this);
    selectDialog.setWindowTitle(tr("实时 FFT 分析设置"));
    selectDialog.setMinimumWidth(400);
    QVBoxLayout* layout = new QVBoxLayout(&selectDialog);

    // 曲线选择
    QGroupBox* curveGroup = new QGroupBox(tr("选择曲线"));
    QFormLayout* curveLayout = new QFormLayout(curveGroup);

    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        // 排除差值曲线
        bool isDiff = false;
        for (const auto& info : m_diffCurves) {
            if (m_plot->graph(i) == info.graph) {
                isDiff = true;
                break;
            }
        }
        if (!isDiff) {
            QString name = m_plot->graph(i)->name();
            int dataCount = m_plot->graph(i)->data()->size();
            curveCombo->addItem(QString("%1 (%2 点)").arg(name).arg(dataCount), i);
        }
    }
    curveLayout->addRow(tr("曲线:"), curveCombo);
    layout->addWidget(curveGroup);

    // FFT设置
    QGroupBox* fftGroup = new QGroupBox(tr("FFT 参数"));
    QFormLayout* fftLayout = new QFormLayout(fftGroup);

    QComboBox* fftSizeCombo = new QComboBox;
    fftSizeCombo->addItem("256", 256);
    fftSizeCombo->addItem("512", 512);
    fftSizeCombo->addItem("1024", 1024);
    fftSizeCombo->addItem("2048", 2048);
    fftSizeCombo->addItem("4096", 4096);
    fftSizeCombo->setCurrentIndex(2);  // 默认1024
    fftLayout->addRow(tr("FFT点数:"), fftSizeCombo);

    QDoubleSpinBox* sampleRateSpin = new QDoubleSpinBox;
    sampleRateSpin->setRange(1, 10000000);
    sampleRateSpin->setValue(m_sampleRate);
    sampleRateSpin->setSuffix(" Hz");
    sampleRateSpin->setDecimals(0);
    fftLayout->addRow(tr("采样率:"), sampleRateSpin);

    QComboBox* windowCombo = new QComboBox;
    windowCombo->addItem(tr("汉宁窗"), static_cast<int>(WindowType::Hanning));
    windowCombo->addItem(tr("汉明窗"), static_cast<int>(WindowType::Hamming));
    windowCombo->addItem(tr("布莱克曼窗"), static_cast<int>(WindowType::Blackman));
    windowCombo->addItem(tr("矩形窗"), static_cast<int>(WindowType::Rectangular));
    windowCombo->addItem(tr("凯泽窗"), static_cast<int>(WindowType::Kaiser));
    windowCombo->addItem(tr("平顶窗"), static_cast<int>(WindowType::FlatTop));
    fftLayout->addRow(tr("窗口函数:"), windowCombo);

    layout->addWidget(fftGroup);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &selectDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &selectDialog, &QDialog::reject);

    if (selectDialog.exec() != QDialog::Accepted) {
        return;
    }

    int curveIndex = curveCombo->currentData().toInt();
    if (curveIndex < 0 || curveIndex >= m_plot->graphCount()) {
        return;
    }

    // 获取曲线数据
    QCPGraph* graph = m_plot->graph(curveIndex);
    QString curveName = graph->name();

    // 创建FFT配置
    FFTConfig config;
    config.fftSize = fftSizeCombo->currentData().toInt();
    config.sampleRate = sampleRateSpin->value();
    config.windowType = static_cast<WindowType>(windowCombo->currentData().toInt());

    // 创建实时FFT窗口
    RealTimeFFTWindow* fftWindow = new RealTimeFFTWindow(this);
    fftWindow->setAttribute(Qt::WA_DeleteOnClose);
    fftWindow->setWindowTitle(tr("实时 FFT - %1").arg(curveName));
    fftWindow->setFFTConfig(config);

    // 复制当前数据到FFT窗口
    QVector<double> yData;
    for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
        yData.append(it->value);
    }

    // 只取最后N个点（N=FFT点数）
    int fftSize = config.fftSize;
    if (yData.size() > fftSize) {
        yData = yData.mid(yData.size() - fftSize);
    }
    fftWindow->appendData(yData);

    // 建立数据连接 - 保存FFT窗口引用
    RealTimeFFTConnection conn;
    conn.window = fftWindow;
    conn.curveIndex = curveIndex;
    m_realTimeFFTWindows.append(conn);

    // 连接窗口关闭信号以清理
    connect(fftWindow, &RealTimeFFTWindow::windowClosed, this, [this, fftWindow]() {
        for (int i = m_realTimeFFTWindows.size() - 1; i >= 0; --i) {
            if (m_realTimeFFTWindows[i].window == fftWindow) {
                m_realTimeFFTWindows.removeAt(i);
                break;
            }
        }
    });

    fftWindow->show();

    LOG_INFO(QString("Real-time FFT window opened for curve '%1': FFT size=%2, sample rate=%3 Hz")
        .arg(curveName).arg(config.fftSize).arg(config.sampleRate));
}

void PlotterWindow::setCurveYAxis(int curveIndex, bool useRightAxis)
{
    if (curveIndex < 0 || curveIndex >= m_plot->graphCount()) {
        return;
    }

    QCPGraph* graph = m_plot->graph(curveIndex);
    if (!graph) return;

    if (useRightAxis) {
        // 使用右轴
        graph->setValueAxis(m_rightYAxis);
        m_rightAxisCurves.insert(curveIndex);

        // 自动显示右轴
        if (!m_rightAxisVisible) {
            setRightYAxisVisible(true);
        }
    } else {
        // 使用左轴
        graph->setValueAxis(m_plot->yAxis);
        m_rightAxisCurves.remove(curveIndex);

        // 如果没有曲线使用右轴，自动隐藏
        if (m_rightAxisCurves.isEmpty()) {
            setRightYAxisVisible(false);
        }
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
    LOG_INFO(QString("Curve %1 set to use %2 axis")
        .arg(curveIndex).arg(useRightAxis ? "right" : "left"));
}

bool PlotterWindow::curveUsesRightAxis(int curveIndex) const
{
    return m_rightAxisCurves.contains(curveIndex);
}

void PlotterWindow::setRightYAxisRange(double min, double max)
{
    if (m_rightYAxis) {
        m_rightYAxis->setRange(min, max);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void PlotterWindow::setRightYAxisLabel(const QString& label)
{
    if (m_rightYAxis) {
        m_rightYAxis->setLabel(label);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void PlotterWindow::setRightYAxisVisible(bool visible)
{
    m_rightAxisVisible = visible;
    if (m_rightYAxis) {
        m_rightYAxis->setVisible(visible);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void PlotterWindow::onConfigureYAxisClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Y轴配置"));
    dialog.resize(500, 450);
    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // ========== 左Y轴设置 ==========
    QGroupBox* leftGroup = new QGroupBox(tr("左Y轴 (Y1)"));
    QFormLayout* leftLayout = new QFormLayout(leftGroup);

    QLineEdit* leftLabelEdit = new QLineEdit;
    leftLabelEdit->setText(m_plot->yAxis->label());
    leftLayout->addRow(tr("标签:"), leftLabelEdit);

    QDoubleSpinBox* leftMinSpin = new QDoubleSpinBox;
    leftMinSpin->setRange(-1e9, 1e9);
    leftMinSpin->setDecimals(3);
    leftMinSpin->setValue(m_plot->yAxis->range().lower);
    leftLayout->addRow(tr("最小值:"), leftMinSpin);

    QDoubleSpinBox* leftMaxSpin = new QDoubleSpinBox;
    leftMaxSpin->setRange(-1e9, 1e9);
    leftMaxSpin->setDecimals(3);
    leftMaxSpin->setValue(m_plot->yAxis->range().upper);
    leftLayout->addRow(tr("最大值:"), leftMaxSpin);

    mainLayout->addWidget(leftGroup);

    // ========== 右Y轴设置 ==========
    QGroupBox* rightGroup = new QGroupBox(tr("右Y轴 (Y2)"));
    QFormLayout* rightLayout = new QFormLayout(rightGroup);

    QCheckBox* rightVisibleCheck = new QCheckBox(tr("显示右Y轴"));
    rightVisibleCheck->setChecked(m_rightAxisVisible);
    rightLayout->addRow(rightVisibleCheck);

    QLineEdit* rightLabelEdit = new QLineEdit;
    rightLabelEdit->setText(m_rightYAxis ? m_rightYAxis->label() : tr("Y2"));
    rightLayout->addRow(tr("标签:"), rightLabelEdit);

    QDoubleSpinBox* rightMinSpin = new QDoubleSpinBox;
    rightMinSpin->setRange(-1e9, 1e9);
    rightMinSpin->setDecimals(3);
    rightMinSpin->setValue(m_rightYAxis ? m_rightYAxis->range().lower : -10);
    rightLayout->addRow(tr("最小值:"), rightMinSpin);

    QDoubleSpinBox* rightMaxSpin = new QDoubleSpinBox;
    rightMaxSpin->setRange(-1e9, 1e9);
    rightMaxSpin->setDecimals(3);
    rightMaxSpin->setValue(m_rightYAxis ? m_rightYAxis->range().upper : 10);
    rightLayout->addRow(tr("最大值:"), rightMaxSpin);

    mainLayout->addWidget(rightGroup);

    // ========== 曲线Y轴分配 ==========
    QGroupBox* curveGroup = new QGroupBox(tr("曲线Y轴分配"));
    QVBoxLayout* curveLayout = new QVBoxLayout(curveGroup);

    // 创建曲线列表，每个曲线有左/右轴选择
    struct CurveAxisItem {
        QLabel* nameLabel;
        QComboBox* axisCombo;
    };
    QVector<CurveAxisItem> curveItems;

    if (m_plot->graphCount() > 0) {
        QGridLayout* gridLayout = new QGridLayout;
        gridLayout->addWidget(new QLabel(tr("曲线名称")), 0, 0);
        gridLayout->addWidget(new QLabel(tr("Y轴")), 0, 1);

        for (int i = 0; i < m_plot->graphCount(); ++i) {
            // 跳过差值曲线
            bool isDiff = false;
            for (const auto& info : m_diffCurves) {
                if (m_plot->graph(i) == info.graph) {
                    isDiff = true;
                    break;
                }
            }
            if (isDiff) continue;

            CurveAxisItem item;
            item.nameLabel = new QLabel(m_plot->graph(i)->name());
            item.nameLabel->setStyleSheet(QString("color: %1; font-weight: bold;")
                .arg(m_plot->graph(i)->pen().color().name()));

            item.axisCombo = new QComboBox;
            item.axisCombo->addItem(tr("左轴 (Y1)"), false);
            item.axisCombo->addItem(tr("右轴 (Y2)"), true);
            item.axisCombo->setCurrentIndex(curveUsesRightAxis(i) ? 1 : 0);
            item.axisCombo->setProperty("curveIndex", i);

            int row = gridLayout->rowCount();
            gridLayout->addWidget(item.nameLabel, row, 0);
            gridLayout->addWidget(item.axisCombo, row, 1);

            curveItems.append(item);
        }

        curveLayout->addLayout(gridLayout);
    } else {
        curveLayout->addWidget(new QLabel(tr("当前没有曲线")));
    }

    mainLayout->addWidget(curveGroup);

    // ========== 按钮 ==========
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    mainLayout->addWidget(buttonBox);

    // 应用设置的Lambda
    auto applySettings = [&]() {
        // 应用左Y轴设置
        m_plot->yAxis->setLabel(leftLabelEdit->text());
        m_plot->yAxis->setRange(leftMinSpin->value(), leftMaxSpin->value());

        // 应用右Y轴设置
        setRightYAxisVisible(rightVisibleCheck->isChecked());
        setRightYAxisLabel(rightLabelEdit->text());
        setRightYAxisRange(rightMinSpin->value(), rightMaxSpin->value());

        // 应用曲线Y轴分配
        for (const auto& item : curveItems) {
            int idx = item.axisCombo->property("curveIndex").toInt();
            bool useRight = item.axisCombo->currentData().toBool();
            setCurveYAxis(idx, useRight);
        }

        m_plot->replot(QCustomPlot::rpQueuedReplot);
    };

    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, applySettings);
    connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
        applySettings();
        dialog.accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void PlotterWindow::onMeasureCursorClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("测量游标"));
    dialog.resize(450, 400);
    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // 获取当前X轴范围，用于设置游标默认位置
    double xMin = m_plot->xAxis->range().lower;
    double xMax = m_plot->xAxis->range().upper;
    double xRange = xMax - xMin;

    // 如果游标位置为0且不在当前范围内，设置默认位置
    if (m_measureCursor1.xPos == 0 && (xMin > 0 || xMax < 0)) {
        m_measureCursor1.xPos = xMin + xRange * 0.25;  // 左侧25%位置
    }
    if (m_measureCursor2.xPos == 0 && (xMin > 0 || xMax < 0)) {
        m_measureCursor2.xPos = xMin + xRange * 0.75;  // 右侧75%位置
    }

    // ========== 启用/禁用测量模式 ==========
    QGroupBox* modeGroup = new QGroupBox(tr("测量模式"));
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);

    QCheckBox* enableCheck = new QCheckBox(tr("启用测量游标"));
    enableCheck->setChecked(m_measureModeEnabled);
    modeLayout->addWidget(enableCheck);

    QLabel* tipLabel = new QLabel(tr("提示: 启用后可在图表上拖动游标位置"));
    tipLabel->setObjectName("plotTipLabel");
    modeLayout->addWidget(tipLabel);

    mainLayout->addWidget(modeGroup);

    // ========== 游标1设置 ==========
    QGroupBox* cursor1Group = new QGroupBox(tr("游标 1 (A)"));
    QFormLayout* cursor1Layout = new QFormLayout(cursor1Group);

    QCheckBox* cursor1Check = new QCheckBox(tr("显示"));
    cursor1Check->setChecked(m_measureCursor1.active);
    cursor1Layout->addRow(cursor1Check);

    QDoubleSpinBox* cursor1XSpin = new QDoubleSpinBox;
    cursor1XSpin->setRange(-1e9, 1e9);
    cursor1XSpin->setDecimals(3);
    cursor1XSpin->setValue(m_measureCursor1.xPos);
    cursor1Layout->addRow(tr("X 位置:"), cursor1XSpin);

    // 显示当前Y值
    QLabel* cursor1YLabel = new QLabel;
    cursor1YLabel->setObjectName("plotCursor1Label");
    cursor1Layout->addRow(tr("当前值:"), cursor1YLabel);

    mainLayout->addWidget(cursor1Group);

    // ========== 游标2设置 ==========
    QGroupBox* cursor2Group = new QGroupBox(tr("游标 2 (B)"));
    QFormLayout* cursor2Layout = new QFormLayout(cursor2Group);

    QCheckBox* cursor2Check = new QCheckBox(tr("显示"));
    cursor2Check->setChecked(m_measureCursor2.active);
    cursor2Layout->addRow(cursor2Check);

    QDoubleSpinBox* cursor2XSpin = new QDoubleSpinBox;
    cursor2XSpin->setRange(-1e9, 1e9);
    cursor2XSpin->setDecimals(3);
    cursor2XSpin->setValue(m_measureCursor2.xPos);
    cursor2Layout->addRow(tr("X 位置:"), cursor2XSpin);

    // 显示当前Y值
    QLabel* cursor2YLabel = new QLabel;
    cursor2YLabel->setObjectName("plotCursor2Label");
    cursor2Layout->addRow(tr("当前值:"), cursor2YLabel);

    mainLayout->addWidget(cursor2Group);

    // ========== 差值显示 ==========
    QGroupBox* deltaGroup = new QGroupBox(tr("测量结果 (B - A)"));
    QFormLayout* deltaLayout = new QFormLayout(deltaGroup);

    QLabel* deltaXLabel = new QLabel;
    deltaXLabel->setObjectName("plotDeltaLabel");
    deltaLayout->addRow(tr("ΔX:"), deltaXLabel);

    QLabel* deltaYLabel = new QLabel;
    deltaYLabel->setObjectName("plotDeltaLabel");
    deltaLayout->addRow(tr("ΔY:"), deltaYLabel);

    mainLayout->addWidget(deltaGroup);

    // 更新显示的Lambda
    auto updateValues = [&]() {
        double x1 = cursor1XSpin->value();
        double x2 = cursor2XSpin->value();

        // 获取各曲线在x1和x2处的Y值
        QString y1Str, y2Str, deltaYStr;

        for (int i = 0; i < m_plot->graphCount(); ++i) {
            // 跳过差值曲线
            bool isDiff = false;
            for (const auto& info : m_diffCurves) {
                if (m_plot->graph(i) == info.graph) {
                    isDiff = true;
                    break;
                }
            }
            if (isDiff) continue;

            QCPGraph* graph = m_plot->graph(i);
            QString name = graph->name();

            // 查找Y值
            double y1 = 0, y2 = 0;
            bool found1 = false, found2 = false;

            auto it1 = graph->data()->findBegin(x1, false);
            if (it1 != graph->data()->end()) {
                y1 = it1->value;
                found1 = true;
            }

            auto it2 = graph->data()->findBegin(x2, false);
            if (it2 != graph->data()->end()) {
                y2 = it2->value;
                found2 = true;
            }

            if (found1) {
                y1Str += QString("%1: %2\n").arg(name).arg(y1, 0, 'f', 3);
            }
            if (found2) {
                y2Str += QString("%1: %2\n").arg(name).arg(y2, 0, 'f', 3);
            }
            if (found1 && found2) {
                deltaYStr += QString("%1: %2\n").arg(name).arg(y2 - y1, 0, 'f', 3);
            }
        }

        cursor1YLabel->setText(y1Str.isEmpty() ? tr("无数据") : y1Str.trimmed());
        cursor2YLabel->setText(y2Str.isEmpty() ? tr("无数据") : y2Str.trimmed());
        deltaXLabel->setText(QString::number(x2 - x1, 'f', 3));
        deltaYLabel->setText(deltaYStr.isEmpty() ? tr("无数据") : deltaYStr.trimmed());
    };

    // 初始更新
    updateValues();

    // 连接信号更新值
    connect(cursor1XSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateValues);
    connect(cursor2XSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), updateValues);

    // ========== 按钮 ==========
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    mainLayout->addWidget(buttonBox);

    // 应用设置
    auto applySettings = [&]() {
        m_measureModeEnabled = enableCheck->isChecked();
        m_measureCursor1.active = cursor1Check->isChecked();
        m_measureCursor1.xPos = cursor1XSpin->value();
        m_measureCursor2.active = cursor2Check->isChecked();
        m_measureCursor2.xPos = cursor2XSpin->value();

        updateMeasureCursors();
    };

    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, applySettings);
    connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
        applySettings();
        dialog.accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

void PlotterWindow::updateMeasureCursors()
{
    // 游标1
    if (m_measureModeEnabled && m_measureCursor1.active) {
        // 创建或更新游标1
        if (!m_measureCursor1.line) {
            m_measureCursor1.line = new QCPItemLine(m_plot);
            m_measureCursor1.line->setPen(QPen(QColor(52, 152, 219), 2, Qt::DashLine));  // 蓝色
        }

        m_measureCursor1.line->start->setCoords(m_measureCursor1.xPos, m_plot->yAxis->range().lower);
        m_measureCursor1.line->end->setCoords(m_measureCursor1.xPos, m_plot->yAxis->range().upper);
        m_measureCursor1.line->setVisible(true);

        // 创建或更新标签
        if (!m_measureCursor1.label) {
            m_measureCursor1.label = new QCPItemText(m_plot);
            m_measureCursor1.label->setColor(QColor(52, 152, 219));
            m_measureCursor1.label->setFont(QFont("", 9, QFont::Bold));
            m_measureCursor1.label->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
        }

        m_measureCursor1.label->position->setCoords(m_measureCursor1.xPos, m_plot->yAxis->range().upper);
        m_measureCursor1.label->setText(QString("A: %1").arg(m_measureCursor1.xPos, 0, 'f', 1));
        m_measureCursor1.label->setVisible(true);
    } else {
        // 隐藏游标1
        if (m_measureCursor1.line) m_measureCursor1.line->setVisible(false);
        if (m_measureCursor1.label) m_measureCursor1.label->setVisible(false);
    }

    // 游标2
    if (m_measureModeEnabled && m_measureCursor2.active) {
        // 创建或更新游标2
        if (!m_measureCursor2.line) {
            m_measureCursor2.line = new QCPItemLine(m_plot);
            m_measureCursor2.line->setPen(QPen(QColor(231, 76, 60), 2, Qt::DashLine));  // 红色
        }

        m_measureCursor2.line->start->setCoords(m_measureCursor2.xPos, m_plot->yAxis->range().lower);
        m_measureCursor2.line->end->setCoords(m_measureCursor2.xPos, m_plot->yAxis->range().upper);
        m_measureCursor2.line->setVisible(true);

        // 创建或更新标签
        if (!m_measureCursor2.label) {
            m_measureCursor2.label = new QCPItemText(m_plot);
            m_measureCursor2.label->setColor(QColor(231, 76, 60));
            m_measureCursor2.label->setFont(QFont("", 9, QFont::Bold));
            m_measureCursor2.label->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
        }

        m_measureCursor2.label->position->setCoords(m_measureCursor2.xPos, m_plot->yAxis->range().upper);
        m_measureCursor2.label->setText(QString("B: %1").arg(m_measureCursor2.xPos, 0, 'f', 1));
        m_measureCursor2.label->setVisible(true);
    } else {
        // 隐藏游标2
        if (m_measureCursor2.line) m_measureCursor2.line->setVisible(false);
        if (m_measureCursor2.label) m_measureCursor2.label->setVisible(false);
    }

    // 差值显示
    if (m_measureModeEnabled && m_measureCursor1.active && m_measureCursor2.active) {
        if (!m_measureDeltaText) {
            m_measureDeltaText = new QCPItemText(m_plot);
            m_measureDeltaText->setPositionAlignment(Qt::AlignTop | Qt::AlignRight);
            m_measureDeltaText->position->setType(QCPItemPosition::ptAxisRectRatio);
            m_measureDeltaText->position->setCoords(0.98, 0.02);
            m_measureDeltaText->setFont(QFont("Consolas", 9));
            m_measureDeltaText->setPadding(QMargins(5, 5, 5, 5));
            m_measureDeltaText->setBrush(QBrush(QColor(255, 255, 255, 220)));
            m_measureDeltaText->setPen(QPen(Qt::gray));
        }

        // 计算差值
        double deltaX = m_measureCursor2.xPos - m_measureCursor1.xPos;
        QString deltaInfo = QString("ΔX = %1\n").arg(deltaX, 0, 'f', 3);

        for (int i = 0; i < m_plot->graphCount(); ++i) {
            // 跳过差值曲线
            bool isDiff = false;
            for (const auto& info : m_diffCurves) {
                if (m_plot->graph(i) == info.graph) {
                    isDiff = true;
                    break;
                }
            }
            if (isDiff) continue;

            QCPGraph* graph = m_plot->graph(i);

            auto it1 = graph->data()->findBegin(m_measureCursor1.xPos, false);
            auto it2 = graph->data()->findBegin(m_measureCursor2.xPos, false);

            if (it1 != graph->data()->end() && it2 != graph->data()->end()) {
                double deltaY = it2->value - it1->value;
                deltaInfo += QString("Δ%1 = %2\n").arg(graph->name()).arg(deltaY, 0, 'f', 3);
            }
        }

        m_measureDeltaText->setText(deltaInfo.trimmed());
        m_measureDeltaText->setVisible(true);
    } else {
        if (m_measureDeltaText) m_measureDeltaText->setVisible(false);
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
    LOG_INFO(QString("Measure cursors updated: mode=%1, cursor1=%2@%3, cursor2=%4@%5")
        .arg(m_measureModeEnabled).arg(m_measureCursor1.active).arg(m_measureCursor1.xPos)
        .arg(m_measureCursor2.active).arg(m_measureCursor2.xPos));
}

void PlotterWindow::onFilterCurveClicked()
{
    if (m_plot->graphCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有曲线数据"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("数字滤波"));
    dialog.resize(500, 450);
    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // ========== 曲线选择 ==========
    QGroupBox* curveGroup = new QGroupBox(tr("选择曲线"));
    QFormLayout* curveLayout = new QFormLayout(curveGroup);

    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        // 跳过差值曲线
        bool isDiff = false;
        for (const auto& info : m_diffCurves) {
            if (m_plot->graph(i) == info.graph) {
                isDiff = true;
                break;
            }
        }
        if (isDiff) continue;

        QString name = m_plot->graph(i)->name();
        int dataCount = m_plot->graph(i)->data()->size();
        curveCombo->addItem(QString("%1 (%2 点)").arg(name).arg(dataCount), i);
    }
    curveLayout->addRow(tr("曲线:"), curveCombo);
    mainLayout->addWidget(curveGroup);

    // ========== 滤波器选择 ==========
    QGroupBox* filterGroup = new QGroupBox(tr("滤波器设置"));
    QFormLayout* filterLayout = new QFormLayout(filterGroup);

    QComboBox* filterTypeCombo = new QComboBox;
    for (DigitalFilterType type : FilterUtils::supportedFilters()) {
        filterTypeCombo->addItem(FilterUtils::getFilterName(type), static_cast<int>(type));
    }
    filterLayout->addRow(tr("滤波器类型:"), filterTypeCombo);

    // 窗口大小（滑动平均、中值）
    QSpinBox* windowSizeSpin = new QSpinBox;
    windowSizeSpin->setRange(3, 101);
    windowSizeSpin->setSingleStep(2);
    windowSizeSpin->setValue(5);
    QWidget* windowSizeRow = new QWidget;
    QHBoxLayout* windowSizeLayout = new QHBoxLayout(windowSizeRow);
    windowSizeLayout->setContentsMargins(0, 0, 0, 0);
    windowSizeLayout->addWidget(windowSizeSpin);
    windowSizeLayout->addWidget(new QLabel(tr("点")));
    filterLayout->addRow(tr("窗口大小:"), windowSizeRow);

    // 平滑系数（指数移动平均）
    QDoubleSpinBox* alphaSpin = new QDoubleSpinBox;
    alphaSpin->setRange(0.01, 1.0);
    alphaSpin->setSingleStep(0.05);
    alphaSpin->setValue(0.3);
    alphaSpin->setDecimals(2);
    filterLayout->addRow(tr("平滑系数 α:"), alphaSpin);

    // 截止频率（低通、高通）
    QDoubleSpinBox* cutoffSpin = new QDoubleSpinBox;
    cutoffSpin->setRange(0.1, 10000);
    cutoffSpin->setValue(100);
    cutoffSpin->setSuffix(" Hz");
    filterLayout->addRow(tr("截止频率:"), cutoffSpin);

    // 采样率
    QDoubleSpinBox* sampleRateSpin = new QDoubleSpinBox;
    sampleRateSpin->setRange(1, 1000000);
    sampleRateSpin->setValue(m_sampleRate);
    sampleRateSpin->setSuffix(" Hz");
    filterLayout->addRow(tr("采样率:"), sampleRateSpin);

    // 低/高截止频率（带通、带阻）
    QDoubleSpinBox* lowCutoffSpin = new QDoubleSpinBox;
    lowCutoffSpin->setRange(0.1, 10000);
    lowCutoffSpin->setValue(50);
    lowCutoffSpin->setSuffix(" Hz");
    filterLayout->addRow(tr("低截止频率:"), lowCutoffSpin);

    QDoubleSpinBox* highCutoffSpin = new QDoubleSpinBox;
    highCutoffSpin->setRange(0.1, 10000);
    highCutoffSpin->setValue(200);
    highCutoffSpin->setSuffix(" Hz");
    filterLayout->addRow(tr("高截止频率:"), highCutoffSpin);

    // 卡尔曼参数
    QDoubleSpinBox* processNoiseSpin = new QDoubleSpinBox;
    processNoiseSpin->setRange(0.0001, 10);
    processNoiseSpin->setValue(0.01);
    processNoiseSpin->setDecimals(4);
    filterLayout->addRow(tr("过程噪声 Q:"), processNoiseSpin);

    QDoubleSpinBox* measureNoiseSpin = new QDoubleSpinBox;
    measureNoiseSpin->setRange(0.0001, 10);
    measureNoiseSpin->setValue(0.1);
    measureNoiseSpin->setDecimals(4);
    filterLayout->addRow(tr("测量噪声 R:"), measureNoiseSpin);

    mainLayout->addWidget(filterGroup);

    // 根据滤波器类型显示/隐藏相关参数
    // 辅助函数：设置 QFormLayout 中某行的可见性
    auto setRowVisible = [filterLayout](QWidget* field, bool visible) {
        // 获取行号
        int rowIdx = -1;
        for (int i = 0; i < filterLayout->rowCount(); ++i) {
            QLayoutItem* item = filterLayout->itemAt(i, QFormLayout::FieldRole);
            if (item && item->widget() == field) {
                rowIdx = i;
                break;
            }
        }
        if (rowIdx >= 0) {
            // Qt5 兼容：手动控制行内控件的可见性
            QLayoutItem* labelItem = filterLayout->itemAt(rowIdx, QFormLayout::LabelRole);
            QLayoutItem* fieldItem = filterLayout->itemAt(rowIdx, QFormLayout::FieldRole);
            if (labelItem && labelItem->widget()) {
                labelItem->widget()->setVisible(visible);
            }
            if (fieldItem && fieldItem->widget()) {
                fieldItem->widget()->setVisible(visible);
            }
        }
    };

    auto updateVisibility = [&]() {
        DigitalFilterType type = static_cast<DigitalFilterType>(filterTypeCombo->currentData().toInt());

        // 隐藏所有参数行
        setRowVisible(windowSizeRow, false);
        setRowVisible(alphaSpin, false);
        setRowVisible(cutoffSpin, false);
        setRowVisible(sampleRateSpin, false);
        setRowVisible(lowCutoffSpin, false);
        setRowVisible(highCutoffSpin, false);
        setRowVisible(processNoiseSpin, false);
        setRowVisible(measureNoiseSpin, false);

        switch (type) {
        case DigitalFilterType::MovingAverage:
        case DigitalFilterType::Median:
            setRowVisible(windowSizeRow, true);
            break;

        case DigitalFilterType::ExponentialMA:
            setRowVisible(alphaSpin, true);
            break;

        case DigitalFilterType::LowPass:
        case DigitalFilterType::HighPass:
            setRowVisible(cutoffSpin, true);
            setRowVisible(sampleRateSpin, true);
            break;

        case DigitalFilterType::BandPass:
        case DigitalFilterType::BandStop:
            setRowVisible(lowCutoffSpin, true);
            setRowVisible(highCutoffSpin, true);
            setRowVisible(sampleRateSpin, true);
            break;

        case DigitalFilterType::Kalman:
            setRowVisible(processNoiseSpin, true);
            setRowVisible(measureNoiseSpin, true);
            break;
        }
    };

    connect(filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), updateVisibility);
    updateVisibility();

    // ========== 输出选项 ==========
    QGroupBox* outputGroup = new QGroupBox(tr("输出选项"));
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);

    QRadioButton* replaceRadio = new QRadioButton(tr("替换原曲线"));
    QRadioButton* newCurveRadio = new QRadioButton(tr("创建新曲线"));
    newCurveRadio->setChecked(true);

    QLineEdit* newNameEdit = new QLineEdit;
    newNameEdit->setPlaceholderText(tr("滤波后的曲线名称"));

    outputLayout->addWidget(replaceRadio);
    outputLayout->addWidget(newCurveRadio);
    outputLayout->addWidget(newNameEdit);

    mainLayout->addWidget(outputGroup);

    // 更新默认名称
    auto updateDefaultName = [&]() {
        int idx = curveCombo->currentData().toInt();
        QString baseName = m_plot->graph(idx)->name();
        QString filterName = filterTypeCombo->currentText();
        newNameEdit->setText(QString("%1 (%2)").arg(baseName).arg(filterName));
    };
    connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), updateDefaultName);
    connect(filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), updateDefaultName);
    updateDefaultName();

    // ========== 按钮 ==========
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 获取参数
    int curveIndex = curveCombo->currentData().toInt();
    QCPGraph* sourceGraph = m_plot->graph(curveIndex);

    // 构建滤波配置
    FilterConfig config;
    config.type = static_cast<DigitalFilterType>(filterTypeCombo->currentData().toInt());
    config.windowSize = windowSizeSpin->value();
    config.alpha = alphaSpin->value();
    config.cutoffFrequency = cutoffSpin->value();
    config.sampleRate = sampleRateSpin->value();
    config.lowCutoff = lowCutoffSpin->value();
    config.highCutoff = highCutoffSpin->value();
    config.processNoise = processNoiseSpin->value();
    config.measureNoise = measureNoiseSpin->value();

    // 提取数据
    QVector<double> xData, yData;
    for (auto it = sourceGraph->data()->constBegin(); it != sourceGraph->data()->constEnd(); ++it) {
        xData.append(it->key);
        yData.append(it->value);
    }

    if (yData.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("所选曲线没有数据"));
        return;
    }

    // 执行滤波
    QVector<double> filteredData = FilterUtils::filter(yData, config);

    // 输出结果
    if (replaceRadio->isChecked()) {
        // 替换原曲线
        sourceGraph->data()->clear();
        for (int i = 0; i < xData.size(); ++i) {
            sourceGraph->addData(xData[i], filteredData[i]);
        }
        LOG_INFO(QString("Curve '%1' filtered in place with %2")
            .arg(sourceGraph->name()).arg(FilterUtils::getFilterName(config.type)));
    } else {
        // 创建新曲线
        QString newName = newNameEdit->text().trimmed();
        if (newName.isEmpty()) {
            newName = QString("%1 (滤波)").arg(sourceGraph->name());
        }

        QCPGraph* newGraph = m_plot->addGraph();
        newGraph->setName(newName);

        // 使用对比度更高的颜色和更明显的虚线
        QColor baseColor = sourceGraph->pen().color();
        // 使用互补色或加深色，而非 lighter
        QColor newColor;
        if (baseColor.lightness() > 128) {
            newColor = baseColor.darker(150);  // 浅色曲线用深色滤波
        } else {
            newColor = baseColor.lighter(150);  // 深色曲线用浅色滤波
        }
        // 确保颜色有足够对比度
        newColor.setAlpha(255);
        newGraph->setPen(createCurvePen(newColor, 2.0, Qt::DashLine));  // 线宽2.0更明显
        newGraph->setAntialiased(m_renderQualityMode == RenderQualityMode::HighQuality);
        newGraph->setAdaptiveSampling(true);

        for (int i = 0; i < xData.size(); ++i) {
            newGraph->addData(xData[i], filteredData[i]);
        }

        // 添加到实时滤波列表，以便持续更新
        RealTimeFilterInfo filterInfo;
        filterInfo.filterGraph = newGraph;
        filterInfo.sourceCurveIndex = curveIndex;
        filterInfo.config = config;
        FilterUtils::resetState(filterInfo.state);  // 重置滤波状态
        // 用历史数据初始化滤波状态
        for (double value : yData) {
            FilterUtils::filterPoint(value, filterInfo.config, filterInfo.state);
        }
        m_realTimeFilters.append(filterInfo);
        if (m_controlPanel) {
            m_controlPanel->updateCurveList();
        }

        LOG_INFO(QString("New filtered curve '%1' created from '%2' with %3 (real-time enabled)")
            .arg(newName).arg(sourceGraph->name()).arg(FilterUtils::getFilterName(config.type)));
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
    QMessageBox::information(this, tr("滤波完成"),
        tr("已使用 %1 对数据进行滤波处理").arg(FilterUtils::getFilterName(config.type)));
}

void PlotterWindow::onTriggerCaptureClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("触发捕获设置"));
    dialog.setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // ========== 触发源选择 ==========
    QGroupBox* sourceGroup = new QGroupBox(tr("触发源"));
    QFormLayout* sourceLayout = new QFormLayout(sourceGroup);

    QComboBox* sourceCurveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        // 排除差值曲线
        bool isDiffCurve = false;
        for (const auto& diff : m_diffCurves) {
            if (diff.graph == graph) {
                isDiffCurve = true;
                break;
            }
        }
        if (!isDiffCurve) {
            sourceCurveCombo->addItem(graph->name(), i);
        }
    }

    if (sourceCurveCombo->count() == 0) {
        QMessageBox::warning(this, tr("无可用曲线"), tr("没有可用于触发的曲线"));
        return;
    }

    // 选择当前配置的曲线
    for (int i = 0; i < sourceCurveCombo->count(); ++i) {
        if (sourceCurveCombo->itemData(i).toInt() == m_triggerConfig.sourceCurve) {
            sourceCurveCombo->setCurrentIndex(i);
            break;
        }
    }

    sourceLayout->addRow(tr("触发曲线:"), sourceCurveCombo);
    mainLayout->addWidget(sourceGroup);

    // ========== 触发条件 ==========
    QGroupBox* conditionGroup = new QGroupBox(tr("触发条件"));
    QFormLayout* conditionLayout = new QFormLayout(conditionGroup);

    QComboBox* triggerTypeCombo = new QComboBox;
    triggerTypeCombo->addItem(tr("上升沿"), static_cast<int>(TriggerType::RisingEdge));
    triggerTypeCombo->addItem(tr("下降沿"), static_cast<int>(TriggerType::FallingEdge));
    triggerTypeCombo->addItem(tr("高电平"), static_cast<int>(TriggerType::LevelHigh));
    triggerTypeCombo->addItem(tr("低电平"), static_cast<int>(TriggerType::LevelLow));

    // 选择当前类型
    for (int i = 0; i < triggerTypeCombo->count(); ++i) {
        if (triggerTypeCombo->itemData(i).toInt() == static_cast<int>(m_triggerConfig.type)) {
            triggerTypeCombo->setCurrentIndex(i);
            break;
        }
    }
    conditionLayout->addRow(tr("触发类型:"), triggerTypeCombo);

    QDoubleSpinBox* levelSpin = new QDoubleSpinBox;
    levelSpin->setRange(-1e9, 1e9);
    levelSpin->setDecimals(4);
    levelSpin->setValue(m_triggerConfig.level);
    conditionLayout->addRow(tr("触发电平:"), levelSpin);

    mainLayout->addWidget(conditionGroup);

    // ========== 捕获参数 ==========
    QGroupBox* captureGroup = new QGroupBox(tr("捕获参数"));
    QFormLayout* captureLayout = new QFormLayout(captureGroup);

    QSpinBox* preTriggerSpin = new QSpinBox;
    preTriggerSpin->setRange(0, 10000);
    preTriggerSpin->setValue(m_triggerConfig.preTriggerPoints);
    preTriggerSpin->setSuffix(tr(" 点"));
    captureLayout->addRow(tr("触发前点数:"), preTriggerSpin);

    QSpinBox* postTriggerSpin = new QSpinBox;
    postTriggerSpin->setRange(1, 50000);
    postTriggerSpin->setValue(m_triggerConfig.postTriggerPoints);
    postTriggerSpin->setSuffix(tr(" 点"));
    captureLayout->addRow(tr("触发后点数:"), postTriggerSpin);

    QCheckBox* singleShotCheck = new QCheckBox(tr("单次触发（触发一次后停止）"));
    singleShotCheck->setChecked(m_triggerConfig.singleShot);
    captureLayout->addRow(singleShotCheck);

    mainLayout->addWidget(captureGroup);

    // ========== 启用/禁用 ==========
    QCheckBox* enableCheck = new QCheckBox(tr("启用触发捕获"));
    enableCheck->setChecked(m_triggerConfig.enabled);
    mainLayout->addWidget(enableCheck);

    // ========== 按钮 ==========
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 保存配置
    m_triggerConfig.sourceCurve = sourceCurveCombo->currentData().toInt();
    m_triggerConfig.type = static_cast<TriggerType>(triggerTypeCombo->currentData().toInt());
    m_triggerConfig.level = levelSpin->value();
    m_triggerConfig.preTriggerPoints = preTriggerSpin->value();
    m_triggerConfig.postTriggerPoints = postTriggerSpin->value();
    m_triggerConfig.singleShot = singleShotCheck->isChecked();
    m_triggerConfig.enabled = enableCheck->isChecked();

    // 初始化触发状态
    if (m_triggerConfig.enabled) {
        m_triggerArmed = true;
        m_triggerFired = false;
        m_lastTriggerValue = 0;
        m_triggerPosition = 0;

        // 初始化预触发缓冲
        m_preTriggerBuffer.resize(m_plot->graphCount());
        for (auto& buf : m_preTriggerBuffer) {
            buf.clear();
            buf.reserve(m_triggerConfig.preTriggerPoints);
        }

        // 创建触发电平线
        if (!m_triggerLevelLine) {
            m_triggerLevelLine = new QCPItemLine(m_plot);
            m_triggerLevelLine->setPen(QPen(Qt::red, 1, Qt::DashDotLine));
        }
        m_triggerLevelLine->setVisible(true);
        m_triggerLevelLine->start->setType(QCPItemPosition::ptPlotCoords);
        m_triggerLevelLine->end->setType(QCPItemPosition::ptPlotCoords);
        m_triggerLevelLine->start->setCoords(m_plot->xAxis->range().lower, m_triggerConfig.level);
        m_triggerLevelLine->end->setCoords(m_plot->xAxis->range().upper, m_triggerConfig.level);

        // 创建触发状态文本
        if (!m_triggerStatusText) {
            m_triggerStatusText = new QCPItemText(m_plot);
            m_triggerStatusText->setPositionAlignment(Qt::AlignTop | Qt::AlignRight);
            m_triggerStatusText->position->setType(QCPItemPosition::ptAxisRectRatio);
            m_triggerStatusText->position->setCoords(0.99, 0.01);
            m_triggerStatusText->setFont(QFont("sans", 10, QFont::Bold));
            m_triggerStatusText->setColor(Qt::red);
        }
        m_triggerStatusText->setVisible(true);
        m_triggerStatusText->setText(tr("触发: 等待中"));

        LOG_INFO(QString("Trigger capture enabled: type=%1, level=%2")
            .arg(static_cast<int>(m_triggerConfig.type)).arg(m_triggerConfig.level));
    } else {
        m_triggerArmed = false;
        m_triggerFired = false;

        // 隐藏触发相关UI
        if (m_triggerLevelLine) {
            m_triggerLevelLine->setVisible(false);
        }
        if (m_triggerStatusText) {
            m_triggerStatusText->setVisible(false);
        }

        LOG_INFO("Trigger capture disabled");
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::checkTriggerCondition(int curveIndex, double value)
{
    if (!m_triggerConfig.enabled || curveIndex != m_triggerConfig.sourceCurve) {
        return;
    }

    // 单次触发已触发，不再检测
    if (m_triggerConfig.singleShot && m_triggerFired) {
        return;
    }

    bool triggered = false;

    switch (m_triggerConfig.type) {
    case TriggerType::RisingEdge:
        // 上升沿：上次值 < 电平 且 当前值 >= 电平
        triggered = (m_lastTriggerValue < m_triggerConfig.level && value >= m_triggerConfig.level);
        break;

    case TriggerType::FallingEdge:
        // 下降沿：上次值 > 电平 且 当前值 <= 电平
        triggered = (m_lastTriggerValue > m_triggerConfig.level && value <= m_triggerConfig.level);
        break;

    case TriggerType::LevelHigh:
        // 高电平：当前值 >= 电平
        triggered = (value >= m_triggerConfig.level);
        break;

    case TriggerType::LevelLow:
        // 低电平：当前值 <= 电平
        triggered = (value <= m_triggerConfig.level);
        break;
    }

    m_lastTriggerValue = value;

    if (triggered && m_triggerArmed) {
        m_triggerFired = true;
        m_triggerArmed = false;
        m_triggerPosition = m_dataIndex;

        // 更新状态显示
        if (m_triggerStatusText) {
            m_triggerStatusText->setText(tr("触发: 已触发 @%1").arg(m_triggerPosition));
            m_triggerStatusText->setColor(Qt::green);
        }

        LOG_INFO(QString("Trigger fired at position %1, value=%2").arg(m_triggerPosition).arg(value));

        // 在单次模式下，触发后暂停
        if (m_triggerConfig.singleShot) {
            // 延迟一定点数后暂停（等待后触发数据收集完成）
            QTimer::singleShot(100, this, [this]() {
                if (m_triggerConfig.singleShot && m_triggerFired) {
                    setPaused(true);
                    if (m_pauseAction) {
                        m_pauseAction->setChecked(true);
                    }
                }
            });
        }
    }

    // 重新准备触发（非单次模式）
    if (!m_triggerConfig.singleShot && m_triggerFired) {
        // 收集完后触发点数后重新准备
        if (m_dataIndex - m_triggerPosition >= m_triggerConfig.postTriggerPoints) {
            m_triggerArmed = true;
            m_triggerFired = false;
            if (m_triggerStatusText) {
                m_triggerStatusText->setText(tr("触发: 等待中"));
                m_triggerStatusText->setColor(Qt::red);
            }
        }
    }
}

void PlotterWindow::onPeakAnnotationClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("峰值自动标注设置"));
    dialog.setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // ========== 目标曲线选择 ==========
    QGroupBox* targetGroup = new QGroupBox(tr("目标曲线"));
    QFormLayout* targetLayout = new QFormLayout(targetGroup);

    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        // 排除差值曲线
        bool isDiffCurve = false;
        for (const auto& diff : m_diffCurves) {
            if (diff.graph == graph) {
                isDiffCurve = true;
                break;
            }
        }
        if (!isDiffCurve) {
            curveCombo->addItem(graph->name(), i);
        }
    }

    if (curveCombo->count() == 0) {
        QMessageBox::warning(this, tr("无可用曲线"), tr("没有可用于峰值检测的曲线"));
        return;
    }

    // 选择当前配置的曲线
    for (int i = 0; i < curveCombo->count(); ++i) {
        if (curveCombo->itemData(i).toInt() == m_peakConfig.targetCurve) {
            curveCombo->setCurrentIndex(i);
            break;
        }
    }
    targetLayout->addRow(tr("曲线:"), curveCombo);
    mainLayout->addWidget(targetGroup);

    // ========== 检测参数 ==========
    QGroupBox* paramGroup = new QGroupBox(tr("检测参数"));
    QFormLayout* paramLayout = new QFormLayout(paramGroup);

    QDoubleSpinBox* thresholdSpin = new QDoubleSpinBox;
    thresholdSpin->setRange(0.001, 1.0);
    thresholdSpin->setDecimals(3);
    thresholdSpin->setSingleStep(0.01);
    thresholdSpin->setValue(m_peakConfig.threshold);
    thresholdSpin->setToolTip(tr("峰值必须高于数据振幅的此比例才被识别"));
    paramLayout->addRow(tr("阈值（振幅比例）:"), thresholdSpin);

    QSpinBox* minDistanceSpin = new QSpinBox;
    minDistanceSpin->setRange(1, 1000);
    minDistanceSpin->setValue(m_peakConfig.minDistance);
    minDistanceSpin->setSuffix(tr(" 点"));
    minDistanceSpin->setToolTip(tr("相邻峰值之间的最小距离"));
    paramLayout->addRow(tr("最小峰值间距:"), minDistanceSpin);

    QSpinBox* maxCountSpin = new QSpinBox;
    maxCountSpin->setRange(1, 100);
    maxCountSpin->setValue(m_peakConfig.maxPeakCount);
    maxCountSpin->setToolTip(tr("最多标注的峰值数量"));
    paramLayout->addRow(tr("最大标注数量:"), maxCountSpin);

    mainLayout->addWidget(paramGroup);

    // ========== 峰值类型 ==========
    QGroupBox* typeGroup = new QGroupBox(tr("峰值类型"));
    QVBoxLayout* typeLayout = new QVBoxLayout(typeGroup);

    QCheckBox* maxPeaksCheck = new QCheckBox(tr("标注极大值（峰）"));
    maxPeaksCheck->setChecked(m_peakConfig.showMaxPeaks);
    typeLayout->addWidget(maxPeaksCheck);

    QCheckBox* minPeaksCheck = new QCheckBox(tr("标注极小值（谷）"));
    minPeaksCheck->setChecked(m_peakConfig.showMinPeaks);
    typeLayout->addWidget(minPeaksCheck);

    mainLayout->addWidget(typeGroup);

    // ========== 启用/禁用 ==========
    QCheckBox* enableCheck = new QCheckBox(tr("启用峰值自动标注"));
    enableCheck->setChecked(m_peakConfig.enabled);
    mainLayout->addWidget(enableCheck);

    // ========== 按钮 ==========
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 保存配置
    m_peakConfig.targetCurve = curveCombo->currentData().toInt();
    m_peakConfig.threshold = thresholdSpin->value();
    m_peakConfig.minDistance = minDistanceSpin->value();
    m_peakConfig.maxPeakCount = maxCountSpin->value();
    m_peakConfig.showMaxPeaks = maxPeaksCheck->isChecked();
    m_peakConfig.showMinPeaks = minPeaksCheck->isChecked();
    m_peakConfig.enabled = enableCheck->isChecked();

    // 清除旧的标注
    for (auto* marker : m_peakMarkers) {
        m_plot->removeItem(marker);
    }
    m_peakMarkers.clear();;

    for (auto* label : m_peakLabels) {
        m_plot->removeItem(label);
    }
    m_peakLabels.clear();

    if (m_peakConfig.enabled) {
        updatePeakAnnotations();
        LOG_INFO(QString("Peak annotation enabled: curve=%1, threshold=%2, maxCount=%3")
            .arg(m_peakConfig.targetCurve).arg(m_peakConfig.threshold).arg(m_peakConfig.maxPeakCount));
    } else {
        LOG_INFO("Peak annotation disabled");
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::updatePeakAnnotations()
{
    if (!m_peakConfig.enabled) {
        return;
    }

    if (m_peakConfig.targetCurve < 0 || m_peakConfig.targetCurve >= m_plot->graphCount()) {
        LOG_DEBUG(QString("Peak annotation: invalid target curve %1, graph count %2")
            .arg(m_peakConfig.targetCurve).arg(m_plot->graphCount()));
        return;
    }

    QCPGraph* graph = m_plot->graph(m_peakConfig.targetCurve);
    if (!graph || graph->data()->isEmpty()) {
        LOG_DEBUG("Peak annotation: graph is null or empty");
        return;
    }

    // 只获取当前可见范围内的数据，而不是全部数据
    double xMin = m_plot->xAxis->range().lower;
    double xMax = m_plot->xAxis->range().upper;

    // 提取可见范围内的数据
    QVector<double> xData, yData;
    for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
        if (it->key >= xMin && it->key <= xMax) {
            xData.append(it->key);
            yData.append(it->value);
        }
    }

    if (yData.size() < 3) {
        LOG_DEBUG(QString("Peak annotation: not enough data points in visible range (%1)").arg(yData.size()));
        return;
    }

    // 计算数据范围用于阈值
    double minVal = *std::min_element(yData.begin(), yData.end());
    double maxVal = *std::max_element(yData.begin(), yData.end());
    double range = maxVal - minVal;

    // 使用更宽松的阈值计算：取范围的百分比，但设置最小绝对阈值
    double threshold = range * m_peakConfig.threshold;
    // 如果范围太小，使用绝对阈值避免检测不到峰值
    if (range < 1.0) {
        threshold = range * 0.01;  // 1% 的范围作为阈值
    }
    if (threshold < 0.001) {
        threshold = 0.001;  // 最小阈值
    }

    LOG_INFO(QString("Peak annotation: data range [%1, %2], range=%3, threshold=%4, points=%5")
        .arg(minVal).arg(maxVal).arg(range).arg(threshold).arg(yData.size()));

    // 清除旧的标注
    for (auto* marker : m_peakMarkers) {
        m_plot->removeItem(marker);
    }
    m_peakMarkers.clear();;

    for (auto* label : m_peakLabels) {
        m_plot->removeItem(label);
    }
    m_peakLabels.clear();

    // 存储找到的峰值（索引，值，是否为极大值）
    struct PeakInfo {
        int index;
        double x;
        double y;
        bool isMax;
        double prominence;  // 峰值突出度
        bool isGlobalExtreme = false;  // 是否为全局极值
    };
    QVector<PeakInfo> peaks;

    // 先找出全局最大值和最小值
    int globalMaxIdx = 0, globalMinIdx = 0;
    double globalMax = yData[0], globalMin = yData[0];
    for (int i = 1; i < yData.size(); ++i) {
        if (yData[i] > globalMax) {
            globalMax = yData[i];
            globalMaxIdx = i;
        }
        if (yData[i] < globalMin) {
            globalMin = yData[i];
            globalMinIdx = i;
        }
    }

    // 添加全局最大值
    if (m_peakConfig.showMaxPeaks) {
        peaks.append({globalMaxIdx, xData[globalMaxIdx], globalMax, true, range, true});
    }

    // 添加全局最小值
    if (m_peakConfig.showMinPeaks) {
        peaks.append({globalMinIdx, xData[globalMinIdx], globalMin, false, range, true});
    }

    // 检测局部峰值 - 使用更宽松的条件
    for (int i = 1; i < yData.size() - 1; ++i) {
        // 跳过全局极值点（已经添加过了）
        if (i == globalMaxIdx || i == globalMinIdx) continue;

        double prev = yData[i - 1];
        double curr = yData[i];
        double next = yData[i + 1];

        // 检测极大值：当前点大于前后两点
        if (m_peakConfig.showMaxPeaks && curr > prev && curr > next) {
            double prominence = qMax(curr - prev, curr - next);
            if (prominence >= threshold) {
                peaks.append({i, xData[i], yData[i], true, prominence, false});
            }
        }

        // 检测极小值：当前点小于前后两点
        if (m_peakConfig.showMinPeaks && curr < prev && curr < next) {
            double prominence = qMax(prev - curr, next - curr);
            if (prominence >= threshold) {
                peaks.append({i, xData[i], yData[i], false, prominence, false});
            }
        }
    }

    // 按突出度排序，但全局极值始终在前
    std::sort(peaks.begin(), peaks.end(), [](const PeakInfo& a, const PeakInfo& b) {
        if (a.isGlobalExtreme != b.isGlobalExtreme) return a.isGlobalExtreme;
        return a.prominence > b.prominence;
    });

    // 应用最小间距过滤（全局极值不参与过滤）
    QVector<PeakInfo> filteredPeaks;
    for (const auto& peak : peaks) {
        if (peak.isGlobalExtreme) {
            filteredPeaks.append(peak);
            continue;
        }
        bool tooClose = false;
        for (const auto& existing : filteredPeaks) {
            if (qAbs(peak.index - existing.index) < m_peakConfig.minDistance) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            filteredPeaks.append(peak);
            if (filteredPeaks.size() >= m_peakConfig.maxPeakCount) {
                break;
            }
        }
    }

    // 创建标注
    for (const auto& peak : filteredPeaks) {
        // 全局极值使用更大的标记
        double markerSize = peak.isGlobalExtreme ? 10.0 : 6.0;
        QColor markerColor = peak.isMax ? Qt::red : Qt::blue;
        if (peak.isGlobalExtreme) {
            markerColor = peak.isMax ? QColor(255, 0, 0) : QColor(0, 0, 255);  // 纯红/纯蓝
        }

        // 创建圆形标记点
        QCPItemEllipse* marker = new QCPItemEllipse(m_plot);
        marker->topLeft->setType(QCPItemPosition::ptPlotCoords);
        marker->bottomRight->setType(QCPItemPosition::ptPlotCoords);
        double xScale = m_plot->xAxis->range().size() / m_plot->width();
        double yScale = m_plot->yAxis->range().size() / m_plot->height();
        marker->topLeft->setCoords(peak.x - markerSize/2 * xScale, peak.y + markerSize/2 * yScale);
        marker->bottomRight->setCoords(peak.x + markerSize/2 * xScale, peak.y - markerSize/2 * yScale);
        marker->setPen(QPen(markerColor, peak.isGlobalExtreme ? 3.0 : 2.0));
        marker->setBrush(QBrush(markerColor));
        m_peakMarkers.append(marker);

        // 创建标签 - 全局极值显示"Max:"或"Min:"前缀
        QCPItemText* label = new QCPItemText(m_plot);
        label->setPositionAlignment(peak.isMax ? (Qt::AlignBottom | Qt::AlignHCenter) : (Qt::AlignTop | Qt::AlignHCenter));
        label->position->setType(QCPItemPosition::ptPlotCoords);
        double labelOffset = peak.isGlobalExtreme ? range * 0.12 : range * 0.08;
        label->position->setCoords(peak.x, peak.y + (peak.isMax ? labelOffset : -labelOffset));

        // 格式化标签文本
        QString labelText;
        if (peak.isGlobalExtreme) {
            labelText = QString("%1: %2").arg(peak.isMax ? "MAX" : "MIN").arg(peak.y, 0, 'f', 2);
        } else {
            labelText = QString("%1").arg(peak.y, 0, 'f', 1);
        }
        label->setText(labelText);
        label->setFont(QFont("sans", peak.isGlobalExtreme ? 10 : 9, QFont::Bold));
        label->setColor(peak.isMax ? Qt::darkRed : Qt::darkBlue);
        label->setPadding(QMargins(5, 3, 5, 3));
        label->setBrush(QBrush(QColor(255, 255, 255, 230)));
        label->setPen(QPen(markerColor, peak.isGlobalExtreme ? 2 : 1));
        m_peakLabels.append(label);
    }

    LOG_INFO(QString("Peak annotation: found %1 peaks (total candidates: %2)")
        .arg(filteredPeaks.size()).arg(peaks.size()));
}

CurveStatistics PlotterWindow::calculateAdvancedStatistics(int curveIndex, double rangeStart, double rangeEnd)
{
    CurveStatistics stats;

    if (curveIndex < 0 || curveIndex >= m_plot->graphCount()) {
        return stats;
    }

    QCPGraph* graph = m_plot->graph(curveIndex);
    if (graph->data()->isEmpty()) {
        return stats;
    }

    // 收集数据
    QVector<double> xData, yData;
    double firstX = 0, lastX = 0;
    bool first = true;

    for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
        double x = it->key;
        double y = it->value;

        // 检查是否在范围内
        if (rangeStart >= 0 && rangeEnd > rangeStart) {
            if (x < rangeStart || x > rangeEnd) {
                continue;
            }
            stats.isRangeStats = true;
            stats.rangeStart = rangeStart;
            stats.rangeEnd = rangeEnd;
        }

        xData.append(x);
        yData.append(y);

        if (first) {
            firstX = x;
            first = false;
        }
        lastX = x;
    }

    if (yData.isEmpty()) {
        return stats;
    }

    stats.dataCount = yData.size();

    // 计算基本统计
    double sum = 0;
    double sumSq = 0;
    stats.minValue = yData[0];
    stats.maxValue = yData[0];

    for (double v : yData) {
        sum += v;
        sumSq += v * v;
        stats.minValue = qMin(stats.minValue, v);
        stats.maxValue = qMax(stats.maxValue, v);
    }

    stats.average = sum / stats.dataCount;
    double variance = (sumSq / stats.dataCount) - (stats.average * stats.average);
    stats.stdDev = std::sqrt(qMax(0.0, variance));

    // 峰峰值
    stats.peakToPeak = stats.maxValue - stats.minValue;

    // RMS（均方根值）
    stats.rms = std::sqrt(sumSq / stats.dataCount);

    // 波峰因子 (Crest Factor = Peak / RMS)
    double peakAbs = qMax(qAbs(stats.maxValue), qAbs(stats.minValue));
    stats.crestFactor = (stats.rms > 0) ? (peakAbs / stats.rms) : 0;

    // 中位数
    QVector<double> sortedData = yData;
    std::sort(sortedData.begin(), sortedData.end());
    if (stats.dataCount % 2 == 0) {
        stats.median = (sortedData[stats.dataCount / 2 - 1] + sortedData[stats.dataCount / 2]) / 2.0;
    } else {
        stats.median = sortedData[stats.dataCount / 2];
    }

    // 时长和采样率
    stats.duration = lastX - firstX;
    if (stats.duration > 0 && stats.dataCount > 1) {
        stats.sampleRate = (stats.dataCount - 1) / stats.duration;
    }

    return stats;
}

void PlotterWindow::onAdvancedStatsClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("增强统计分析"));
    dialog.setMinimumWidth(500);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // 曲线选择
    QHBoxLayout* selectLayout = new QHBoxLayout;
    selectLayout->addWidget(new QLabel(tr("选择曲线:")));

    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph* graph = m_plot->graph(i);
        bool isDiffCurve = false;
        for (const auto& diff : m_diffCurves) {
            if (diff.graph == graph) {
                isDiffCurve = true;
                break;
            }
        }
        if (!isDiffCurve) {
            curveCombo->addItem(graph->name(), i);
        }
    }
    selectLayout->addWidget(curveCombo);

    // 选区统计复选框
    QCheckBox* rangeCheck = new QCheckBox(tr("使用测量游标区间"));
    rangeCheck->setToolTip(tr("使用游标A和B之间的区间进行统计"));
    rangeCheck->setEnabled(m_measureCursor1.active && m_measureCursor2.active);
    selectLayout->addWidget(rangeCheck);

    selectLayout->addStretch();
    mainLayout->addLayout(selectLayout);

    // 统计结果显示
    QGroupBox* resultGroup = new QGroupBox(tr("统计结果"));
    QGridLayout* resultLayout = new QGridLayout(resultGroup);

    // 创建标签
    QLabel* countLabel = new QLabel("-");
    QLabel* minLabel = new QLabel("-");
    QLabel* maxLabel = new QLabel("-");
    QLabel* ppLabel = new QLabel("-");
    QLabel* avgLabel = new QLabel("-");
    QLabel* medianLabel = new QLabel("-");
    QLabel* stdLabel = new QLabel("-");
    QLabel* rmsLabel = new QLabel("-");
    QLabel* cfLabel = new QLabel("-");
    QLabel* rateLabel = new QLabel("-");
    QLabel* durationLabel = new QLabel("-");

    int row = 0;
    resultLayout->addWidget(new QLabel(tr("数据点数:")), row, 0);
    resultLayout->addWidget(countLabel, row, 1);
    resultLayout->addWidget(new QLabel(tr("采样率:")), row, 2);
    resultLayout->addWidget(rateLabel, row, 3);
    row++;

    resultLayout->addWidget(new QLabel(tr("数据时长:")), row, 0);
    resultLayout->addWidget(durationLabel, row, 1);
    row++;

    resultLayout->addWidget(new QLabel(tr("最小值:")), row, 0);
    resultLayout->addWidget(minLabel, row, 1);
    resultLayout->addWidget(new QLabel(tr("最大值:")), row, 2);
    resultLayout->addWidget(maxLabel, row, 3);
    row++;

    resultLayout->addWidget(new QLabel(tr("峰峰值:")), row, 0);
    resultLayout->addWidget(ppLabel, row, 1);
    resultLayout->addWidget(new QLabel(tr("平均值:")), row, 2);
    resultLayout->addWidget(avgLabel, row, 3);
    row++;

    resultLayout->addWidget(new QLabel(tr("中位数:")), row, 0);
    resultLayout->addWidget(medianLabel, row, 1);
    resultLayout->addWidget(new QLabel(tr("标准差:")), row, 2);
    resultLayout->addWidget(stdLabel, row, 3);
    row++;

    resultLayout->addWidget(new QLabel(tr("RMS:")), row, 0);
    resultLayout->addWidget(rmsLabel, row, 1);
    resultLayout->addWidget(new QLabel(tr("波峰因子:")), row, 2);
    resultLayout->addWidget(cfLabel, row, 3);

    mainLayout->addWidget(resultGroup);

    // 更新统计结果的函数
    auto updateStats = [&]() {
        int idx = curveCombo->currentData().toInt();
        double rangeStart = -1, rangeEnd = -1;

        if (rangeCheck->isChecked() && m_measureCursor1.active && m_measureCursor2.active) {
            rangeStart = qMin(m_measureCursor1.xPos, m_measureCursor2.xPos);
            rangeEnd = qMax(m_measureCursor1.xPos, m_measureCursor2.xPos);
        }

        CurveStatistics stats = calculateAdvancedStatistics(idx, rangeStart, rangeEnd);

        countLabel->setText(QString::number(stats.dataCount));
        minLabel->setText(QString::number(stats.minValue, 'f', 4));
        maxLabel->setText(QString::number(stats.maxValue, 'f', 4));
        ppLabel->setText(QString::number(stats.peakToPeak, 'f', 4));
        avgLabel->setText(QString::number(stats.average, 'f', 4));
        medianLabel->setText(QString::number(stats.median, 'f', 4));
        stdLabel->setText(QString::number(stats.stdDev, 'f', 4));
        rmsLabel->setText(QString::number(stats.rms, 'f', 4));
        cfLabel->setText(QString::number(stats.crestFactor, 'f', 3));

        if (stats.sampleRate > 0) {
            rateLabel->setText(QString("%1 Hz").arg(stats.sampleRate, 0, 'f', 1));
        } else {
            rateLabel->setText("-");
        }

        if (stats.duration > 0) {
            if (stats.duration < 1) {
                durationLabel->setText(QString("%1 ms").arg(stats.duration * 1000, 0, 'f', 1));
            } else {
                durationLabel->setText(QString("%1 s").arg(stats.duration, 0, 'f', 2));
            }
        } else {
            durationLabel->setText("-");
        }
    };

    connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), updateStats);
    connect(rangeCheck, &QCheckBox::toggled, updateStats);
    updateStats();

    // 导出按钮
    QPushButton* exportBtn = new QPushButton(tr("导出统计报告"));
    connect(exportBtn, &QPushButton::clicked, [&]() {
        QString filename = QFileDialog::getSaveFileName(
            this, tr("导出统计报告"), QString(), tr("文本文件 (*.txt);;所有文件 (*)"));
        if (filename.isEmpty()) return;

        QFile file(filename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            // Qt6 默认使用 UTF-8 编码

            int idx = curveCombo->currentData().toInt();
            double rangeStart = -1, rangeEnd = -1;
            if (rangeCheck->isChecked() && m_measureCursor1.active && m_measureCursor2.active) {
                rangeStart = qMin(m_measureCursor1.xPos, m_measureCursor2.xPos);
                rangeEnd = qMax(m_measureCursor1.xPos, m_measureCursor2.xPos);
            }
            CurveStatistics stats = calculateAdvancedStatistics(idx, rangeStart, rangeEnd);

            out << "====== 曲线统计报告 ======\n";
            out << "曲线名称: " << m_plot->graph(idx)->name() << "\n";
            out << "统计时间: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
            if (stats.isRangeStats) {
                out << "统计区间: " << stats.rangeStart << " - " << stats.rangeEnd << "\n";
            }
            out << "\n";
            out << "数据点数: " << stats.dataCount << "\n";
            out << "采样率: " << (stats.sampleRate > 0 ? QString::number(stats.sampleRate, 'f', 1) + " Hz" : "N/A") << "\n";
            out << "数据时长: " << stats.duration << " s\n";
            out << "\n";
            out << "最小值: " << stats.minValue << "\n";
            out << "最大值: " << stats.maxValue << "\n";
            out << "峰峰值: " << stats.peakToPeak << "\n";
            out << "平均值: " << stats.average << "\n";
            out << "中位数: " << stats.median << "\n";
            out << "标准差: " << stats.stdDev << "\n";
            out << "RMS: " << stats.rms << "\n";
            out << "波峰因子: " << stats.crestFactor << "\n";

            file.close();
            QMessageBox::information(this, tr("导出成功"), tr("统计报告已保存到:\n%1").arg(filename));
        }
    });
    mainLayout->addWidget(exportBtn);

    // 关闭按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    dialog.exec();
}

void PlotterWindow::onAlarmConfigClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("数据报警配置"));
    dialog.setMinimumWidth(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // 启用开关
    QCheckBox* enableCheck = new QCheckBox(tr("启用数据报警"));
    enableCheck->setChecked(m_alarmConfig.enabled);
    mainLayout->addWidget(enableCheck);

    // 报警源曲线选择
    QGroupBox* sourceGroup = new QGroupBox(tr("报警源"));
    QHBoxLayout* sourceLayout = new QHBoxLayout(sourceGroup);
    QLabel* sourceLabel = new QLabel(tr("监控曲线:"));
    QComboBox* sourceCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        sourceCombo->addItem(m_plot->graph(i)->name(), i);
    }
    sourceCombo->setCurrentIndex(m_alarmConfig.sourceCurve);
    sourceLayout->addWidget(sourceLabel);
    sourceLayout->addWidget(sourceCombo);
    sourceLayout->addStretch();
    mainLayout->addWidget(sourceGroup);

    // 上下限报警
    QGroupBox* limitGroup = new QGroupBox(tr("上下限报警"));
    QGridLayout* limitLayout = new QGridLayout(limitGroup);

    QCheckBox* upperCheck = new QCheckBox(tr("启用上限报警"));
    upperCheck->setChecked(m_alarmConfig.upperLimitEnabled);
    QDoubleSpinBox* upperSpin = new QDoubleSpinBox;
    upperSpin->setRange(-1e9, 1e9);
    upperSpin->setDecimals(4);
    upperSpin->setValue(m_alarmConfig.upperLimit);

    QCheckBox* lowerCheck = new QCheckBox(tr("启用下限报警"));
    lowerCheck->setChecked(m_alarmConfig.lowerLimitEnabled);
    QDoubleSpinBox* lowerSpin = new QDoubleSpinBox;
    lowerSpin->setRange(-1e9, 1e9);
    lowerSpin->setDecimals(4);
    lowerSpin->setValue(m_alarmConfig.lowerLimit);

    limitLayout->addWidget(upperCheck, 0, 0);
    limitLayout->addWidget(new QLabel(tr("上限值:")), 0, 1);
    limitLayout->addWidget(upperSpin, 0, 2);
    limitLayout->addWidget(lowerCheck, 1, 0);
    limitLayout->addWidget(new QLabel(tr("下限值:")), 1, 1);
    limitLayout->addWidget(lowerSpin, 1, 2);
    mainLayout->addWidget(limitGroup);

    // 变化率报警
    QGroupBox* rateGroup = new QGroupBox(tr("变化率报警"));
    QHBoxLayout* rateLayout = new QHBoxLayout(rateGroup);
    QCheckBox* rateCheck = new QCheckBox(tr("启用变化率报警"));
    rateCheck->setChecked(m_alarmConfig.rateAlarmEnabled);
    QDoubleSpinBox* rateSpin = new QDoubleSpinBox;
    rateSpin->setRange(0, 1e6);
    rateSpin->setDecimals(2);
    rateSpin->setValue(m_alarmConfig.rateLimit);
    rateLayout->addWidget(rateCheck);
    rateLayout->addWidget(new QLabel(tr("变化率限制:")));
    rateLayout->addWidget(rateSpin);
    rateLayout->addWidget(new QLabel(tr("单位/采样点")));
    rateLayout->addStretch();
    mainLayout->addWidget(rateGroup);

    // 报警动作
    QGroupBox* actionGroup = new QGroupBox(tr("报警动作"));
    QVBoxLayout* actionLayout = new QVBoxLayout(actionGroup);
    QCheckBox* soundCheck = new QCheckBox(tr("声音提示"));
    soundCheck->setChecked(m_alarmConfig.soundAlert);
    QCheckBox* visualCheck = new QCheckBox(tr("视觉提示（界面闪烁）"));
    visualCheck->setChecked(m_alarmConfig.visualAlert);
    QCheckBox* pauseCheck = new QCheckBox(tr("自动暂停采集"));
    pauseCheck->setChecked(m_alarmConfig.autoPause);
    actionLayout->addWidget(soundCheck);
    actionLayout->addWidget(visualCheck);
    actionLayout->addWidget(pauseCheck);
    mainLayout->addWidget(actionGroup);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        m_alarmConfig.enabled = enableCheck->isChecked();
        m_alarmConfig.sourceCurve = sourceCombo->currentData().toInt();
        m_alarmConfig.upperLimitEnabled = upperCheck->isChecked();
        m_alarmConfig.upperLimit = upperSpin->value();
        m_alarmConfig.lowerLimitEnabled = lowerCheck->isChecked();
        m_alarmConfig.lowerLimit = lowerSpin->value();
        m_alarmConfig.rateAlarmEnabled = rateCheck->isChecked();
        m_alarmConfig.rateLimit = rateSpin->value();
        m_alarmConfig.soundAlert = soundCheck->isChecked();
        m_alarmConfig.visualAlert = visualCheck->isChecked();
        m_alarmConfig.autoPause = pauseCheck->isChecked();

        updateAlarmDisplay();
    }
}

void PlotterWindow::checkAlarmCondition(int curveIndex, double value)
{
    if (!m_alarmConfig.enabled) return;
    if (curveIndex != m_alarmConfig.sourceCurve) return;

    bool alarmTriggered = false;
    QString alarmReason;

    // 检查上限
    if (m_alarmConfig.upperLimitEnabled && value > m_alarmConfig.upperLimit) {
        alarmTriggered = true;
        alarmReason = tr("超过上限 (%1 > %2)").arg(value, 0, 'f', 4).arg(m_alarmConfig.upperLimit);
    }

    // 检查下限
    if (m_alarmConfig.lowerLimitEnabled && value < m_alarmConfig.lowerLimit) {
        alarmTriggered = true;
        alarmReason = tr("低于下限 (%1 < %2)").arg(value, 0, 'f', 4).arg(m_alarmConfig.lowerLimit);
    }

    // 检查变化率
    if (m_alarmConfig.rateAlarmEnabled && m_lastAlarmValue != 0) {
        double rate = qAbs(value - m_lastAlarmValue);
        if (rate > m_alarmConfig.rateLimit) {
            alarmTriggered = true;
            alarmReason = tr("变化率过大 (Δ=%1 > %2)").arg(rate, 0, 'f', 4).arg(m_alarmConfig.rateLimit);
        }
    }
    m_lastAlarmValue = value;

    if (alarmTriggered && !m_alarmActive) {
        m_alarmActive = true;
        m_alarmCount++;

        // 声音提示
        if (m_alarmConfig.soundAlert) {
            QApplication::beep();
        }

        // 视觉提示 - 启动闪烁
        if (m_alarmConfig.visualAlert) {
            if (!m_alarmBlinkTimer) {
                m_alarmBlinkTimer = new QTimer(this);
                connect(m_alarmBlinkTimer, &QTimer::timeout, [this]() {
                    static bool toggle = false;
                    toggle = !toggle;
                    if (m_alarmStatusText) {
                        m_alarmStatusText->setColor(toggle ? Qt::red : Qt::darkRed);
                        m_plot->replot(QCustomPlot::rpQueuedReplot);
                    }
                });
            }
            m_alarmBlinkTimer->start(500);
        }

        // 更新状态文本
        if (m_alarmStatusText) {
            m_alarmStatusText->setText(tr("报警! %1").arg(alarmReason));
            m_alarmStatusText->setVisible(true);
        }

        // 自动暂停
        if (m_alarmConfig.autoPause) {
            setPaused(true);
        }
    } else if (!alarmTriggered && m_alarmActive) {
        // 报警解除
        m_alarmActive = false;
        if (m_alarmBlinkTimer) {
            m_alarmBlinkTimer->stop();
        }
        if (m_alarmStatusText) {
            m_alarmStatusText->setText(tr("正常"));
            m_alarmStatusText->setColor(Qt::green);
        }
    }
}

void PlotterWindow::updateAlarmDisplay()
{
    // 移除旧的报警线
    if (m_upperLimitLine) {
        m_plot->removeItem(m_upperLimitLine);
        m_upperLimitLine = nullptr;
    }
    if (m_lowerLimitLine) {
        m_plot->removeItem(m_lowerLimitLine);
        m_lowerLimitLine = nullptr;
    }
    if (m_alarmStatusText) {
        m_plot->removeItem(m_alarmStatusText);
        m_alarmStatusText = nullptr;
    }

    if (!m_alarmConfig.enabled) {
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return;
    }

    // 创建上限线
    if (m_alarmConfig.upperLimitEnabled) {
        m_upperLimitLine = new QCPItemLine(m_plot);
        m_upperLimitLine->setPen(QPen(Qt::red, 2, Qt::DashLine));
        m_upperLimitLine->start->setTypeY(QCPItemPosition::ptPlotCoords);
        m_upperLimitLine->start->setTypeX(QCPItemPosition::ptAxisRectRatio);
        m_upperLimitLine->end->setTypeY(QCPItemPosition::ptPlotCoords);
        m_upperLimitLine->end->setTypeX(QCPItemPosition::ptAxisRectRatio);
        m_upperLimitLine->start->setCoords(0, m_alarmConfig.upperLimit);
        m_upperLimitLine->end->setCoords(1, m_alarmConfig.upperLimit);
    }

    // 创建下限线
    if (m_alarmConfig.lowerLimitEnabled) {
        m_lowerLimitLine = new QCPItemLine(m_plot);
        m_lowerLimitLine->setPen(QPen(Qt::blue, 2, Qt::DashLine));
        m_lowerLimitLine->start->setTypeY(QCPItemPosition::ptPlotCoords);
        m_lowerLimitLine->start->setTypeX(QCPItemPosition::ptAxisRectRatio);
        m_lowerLimitLine->end->setTypeY(QCPItemPosition::ptPlotCoords);
        m_lowerLimitLine->end->setTypeX(QCPItemPosition::ptAxisRectRatio);
        m_lowerLimitLine->start->setCoords(0, m_alarmConfig.lowerLimit);
        m_lowerLimitLine->end->setCoords(1, m_alarmConfig.lowerLimit);
    }

    // 创建状态文本
    m_alarmStatusText = new QCPItemText(m_plot);
    m_alarmStatusText->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_alarmStatusText->position->setCoords(0.98, 0.02);
    m_alarmStatusText->setPositionAlignment(Qt::AlignRight | Qt::AlignTop);
    m_alarmStatusText->setText(tr("监控中"));
    m_alarmStatusText->setColor(Qt::green);
    m_alarmStatusText->setFont(QFont("Sans", 10, QFont::Bold));

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::onAddMarkerClicked()
{
    if (m_plot->graphCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("没有可用的曲线"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("添加数据标记"));
    dialog.setMinimumWidth(350);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // 曲线选择
    QHBoxLayout* curveLayout = new QHBoxLayout;
    curveLayout->addWidget(new QLabel(tr("目标曲线:")));
    QComboBox* curveCombo = new QComboBox;
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        curveCombo->addItem(m_plot->graph(i)->name(), i);
    }
    curveLayout->addWidget(curveCombo);
    curveLayout->addStretch();
    mainLayout->addLayout(curveLayout);

    // 位置输入
    QGroupBox* posGroup = new QGroupBox(tr("标记位置"));
    QGridLayout* posLayout = new QGridLayout(posGroup);

    QDoubleSpinBox* xSpin = new QDoubleSpinBox;
    xSpin->setRange(-1e9, 1e9);
    xSpin->setDecimals(4);
    if (m_measureCursor1.active) {
        xSpin->setValue(m_measureCursor1.xPos);
    }

    QDoubleSpinBox* ySpin = new QDoubleSpinBox;
    ySpin->setRange(-1e9, 1e9);
    ySpin->setDecimals(4);

    posLayout->addWidget(new QLabel(tr("X位置:")), 0, 0);
    posLayout->addWidget(xSpin, 0, 1);
    posLayout->addWidget(new QLabel(tr("Y位置:")), 1, 0);
    posLayout->addWidget(ySpin, 1, 1);

    // 自动获取Y值按钮
    QPushButton* autoYBtn = new QPushButton(tr("从曲线获取Y值"));
    connect(autoYBtn, &QPushButton::clicked, [&]() {
        int idx = curveCombo->currentData().toInt();
        if (idx >= 0 && idx < m_plot->graphCount()) {
            QCPGraph* graph = m_plot->graph(idx);
            double x = xSpin->value();
            // 查找最近的数据点
            auto data = graph->data();
            auto it = data->findBegin(x, false);
            if (it != data->end()) {
                ySpin->setValue(it->value);
            }
        }
    });
    posLayout->addWidget(autoYBtn, 0, 2, 2, 1);
    mainLayout->addWidget(posGroup);

    // 标记文本
    QHBoxLayout* textLayout = new QHBoxLayout;
    textLayout->addWidget(new QLabel(tr("标记文本:")));
    QLineEdit* textEdit = new QLineEdit;
    textEdit->setPlaceholderText(tr("输入标记说明..."));
    textLayout->addWidget(textEdit);
    mainLayout->addLayout(textLayout);

    // 颜色选择
    QHBoxLayout* colorLayout = new QHBoxLayout;
    colorLayout->addWidget(new QLabel(tr("标记颜色:")));
    QPushButton* colorBtn = new QPushButton;
    QColor selectedColor = Qt::red;
    colorBtn->setStyleSheet(QString("background-color: %1").arg(selectedColor.name()));
    colorBtn->setFixedSize(60, 25);
    connect(colorBtn, &QPushButton::clicked, [&]() {
        QColor color = QColorDialog::getColor(selectedColor, this, tr("选择颜色"));
        if (color.isValid()) {
            selectedColor = color;
            colorBtn->setStyleSheet(QString("background-color: %1").arg(color.name()));
        }
    });
    colorLayout->addWidget(colorBtn);
    colorLayout->addStretch();
    mainLayout->addLayout(colorLayout);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        DataMarker marker;
        marker.curveIndex = curveCombo->currentData().toInt();
        marker.xPos = xSpin->value();
        marker.yPos = ySpin->value();
        marker.label = textEdit->text();
        marker.color = selectedColor;

        // 创建追踪点
        marker.tracer = new QCPItemTracer(m_plot);
        marker.tracer->setGraph(m_plot->graph(marker.curveIndex));
        marker.tracer->setGraphKey(marker.xPos);
        marker.tracer->setStyle(QCPItemTracer::tsCircle);
        marker.tracer->setPen(QPen(marker.color, 2));
        marker.tracer->setBrush(marker.color);
        marker.tracer->setSize(8);

        // 创建文本标签
        marker.text = new QCPItemText(m_plot);
        marker.text->position->setParentAnchor(marker.tracer->position);
        marker.text->position->setCoords(0, -15);
        marker.text->setText(marker.label.isEmpty() ?
            QString("(%1, %2)").arg(marker.xPos, 0, 'f', 2).arg(marker.yPos, 0, 'f', 2) :
            marker.label);
        marker.text->setColor(marker.color);
        marker.text->setFont(QFont("Sans", 9));
        marker.text->setPadding(QMargins(4, 2, 4, 2));
        marker.text->setBrush(QBrush(QColor(255, 255, 255, 200)));
        marker.text->setPen(QPen(marker.color));

        m_dataMarkers.append(marker);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void PlotterWindow::clearAllMarkers()
{
    for (const DataMarker& marker : m_dataMarkers) {
        if (marker.tracer) {
            m_plot->removeItem(marker.tracer);
        }
        if (marker.text) {
            m_plot->removeItem(marker.text);
        }
    }
    m_dataMarkers.clear();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::onApplyScenePresetClicked()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("嵌入式场景预设"));
    dialog.setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    QLabel* infoLabel = new QLabel(tr("选择一个预设场景，系统将自动配置曲线和显示设置："));
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // 场景列表
    QListWidget* sceneList = new QListWidget;
    sceneList->addItem(tr("PID 调参场景"));
    sceneList->addItem(tr("电机控制场景"));
    sceneList->addItem(tr("传感器监测场景"));
    sceneList->addItem(tr("ADC 采样场景"));
    sceneList->addItem(tr("电源监测场景"));
    sceneList->addItem(tr("温度控制场景"));
    sceneList->setCurrentRow(0);
    mainLayout->addWidget(sceneList);

    // 场景描述
    QGroupBox* descGroup = new QGroupBox(tr("场景描述"));
    QVBoxLayout* descLayout = new QVBoxLayout(descGroup);
    QLabel* descLabel = new QLabel;
    descLabel->setWordWrap(true);
    descLayout->addWidget(descLabel);
    mainLayout->addWidget(descGroup);

    // 更新描述的函数
    auto updateDesc = [descLabel](int index) {
        switch (index) {
        case 0:  // PID
            descLabel->setText(tr("PID 调参场景：\n"
                "- 曲线1: 设定值 (Setpoint) - 蓝色\n"
                "- 曲线2: 实际值 (Actual) - 绿色\n"
                "- 曲线3: 误差值 (Error) - 红色\n"
                "- 曲线4: 控制输出 (Output) - 橙色\n"
                "预设参考线显示设定值"));
            break;
        case 1:  // 电机
            descLabel->setText(tr("电机控制场景：\n"
                "- 曲线1: 速度 (Speed) - 蓝色\n"
                "- 曲线2: 位置 (Position) - 绿色，右Y轴\n"
                "- 曲线3: 电流 (Current) - 红色\n"
                "- 曲线4: 目标速度 (Target) - 虚线"));
            break;
        case 2:  // 传感器
            descLabel->setText(tr("传感器监测场景：\n"
                "- 曲线1-4: 传感器通道 1-4\n"
                "- 使用不同颜色区分\n"
                "- 支持多通道同时监测"));
            break;
        case 3:  // ADC
            descLabel->setText(tr("ADC 采样场景：\n"
                "- 曲线1: ADC 原始值 - 蓝色\n"
                "- 曲线2: 滤波后值 - 绿色\n"
                "- 曲线3: 参考电压 - 红色虚线\n"
                "Y轴范围: 0-4095 (12bit)"));
            break;
        case 4:  // 电源
            descLabel->setText(tr("电源监测场景：\n"
                "- 曲线1: 电压 (Voltage) - 蓝色\n"
                "- 曲线2: 电流 (Current) - 红色，右Y轴\n"
                "- 曲线3: 功率 (Power) - 绿色\n"
                "含上下限报警预设"));
            break;
        case 5:  // 温度
            descLabel->setText(tr("温度控制场景：\n"
                "- 曲线1: 目标温度 (Target) - 蓝色虚线\n"
                "- 曲线2: 实际温度 (Actual) - 红色\n"
                "- 曲线3: 加热功率 (Heater) - 橙色\n"
                "含温度范围报警"));
            break;
        }
    };
    connect(sceneList, &QListWidget::currentRowChanged, updateDesc);
    updateDesc(0);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        EmbeddedSceneType sceneType = static_cast<EmbeddedSceneType>(sceneList->currentRow());
        applyScenePreset(sceneType);
    }
}

void PlotterWindow::applyScenePreset(EmbeddedSceneType sceneType)
{
    // 清除现有曲线和设置
    clearAll();
    removeReferenceLine();

    // 场景切换前彻底重置曲线集合，避免重复叠加导致性能下降
    while (m_plot->graphCount() > 0) {
        m_plot->removeGraph(0);
    }
    m_diffCurves.clear();
    m_staticCurves.clear();
    m_realTimeFilters.clear();
    m_rightAxisCurves.clear();
    setRightYAxisVisible(false);
    if (m_controlPanel) {
        m_controlPanel->updateCurveList();
    }

    // 重置报警配置
    m_alarmConfig = AlarmConfig();
    updateAlarmDisplay();

    switch (sceneType) {
    case EmbeddedSceneType::PIDControl:
        // PID 调参场景
        setWindowTitle(tr("绘图窗口 - PID 调参"));
        addCurve(tr("设定值"), QColor(31, 119, 180));    // 蓝色
        addCurve(tr("实际值"), QColor(44, 160, 44));     // 绿色
        addCurve(tr("误差"), QColor(214, 39, 40));       // 红色
        addCurve(tr("输出"), QColor(255, 127, 14));      // 橙色
        m_plot->yAxis->setLabel(tr("数值"));
        m_plot->xAxis->setLabel(tr("采样点"));
        m_visiblePoints = 500;
        break;

    case EmbeddedSceneType::MotorControl:
        // 电机控制场景
        setWindowTitle(tr("绘图窗口 - 电机控制"));
        addCurve(tr("速度"), QColor(31, 119, 180));      // 蓝色
        addCurve(tr("位置"), QColor(44, 160, 44));       // 绿色
        addCurve(tr("电流"), QColor(214, 39, 40));       // 红色
        addCurve(tr("目标速度"), QColor(148, 103, 189)); // 紫色
        // 位置使用右轴
        setRightYAxisVisible(true);
        setRightYAxisLabel(tr("位置"));
        setCurveYAxis(1, true);
        // 目标速度使用虚线
        setCurveLineStyle(3, Qt::DashLine);
        m_plot->yAxis->setLabel(tr("速度/电流"));
        m_visiblePoints = 500;
        break;

    case EmbeddedSceneType::SensorMonitor:
        // 传感器监测场景
        setWindowTitle(tr("绘图窗口 - 传感器监测"));
        addCurve(tr("通道1"), QColor(31, 119, 180));
        addCurve(tr("通道2"), QColor(255, 127, 14));
        addCurve(tr("通道3"), QColor(44, 160, 44));
        addCurve(tr("通道4"), QColor(214, 39, 40));
        m_plot->yAxis->setLabel(tr("传感器值"));
        m_visiblePoints = 300;
        break;

    case EmbeddedSceneType::ADCSampling:
        // ADC 采样场景
        setWindowTitle(tr("绘图窗口 - ADC 采样"));
        addCurve(tr("ADC原始值"), QColor(31, 119, 180));
        addCurve(tr("滤波值"), QColor(44, 160, 44));
        addCurve(tr("参考电压"), QColor(214, 39, 40));
        setCurveLineStyle(2, Qt::DashLine);
        // 12位ADC范围
        setYAxisRange(0, 4095);
        setAutoScale(false);
        m_plot->yAxis->setLabel(tr("ADC值 (12bit)"));
        m_visiblePoints = 500;
        // 设置参考线在中间值
        setReferenceLine(2048, tr("VREF/2"));
        break;

    case EmbeddedSceneType::PowerMonitor:
        // 电源监测场景
        setWindowTitle(tr("绘图窗口 - 电源监测"));
        addCurve(tr("电压"), QColor(31, 119, 180));      // 蓝色
        addCurve(tr("电流"), QColor(214, 39, 40));       // 红色
        addCurve(tr("功率"), QColor(44, 160, 44));       // 绿色
        // 电流使用右轴
        setRightYAxisVisible(true);
        setRightYAxisLabel(tr("电流 (A)"));
        setCurveYAxis(1, true);
        m_plot->yAxis->setLabel(tr("电压 (V) / 功率 (W)"));
        m_visiblePoints = 300;
        // 设置报警
        m_alarmConfig.enabled = true;
        m_alarmConfig.sourceCurve = 0;  // 监控电压
        m_alarmConfig.upperLimitEnabled = true;
        m_alarmConfig.upperLimit = 14.0;  // 电压上限
        m_alarmConfig.lowerLimitEnabled = true;
        m_alarmConfig.lowerLimit = 10.0;  // 电压下限
        m_alarmConfig.visualAlert = true;
        updateAlarmDisplay();
        break;

    case EmbeddedSceneType::TemperatureControl:
        // 温度控制场景
        setWindowTitle(tr("绘图窗口 - 温度控制"));
        addCurve(tr("目标温度"), QColor(31, 119, 180));
        addCurve(tr("实际温度"), QColor(214, 39, 40));
        addCurve(tr("加热功率"), QColor(255, 127, 14));
        setCurveLineStyle(0, Qt::DashLine);
        // 功率使用右轴
        setRightYAxisVisible(true);
        setRightYAxisLabel(tr("功率 (%)"));
        setCurveYAxis(2, true);
        m_plot->yAxis->setLabel(tr("温度 (°C)"));
        m_visiblePoints = 600;
        // 温度报警
        m_alarmConfig.enabled = true;
        m_alarmConfig.sourceCurve = 1;  // 监控实际温度
        m_alarmConfig.upperLimitEnabled = true;
        m_alarmConfig.upperLimit = 80.0;  // 温度上限
        m_alarmConfig.visualAlert = true;
        m_alarmConfig.soundAlert = true;
        updateAlarmDisplay();
        break;
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
    QMessageBox::information(this, tr("场景预设"), tr("场景预设已应用，请按照预设配置发送数据。"));
}

void PlotterWindow::updateRealTimeFFT(int curveIndex, double value)
{
    // 清理已关闭的FFT窗口
    cleanupClosedFFTWindows();

    // 向匹配的实时FFT窗口发送数据
    for (const auto& conn : m_realTimeFFTWindows) {
        if (conn.curveIndex == curveIndex && conn.window) {
            conn.window->appendData(value);
        }
    }
}

void PlotterWindow::cleanupClosedFFTWindows()
{
    // 从后往前遍历，移除无效的窗口引用
    for (int i = m_realTimeFFTWindows.size() - 1; i >= 0; --i) {
        if (!m_realTimeFFTWindows[i].window) {
            m_realTimeFFTWindows.removeAt(i);
        }
    }
}

void PlotterWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void PlotterWindow::retranslateUi()
{
    // 更新窗口标题
    setWindowTitle(tr("绘图窗口 - %1").arg(m_windowId));

    // 更新工具栏 action 文本
    if (m_pauseAction) {
        m_pauseAction->setIcon(style()->standardIcon(m_paused ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
        m_pauseAction->setText(m_paused ? tr("继续") : tr("暂停"));
    }
    if (m_gridAction) {
        m_gridAction->setText(tr("网格"));
    }
    if (m_legendAction) {
        m_legendAction->setText(tr("图例"));
    }
    if (m_scrollModeAction) {
        m_scrollModeAction->setText(tr("滚动模式"));
        m_scrollModeAction->setToolTip(tr("固定宽度，新数据从右侧进入"));
    }
    if (m_renderQualityLabel) {
        m_renderQualityLabel->setText(tr("  渲染:"));
    }
    if (m_visiblePointsLabel) {
        m_visiblePointsLabel->setText(tr("  显示点数:"));
    }
    if (m_visiblePointsSpin) {
        m_visiblePointsSpin->setToolTip(tr("设置可见的数据点数量"));
    }
    if (m_histogramAction) {
        m_histogramAction->setText(tr("直方图视图(&H)"));
    }
    if (m_xyViewAction) {
        m_xyViewAction->setText(tr("XY视图(&Y)"));
    }
    if (m_renderQualityHighAction) {
        m_renderQualityHighAction->setText(tr("高质量（细腻）"));
    }
    if (m_renderQualityPerformanceAction) {
        m_renderQualityPerformanceAction->setText(tr("高性能（流畅）"));
    }
    if (m_diffRealtimeAction) {
        m_diffRealtimeAction->setText(tr("差值曲线实时更新"));
    }
    if (m_filterRealtimeAction) {
        m_filterRealtimeAction->setText(tr("滤波曲线实时更新"));
    }
    if (m_perfDiagAction) {
        m_perfDiagAction->setText(tr("显示性能诊断"));
    }
    if (m_perfDiagLabel) {
        m_perfDiagLabel->setVisible(m_showPerfDiag);
        if (m_perfDiagLabel->text().isEmpty()) {
            m_perfDiagLabel->setText(tr("性能: 等待数据..."));
        }
    }
    if (m_renderQualityCombo) {
        QSignalBlocker blocker(m_renderQualityCombo);
        const int currentIndex = m_renderQualityCombo->currentIndex();
        m_renderQualityCombo->setItemText(0, tr("高质量"));
        m_renderQualityCombo->setItemText(1, tr("高性能"));
        m_renderQualityCombo->setToolTip(tr("切换波形渲染质量档位"));
        m_renderQualityCombo->setCurrentIndex(currentIndex);
    }
}

void PlotterWindow::onHistogramViewClicked()
{
    if (m_plot->graphCount() == 0) {
        QMessageBox::information(this, tr("提示"), tr("当前没有曲线数据"));
        if (m_histogramAction) {
            m_histogramAction->setChecked(false);
        }
        return;
    }

    bool enableHistogram = m_histogramAction ? m_histogramAction->isChecked() : false;

    if (enableHistogram) {
        // 从 XY 模式切换过来时，先清理 XY 相关对象
        if (m_viewMode == PlotViewMode::XY) {
            if (m_xyCurve) {
                m_plot->removePlottable(m_xyCurve);
                m_xyCurve = nullptr;
            }
            if (m_xyControlWidget) {
                m_xyControlWidget->hide();
            }
            if (m_xyViewAction) {
                m_xyViewAction->setChecked(false);
            }
        }

        // 切换到直方图视图
        // 如果有多条曲线，弹出选择对话框
        if (m_plot->graphCount() > 1) {
            QDialog selectDialog(this);
            selectDialog.setWindowTitle(tr("选择直方图数据源"));
            QVBoxLayout* layout = new QVBoxLayout(&selectDialog);

            QComboBox* curveCombo = new QComboBox;
            for (int i = 0; i < m_plot->graphCount(); ++i) {
                // 跳过差值曲线和滤波曲线
                bool isSpecial = false;
                for (const auto& diff : m_diffCurves) {
                    if (diff.graph == m_plot->graph(i)) {
                        isSpecial = true;
                        break;
                    }
                }
                if (m_staticCurves.contains(m_plot->graph(i))) {
                    isSpecial = true;
                }
                if (!isSpecial) {
                    curveCombo->addItem(m_plot->graph(i)->name(), i);
                }
            }

            if (curveCombo->count() == 0) {
                QMessageBox::information(this, tr("提示"), tr("没有可用于直方图分析的原始曲线"));
                if (m_histogramAction) {
                    m_histogramAction->setChecked(false);
                }
                return;
            }

            layout->addWidget(new QLabel(tr("选择曲线:")));
            layout->addWidget(curveCombo);

            // 分箱数量设置
            QSpinBox* binSpin = new QSpinBox;
            binSpin->setRange(10, 200);
            binSpin->setValue(m_histogramBinCount);
            binSpin->setSuffix(tr(" 个分箱"));
            layout->addWidget(new QLabel(tr("分箱数量:")));
            layout->addWidget(binSpin);

            QDialogButtonBox* buttonBox = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            layout->addWidget(buttonBox);
            connect(buttonBox, &QDialogButtonBox::accepted, &selectDialog, &QDialog::accept);
            connect(buttonBox, &QDialogButtonBox::rejected, &selectDialog, &QDialog::reject);

            if (selectDialog.exec() != QDialog::Accepted) {
                if (m_histogramAction) {
                    m_histogramAction->setChecked(false);
                }
                return;
            }

            m_histogramCurveIndex = curveCombo->currentData().toInt();
            m_histogramBinCount = binSpin->value();
        } else {
            m_histogramCurveIndex = 0;
        }

        m_viewMode = PlotViewMode::Histogram;
        m_forceHistogramRefresh = true;

        // 隐藏所有时间序列曲线
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            m_plot->graph(i)->setVisible(false);
        }

        // 创建直方图
        updateHistogramView();
    } else {
        // 切换回时间序列视图
        m_viewMode = PlotViewMode::TimeSeries;
        m_forceHistogramRefresh = false;

        // 移除直方图
        if (m_histogramBars) {
            m_plot->removePlottable(m_histogramBars);
            m_histogramBars = nullptr;
        }

        // 显示所有时间序列曲线
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            m_plot->graph(i)->setVisible(true);
        }

        // 恢复坐标轴标签
        m_plot->xAxis->setLabel(tr("X"));
        m_plot->yAxis->setLabel(tr("Y"));

        m_plot->rescaleAxes();
        setWindowTitle(tr("绘图窗口 - %1").arg(m_windowId));
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotterWindow::updateHistogramView()
{
    if (m_viewMode != PlotViewMode::Histogram) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_forceHistogramRefresh && (now - m_lastHistogramUpdateMs) < 120) {
        return;
    }
    m_forceHistogramRefresh = false;
    m_lastHistogramUpdateMs = now;

    if (m_histogramCurveIndex < 0 || m_histogramCurveIndex >= m_plot->graphCount()) {
        return;
    }

    QCPGraph* sourceGraph = m_plot->graph(m_histogramCurveIndex);
    if (!sourceGraph || sourceGraph->data()->isEmpty()) {
        return;
    }

    // 获取数据
    QVector<double> values;
    const int sourceSize = sourceGraph->data()->size();
    if (sourceSize <= 0 || m_histogramBinCount <= 0) {
        return;
    }

    // 大数据量时按步长采样，避免每次全量扫描导致卡顿
    const int maxSampleCount = qMax(3000, m_histogramBinCount * 80);
    const int sampleStep = qMax(1, sourceSize / maxSampleCount);
    values.reserve(sourceSize / sampleStep + 1);

    int sampleIndex = 0;
    for (auto it = sourceGraph->data()->constBegin(); it != sourceGraph->data()->constEnd(); ++it, ++sampleIndex) {
        if (sampleIndex % sampleStep != 0) {
            continue;
        }
        values.append(it->value);
    }

    if (values.isEmpty()) {
        return;
    }

    // 计算数据范围
    double minVal = *std::min_element(values.begin(), values.end());
    double maxVal = *std::max_element(values.begin(), values.end());

    // 避免除零
    if (qFuzzyCompare(minVal, maxVal)) {
        maxVal = minVal + 1.0;
    }

    double binWidth = (maxVal - minVal) / m_histogramBinCount;

    // 计算直方图数据
    QVector<double> binCenters(m_histogramBinCount);
    QVector<double> binCounts(m_histogramBinCount, 0);

    for (int i = 0; i < m_histogramBinCount; ++i) {
        binCenters[i] = minVal + (i + 0.5) * binWidth;
    }

    for (double val : values) {
        int binIndex = static_cast<int>((val - minVal) / binWidth);
        if (binIndex < 0) binIndex = 0;
        if (binIndex >= m_histogramBinCount) binIndex = m_histogramBinCount - 1;
        binCounts[binIndex]++;
    }

    // 创建或更新直方图
    if (!m_histogramBars) {
        m_histogramBars = new QCPBars(m_plot->xAxis, m_plot->yAxis);
        m_histogramBars->setName(tr("直方图 - %1").arg(sourceGraph->name()));
        m_histogramBars->setWidth(binWidth * 0.9);
        m_histogramBars->setBrush(QBrush(QColor(31, 119, 180, 180)));
        m_histogramBars->setPen(QPen(QColor(31, 119, 180)));
    }

    m_histogramBars->setData(binCenters, binCounts);

    // 更新坐标轴
    m_plot->xAxis->setLabel(tr("数值"));
    m_plot->yAxis->setLabel(tr("频次"));

    m_plot->xAxis->setRange(minVal - binWidth, maxVal + binWidth);
    double maxCount = *std::max_element(binCounts.begin(), binCounts.end());
    m_plot->yAxis->setRange(0, maxCount * 1.1);

    // 显示统计信息
    double sum = 0;
    for (double val : values) {
        sum += val;
    }
    double mean = sum / values.size();

    double variance = 0;
    for (double val : values) {
        variance += (val - mean) * (val - mean);
    }
    variance /= values.size();
    double stdDev = std::sqrt(variance);

    // 更新窗口标题显示统计信息
    setWindowTitle(tr("直方图 - %1 | 均值: %2 | 标准差: %3 | 采样点: %4 / 原始点: %5")
        .arg(sourceGraph->name())
        .arg(mean, 0, 'f', 3)
        .arg(stdDev, 0, 'f', 3)
        .arg(values.size())
        .arg(sourceSize));
}

void PlotterWindow::onXYViewClicked()
{
    if (m_viewMode == PlotViewMode::XY) {
        // 退出XY模式，恢复时间序列模式
        m_viewMode = PlotViewMode::TimeSeries;
        m_forceXYRefresh = false;
        m_xyViewAction->setChecked(false);

        // 移除XY曲线
        if (m_xyCurve) {
            m_plot->removePlottable(m_xyCurve);
            m_xyCurve = nullptr;
        }

        // 恢复显示所有图形
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            m_plot->graph(i)->setVisible(true);
        }

        // 隐藏XY控制面板
        if (m_xyControlWidget) {
            m_xyControlWidget->hide();
        }

        // 恢复坐标轴标签
        m_plot->xAxis->setLabel(tr("X"));
        m_plot->yAxis->setLabel(tr("Y"));

        setWindowTitle(tr("绘图窗口 - %1").arg(m_windowId));
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    } else {
        // 进入XY模式
        m_viewMode = PlotViewMode::XY;
        m_xyViewAction->setChecked(true);
        m_histogramAction->setChecked(false);
        m_forceHistogramRefresh = false;

        // 隐藏直方图
        if (m_histogramBars) {
            m_plot->removePlottable(m_histogramBars);
            m_histogramBars = nullptr;
        }

        // 隐藏所有图形
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            m_plot->graph(i)->setVisible(false);
        }

        // 创建XY控制面板（如果不存在）
        if (!m_xyControlWidget) {
            m_xyControlWidget = new QWidget(this);
            QHBoxLayout* layout = new QHBoxLayout(m_xyControlWidget);
            layout->setContentsMargins(4, 4, 4, 4);
            layout->setSpacing(8);

            layout->addWidget(new QLabel(tr("X通道:")));
            m_xyChannelXCombo = new QComboBox;
            m_xyChannelXCombo->setMinimumWidth(80);
            layout->addWidget(m_xyChannelXCombo);

            layout->addWidget(new QLabel(tr("Y通道:")));
            m_xyChannelYCombo = new QComboBox;
            m_xyChannelYCombo->setMinimumWidth(80);
            layout->addWidget(m_xyChannelYCombo);

            layout->addStretch();

            // 连接信号
            connect(m_xyChannelXCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &PlotterWindow::onXYChannelChanged);
            connect(m_xyChannelYCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &PlotterWindow::onXYChannelChanged);
        }

        // 更新通道选择下拉框
        m_xyChannelXCombo->blockSignals(true);
        m_xyChannelYCombo->blockSignals(true);
        m_xyChannelXCombo->clear();
        m_xyChannelYCombo->clear();

        QVector<int> availableCurveIndices;
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            QCPGraph* graph = m_plot->graph(i);
            if (!graph) {
                continue;
            }

            bool isSpecialCurve = false;
            for (const auto& diff : m_diffCurves) {
                if (diff.graph == graph) {
                    isSpecialCurve = true;
                    break;
                }
            }
            if (!isSpecialCurve && m_staticCurves.contains(graph)) {
                isSpecialCurve = true;
            }
            if (isSpecialCurve) {
                continue;
            }

            availableCurveIndices.append(i);
            QString name = m_plot->graph(i)->name();
            if (name.isEmpty()) {
                name = tr("通道 %1").arg(i + 1);
            }
            m_xyChannelXCombo->addItem(name, i);
            m_xyChannelYCombo->addItem(name, i);
        }

        if (availableCurveIndices.size() < 2) {
            m_xyChannelXCombo->blockSignals(false);
            m_xyChannelYCombo->blockSignals(false);
            QMessageBox::information(this, tr("提示"), tr("XY视图至少需要2条原始曲线"));
            m_viewMode = PlotViewMode::TimeSeries;
            m_xyViewAction->setChecked(false);
            for (int i = 0; i < m_plot->graphCount(); ++i) {
                m_plot->graph(i)->setVisible(true);
            }
            if (m_xyControlWidget) {
                m_xyControlWidget->hide();
            }
            setWindowTitle(tr("绘图窗口 - %1").arg(m_windowId));
            m_plot->replot(QCustomPlot::rpQueuedReplot);
            return;
        }

        // 默认选择前两个可用通道
        const int xIndex = m_xyChannelXCombo->findData(m_xyChannelX);
        const int yIndex = m_xyChannelYCombo->findData(m_xyChannelY);
        m_xyChannelXCombo->setCurrentIndex(xIndex >= 0 ? xIndex : 0);
        m_xyChannelYCombo->setCurrentIndex(yIndex >= 0 ? yIndex : 1);

        m_xyChannelXCombo->blockSignals(false);
        m_xyChannelYCombo->blockSignals(false);

        // 显示控制面板
        m_xyControlWidget->setParent(m_plot);
        m_xyControlWidget->move(10, 10);
        m_xyControlWidget->show();
        m_xyControlWidget->raise();

        m_forceXYRefresh = true;
        updateXYView();
    }
}

void PlotterWindow::onXYChannelChanged()
{
    if (m_xyChannelXCombo && m_xyChannelYCombo) {
        m_xyChannelX = m_xyChannelXCombo->currentData().toInt();
        m_xyChannelY = m_xyChannelYCombo->currentData().toInt();
        m_forceXYRefresh = true;
        updateXYView();
    }
}

void PlotterWindow::updateXYView()
{
    if (m_viewMode != PlotViewMode::XY) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_forceXYRefresh && (now - m_lastXYUpdateMs) < 80) {
        return;
    }
    m_forceXYRefresh = false;
    m_lastXYUpdateMs = now;

    // 检查通道有效性
    if (m_xyChannelX < 0 || m_xyChannelX >= m_plot->graphCount() ||
        m_xyChannelY < 0 || m_xyChannelY >= m_plot->graphCount()) {
        return;
    }

    QCPGraph* graphX = m_plot->graph(m_xyChannelX);
    QCPGraph* graphY = m_plot->graph(m_xyChannelY);

    if (!graphX || !graphY || graphX->data()->isEmpty() || graphY->data()->isEmpty()) {
        return;
    }

    // 创建或获取XY曲线
    if (!m_xyCurve) {
        m_xyCurve = new QCPCurve(m_plot->xAxis, m_plot->yAxis);
        m_xyCurve->setName(tr("XY: %1 vs %2").arg(graphX->name()).arg(graphY->name()));
        m_xyCurve->setPen(createCurvePen(QColor(31, 119, 180), 1.7));
        m_xyCurve->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 4));
        m_xyCurve->setAntialiased(m_renderQualityMode == RenderQualityMode::HighQuality);
    }

    // 获取数据（取两个通道数据点数的最小值），并在大数据量下抽样以提升流畅度
    const int rawCount = qMin(graphX->data()->size(), graphY->data()->size());
    if (rawCount <= 0) {
        return;
    }

    const int maxPlotPoints = qBound(1000, m_maxDataPoints, 6000);
    const int sampleStep = qMax(1, rawCount / maxPlotPoints);
    const int dataCount = (rawCount + sampleStep - 1) / sampleStep;

    QVector<double> xData(dataCount);
    QVector<double> yData(dataCount);
    QVector<double> tData(dataCount);  // 参数t

    auto itX = graphX->data()->constBegin();
    auto itY = graphY->data()->constBegin();

    int writeIndex = 0;
    int rawIndex = 0;
    for (; rawIndex < rawCount && itX != graphX->data()->constEnd() && itY != graphY->data()->constEnd();
           ++rawIndex, ++itX, ++itY) {
        if (rawIndex % sampleStep != 0) {
            continue;
        }
        if (writeIndex >= dataCount) {
            break;
        }
        tData[writeIndex] = writeIndex;
        xData[writeIndex] = itX->value;
        yData[writeIndex] = itY->value;
        ++writeIndex;
    }

    if (writeIndex < dataCount) {
        xData.resize(writeIndex);
        yData.resize(writeIndex);
        tData.resize(writeIndex);
    }

    m_xyCurve->setData(tData, xData, yData);

    // 更新坐标轴标签
    m_plot->xAxis->setLabel(graphX->name().isEmpty() ? tr("X通道") : graphX->name());
    m_plot->yAxis->setLabel(graphY->name().isEmpty() ? tr("Y通道") : graphY->name());

    // 自动缩放
    if (!xData.isEmpty() && !yData.isEmpty()) {
        double xMin = *std::min_element(xData.begin(), xData.end());
        double xMax = *std::max_element(xData.begin(), xData.end());
        double yMin = *std::min_element(yData.begin(), yData.end());
        double yMax = *std::max_element(yData.begin(), yData.end());

        double xMargin = (xMax - xMin) * 0.1;
        double yMargin = (yMax - yMin) * 0.1;

        if (qFuzzyCompare(xMin, xMax)) xMargin = 1.0;
        if (qFuzzyCompare(yMin, yMax)) yMargin = 1.0;

        m_plot->xAxis->setRange(xMin - xMargin, xMax + xMargin);
        m_plot->yAxis->setRange(yMin - yMargin, yMax + yMargin);
    }

    // 更新窗口标题
    setWindowTitle(tr("XY视图 - %1 vs %2 | 显示点: %3 / 原始点: %4")
        .arg(graphX->name())
        .arg(graphY->name())
        .arg(xData.size())
        .arg(rawCount));

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

} // namespace ComAssistant
