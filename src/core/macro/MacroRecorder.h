/**
 * @file MacroRecorder.h
 * @brief 宏录制回放功能
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_MACRORECORDER_H
#define COMASSISTANT_MACRORECORDER_H

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QVector>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

namespace ComAssistant {

/**
 * @brief 宏事件类型
 */
enum class MacroEventType {
    Send,       ///< 发送数据
    Receive,    ///< 接收数据
    Delay,      ///< 延时
    Comment     ///< 注释
};

/**
 * @brief 宏事件
 */
struct MacroEvent {
    MacroEventType type = MacroEventType::Send;
    QByteArray data;
    qint64 timestamp = 0;       ///< 相对于录制开始的时间(ms)
    qint64 delayMs = 0;         ///< 延时时间
    QString comment;            ///< 注释
    bool isHex = false;         ///< 是否为HEX格式

    QJsonObject toJson() const;
    static MacroEvent fromJson(const QJsonObject& json);
};

/**
 * @brief 宏数据
 */
struct MacroData {
    QString name;
    QString description;
    QDateTime createTime;
    QDateTime modifyTime;
    QVector<MacroEvent> events;
    int version = 1;

    QJsonObject toJson() const;
    static MacroData fromJson(const QJsonObject& json);

    bool saveToFile(const QString& filePath) const;
    static bool loadFromFile(const QString& filePath, MacroData& macro, QString& errorMsg);
};

/**
 * @brief 宏录制器
 */
class MacroRecorder : public QObject {
    Q_OBJECT

public:
    explicit MacroRecorder(QObject* parent = nullptr);
    ~MacroRecorder() override = default;

    /**
     * @brief 开始录制
     */
    void startRecording(const QString& macroName = QString());

    /**
     * @brief 停止录制
     */
    MacroData stopRecording();

    /**
     * @brief 是否正在录制
     */
    bool isRecording() const { return m_recording; }

    /**
     * @brief 记录发送事件
     */
    void recordSend(const QByteArray& data, bool isHex = false);

    /**
     * @brief 记录接收事件
     */
    void recordReceive(const QByteArray& data);

    /**
     * @brief 添加延时事件
     */
    void addDelay(qint64 delayMs);

    /**
     * @brief 添加注释
     */
    void addComment(const QString& comment);

    /**
     * @brief 获取当前录制的事件数
     */
    int eventCount() const { return m_events.size(); }

    /**
     * @brief 获取录制时长
     */
    qint64 recordingDuration() const;

    /**
     * @brief 设置是否录制接收数据
     */
    void setRecordReceive(bool enabled) { m_recordReceive = enabled; }

signals:
    /**
     * @brief 事件被记录
     */
    void eventRecorded(const MacroEvent& event);

    /**
     * @brief 录制状态改变
     */
    void recordingStateChanged(bool recording);

private:
    bool m_recording = false;
    bool m_recordReceive = false;
    QDateTime m_startTime;
    QString m_macroName;
    QVector<MacroEvent> m_events;
};

/**
 * @brief 宏播放器
 */
class MacroPlayer : public QObject {
    Q_OBJECT

public:
    explicit MacroPlayer(QObject* parent = nullptr);
    ~MacroPlayer() override;

    /**
     * @brief 加载宏
     */
    void loadMacro(const MacroData& macro);

    /**
     * @brief 开始播放
     */
    bool startPlayback();

    /**
     * @brief 暂停播放
     */
    void pausePlayback();

    /**
     * @brief 继续播放
     */
    void resumePlayback();

    /**
     * @brief 停止播放
     */
    void stopPlayback();

    /**
     * @brief 是否正在播放
     */
    bool isPlaying() const { return m_playing; }

    /**
     * @brief 是否暂停
     */
    bool isPaused() const { return m_paused; }

    /**
     * @brief 设置循环播放
     */
    void setLoopEnabled(bool enabled) { m_loopEnabled = enabled; }
    bool isLoopEnabled() const { return m_loopEnabled; }

    /**
     * @brief 设置循环次数 (0=无限)
     */
    void setLoopCount(int count) { m_loopCount = count; }
    int loopCount() const { return m_loopCount; }

    /**
     * @brief 设置播放速度倍率
     */
    void setSpeedMultiplier(double multiplier) { m_speedMultiplier = multiplier; }
    double speedMultiplier() const { return m_speedMultiplier; }

    /**
     * @brief 设置是否包含接收事件
     */
    void setIncludeReceive(bool include) { m_includeReceive = include; }

    /**
     * @brief 获取当前播放进度
     */
    int currentEventIndex() const { return m_currentIndex; }
    int totalEvents() const { return m_sendEvents.size(); }

    /**
     * @brief 获取当前循环次数
     */
    int currentLoop() const { return m_currentLoop; }

signals:
    /**
     * @brief 请求发送数据
     */
    void sendData(const QByteArray& data);

    /**
     * @brief 播放进度更新
     */
    void progressUpdated(int currentEvent, int totalEvents, int currentLoop);

    /**
     * @brief 播放完成
     */
    void playbackFinished();

    /**
     * @brief 播放状态改变
     */
    void playbackStateChanged(bool playing, bool paused);

    /**
     * @brief 当前事件
     */
    void currentEventChanged(const MacroEvent& event);

private slots:
    void onPlaybackTimer();

private:
    void executeEvent(const MacroEvent& event);
    void scheduleNextEvent();

private:
    MacroData m_macro;
    QVector<MacroEvent> m_sendEvents;  ///< 仅包含发送和延时事件

    bool m_playing = false;
    bool m_paused = false;
    bool m_loopEnabled = false;
    int m_loopCount = 1;
    int m_currentLoop = 0;
    double m_speedMultiplier = 1.0;
    bool m_includeReceive = false;

    int m_currentIndex = 0;
    QTimer* m_playbackTimer = nullptr;
    qint64 m_pauseRemaining = 0;
};

/**
 * @brief 宏管理器
 */
class MacroManager : public QObject {
    Q_OBJECT

public:
    static MacroManager* instance();

    /**
     * @brief 获取宏存储目录
     */
    QString macrosDirectory() const;

    /**
     * @brief 获取所有可用宏
     */
    QStringList availableMacros() const;

    /**
     * @brief 加载宏
     */
    bool loadMacro(const QString& name, MacroData& macro);

    /**
     * @brief 保存宏
     */
    bool saveMacro(const MacroData& macro);

    /**
     * @brief 删除宏
     */
    bool deleteMacro(const QString& name);

    /**
     * @brief 重命名宏
     */
    bool renameMacro(const QString& oldName, const QString& newName);

    /**
     * @brief 导入宏
     */
    bool importMacro(const QString& filePath);

    /**
     * @brief 导出宏
     */
    bool exportMacro(const QString& name, const QString& filePath);

signals:
    void macroListChanged();

private:
    MacroManager(QObject* parent = nullptr);
    QString macroFilePath(const QString& name) const;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MACRORECORDER_H
