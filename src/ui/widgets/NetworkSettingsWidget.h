/**
 * @file NetworkSettingsWidget.h
 * @brief 网络设置组件
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_NETWORKSETTINGSWIDGET_H
#define COMASSISTANT_NETWORKSETTINGSWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QEvent>
#include "config/AppConfig.h"

namespace ComAssistant {

/**
 * @brief 网络设置组件
 */
class NetworkSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit NetworkSettingsWidget(QWidget* parent = nullptr);
    ~NetworkSettingsWidget() override = default;

    void setConfig(const NetworkConfig& config);
    NetworkConfig config() const;

    void setMode(NetworkMode mode);

signals:
    void configChanged(const NetworkConfig& config);

private slots:
    void onConfigChanged();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();

private:
    QGroupBox* m_remoteGroup = nullptr;
    QGroupBox* m_localGroup = nullptr;
    QLabel* m_remoteIpLabel = nullptr;
    QLabel* m_remotePortLabel = nullptr;
    QLabel* m_localPortLabel = nullptr;
    QLabel* m_maxConnectionsLabel = nullptr;
    QLineEdit* m_remoteIpEdit = nullptr;
    QSpinBox* m_remotePortSpin = nullptr;
    QSpinBox* m_localPortSpin = nullptr;
    QSpinBox* m_maxConnectionsSpin = nullptr;

    NetworkConfig m_config;
};

} // namespace ComAssistant

#endif // COMASSISTANT_NETWORKSETTINGSWIDGET_H
