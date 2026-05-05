/**
 * @file ThemeButton.h
 * @brief 主题切换按钮 — 自定义绘制，绕过 Qt5 QSS 对 QPushButton 图标的渲染 bug
 * @author ComAssistant Team
 */

#ifndef COMASSISTANT_THEMEBUTTON_H
#define COMASSISTANT_THEMEBUTTON_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QtMath>

namespace ComAssistant {

/**
 * @brief 主题切换按钮
 *
 * 使用 QWidget + 自定义 paintEvent 绘制太阳/月亮图标，
 * 完全绕过 Qt5 QSS 引擎对 QPushButton 图标的渲染干扰。
 */
class ThemeButton : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeButton(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_isDark(false)
        , m_hovered(false)
        , m_pressed(false)
    {
        setFixedSize(32, 32);
        setCursor(Qt::PointingHandCursor);
        setToolTip(tr("切换主题"));
    }

    /**
     * @brief 设置当前是否为深色主题
     * @param dark true 表示深色主题（显示太阳图标），false 表示浅色主题（显示月亮图标）
     */
    void setDarkTheme(bool dark)
    {
        m_isDark = dark;
        update();  // 始终触发重绘，确保初始状态也能正确显示
    }

    bool isDarkTheme() const { return m_isDark; }

    /** @brief 兼容 QPushButton 接口：isChecked 对应 isDarkTheme */
    bool isChecked() const { return m_isDark; }

    /** @brief 兼容 QPushButton 接口：setChecked 对应 setDarkTheme */
    void setChecked(bool checked) { setDarkTheme(checked); }

    QSize sizeHint() const override { return QSize(32, 32); }

signals:
    /** @brief 按钮被点击时发出 */
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 背景圆形
        QRectF bgRect(0, 0, width(), height());
        QColor bgColor;
        if (m_isDark) {
            bgColor = m_pressed ? QColor("#48484a") :
                      m_hovered ? QColor("#48484a") :
                      QColor("#3a3a3c");
        } else {
            bgColor = m_pressed ? QColor("#c7c7cc") :
                      m_hovered ? QColor("#d2d2d7") :
                      QColor("#e8e8ed");
        }
        p.setPen(Qt::NoPen);
        p.setBrush(bgColor);
        p.drawEllipse(bgRect);

        // 图标绘制（居中，20x20 区域）
        int ox = (width() - 20) / 2;
        int oy = (height() - 20) / 2;

        if (m_isDark) {
            // 深色主题下显示太阳图标
            drawSunIcon(p, ox, oy);
        } else {
            // 浅色主题下显示月亮图标
            drawMoonIcon(p, ox, oy);
        }

        p.end();
    }

    void enterEvent(QEvent*) override
    {
        m_hovered = true;
        update();
    }

    void leaveEvent(QEvent*) override
    {
        m_hovered = false;
        update();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_pressed) {
            m_pressed = false;
            update();
            if (rect().contains(event->pos())) {
                emit clicked();
            }
        }
    }

private:
    /**
     * @brief 绘制太阳图标（黄色圆形 + 8 条光线）
     */
    void drawSunIcon(QPainter& p, int ox, int oy)
    {
        // 太阳中心圆
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#ffcc00"));
        p.drawEllipse(ox + 4, oy + 4, 12, 12);

        // 光线
        p.setPen(QPen(QColor("#ffcc00"), 2));
        p.setBrush(Qt::NoBrush);
        for (int i = 0; i < 8; i++) {
            double angle = i * M_PI / 4.0;
            int cx = ox + 10;
            int cy = oy + 10;
            int x1 = cx + int(7 * cos(angle));
            int y1 = cy + int(7 * sin(angle));
            int x2 = cx + int(9 * cos(angle));
            int y2 = cy + int(9 * sin(angle));
            p.drawLine(x1, y1, x2, y2);
        }
    }

    /**
     * @brief 绘制月亮图标（深色新月形）
     */
    void drawMoonIcon(QPainter& p, int ox, int oy)
    {
        // 先画深色满圆
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#1d1d1f"));
        p.drawEllipse(ox + 3, oy + 3, 14, 14);

        // 再用背景色画偏移圆，形成新月效果
        QColor bgColor = m_pressed ? QColor("#c7c7cc") :
                         m_hovered ? QColor("#d2d2d7") :
                         QColor("#e8e8ed");
        p.setBrush(bgColor);
        p.drawEllipse(ox + 6, oy + 1, 14, 14);
    }

    bool m_isDark;     ///< 当前是否为深色主题
    bool m_hovered;    ///< 鼠标悬停状态
    bool m_pressed;    ///< 鼠标按下状态
};

} // namespace ComAssistant

#endif // COMASSISTANT_THEMEBUTTON_H
