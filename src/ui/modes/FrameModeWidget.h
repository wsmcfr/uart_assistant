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
#include <QVector>

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

#ifdef COMASSISTANT_TESTS
    /**
     * @brief 测试专用：立即把待刷新帧落到表格。
     *
     * 主要流程：直接调用批量刷新槽函数，等价于刷新定时器到点触发。
     * 该接口只在测试目标中编译，生产程序不会暴露。
     */
    void flushPendingFramesForTest() { flushPendingFrames(); }

    /**
     * @brief 测试专用：获取已记录帧数量。
     * @return m_frames 当前元素数量，用于验证 clear() 后记录已清空。
     */
    int frameCountForTest() const { return m_frames.size(); }

    /**
     * @brief 测试专用：获取待刷新帧数量。
     * @return m_pendingFrames 当前元素数量，用于验证 clear() 后 pending 队列已清空。
     */
    int pendingFrameCountForTest() const { return m_pendingFrames.size(); }

    /**
     * @brief 测试专用：获取已记录帧容器容量。
     * @return m_frames 当前 capacity，用于验证清空后是否释放历史容量。
     */
    int frameCapacityForTest() const { return m_frames.capacity(); }

    /**
     * @brief 测试专用：获取待刷新帧容器容量。
     * @return m_pendingFrames 当前 capacity，用于验证清空后是否释放历史容量。
     */
    int pendingFrameCapacityForTest() const { return m_pendingFrames.capacity(); }

    /**
     * @brief 测试专用：获取接收缓冲区当前长度。
     * @return m_buffer 当前 size，用于验证清空后没有残留半包数据。
     */
    int receiveBufferSizeForTest() const { return m_buffer.size(); }

    /**
     * @brief 测试专用：获取接收缓冲区容量。
     * @return m_buffer 当前 capacity，用于验证清空后是否释放历史容量。
     */
    int receiveBufferCapacityForTest() const { return m_buffer.capacity(); }

    /**
     * @brief 测试专用：获取有效帧统计。
     * @return m_validFrames 当前值，用于验证校验结果统计。
     */
    int validFrameCountForTest() const { return m_validFrames; }

    /**
     * @brief 测试专用：获取无效帧统计。
     * @return m_invalidFrames 当前值，用于验证校验结果统计。
     */
    int invalidFrameCountForTest() const { return m_invalidFrames; }

    /**
     * @brief 测试专用：获取第一帧错误信息。
     * @return 第一条记录的 errorInfo；没有记录时返回空字符串。
     */
    QString firstFrameErrorForTest() const
    {
        return m_frames.isEmpty() ? QString() : m_frames.first().errorInfo;
    }
#endif

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
    QByteArray buildFramePayloadForSending(const QByteArray& payload) const; ///< 按当前帧头/校验/帧尾构造发送帧

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
    QVector<FrameData> m_frames;             ///< 已记录帧，使用 QVector 便于清空时明确释放容量
    QVector<FrameData> m_pendingFrames;      ///< 待批量刷新到表格的帧，使用 QVector 便于清空时明确释放容量
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
