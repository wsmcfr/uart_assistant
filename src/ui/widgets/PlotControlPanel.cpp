/**
 * @file PlotControlPanel.cpp
 * @brief 绘图控制面板实现
 * @author ComAssistant Team
 * @date 2026-01-28
 */

#include "PlotControlPanel.h"
#include "../PlotterWindow.h"
#include <QSignalBlocker>

namespace ComAssistant {

PlotControlPanel::PlotControlPanel(PlotterWindow* plotterWindow, QWidget* parent)
    : QDockWidget(tr("控制面板"), parent)
    , m_plotterWindow(plotterWindow)
{
    setObjectName("plotControlPanel");
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);

    setupUI();
}

void PlotControlPanel::setupUI()
{
    // 主容器 - 不使用滚动区域，更紧凑
    QWidget* mainWidget = new QWidget;
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    // 添加各功能区
    mainLayout->addWidget(createCurveManagement());
    mainLayout->addWidget(createAxisSettings());
    mainLayout->addWidget(createDisplaySettings());
    mainLayout->addWidget(createDataSettings());
    mainLayout->addStretch();

    setWidget(mainWidget);
    setFixedWidth(200);
    setWindowTitle(tr("控制面板"));
}

QGroupBox* PlotControlPanel::createCurveManagement()
{
    QGroupBox* group = new QGroupBox(tr("曲线管理"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 12, 6, 6);
    layout->setSpacing(4);

    m_curveList = new QListWidget;
    m_curveList->setFixedHeight(80);  // 更紧凑的高度
    m_curveList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_curveList);

    // 颜色按钮
    QPushButton* colorBtn = new QPushButton(tr("修改颜色"));
    colorBtn->setFixedHeight(26);
    connect(colorBtn, &QPushButton::clicked, this, [this]() {
        int row = m_curveList->currentRow();
        if (row >= 0) {
            QColor color = QColorDialog::getColor(Qt::white, this, tr("选择曲线颜色"));
            if (color.isValid()) {
                emit curveColorChanged(row, color);
            }
        }
    });
    layout->addWidget(colorBtn);

    QPushButton* diffBtn = new QPushButton(tr("差值曲线..."));
    diffBtn->setFixedHeight(26);
    connect(diffBtn, &QPushButton::clicked, this, [this]() {
        emit differenceCurveRequested();
    });
    layout->addWidget(diffBtn);

    return group;
}

QGroupBox* PlotControlPanel::createAxisSettings()
{
    QGroupBox* group = new QGroupBox(tr("坐标轴"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 12, 6, 6);
    layout->setSpacing(4);

    // 自动缩放
    m_autoScaleCheck = new QCheckBox(tr("自动缩放"));
    m_autoScaleCheck->setChecked(true);
    connect(m_autoScaleCheck, &QCheckBox::toggled, this, &PlotControlPanel::autoScaleChanged);
    layout->addWidget(m_autoScaleCheck);

    // 使用Grid布局让X/Y对齐
    QGridLayout* rangeGrid = new QGridLayout;
    rangeGrid->setSpacing(4);
    rangeGrid->setContentsMargins(0, 4, 0, 0);

    // X轴范围
    QLabel* xLabel = new QLabel(tr("X:"));
    xLabel->setFixedWidth(20);
    rangeGrid->addWidget(xLabel, 0, 0);

    m_xMinSpin = new QDoubleSpinBox;
    m_xMinSpin->setRange(-1e9, 1e9);
    m_xMinSpin->setDecimals(0);
    m_xMinSpin->setFixedWidth(55);
    m_xMinSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    rangeGrid->addWidget(m_xMinSpin, 0, 1);

    QLabel* xDash = new QLabel("-");
    xDash->setAlignment(Qt::AlignCenter);
    xDash->setFixedWidth(12);
    rangeGrid->addWidget(xDash, 0, 2);

    m_xMaxSpin = new QDoubleSpinBox;
    m_xMaxSpin->setRange(-1e9, 1e9);
    m_xMaxSpin->setDecimals(0);
    m_xMaxSpin->setValue(1000);
    m_xMaxSpin->setFixedWidth(55);
    m_xMaxSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    rangeGrid->addWidget(m_xMaxSpin, 0, 3);

    // Y轴范围
    QLabel* yLabel = new QLabel(tr("Y:"));
    yLabel->setFixedWidth(20);
    rangeGrid->addWidget(yLabel, 1, 0);

    m_yMinSpin = new QDoubleSpinBox;
    m_yMinSpin->setRange(-1e9, 1e9);
    m_yMinSpin->setDecimals(1);
    m_yMinSpin->setValue(-10);
    m_yMinSpin->setFixedWidth(55);
    m_yMinSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    rangeGrid->addWidget(m_yMinSpin, 1, 1);

    QLabel* yDash = new QLabel("-");
    yDash->setAlignment(Qt::AlignCenter);
    yDash->setFixedWidth(12);
    rangeGrid->addWidget(yDash, 1, 2);

    m_yMaxSpin = new QDoubleSpinBox;
    m_yMaxSpin->setRange(-1e9, 1e9);
    m_yMaxSpin->setDecimals(1);
    m_yMaxSpin->setValue(10);
    m_yMaxSpin->setFixedWidth(55);
    m_yMaxSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    rangeGrid->addWidget(m_yMaxSpin, 1, 3);

    layout->addLayout(rangeGrid);

    // 应用按钮
    QPushButton* applyBtn = new QPushButton(tr("应用范围"));
    applyBtn->setFixedHeight(26);
    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        emit xRangeChanged(m_xMinSpin->value(), m_xMaxSpin->value());
        emit yRangeChanged(m_yMinSpin->value(), m_yMaxSpin->value());
    });
    layout->addWidget(applyBtn);

    return group;
}

