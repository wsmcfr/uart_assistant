/**
 * @file DataTableWidget.h
 * @brief 数据表格视图控件
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#ifndef DATATABLEWIDGET_H
#define DATATABLEWIDGET_H

#include <QWidget>
#include <QTableView>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QToolBar>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QDateTime>
#include <QVector>
#include <QMutex>

namespace ComAssistant {

/**
 * @brief 表格数据记录结构体
 */
struct TableDataRecord {
    int index = 0;                    ///< 序号
    QDateTime timestamp;              ///< 时间戳
    QString direction;                ///< 方向（TX/RX）
    QByteArray rawData;               ///< 原始数据
    QString hexString;                ///< HEX格式字符串
    QString asciiString;              ///< ASCII格式字符串
    QVector<double> parsedValues;     ///< 解析出的数值
    QString protocol;                 ///< 协议类型
    QString description;              ///< 描述/备注
};

/**
 * @brief 数据表格视图控件
 *
 * 以表格形式显示接收/发送的数据，支持实时更新、过滤、搜索和导出。
 */
class DataTableWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 显示列枚举
     */
    enum Column {
        ColIndex = 0,      ///< 序号
        ColTimestamp,      ///< 时间戳
        ColDirection,      ///< 方向
        ColHex,            ///< HEX数据
        ColAscii,          ///< ASCII数据
        ColValues,         ///< 解析数值
        ColProtocol,       ///< 协议
        ColDescription,    ///< 描述
        ColCount           ///< 列数
    };

    /**
     * @brief 构造函数
     * @param parent 父控件
     */
    explicit DataTableWidget(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DataTableWidget() override;

    /**
     * @brief 添加接收数据
     * @param data 原始数据
     * @param protocol 协议类型
     * @param parsedValues 解析出的数值
     */
    void addReceivedData(const QByteArray& data,
                         const QString& protocol = QString(),
                         const QVector<double>& parsedValues = QVector<double>());

    /**
     * @brief 添加发送数据
     * @param data 原始数据
     * @param protocol 协议类型
     */
    void addSentData(const QByteArray& data,
                     const QString& protocol = QString());

    /**
     * @brief 清空所有数据
     */
    void clearAll();

    /**
     * @brief 设置最大记录数
     * @param maxRecords 最大记录数
     */
    void setMaxRecords(int maxRecords);

    /**
     * @brief 获取最大记录数
     * @return 最大记录数
     */
    int maxRecords() const { return m_maxRecords; }

    /**
     * @brief 设置是否显示接收数据
     * @param show 是否显示
     */
    void setShowReceived(bool show);

    /**
     * @brief 设置是否显示发送数据
     * @param show 是否显示
     */
    void setShowSent(bool show);

    /**
     * @brief 设置自动滚动
     * @param enabled 是否启用
     */
    void setAutoScroll(bool enabled);

    /**
     * @brief 导出数据为CSV
     * @param filename 文件名
     * @return 是否成功
     */
    bool exportToCsv(const QString& filename);

    /**
     * @brief 获取记录数量
     * @return 记录数量
     */
    int recordCount() const { return m_records.size(); }

    /**
     * @brief 获取选中的记录
     * @return 选中的记录列表
     */
    QVector<TableDataRecord> selectedRecords() const;

signals:
    /**
     * @brief 记录数量变化信号
     * @param count 当前记录数
     */
    void recordCountChanged(int count);

    /**
     * @brief 双击记录信号
     * @param record 被双击的记录
     */
    void recordDoubleClicked(const TableDataRecord& record);

public slots:
    /**
     * @brief 暂停/恢复更新
     * @param paused 是否暂停
     */
    void setPaused(bool paused);

    /**
     * @brief 设置过滤文本
     * @param text 过滤文本
     */
    void setFilterText(const QString& text);

    /**
     * @brief 复制选中行到剪贴板
     */
    void copySelected();

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onFilterTextChanged(const QString& text);
    void onDirectionFilterChanged(int index);
    void onClearClicked();
    void onExportClicked();
    void onCopyClicked();
    void onAutoScrollToggled(bool checked);
    void onPauseToggled(bool checked);
    void onTableDoubleClicked(const QModelIndex& index);
    void updateDisplay();

private:
    void setupUi();
    void setupToolBar();
    void setupTable();
    void retranslateUi();
    void addRecords(const QVector<TableDataRecord>& records);  ///< 批量添加记录到模型，减少高频接收时的表格刷新开销
    void trimRecords();
    QString formatHexString(const QByteArray& data);
    QString formatAsciiString(const QByteArray& data);
    QString formatParsedValues(const QVector<double>& values);
    void updateStatusLabel();

private:
    // UI组件
    QToolBar* m_toolBar = nullptr;
    QTableView* m_tableView = nullptr;
    QStandardItemModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxyModel = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QComboBox* m_directionCombo = nullptr;
    QLabel* m_statusLabel = nullptr;
    QCheckBox* m_autoScrollCheck = nullptr;
    QAction* m_pauseAction = nullptr;
    QAction* m_clearAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_copyAction = nullptr;

    // 数据存储
    QVector<TableDataRecord> m_records;
    QVector<TableDataRecord> m_pendingRecords;  ///< 待添加的记录（批量更新）
    QMutex m_mutex;

    // 配置
    int m_maxRecords = 10000;
    int m_recordIndex = 0;
    bool m_autoScroll = true;
    bool m_paused = false;
    bool m_showReceived = true;
    bool m_showSent = true;

    // 更新定时器
    QTimer* m_updateTimer = nullptr;
    static const int UPDATE_INTERVAL_MS = 100;  ///< 批量更新间隔
};

} // namespace ComAssistant

#endif // DATATABLEWIDGET_H
