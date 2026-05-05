/**
 * @file DebugModeWidget.h
 * @brief 调试模式组件
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef DEBUGMODEWIDGET_H
#define DEBUGMODEWIDGET_H

#include "IModeWidget.h"
#include <QTableWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QToolBar>
#include <QTimer>
#include <QTreeWidget>
#include <QDateTime>
#include <QLineEdit>
#include <QCheckBox>
#include <QEvent>
#include <QLabel>
#include <QPushButton>

namespace ComAssistant {

/**
 * @brief 调试模式数据记录结构
 */
struct DebugDataRecord {
    int index = 0;
    QDateTime timestamp;
    bool isTx = false;          // true=发送, false=接收
    QByteArray data;
    QString parsed;             // 解析后的描述
    bool isBreakpoint = false;  // 是否为断点
};

/**
 * @brief 调试模式组件
 */
class DebugModeWidget : public IModeWidget {
    Q_OBJECT

public:
    explicit DebugModeWidget(QWidget* parent = nullptr);
    ~DebugModeWidget() override = default;

    // IModeWidget 接口实现
    QString modeName() const override { return tr("调试"); }
    QString modeIcon() const override { return "debug"; }
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;
    bool exportToFile(const QString& fileName) override;
    void onActivated() override;
    void onDeactivated() override;
    QWidget* modeToolBar() override { return m_toolBar; }

    // 调试功能
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }
    void addBreakpoint(const QByteArray& pattern);
    void removeBreakpoint(const QByteArray& pattern);
    void clearBreakpoints();

    // 监视变量
    void setWatchVariable(const QString& name, const QVariant& value);
    void clearWatchVariables();

private slots:
    void onRecordSelected(int row, int column);
    void onTogglePause();
    void onStepNext();
    void onToggleHexDisplay();
    void onToggleTimeDiff();
    void onClearRecords();
    void onExportRecords();
    void onFilterChanged(const QString& text);
    void onAddBreakpoint();
    void onSendClicked();
    void onTableScrollValueChanged(int value);
    void flushPendingRecords();  ///< 批量刷新待显示调试记录，避免高频 insertRow 卡顿

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupToolBar();
    void retranslateUi();
    void addRecord(const DebugDataRecord& record);
    void fillRecordRow(int row, const DebugDataRecord& record); ///< 填充一行调试记录，批量扩表后复用
    void trimRecordRows();                               ///< 限制调试记录常驻数量
    void scheduleRecordFlush();                          ///< 安排批量刷新表格
    void updateRecordDetail(const DebugDataRecord& record);
    bool checkBreakpoint(const QByteArray& data);
    QString formatData(const QByteArray& data, bool hex);
    QString formatTimeDiff(const QDateTime& prev, const QDateTime& current);

    // 智能滚屏辅助方法
    void showSmartScrollIndicator(const QString& message);
    void hideSmartScrollIndicator();

    // UI 组件
    QSplitter* m_mainSplitter;
    QSplitter* m_bottomSplitter;
    QTableWidget* m_recordTable;
    QTextEdit* m_detailView;
    QTreeWidget* m_watchTree;
    QToolBar* m_toolBar;
    QLineEdit* m_sendEdit = nullptr;
    QCheckBox* m_sendHexCheck = nullptr;

    // 需要国际化的UI元素
    QLabel* m_filterLabel = nullptr;
    QLineEdit* m_filterEdit = nullptr;
    QCheckBox* m_txCheck = nullptr;
    QCheckBox* m_rxCheck = nullptr;
    QLabel* m_detailLabel = nullptr;
    QLabel* m_watchLabel = nullptr;
    QLabel* m_sendLabel = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QLabel* m_statsLabel = nullptr;
    QLabel* m_smartScrollIndicator = nullptr;  ///< 智能滚屏提示标签

    // 工具栏动作（需要翻译更新）
    QAction* m_pauseAction = nullptr;
    QAction* m_stepAction = nullptr;
    QAction* m_breakpointAction = nullptr;
    QAction* m_hexAction = nullptr;
    QAction* m_timeDiffAction = nullptr;
    QAction* m_clearAction = nullptr;
    QAction* m_exportAction = nullptr;

    // 数据
    QList<DebugDataRecord> m_records;
    QList<DebugDataRecord> m_recordsToFlush;     ///< 未暂停状态下待批量刷新到表格的记录
    QList<QByteArray> m_breakpoints;
    QByteArray m_rxBuffer;
    QByteArray m_txBuffer;
    QDateTime m_lastTimestamp;

    // 状态
    bool m_paused = false;
    bool m_hexDisplay = true;
    bool m_showTimeDiff = true;
    bool m_smartScrollPaused = false;       ///< 智能滚屏暂停标志
    int m_totalTx = 0;
    int m_totalRx = 0;
    int m_recordIndex = 0;
    int m_maxRecords = 10000;                    ///< UI 中保留的最大调试记录数
    int m_recordFlushIntervalMs = 33;            ///< 调试表格刷新间隔，约 30fps
    int m_recordFlushBatchSize = 300;            ///< 单次最多落表记录数
    QTimer* m_recordFlushTimer = nullptr;        ///< 调试表格批量刷新定时器

    // 暂停时的缓冲
    QList<DebugDataRecord> m_pendingRecords;
};

} // namespace ComAssistant

#endif // DEBUGMODEWIDGET_H