QGroupBox* PlotControlPanel::createDisplaySettings()
{
    QGroupBox* group = new QGroupBox(tr("显示"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 12, 6, 6);
    layout->setSpacing(2);

    /*
     * OpenGL 开关放到右侧控制面板里显式展示。
     * 用户已经明确要求它必须看得见、可手动切换，所以这里不再用“抗锯齿”
     * 这种容易歧义的文案占位，而是直接绑定真实的 OpenGL 状态。
     */
    m_openGLCheck = new QCheckBox(tr("启用 OpenGL"));
    m_openGLCheck->setChecked(m_plotterWindow ? m_plotterWindow->isOpenGLEnabled() : false);
    connect(m_openGLCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_plotterWindow) {
            m_plotterWindow->setOpenGLEnabled(checked);
        }
    });
    layout->addWidget(m_openGLCheck);

    m_gridCheck = new QCheckBox(tr("显示网格"));
    m_gridCheck->setChecked(true);
    connect(m_gridCheck, &QCheckBox::toggled, this, &PlotControlPanel::gridVisibleChanged);
    layout->addWidget(m_gridCheck);

    m_legendCheck = new QCheckBox(tr("显示图例"));
    m_legendCheck->setChecked(true);
    connect(m_legendCheck, &QCheckBox::toggled, this, &PlotControlPanel::legendVisibleChanged);
    layout->addWidget(m_legendCheck);

    return group;
}

QGroupBox* PlotControlPanel::createDataSettings()
{
    QGroupBox* group = new QGroupBox(tr("数据"));
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setContentsMargins(6, 12, 6, 6);
    layout->setSpacing(4);

    QHBoxLayout* pointsLayout = new QHBoxLayout;
    pointsLayout->setSpacing(4);
    QLabel* pointsLabel = new QLabel(tr("最大点数:"));
    pointsLabel->setFixedWidth(60);
    pointsLayout->addWidget(pointsLabel);

    m_maxPointsSpin = new QSpinBox;
    m_maxPointsSpin->setRange(100, 100000);
    m_maxPointsSpin->setSingleStep(1000);
    m_maxPointsSpin->setValue(5000);
    m_maxPointsSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    connect(m_maxPointsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PlotControlPanel::maxDataPointsChanged);
    pointsLayout->addWidget(m_maxPointsSpin);
    layout->addLayout(pointsLayout);

    // 清空数据按钮
    QPushButton* clearBtn = new QPushButton(tr("清空数据"));
    clearBtn->setFixedHeight(26);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_plotterWindow) {
            m_plotterWindow->clearAll();
        }
    });
    layout->addWidget(clearBtn);

    return group;
}

void PlotControlPanel::updateCurveList()
{
    if (!m_curveList) {
        return;
    }

    QSignalBlocker blocker(*m_curveList);
    m_curveList->clear();
    if (!m_plotterWindow) {
        return;
    }

    int count = m_plotterWindow->curveCount();
    for (int i = 0; i < count; ++i) {
        QString curveName = m_plotterWindow->curveName(i);
        if (curveName.isEmpty()) {
            curveName = tr("曲线 %1").arg(i + 1);
        }

        QListWidgetItem* item = new QListWidgetItem(curveName);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_plotterWindow->isCurveVisible(i) ? Qt::Checked : Qt::Unchecked);
        m_curveList->addItem(item);
    }

    // 连接复选框信号
    disconnect(m_curveList, &QListWidget::itemChanged, nullptr, nullptr);
    connect(m_curveList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        int row = m_curveList->row(item);
        emit curveVisibilityChanged(row, item->checkState() == Qt::Checked);
    });
}

