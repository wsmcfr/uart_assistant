/**
 * @file SerialSettingsWidget.h
 * @brief 串口设置组件
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_SERIALSETTINGSWIDGET_H
#define COMASSISTANT_SERIALSETTINGSWIDGET_H

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QEvent>
#include "config/AppConfig.h"

namespace ComAssistant {

/**
 * @brief 串口设置组件
 */
class SerialSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit SerialSettingsWidget(QWidget* parent = nullptr);
    ~SerialSettingsWidget() override = default;

    /**
     * @brief 设置配置
     */
    void setConfig(const SerialConfig& config);

    /**
     * @brief 获取配置
     */
    SerialConfig config() const;

    /**
     * @brief 刷新串口列表
     */
    void refreshPorts();

signals:
    /**
     * @brief 配置变化信号
     */
    void configChanged(const SerialConfig& config);

private slots:
    void onPortChanged(int index);
    void onBaudRateChanged(int index);
    void onDataBitsChanged(int index);
    void onStopBitsChanged(int index);
    void onParityChanged(int index);
    void onFlowControlChanged(int index);
    void onRefreshClicked();
    void onAutoRefresh();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void populateBaudRates();
    void updateConfig();
    void retranslateUi();

private:
    QComboBox* m_portCombo = nullptr;
    QComboBox* m_baudRateCombo = nullptr;
    QComboBox* m_dataBitsCombo = nullptr;
    QComboBox* m_stopBitsCombo = nullptr;
    QComboBox* m_parityCombo = nullptr;
    QComboBox* m_flowControlCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QCheckBox* m_autoRefreshCheck = nullptr;

    QTimer* m_autoRefreshTimer = nullptr;
    SerialConfig m_config;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SERIALSETTINGSWIDGET_H
