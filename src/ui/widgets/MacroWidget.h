/**
 * @file MacroWidget.h
 * @brief 宏录制回放界面组件
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_MACROWIDGET_H
#define COMASSISTANT_MACROWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>
#include <QTimer>
#include <QEvent>

namespace ComAssistant {

class MacroRecorder;
class MacroPlayer;
struct MacroData;
struct MacroEvent;

/**
 * @brief 宏录制回放界面组件
 */
class MacroWidget : public QWidget {
    Q_OBJECT

public:
    explicit MacroWidget(QWidget* parent = nullptr);
    ~MacroWidget() override;

    /**
     * @brief 获取录制器
     */
    MacroRecorder* recorder() const { return m_recorder; }

    /**
     * @brief 获取播放器
     */
    MacroPlayer* player() const { return m_player; }

    /**
     * @brief 记录发送事件（外部调用）
     */
    void recordSendEvent(const QByteArray& data, bool isHex = false);

    /**
     * @brief 记录接收事件（外部调用）
     */
    void recordReceiveEvent(const QByteArray& data);

signals:
    /**
     * @brief 请求发送数据
     */
    void sendData(const QByteArray& data);

private slots:
    void onRecordClicked();
    void onStopRecordClicked();
    void onPlayClicked();
    void onPauseClicked();
    void onStopPlayClicked();

    void onMacroSelected(int index);
    void onDeleteClicked();
    void onImportClicked();
    void onExportClicked();
    void onRefreshList();

    void onRecordingStateChanged(bool recording);
    void onEventRecorded(const MacroEvent& event);
    void onPlaybackStateChanged(bool playing, bool paused);
    void onProgressUpdated(int current, int total, int loop);
    void onPlaybackFinished();
    void onCurrentEventChanged(const MacroEvent& event);

    void updateRecordingTime();

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void loadMacroList();
    void updateButtonStates();
    QString formatEvent(const MacroEvent& event) const;
    void retranslateUi();

private:
    // 录制控件
    QGroupBox* m_recordGroup = nullptr;
    QPushButton* m_recordBtn = nullptr;
    QPushButton* m_stopRecordBtn = nullptr;
    QCheckBox* m_recordReceiveCheck = nullptr;
    QLabel* m_recordTimeLabel = nullptr;
    QLabel* m_eventCountLabel = nullptr;
    QTimer* m_recordTimer = nullptr;

    // 宏列表
    QGroupBox* m_macroListGroup = nullptr;
    QComboBox* m_macroCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;

    // 播放控件
    QGroupBox* m_playGroup = nullptr;
    QPushButton* m_playBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_stopPlayBtn = nullptr;
    QCheckBox* m_loopCheck = nullptr;
    QSpinBox* m_loopCountSpin = nullptr;
    QDoubleSpinBox* m_speedSpin = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_loopCountLabel = nullptr;
    QLabel* m_speedLabel = nullptr;

    // 事件列表
    QListWidget* m_eventList = nullptr;

    // 核心对象
    MacroRecorder* m_recorder = nullptr;
    MacroPlayer* m_player = nullptr;

    // 当前加载的宏
    MacroData* m_currentMacro = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MACROWIDGET_H
