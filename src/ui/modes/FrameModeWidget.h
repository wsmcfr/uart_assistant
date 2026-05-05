/**
 * @file FrameModeWidget.h
 * @brief 帧模式组件
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef FRAMEMODEWIDGET_H
#define FRAMEMODEWIDGET_H

#include "IModeWidget.h"
#include <QTableWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QToolBar>
#include <QLineEdit>
#include <QSpinBox>
#include <QTimer>
#include <QDateTime>
#include <QEvent>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>

namespace ComAssistant {

/**
 * @brief 帧数据结构
 */
struct FrameData {
    int index = 0;              // 帧序号
    QDateTime timestamp;        // 时间戳
    QByteArray rawData;         // 原始数据
    bool valid = false;         // 是否有效
    QString errorInfo;          // 错误信息
    QVariantMap parsedFields;   // 解析后的字段
};

/**
 * @brief 帧模式解析配置（用于UI模式专用）
 */
struct ModeFrameConfig {
    QByteArray header;          // 帧头
    QByteArray footer;          // 帧尾
    int lengthFieldPos = -1;    // 长度字段位置（-1表示不使用）
    int lengthFieldSize = 1;    // 长度字段大小（1或2字节）
    bool lengthBigEndian = false; // 长度字段字节序
    bool lengthIncludesHeader = false;  // 长度是否包含帧头
    int checksumType = 0;       // 校验类型：0=无, 1=XOR, 2=SUM, 3=CRC16
    int checksumPos = -1;       // 校验位置（-1表示帧尾前）
    int maxFrameSize = 1024;    // 最大帧长度
    int timeout = 100;          // 超时时间（毫秒）
};

/**
 * @brief 帧模式组件
 */
class FrameModeWidget : public IModeWidget {
    Q_OBJECT

public:
    explicit FrameModeWidget(QWidget* parent = nullptr);
    ~FrameModeWidget() override = default;

    // IModeWidget 接口实现
    QString modeName() const override { return tr("帧模式"); }
    QString modeIcon() const override { return "frame"; }
    void appendReceivedData(const QByteArray& data) override;
    void appendSentData(const QByteArray& data) override;
    void clear() override;
    bool exportToFile(const QString& fileName) override;
    void onActivated() override;
    void onDeactivated() override;
    QWidget* modeToolBar() override { return m_toolBar; }

    // 帧配置
    void setFrameConfig(const ModeFrameConfig& config);
    ModeFrameConfig frameConfig() const { return m_config; }

private slots:
    void onFrameSelected(int row, int column);
    void onConfigChanged();
    void onClearFrames();
    void onExportFrames();
    void onFrameTimeout();
    void onFilterChanged(const QString& text);
    void onSendFrame();
    void flushPendingFrames();  ///< 批量刷新待显示帧，避免高频 insertRow 卡住 UI

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void setupToolBar();
    void retranslateUi();
    void processBuffer();
    void addFrame(const FrameData& frame);
    void fillFrameRow(int row, const FrameData& frame); ///< 填充一行帧数据，批量扩表后复用
    void trimFrameRecords();                         ///< 限制帧记录数量，防止 UI 内存无限增长
    void scheduleFrameFlush();                       ///< 安排批量刷新帧表格
    void updateFrameDetail(const FrameData& frame);
    void updateStatistics();
    bool validateFrame(const QByteArray& data, QString& error);
    QByteArray calculateChecksum(const QByteArray& data);

    // UI 组件
    QSplitter* m_splitter;
    QTableWidget* m_frameTable;
    QTextEdit* m_detailView;
    QToolBar* m_toolBar;
    QLineEdit* m_headerEdit;
    QLineEdit* m_footerEdit;
    QLineEdit* m_filterEdit;
    QLineEdit* m_sendEdit;

    // 需要国际化的UI元素
    QLabel* m_filterLabel = nullptr;
    QLabel* m_detailLabel = nullptr;
    QLabel* m_statsLabel = nullptr;
    QLabel* m_sendLabel = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QPushButton* m_sendWithHeaderBtn = nullptr;
    QLabel* m_headerLabel = nullptr;
    QLabel* m_footerLabel = nullptr;
    QLabel* m_checksumLabel = nullptr;
    QComboBox* m_checksumCombo = nullptr;

    // 工具栏动作（需要翻译更新）
    QAction* m_clearAction = nullptr;
    QAction* m_exportAction = nullptr;

    // 数据
    QList<FrameData> m_frames;
    QList<FrameData> m_pendingFrames;        ///< 待批量刷新到表格的帧
    QByteArray m_buffer;
    ModeFrameConfig m_config;
    QTimer* m_timeoutTimer;
    QTimer* m_frameFlushTimer = nullptr;     ///< 帧表格批量刷新定时器
    int m_maxFrameRecords = 10000;           ///< UI 中保留的最大帧记录数
    int m_frameFlushIntervalMs = 33;         ///< 帧表格刷新间隔，约 30fps
    int m_frameFlushBatchSize = 300;         ///< 单次最多落表帧数，避免一次刷新阻塞过久

    // 统计
    int m_totalFrames = 0;
    int m_validFrames = 0;
    int m_invalidFrames = 0;
};

} // namespace ComAssistant

#endif // FRAMEMODEWIDGET_H