void PlotControlPanel::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDockWidget::changeEvent(event);
}

void PlotControlPanel::retranslateUi()
{
    // 记录当前状态，避免切语言时丢失用户配置
    const double xMin = m_xMinSpin ? m_xMinSpin->value() : 0.0;
    const double xMax = m_xMaxSpin ? m_xMaxSpin->value() : 1000.0;
    const double yMin = m_yMinSpin ? m_yMinSpin->value() : -10.0;
    const double yMax = m_yMaxSpin ? m_yMaxSpin->value() : 10.0;
    const bool autoScale = m_autoScaleCheck ? m_autoScaleCheck->isChecked() : true;
    const bool openGlEnabled = m_openGLCheck ? m_openGLCheck->isChecked()
                                             : (m_plotterWindow ? m_plotterWindow->isOpenGLEnabled() : false);
    const bool showGrid = m_gridCheck ? m_gridCheck->isChecked() : true;
    const bool showLegend = m_legendCheck ? m_legendCheck->isChecked() : true;
    const int maxPoints = m_maxPointsSpin ? m_maxPointsSpin->value() : 5000;

    QVector<Qt::CheckState> curveStates;
    int selectedRow = -1;
    if (m_curveList) {
        selectedRow = m_curveList->currentRow();
        curveStates.reserve(m_curveList->count());
        for (int i = 0; i < m_curveList->count(); ++i) {
            if (QListWidgetItem* item = m_curveList->item(i)) {
                curveStates.push_back(item->checkState());
            } else {
                curveStates.push_back(Qt::Checked);
            }
        }
    }

    setupUI();

    if (m_xMinSpin) {
        QSignalBlocker blocker(*m_xMinSpin);
        m_xMinSpin->setValue(xMin);
    }
    if (m_xMaxSpin) {
        QSignalBlocker blocker(*m_xMaxSpin);
        m_xMaxSpin->setValue(xMax);
    }
    if (m_yMinSpin) {
        QSignalBlocker blocker(*m_yMinSpin);
        m_yMinSpin->setValue(yMin);
    }
    if (m_yMaxSpin) {
        QSignalBlocker blocker(*m_yMaxSpin);
        m_yMaxSpin->setValue(yMax);
    }
    if (m_autoScaleCheck) {
        QSignalBlocker blocker(*m_autoScaleCheck);
        m_autoScaleCheck->setChecked(autoScale);
    }
    if (m_openGLCheck) {
        QSignalBlocker blocker(*m_openGLCheck);
        m_openGLCheck->setChecked(openGlEnabled);
    }
    if (m_gridCheck) {
        QSignalBlocker blocker(*m_gridCheck);
        m_gridCheck->setChecked(showGrid);
    }
    if (m_legendCheck) {
        QSignalBlocker blocker(*m_legendCheck);
        m_legendCheck->setChecked(showLegend);
    }
    if (m_maxPointsSpin) {
        QSignalBlocker blocker(*m_maxPointsSpin);
        m_maxPointsSpin->setValue(maxPoints);
    }

    updateCurveList();

    if (m_curveList) {
        QSignalBlocker blocker(*m_curveList);
        const int restoreCount = qMin(m_curveList->count(), curveStates.size());
        for (int i = 0; i < restoreCount; ++i) {
            if (QListWidgetItem* item = m_curveList->item(i)) {
                item->setCheckState(curveStates[i]);
            }
        }
        if (selectedRow >= 0 && selectedRow < m_curveList->count()) {
            m_curveList->setCurrentRow(selectedRow);
        }
    }
}

void PlotControlPanel::setXRange(double min, double max)
{
    m_xMinSpin->blockSignals(true);
    m_xMaxSpin->blockSignals(true);
    m_xMinSpin->setValue(min);
    m_xMaxSpin->setValue(max);
    m_xMinSpin->blockSignals(false);
    m_xMaxSpin->blockSignals(false);
}

void PlotControlPanel::setYRange(double min, double max)
{
    m_yMinSpin->blockSignals(true);
    m_yMaxSpin->blockSignals(true);
    m_yMinSpin->setValue(min);
    m_yMaxSpin->setValue(max);
    m_yMinSpin->blockSignals(false);
    m_yMaxSpin->blockSignals(false);
}

void PlotControlPanel::setOpenGLEnabled(bool enabled)
{
    if (!m_openGLCheck) {
        return;
    }

    QSignalBlocker blocker(*m_openGLCheck);
    m_openGLCheck->setChecked(enabled);
}

} // namespace ComAssistant
