/**
 * @file Attitude3DControl.h
 * @brief 3D姿态显示控件
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef COMASSISTANT_ATTITUDE3DCONTROL_H
#define COMASSISTANT_ATTITUDE3DCONTROL_H

#include "../IInteractiveControl.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QVector3D>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>

namespace ComAssistant {

/**
 * @brief OpenGL 3D渲染视图
 */
class AttitudeGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit AttitudeGLWidget(QWidget* parent = nullptr);
    ~AttitudeGLWidget() override = default;

    /**
     * @brief 设置欧拉角（角度制）
     */
    void setAttitude(double roll, double pitch, double yaw);

    /**
     * @brief 获取欧拉角
     */
    double roll() const { return m_roll; }
    double pitch() const { return m_pitch; }
    double yaw() const { return m_yaw; }

    /**
     * @brief 设置模型颜色
     */
    void setModelColor(const QColor& color) { m_modelColor = color; update(); }

    /**
     * @brief 设置是否显示坐标轴
     */
    void setShowAxes(bool show) { m_showAxes = show; update(); }

    /**
     * @brief 设置是否显示网格
     */
    void setShowGrid(bool show) { m_showGrid = show; update(); }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void drawModel();
    void drawAxes();
    void drawGrid();
    void drawBox(float sx, float sy, float sz);
    void drawArrow(const QVector3D& from, const QVector3D& to, const QColor& color);

    // 欧拉角（角度）
    double m_roll = 0.0;   // 绕X轴（横滚）
    double m_pitch = 0.0;  // 绕Y轴（俯仰）
    double m_yaw = 0.0;    // 绕Z轴（偏航）

    // 视图控制
    float m_viewRotX = 30.0f;  // 视图X轴旋转
    float m_viewRotY = -45.0f; // 视图Y轴旋转
    float m_zoom = 1.0f;       // 缩放

    // 鼠标交互
    QPoint m_lastMousePos;

    // 显示选项
    QColor m_modelColor = QColor(52, 152, 219);  // 蓝色
    bool m_showAxes = true;
    bool m_showGrid = true;
};

/**
 * @brief 3D姿态显示控件
 *
 * 用于显示IMU/飞控的姿态数据（Roll/Pitch/Yaw）
 */
class Attitude3DControl : public IInteractiveControl
{
    Q_OBJECT

public:
    explicit Attitude3DControl(QWidget* parent = nullptr);
    ~Attitude3DControl() override = default;

    // IInteractiveControl 接口实现
    ControlType controlType() const override { return ControlType::Gauge; }  // 使用Gauge类型
    QString controlTypeName() const override { return QStringLiteral("Attitude3D"); }
    QString controlName() const override { return m_name; }
    void setControlName(const QString& name) override;
    double currentValue() const override { return m_yaw; }  // 返回偏航角
    void setValue(double value) override;

    void setSendTemplate(const QString& tmpl) override { m_sendTemplate = tmpl; }
    QString sendTemplate() const override { return m_sendTemplate; }
    QByteArray buildSendData() const override;

    void bindDataChannel(int channel) override;
    int boundDataChannel() const override { return m_rollChannel; }
    void updateDisplayValue(double value) override;

    QJsonObject toJson() const override;
    bool fromJson(const QJsonObject& obj) override;

    // 姿态专属接口
    void setAttitude(double roll, double pitch, double yaw);
    void setRoll(double roll);
    void setPitch(double pitch);
    void setYaw(double yaw);

    double roll() const { return m_roll; }
    double pitch() const { return m_pitch; }
    double yaw() const { return m_yaw; }

    // 数据通道绑定（三轴各绑定一个通道）
    void bindRollChannel(int channel) { m_rollChannel = channel; }
    void bindPitchChannel(int channel) { m_pitchChannel = channel; }
    void bindYawChannel(int channel) { m_yawChannel = channel; }

    int rollChannel() const { return m_rollChannel; }
    int pitchChannel() const { return m_pitchChannel; }
    int yawChannel() const { return m_yawChannel; }

    /**
     * @brief 更新指定通道的值
     */
    void updateChannelValue(int channel, double value);

protected:
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onNameEdited();
    void showConfigDialog();

private:
    void setupUi();
    void retranslateUi();
    void updateAttitudeDisplay();

    // UI组件
    QLineEdit* m_nameEdit = nullptr;
    AttitudeGLWidget* m_glWidget = nullptr;
    QLabel* m_rollLabel = nullptr;
    QLabel* m_pitchLabel = nullptr;
    QLabel* m_yawLabel = nullptr;

    // 配置
    QString m_name = "Attitude";
    QString m_sendTemplate;
    double m_roll = 0.0;
    double m_pitch = 0.0;
    double m_yaw = 0.0;

    // 数据通道绑定
    int m_rollChannel = -1;
    int m_pitchChannel = -1;
    int m_yawChannel = -1;
};

} // namespace ComAssistant

#endif // COMASSISTANT_ATTITUDE3DCONTROL_H
