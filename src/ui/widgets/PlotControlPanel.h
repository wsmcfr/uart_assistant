/**
 * @file PlotControlPanel.h
 * @brief 绘图控制面板 - 右侧可停靠的控制面板
 * @author ComAssistant Team
 * @date 2026-01-28
 */

#ifndef PLOTCONTROLPANEL_H
#define PLOTCONTROLPANEL_H

#include <QWidget>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QListWidget>
#include <QColorDialog>
#include <QEvent>

namespace ComAssistant {

class PlotterWindow;

/**
 * @brief 绘图控制面板
 *
 * 提供曲线管理、坐标轴设置、显示设置等功能
 */
class PlotControlPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit PlotControlPanel(PlotterWindow* plotterWindow, QWidget* parent = nullptr);
    ~PlotControlPanel() override = default;

    /**
     * @brief 更新曲线列表
     */
    void updateCurveList();

    /**
     * @brief 设置X轴范围显示
     */
    void setXRange(double min, double max);

    /**
     * @brief 设置Y轴范围显示
     */
    void setYRange(double min, double max);

    /**
     * @brief 同步 OpenGL 开关显示状态
     * @param enabled 是否启用 OpenGL
     */
    void setOpenGLEnabled(bool enabled);

signals:
    /**
     * @brief 曲线可见性改变
     */
    void curveVisibilityChanged(int index, bool visible);

    /**
     * @brief 曲线颜色改变
     */
    void curveColorChanged(int index, const QColor& color);

    /**
     * @brief X轴范围改变
     */
    void xRangeChanged(double min, double max);

    /**
     * @brief Y轴范围改变
     */
    void yRangeChanged(double min, double max);

    /**
     * @brief 自动缩放状态改变
     */
    void autoScaleChanged(bool enabled);

    /**
     * @brief 网格显示状态改变
     */
    void gridVisibleChanged(bool visible);

    /**
     * @brief 图例显示状态改变
     */
    void legendVisibleChanged(bool visible);

    /**
     * @brief 数据点数改变
     */
    void maxDataPointsChanged(int points);

    /**
     * @brief 请求管理差值曲线
     */
    void differenceCurveRequested();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUI();
    void retranslateUi();
    QGroupBox* createCurveManagement();
    QGroupBox* createAxisSettings();
    QGroupBox* createDisplaySettings();
    QGroupBox* createDataSettings();

    PlotterWindow* m_plotterWindow;

    // 曲线管理
    QListWidget* m_curveList;

    // 坐标轴设置
    QDoubleSpinBox* m_xMinSpin;
    QDoubleSpinBox* m_xMaxSpin;
    QDoubleSpinBox* m_yMinSpin;
    QDoubleSpinBox* m_yMaxSpin;
    QCheckBox* m_autoScaleCheck;

    // 显示设置
    QCheckBox* m_gridCheck;
    QCheckBox* m_legendCheck;
    QCheckBox* m_openGLCheck;

    // 数据设置
    QSpinBox* m_maxPointsSpin;
};

} // namespace ComAssistant

#endif // PLOTCONTROLPANEL_H
