/**
 * @file TabbedReceiveWidget.h
 * @brief 多标签接收区组件
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_TABBEDRECEIVEWIDGET_H
#define COMASSISTANT_TABBEDRECEIVEWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTableView>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QDateTime>
#include <QColor>
#include <QMap>
#include <QLabel>
#include <QFont>
#include <QTimer>

namespace ComAssistant {

class ReceiveHighlightHighlighter;
class ReceiveHexModel;

/**
 * @brief 高亮规则
 */
struct HighlightRule {
    QString keyword;        ///< 关键词
    QColor color;           ///< 文字颜色
    bool caseSensitive = false;  ///< 区分大小写
    bool enabled = true;    ///< 是否启用
};

/**
 * @brief 显示模式枚举
 */
enum class ReceiveDisplayMode {
    Serial = 0,     ///< 串口模式 - 简单收发显示
    Terminal = 1,   ///< 终端模式 - VT100/ANSI终端仿真
    Frame = 2,      ///< 帧模式 - 每帧一行显示
    Debug = 3       ///< 调试模式 - 带时间戳和方向
};

/**
 * @brief 多标签接收区组件
 * 包含：文本显示、十六进制显示、过滤功能、高亮功能
 */
class TabbedReceiveWidget : public QWidget {
    Q_OBJECT

public:
    explicit TabbedReceiveWidget(QWidget* parent = nullptr);
    ~TabbedReceiveWidget() override = default;

    void appendData(const QByteArray& data);
    void appendSentData(const QByteArray& data);  // 用于调试模式显示发送数据
    void clear();
    void exportToFile(const QString& fileName);

    void setTimestampEnabled(bool enabled);
    bool isTimestampEnabled() const;

    void setAutoScrollEnabled(bool enabled);
    bool isAutoScrollEnabled() const;

    void setHexDisplayEnabled(bool enabled);
    bool isHexDisplayEnabled() const;

    void setDisplayFont(const QFont& font);
    void setMaxLines(int maxLines);
    void setHexBufferBytes(int bytes);

    // 显示模式
    void setDisplayMode(int mode);
    int displayMode() const { return static_cast<int>(m_displayMode); }

    // 搜索功能
    void showSearchBar();
    void hideSearchBar();

    // 高亮功能
    void addHighlightRule(const HighlightRule& rule);
    void removeHighlightRule(const QString& keyword);
    void clearHighlightRules();
    QVector<HighlightRule> highlightRules() const { return m_highlightRules; }
    void setHighlightEnabled(bool enabled);
    bool isHighlightEnabled() const { return m_highlightEnabled; }

signals:
    void dataReceived(const QByteArray& data);

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void onTabCloseRequested(int index);
    void onFilterTextChanged(const QString& text);
    void onClearClicked();
    void onTimestampToggled(bool checked);
    void onAutoScrollToggled(bool checked);
    void onHexDisplayToggled(bool checked);
    void onHighlightToggled(bool checked);
    void onHighlightSettingsClicked();
    void onScrollBarValueChanged(int value);

private:
    void setupUi();
    void setupMainTab();
    void setupHexTab();
    void setupFilterTab();
    void setupOptionsBar();
    void retranslateUi();

    // 各模式的数据处理
    void appendSerialMode(const QByteArray& data);
    void appendTerminalMode(const QByteArray& data);
    void appendFrameMode(const QByteArray& data);
    void appendDebugMode(const QByteArray& data, bool isSent = false);

    // 终端模式辅助函数
    void processAnsiEscape(const QByteArray& data);
    void terminalWrite(const QString& text);
    void terminalWriteChar(QChar ch);
    void terminalNewLine();
    void terminalCarriageReturn();
    void terminalBackspace();
    void terminalTab();
    void terminalClearLine();
    void terminalSetColor(int colorCode);
    void terminalResetFormat();
    void updateTerminalDisplay();

