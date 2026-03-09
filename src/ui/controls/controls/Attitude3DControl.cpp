/**
 * @file Attitude3DControl.cpp
 * @brief 3D姿态显示控件实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "Attitude3DControl.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QEvent>
#include <QJsonObject>
#include <QMenu>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QPushButton>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>

namespace ComAssistant {

// ============ AttitudeGLWidget 实现 ============

AttitudeGLWidget::AttitudeGLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(150, 150);
}

void AttitudeGLWidget::setAttitude(double roll, double pitch, double yaw)
{
    m_roll = roll;
    m_pitch = pitch;
    m_yaw = yaw;
    update();
}

void AttitudeGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void AttitudeGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void AttitudeGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = float(width()) / float(height());
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float top = nearPlane * qTan(qDegreesToRadians(fov / 2.0f));
    float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 相机位置
    glTranslatef(0.0f, 0.0f, -5.0f * m_zoom);

    // 视图旋转
    glRotatef(m_viewRotX, 1.0f, 0.0f, 0.0f);
    glRotatef(m_viewRotY, 0.0f, 1.0f, 0.0f);

    // 绘制网格
    if (m_showGrid) {
        drawGrid();
    }

    // 绘制坐标轴
    if (m_showAxes) {
        drawAxes();
    }

    // 应用姿态旋转（欧拉角ZYX顺序）
    glRotatef(static_cast<float>(m_yaw), 0.0f, 0.0f, 1.0f);    // Z轴（偏航）
    glRotatef(static_cast<float>(m_pitch), 0.0f, 1.0f, 0.0f);  // Y轴（俯仰）
    glRotatef(static_cast<float>(m_roll), 1.0f, 0.0f, 0.0f);   // X轴（横滚）

    // 绘制模型
    drawModel();
}

void AttitudeGLWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();
}

void AttitudeGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    int dx = event->pos().x() - m_lastMousePos.x();
    int dy = event->pos().y() - m_lastMousePos.y();

    if (event->buttons() & Qt::LeftButton) {
        m_viewRotX += dy * 0.5f;
        m_viewRotY += dx * 0.5f;
        update();
    }

    m_lastMousePos = event->pos();
}

void AttitudeGLWidget::wheelEvent(QWheelEvent* event)
{
    float delta = event->angleDelta().y() / 120.0f;
    m_zoom *= (1.0f - delta * 0.1f);
    m_zoom = qBound(0.5f, m_zoom, 3.0f);
    update();
}

void AttitudeGLWidget::drawModel()
{
    // 绘制简化的飞行器模型（长方体 + 方向指示）
    glPushMatrix();

    // 机身颜色
    glColor4f(m_modelColor.redF(), m_modelColor.greenF(), m_modelColor.blueF(), 0.9f);

    // 绘制机身（扁平长方体）
    drawBox(1.5f, 0.3f, 0.8f);

    // 绘制机头指示（红色箭头）
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.7f, 0.15f, 0.0f);
    glVertex3f(0.7f, -0.15f, 0.0f);
    glEnd();

    // 绘制机翼
    glColor4f(m_modelColor.redF() * 0.8f, m_modelColor.greenF() * 0.8f, m_modelColor.blueF() * 0.8f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.0f);
    drawBox(0.3f, 0.1f, 1.5f);
    glPopMatrix();

    // 绘制尾翼
    glPushMatrix();
    glTranslatef(-0.6f, 0.2f, 0.0f);
    drawBox(0.2f, 0.3f, 0.5f);
    glPopMatrix();

    glPopMatrix();
}

void AttitudeGLWidget::drawBox(float sx, float sy, float sz)
{
    float hx = sx / 2.0f;
    float hy = sy / 2.0f;
    float hz = sz / 2.0f;

    glBegin(GL_QUADS);
    // 前面
    glVertex3f(-hx, -hy, hz);
    glVertex3f(hx, -hy, hz);
    glVertex3f(hx, hy, hz);
    glVertex3f(-hx, hy, hz);
    // 后面
    glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx, hy, -hz);
    glVertex3f(hx, hy, -hz);
    glVertex3f(hx, -hy, -hz);
    // 上面
    glVertex3f(-hx, hy, -hz);
    glVertex3f(-hx, hy, hz);
    glVertex3f(hx, hy, hz);
    glVertex3f(hx, hy, -hz);
    // 下面
    glVertex3f(-hx, -hy, -hz);
    glVertex3f(hx, -hy, -hz);
    glVertex3f(hx, -hy, hz);
    glVertex3f(-hx, -hy, hz);
    // 右面
    glVertex3f(hx, -hy, -hz);
    glVertex3f(hx, hy, -hz);
    glVertex3f(hx, hy, hz);
    glVertex3f(hx, -hy, hz);
    // 左面
    glVertex3f(-hx, -hy, -hz);
    glVertex3f(-hx, -hy, hz);
    glVertex3f(-hx, hy, hz);
    glVertex3f(-hx, hy, -hz);
    glEnd();

    // 绘制边框
    glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-hx, -hy, hz);
    glVertex3f(hx, -hy, hz);
    glVertex3f(hx, hy, hz);
    glVertex3f(-hx, hy, hz);
    glEnd();
}

void AttitudeGLWidget::drawAxes()
{
    glLineWidth(2.0f);

    // X轴（红色）- 前方
    glColor4f(1.0f, 0.3f, 0.3f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(2.0f, 0.0f, 0.0f);
    glEnd();

    // Y轴（绿色）- 上方
    glColor4f(0.3f, 1.0f, 0.3f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 2.0f, 0.0f);
    glEnd();

    // Z轴（蓝色）- 右方
    glColor4f(0.3f, 0.3f, 1.0f, 1.0f);
    glBegin(GL_LINES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 2.0f);
    glEnd();

    glLineWidth(1.0f);
}

void AttitudeGLWidget::drawGrid()
{
    glColor4f(0.3f, 0.3f, 0.3f, 0.5f);
    glLineWidth(1.0f);

    glBegin(GL_LINES);
    for (int i = -5; i <= 5; ++i) {
        glVertex3f(static_cast<float>(i), -0.5f, -5.0f);
        glVertex3f(static_cast<float>(i), -0.5f, 5.0f);
        glVertex3f(-5.0f, -0.5f, static_cast<float>(i));
        glVertex3f(5.0f, -0.5f, static_cast<float>(i));
    }
    glEnd();
}

void AttitudeGLWidget::drawArrow(const QVector3D& from, const QVector3D& to, const QColor& color)
{
    glColor4f(color.redF(), color.greenF(), color.blueF(), 1.0f);
    glBegin(GL_LINES);
    glVertex3f(from.x(), from.y(), from.z());
    glVertex3f(to.x(), to.y(), to.z());
    glEnd();
}

// ============ Attitude3DControl 实现 ============

Attitude3DControl::Attitude3DControl(QWidget* parent)
    : IInteractiveControl(parent)
{
    setupUi();
}

void Attitude3DControl::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    // 名称编辑框
    m_nameEdit = new QLineEdit(m_name);
    m_nameEdit->setAlignment(Qt::AlignCenter);
    m_nameEdit->setStyleSheet(
        "QLineEdit { "
        "  font-weight: bold; "
        "  border: 1px solid transparent; "
        "  border-radius: 3px; "
        "  background: transparent; "
        "} "
        "QLineEdit:hover { "
        "  border: 1px solid #3498db; "
        "} "
        "QLineEdit:focus { "
        "  border: 1px solid #2980b9; "
        "  background: rgba(52, 152, 219, 0.1); "
        "}"
    );
    m_nameEdit->setPlaceholderText(tr("输入名称..."));
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &Attitude3DControl::onNameEdited);
    mainLayout->addWidget(m_nameEdit);

    // 3D视图
    m_glWidget = new AttitudeGLWidget;
    m_glWidget->setMinimumSize(180, 150);
    mainLayout->addWidget(m_glWidget, 1);

    // 姿态数值显示
    QGridLayout* valueLayout = new QGridLayout;
    valueLayout->setSpacing(4);

    m_rollLabel = new QLabel("R: 0.0°");
    m_rollLabel->setStyleSheet("color: #e74c3c; font-size: 11px;");  // 红色
    m_pitchLabel = new QLabel("P: 0.0°");
    m_pitchLabel->setStyleSheet("color: #2ecc71; font-size: 11px;"); // 绿色
    m_yawLabel = new QLabel("Y: 0.0°");
    m_yawLabel->setStyleSheet("color: #3498db; font-size: 11px;");   // 蓝色

    valueLayout->addWidget(m_rollLabel, 0, 0);
    valueLayout->addWidget(m_pitchLabel, 0, 1);
    valueLayout->addWidget(m_yawLabel, 0, 2);

    mainLayout->addLayout(valueLayout);

    setMinimumSize(200, 220);
}

void Attitude3DControl::setControlName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        m_nameEdit->setText(name);
        emit nameChanged(name);
    }
}

void Attitude3DControl::setValue(double value)
{
    setYaw(value);
}

QByteArray Attitude3DControl::buildSendData() const
{
    QString result = m_sendTemplate;
    result.replace("%s", m_name);
    result.replace("%r", QString::number(m_roll, 'f', 2));
    result.replace("%p", QString::number(m_pitch, 'f', 2));
    result.replace("%y", QString::number(m_yaw, 'f', 2));
    return result.toUtf8();
}

void Attitude3DControl::bindDataChannel(int channel)
{
    m_rollChannel = channel;
}

void Attitude3DControl::updateDisplayValue(double value)
{
    // 默认更新横滚角
    setRoll(value);
}

void Attitude3DControl::updateChannelValue(int channel, double value)
{
    if (channel == m_rollChannel) {
        setRoll(value);
    } else if (channel == m_pitchChannel) {
        setPitch(value);
    } else if (channel == m_yawChannel) {
        setYaw(value);
    }
}

QJsonObject Attitude3DControl::toJson() const
{
    QJsonObject obj;
    obj["type"] = controlTypeName();
    obj["name"] = m_name;
    obj["sendTemplate"] = m_sendTemplate;
    obj["rollChannel"] = m_rollChannel;
    obj["pitchChannel"] = m_pitchChannel;
    obj["yawChannel"] = m_yawChannel;
    obj["roll"] = m_roll;
    obj["pitch"] = m_pitch;
    obj["yaw"] = m_yaw;
    return obj;
}

bool Attitude3DControl::fromJson(const QJsonObject& obj)
{
    if (obj["type"].toString() != controlTypeName()) {
        return false;
    }

    setControlName(obj["name"].toString("Attitude"));
    setSendTemplate(obj["sendTemplate"].toString());
    m_rollChannel = obj["rollChannel"].toInt(-1);
    m_pitchChannel = obj["pitchChannel"].toInt(-1);
    m_yawChannel = obj["yawChannel"].toInt(-1);
    setAttitude(
        obj["roll"].toDouble(0),
        obj["pitch"].toDouble(0),
        obj["yaw"].toDouble(0)
    );

    return true;
}

void Attitude3DControl::setAttitude(double roll, double pitch, double yaw)
{
    m_roll = roll;
    m_pitch = pitch;
    m_yaw = yaw;
    m_glWidget->setAttitude(roll, pitch, yaw);
    updateAttitudeDisplay();
}

void Attitude3DControl::setRoll(double roll)
{
    if (!qFuzzyCompare(m_roll, roll)) {
        m_roll = roll;
        m_glWidget->setAttitude(m_roll, m_pitch, m_yaw);
        updateAttitudeDisplay();
        emit valueChanged(roll);
    }
}

void Attitude3DControl::setPitch(double pitch)
{
    if (!qFuzzyCompare(m_pitch, pitch)) {
        m_pitch = pitch;
        m_glWidget->setAttitude(m_roll, m_pitch, m_yaw);
        updateAttitudeDisplay();
        emit valueChanged(pitch);
    }
}

void Attitude3DControl::setYaw(double yaw)
{
    if (!qFuzzyCompare(m_yaw, yaw)) {
        m_yaw = yaw;
        m_glWidget->setAttitude(m_roll, m_pitch, m_yaw);
        updateAttitudeDisplay();
        emit valueChanged(yaw);
    }
}

void Attitude3DControl::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    IInteractiveControl::changeEvent(event);
}

void Attitude3DControl::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    menu.addAction(tr("配置..."), this, &Attitude3DControl::showConfigDialog);
    menu.addSeparator();
    menu.addAction(tr("删除"), this, [this]() {
        emit deleteRequested();
    });
    menu.exec(event->globalPos());
}

void Attitude3DControl::onNameEdited()
{
    QString newName = m_nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        m_nameEdit->setText(m_name);
    } else if (newName != m_name) {
        m_name = newName;
        emit nameChanged(m_name);
    }
}

void Attitude3DControl::retranslateUi()
{
    updateAttitudeDisplay();
}

void Attitude3DControl::updateAttitudeDisplay()
{
    m_rollLabel->setText(QString("R: %1°").arg(m_roll, 0, 'f', 1));
    m_pitchLabel->setText(QString("P: %1°").arg(m_pitch, 0, 'f', 1));
    m_yawLabel->setText(QString("Y: %1°").arg(m_yaw, 0, 'f', 1));
}

void Attitude3DControl::showConfigDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("3D姿态配置 - %1").arg(m_name));
    dialog.setMinimumWidth(350);

    QFormLayout* layout = new QFormLayout(&dialog);

    // 发送模板
    QLineEdit* templateEdit = new QLineEdit(m_sendTemplate);
    templateEdit->setPlaceholderText(tr("例如: ATT %r %p %y\\n"));
    templateEdit->setToolTip(tr("%s=名称, %r=横滚, %p=俯仰, %y=偏航"));
    layout->addRow(tr("发送模板:"), templateEdit);

    // 横滚通道
    QSpinBox* rollChSpin = new QSpinBox;
    rollChSpin->setRange(-1, 99);
    rollChSpin->setSpecialValueText(tr("未绑定"));
    rollChSpin->setValue(m_rollChannel);
    layout->addRow(tr("横滚通道:"), rollChSpin);

    // 俯仰通道
    QSpinBox* pitchChSpin = new QSpinBox;
    pitchChSpin->setRange(-1, 99);
    pitchChSpin->setSpecialValueText(tr("未绑定"));
    pitchChSpin->setValue(m_pitchChannel);
    layout->addRow(tr("俯仰通道:"), pitchChSpin);

    // 偏航通道
    QSpinBox* yawChSpin = new QSpinBox;
    yawChSpin->setRange(-1, 99);
    yawChSpin->setSpecialValueText(tr("未绑定"));
    yawChSpin->setValue(m_yawChannel);
    layout->addRow(tr("偏航通道:"), yawChSpin);

    // 显示选项
    QCheckBox* showAxesCheck = new QCheckBox(tr("显示坐标轴"));
    showAxesCheck->setChecked(true);
    layout->addRow(showAxesCheck);

    QCheckBox* showGridCheck = new QCheckBox(tr("显示网格"));
    showGridCheck->setChecked(true);
    layout->addRow(showGridCheck);

    // 按钮
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addRow(buttonBox);

    if (dialog.exec() == QDialog::Accepted) {
        setSendTemplate(templateEdit->text());
        m_rollChannel = rollChSpin->value();
        m_pitchChannel = pitchChSpin->value();
        m_yawChannel = yawChSpin->value();
        m_glWidget->setShowAxes(showAxesCheck->isChecked());
        m_glWidget->setShowGrid(showGridCheck->isChecked());
    }
}

} // namespace ComAssistant
