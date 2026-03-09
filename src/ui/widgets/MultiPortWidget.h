/**
 * @file MultiPortWidget.h
 * @brief 多串口管理界面组件
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_MULTIPORTWIDGET_H
#define COMASSISTANT_MULTIPORTWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QDialog>
#include <QLineEdit>
#include <QGroupBox>
#include <QEvent>

namespace ComAssistant {

class MultiPortManager;

/**
 * @brief 多串口管理界面组件
 */
class MultiPortWidget : public QWidget {
    Q_OBJECT

public:
    explicit MultiPortWidget(QWidget* parent = nullptr);
    ~MultiPortWidget() override = default;

    /**
     * @brief 刷新可用串口列表
     */
    void refreshAvailablePorts();

    /**
     * @brief 获取选中的实例ID
     */
    QString selectedInstanceId() const;

signals:
    /**
     * @brief 请求在主窗口显示数据
     */
    void showDataRequest(const QString& instanceId, const QByteArray& data, bool isReceive);

    /**
     * @brief 选中的串口改变
     */
    void selectedPortChanged(const QString& instanceId);

    /**
     * @brief 发送到活动串口
     */
    void sendToActivePort(const QByteArray& data);

private slots:
    void onAddPortClicked();
    void onRemovePortClicked();
    void onOpenCloseClicked();
    void onConfigClicked();
    void onSendToAllClicked();

    void onPortCreated(const QString& instanceId);
    void onPortRemoved(const QString& instanceId);
    void onPortOpened(const QString& instanceId);
    void onPortClosed(const QString& instanceId);
    void onDataReceived(const QString& instanceId, const QByteArray& data);
    void onStatisticsUpdated(const QString& instanceId);
    void onPortError(const QString& instanceId, const QString& errorString);

    void onTableSelectionChanged();
    void onTableContextMenu(const QPoint& pos);

    void updatePortsStatus();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void updateTable();
    int findRowByInstanceId(const QString& instanceId) const;
    void updateRowStatus(int row, const QString& instanceId);
    void retranslateUi();

private:
    // 可用串口选择
    QGroupBox* m_addGroup = nullptr;
    QComboBox* m_availablePortsCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_addBtn = nullptr;

    // 串口列表表格
    QTableWidget* m_portTable = nullptr;

    // 操作按钮
    QPushButton* m_openCloseBtn = nullptr;
    QPushButton* m_configBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;
    QPushButton* m_sendToAllBtn = nullptr;

    // 状态显示
    QLabel* m_statusLabel = nullptr;

    // 定时器
    QTimer* m_statusTimer = nullptr;

    // 管理器引用
    MultiPortManager* m_manager = nullptr;

    // 表格列定义
    enum Column {
        ColAlias = 0,
        ColPort,
        ColBaudRate,
        ColStatus,
        ColTx,
        ColRx,
        ColCount
    };
};

/**
 * @brief 串口配置对话框
 */
class PortConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit PortConfigDialog(const QString& instanceId, QWidget* parent = nullptr);

    qint32 baudRate() const;
    int dataBits() const;
    int parity() const;
    int stopBits() const;
    int flowControl() const;
    QString alias() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void loadCurrentConfig();
    void retranslateUi();

private:
    QString m_instanceId;

    QLabel* m_aliasLabel = nullptr;
    QLabel* m_baudRateLabel = nullptr;
    QLabel* m_dataBitsLabel = nullptr;
    QLabel* m_parityLabel = nullptr;
    QLabel* m_stopBitsLabel = nullptr;
    QLabel* m_flowControlLabel = nullptr;

    QLineEdit* m_aliasEdit = nullptr;
    QComboBox* m_baudRateCombo = nullptr;
    QComboBox* m_dataBitsCombo = nullptr;
    QComboBox* m_parityCombo = nullptr;
    QComboBox* m_stopBitsCombo = nullptr;
    QComboBox* m_flowControlCombo = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MULTIPORTWIDGET_H