    void appendToMainView(const QByteArray& data);
    void appendToHexView(const QByteArray& data);
    void updateFilterView();
    void refreshMainView();
    void applyHighlight();
    void scheduleHighlightUpdate();
    void scheduleTerminalDisplayUpdate();
    void trimLineHistory();
    void trimRawDataBuffer();
    void updatePerformanceStats();

    QString formatTimestamp() const;
    QString bytesToHexString(const QByteArray& data) const;

    // 智能滚屏辅助方法
    void showSmartScrollIndicator(const QString& message);
    void hideSmartScrollIndicator();
    void checkScrollPosition();

private:
    QTabWidget* m_tabWidget = nullptr;

    // 主文本显示
    QPlainTextEdit* m_mainTextEdit = nullptr;

    // 十六进制表格
    QTableView* m_hexTable = nullptr;
    ReceiveHexModel* m_hexModel = nullptr;

    // 过滤标签页
    QLineEdit* m_filterInput = nullptr;
    QTextEdit* m_filterResult = nullptr;

    // 选项
    QCheckBox* m_timestampCheck = nullptr;
    QCheckBox* m_autoScrollCheck = nullptr;
    QCheckBox* m_hexDisplayCheck = nullptr;
    QCheckBox* m_highlightCheck = nullptr;
    QPushButton* m_highlightSettingsBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;

    // 数据缓存
    QByteArray m_rawData;
    QByteArray m_utf8Buffer;
    QStringList m_lineHistory;
    QTimer* m_highlightTimer = nullptr;         ///< 高亮延迟应用定时器
    QTimer* m_terminalRefreshTimer = nullptr;   ///< 终端显示节流刷新定时器

    // 状态
    bool m_timestampEnabled = false;
    bool m_autoScrollEnabled = true;
    bool m_hexDisplayEnabled = false;
    bool m_needTimestamp = true;
    int m_maxLines = 10000;
    int m_terminalMaxLines = 1000;
    int m_maxRawDataBytes = 8 * 1024 * 1024;    ///< 原始数据缓存上限（8MB）

    // 智能滚屏
    bool m_smartScrollPaused = false;       ///< 智能滚屏暂停标志
    bool m_userScrolling = false;           ///< 用户正在滚动标志
    QLabel* m_smartScrollIndicator = nullptr; ///< 智能滚屏提示标签

    // 高亮
    bool m_highlightEnabled = true;
    QVector<HighlightRule> m_highlightRules;
    ReceiveHighlightHighlighter* m_highlighter = nullptr;

    // 性能统计
    QLabel* m_perfLabel = nullptr;
    QTimer* m_perfTimer = nullptr;
    qint64 m_perfRxBytes = 0;
    double m_perfRenderCostSumMs = 0.0;
    int m_perfRenderSamples = 0;

    // 显示模式
    ReceiveDisplayMode m_displayMode = ReceiveDisplayMode::Serial;

    // 终端模式状态
    QStringList m_terminalLines;        ///< 终端行缓冲
    int m_terminalCursorX = 0;          ///< 光标X位置
    int m_terminalCursorY = 0;          ///< 光标Y位置
    QByteArray m_ansiBuffer;            ///< ANSI转义序列缓冲
    bool m_inAnsiEscape = false;        ///< 是否在ANSI转义序列中
    QColor m_terminalFgColor;           ///< 当前前景色
    QColor m_terminalBgColor;           ///< 当前背景色
    bool m_terminalBold = false;        ///< 粗体

    // 帧模式状态
    int m_frameCounter = 0;             ///< 帧计数器
    QByteArray m_frameBuffer;           ///< 帧缓冲

    // 调试模式状态
    bool m_lastWasSent = false;         ///< 上一条是否为发送
    QByteArray m_debugRxBuffer;         ///< 接收行缓冲
    QByteArray m_debugTxBuffer;         ///< 发送行缓冲
    QDateTime m_debugRxTimestamp;       ///< 接收行起始时间戳
    QDateTime m_debugTxTimestamp;       ///< 发送行起始时间戳
};

} // namespace ComAssistant

#endif // COMASSISTANT_TABBEDRECEIVEWIDGET_H
