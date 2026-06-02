/**
 * @file MacroRecorder.cpp
 * @brief 宏录制回放功能实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "MacroRecorder.h"
#include "utils/Logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QStandardPaths>
#include <QVariant>

namespace ComAssistant {

// ============== MacroEvent ==============

QJsonObject MacroEvent::toJson() const
{
    QJsonObject obj;
    obj["type"] = static_cast<int>(type);
    obj["data"] = QString::fromLatin1(data.toBase64());
    obj["timestamp"] = timestamp;
    obj["delayMs"] = delayMs;
    obj["comment"] = comment;
    obj["isHex"] = isHex;
    return obj;
}

MacroEvent MacroEvent::fromJson(const QJsonObject& json)
{
    MacroEvent event;
    event.type = static_cast<MacroEventType>(json["type"].toInt());
    event.data = QByteArray::fromBase64(json["data"].toString().toLatin1());
    event.timestamp = json["timestamp"].toVariant().toLongLong();
    event.delayMs = json["delayMs"].toVariant().toLongLong();
    event.comment = json["comment"].toString();
    event.isHex = json["isHex"].toBool();
    return event;
}

// ============== MacroData ==============

QJsonObject MacroData::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["description"] = description;
    obj["createTime"] = createTime.toString(Qt::ISODate);
    obj["modifyTime"] = modifyTime.toString(Qt::ISODate);
    obj["version"] = version;

    QJsonArray eventsArray;
    for (const auto& event : events) {
        eventsArray.append(event.toJson());
    }
    obj["events"] = eventsArray;

    return obj;
}

MacroData MacroData::fromJson(const QJsonObject& json)
{
    MacroData macro;
    macro.name = json["name"].toString();
    macro.description = json["description"].toString();
    macro.createTime = QDateTime::fromString(json["createTime"].toString(), Qt::ISODate);
    macro.modifyTime = QDateTime::fromString(json["modifyTime"].toString(), Qt::ISODate);
    macro.version = json["version"].toInt(1);

    QJsonArray eventsArray = json["events"].toArray();
    for (const auto& eventValue : eventsArray) {
        macro.events.append(MacroEvent::fromJson(eventValue.toObject()));
    }

    return macro;
}

bool MacroData::saveToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool MacroData::loadFromFile(const QString& filePath, MacroData& macro, QString& errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = QObject::tr("无法打开文件");
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        errorMsg = error.errorString();
        return false;
    }

    macro = fromJson(doc.object());
    return true;
}

// ============== MacroRecorder ==============

MacroRecorder::MacroRecorder(QObject* parent)
    : QObject(parent)
{
}

void MacroRecorder::startRecording(const QString& macroName)
{
    if (m_recording) {
        return;
    }

    m_recording = true;
    m_startTime = QDateTime::currentDateTime();
    m_macroName = macroName.isEmpty() ?
        tr("宏_%1").arg(m_startTime.toString("yyyyMMdd_hhmmss")) : macroName;
    m_events.clear();
    m_events.squeeze();
    m_recordedPayloadBytes = 0;

    LOG_INFO(QString("Macro recording started: %1").arg(m_macroName));
    emit recordingStateChanged(true);
}

MacroData MacroRecorder::stopRecording()
{
    if (!m_recording) {
        return MacroData();
    }

    m_recording = false;

    MacroData macro;
    macro.name = m_macroName;
    macro.createTime = m_startTime;
    macro.modifyTime = QDateTime::currentDateTime();
    macro.events = m_events;

    LOG_INFO(QString("Macro recording stopped: %1, %2 events")
        .arg(m_macroName).arg(m_events.size()));

    m_events.clear();
    m_events.squeeze();
    m_recordedPayloadBytes = 0;
    emit recordingStateChanged(false);

    return macro;
}

void MacroRecorder::recordSend(const QByteArray& data, bool isHex)
{
    if (!m_recording) {
        return;
    }

    MacroEvent event;
    event.type = MacroEventType::Send;
    event.data = data;
    event.isHex = isHex;
    event.timestamp = m_startTime.msecsTo(QDateTime::currentDateTime());

    appendBoundedEvent(event);
}

void MacroRecorder::recordReceive(const QByteArray& data)
{
    if (!m_recording || !m_recordReceive) {
        return;
    }

    MacroEvent event;
    event.type = MacroEventType::Receive;
    event.data = data;
    event.timestamp = m_startTime.msecsTo(QDateTime::currentDateTime());

    appendBoundedEvent(event);
}

void MacroRecorder::addDelay(qint64 delayMs)
{
    if (!m_recording) {
        return;
    }

    MacroEvent event;
    event.type = MacroEventType::Delay;
    event.delayMs = delayMs;
    event.timestamp = m_startTime.msecsTo(QDateTime::currentDateTime());

    appendBoundedEvent(event);
}

void MacroRecorder::addComment(const QString& comment)
{
    if (!m_recording) {
        return;
    }

    MacroEvent event;
    event.type = MacroEventType::Comment;
    event.comment = comment;
    event.timestamp = m_startTime.msecsTo(QDateTime::currentDateTime());

    appendBoundedEvent(event);
}

void MacroRecorder::appendBoundedEvent(const MacroEvent& event)
{
    /*
     * 宏录制通常只录少量人工操作，但如果用户勾选“录制接收数据”并长时间
     * 跑串口流，事件数组会像接收历史一样无限增长。这里所有事件统一走
     * 有界追加，超过数量或数据字节上限时删除最早事件，只保留最近操作。
     */
    m_events.append(event);
    m_recordedPayloadBytes += eventPayloadBytes(event);
    trimRecordedEvents();

    /*
     * 单条接收数据如果已经超过字节上限，裁剪会把它直接删除。只有事件仍在
     * 录制列表中时才通知 UI，避免列表显示了一个最终不会被保存的幽灵事件。
     */
    if (!m_events.isEmpty() && m_events.last().timestamp == event.timestamp
            && m_events.last().type == event.type
            && m_events.last().data == event.data
            && m_events.last().comment == event.comment) {
        emit eventRecorded(event);
    }
}

qint64 MacroRecorder::recordedPayloadBytes() const
{
    return m_recordedPayloadBytes;
}

qint64 MacroRecorder::eventPayloadBytes(const MacroEvent& event)
{
    /*
     * 这里只统计会随着用户输入或接收数据显著增长的字段；枚举、时间戳等固定
     * 元数据按事件数量上限控制。注释按 UTF-16 QChar 估算，贴近 QString 内存。
     */
    return static_cast<qint64>(event.data.size())
        + event.comment.size() * static_cast<qint64>(sizeof(QChar));
}

void MacroRecorder::trimRecordedEvents()
{
    bool trimmed = false;
    while (m_events.size() > kMaxRecordedEvents) {
        m_recordedPayloadBytes -= eventPayloadBytes(m_events.first());
        m_events.remove(0);
        trimmed = true;
    }

    while (!m_events.isEmpty() && recordedPayloadBytes() > kMaxRecordedBytes) {
        m_recordedPayloadBytes -= eventPayloadBytes(m_events.first());
        m_events.remove(0);
        trimmed = true;
    }

    if (m_recordedPayloadBytes < 0) {
        /*
         * 理论上不会小于 0；这里作为防御性兜底，避免未来事件字段调整后出现
         * 负值并导致后续字节上限判断失真。
         */
        m_recordedPayloadBytes = 0;
    }

    if (trimmed) {
        m_events.squeeze();
    }
}

qint64 MacroRecorder::recordingDuration() const
{
    if (!m_recording) {
        return 0;
    }
    return m_startTime.msecsTo(QDateTime::currentDateTime());
}

// ============== MacroPlayer ==============

MacroPlayer::MacroPlayer(QObject* parent)
    : QObject(parent)
{
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setSingleShot(true);
    connect(m_playbackTimer, &QTimer::timeout, this, &MacroPlayer::onPlaybackTimer);
}

MacroPlayer::~MacroPlayer()
{
    stopPlayback();
}

void MacroPlayer::loadMacro(const MacroData& macro)
{
    m_macro = macro;

    // 提取发送和延时事件
    m_sendEvents.clear();
    for (const auto& event : macro.events) {
        if (event.type == MacroEventType::Send ||
            event.type == MacroEventType::Delay) {
            m_sendEvents.append(event);
        }
    }
}

bool MacroPlayer::startPlayback()
{
    if (m_sendEvents.isEmpty()) {
        LOG_WARN("No events to play");
        return false;
    }

    m_playing = true;
    m_paused = false;
    m_currentIndex = 0;
    m_currentLoop = 1;
    m_pauseRemaining = 0;

    LOG_INFO(QString("Macro playback started: %1").arg(m_macro.name));
    emit playbackStateChanged(true, false);

    scheduleNextEvent();
    return true;
}

void MacroPlayer::pausePlayback()
{
    if (!m_playing || m_paused) {
        return;
    }

    m_paused = true;
    m_pauseRemaining = m_playbackTimer->remainingTime();
    m_playbackTimer->stop();

    LOG_INFO("Macro playback paused");
    emit playbackStateChanged(true, true);
}

void MacroPlayer::resumePlayback()
{
    if (!m_playing || !m_paused) {
        return;
    }

    m_paused = false;
    if (m_pauseRemaining > 0) {
        m_playbackTimer->start(static_cast<int>(m_pauseRemaining / m_speedMultiplier));
    } else {
        scheduleNextEvent();
    }

    LOG_INFO("Macro playback resumed");
    emit playbackStateChanged(true, false);
}

void MacroPlayer::stopPlayback()
{
    if (!m_playing) {
        return;
    }

    m_playbackTimer->stop();
    m_playing = false;
    m_paused = false;
    m_currentIndex = 0;

    LOG_INFO("Macro playback stopped");
    emit playbackStateChanged(false, false);
}

void MacroPlayer::onPlaybackTimer()
{
    if (!m_playing || m_paused) {
        return;
    }

    if (m_currentIndex >= m_sendEvents.size()) {
        // 当前循环完成
        if (m_loopEnabled && (m_loopCount == 0 || m_currentLoop < m_loopCount)) {
            m_currentLoop++;
            m_currentIndex = 0;
            LOG_INFO(QString("Macro loop %1").arg(m_currentLoop));
            scheduleNextEvent();
        } else {
            // 播放完成
            m_playing = false;
            emit playbackStateChanged(false, false);
            emit playbackFinished();
            LOG_INFO("Macro playback finished");
        }
        return;
    }

    const MacroEvent& event = m_sendEvents[m_currentIndex];
    executeEvent(event);
    emit currentEventChanged(event);
    emit progressUpdated(m_currentIndex + 1, m_sendEvents.size(), m_currentLoop);

    m_currentIndex++;
    scheduleNextEvent();
}

void MacroPlayer::executeEvent(const MacroEvent& event)
{
    switch (event.type) {
    case MacroEventType::Send:
        emit sendData(event.data);
        break;

    case MacroEventType::Delay:
        // 延时在scheduleNextEvent中处理
        break;

    default:
        break;
    }
}

void MacroPlayer::scheduleNextEvent()
{
    if (m_currentIndex >= m_sendEvents.size()) {
        // 触发循环检查
        m_playbackTimer->start(1);
        return;
    }

    qint64 delay = 0;

    // 计算到下一个事件的延时
    if (m_currentIndex > 0) {
        const MacroEvent& prevEvent = m_sendEvents[m_currentIndex - 1];
        const MacroEvent& currEvent = m_sendEvents[m_currentIndex];

        if (prevEvent.type == MacroEventType::Delay) {
            delay = prevEvent.delayMs;
        } else {
            delay = currEvent.timestamp - prevEvent.timestamp;
        }
    }

    // 应用速度倍率
    delay = static_cast<qint64>(delay / m_speedMultiplier);
    if (delay < 1) delay = 1;

    m_playbackTimer->start(static_cast<int>(delay));
}

// ============== MacroManager ==============

MacroManager* MacroManager::instance()
{
    static MacroManager s_instance;
    return &s_instance;
}

MacroManager::MacroManager(QObject* parent)
    : QObject(parent)
{
    // 确保宏目录存在
    QDir dir(macrosDirectory());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString MacroManager::macrosDirectory() const
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appData + "/macros";
}

QString MacroManager::macroFilePath(const QString& name) const
{
    return macrosDirectory() + "/" + name + ".macro";
}

QStringList MacroManager::availableMacros() const
{
    QDir dir(macrosDirectory());
    QStringList filters;
    filters << "*.macro";

    QStringList macros;
    for (const auto& file : dir.entryList(filters, QDir::Files)) {
        macros << QFileInfo(file).baseName();
    }
    return macros;
}

bool MacroManager::loadMacro(const QString& name, MacroData& macro)
{
    QString errorMsg;
    return MacroData::loadFromFile(macroFilePath(name), macro, errorMsg);
}

bool MacroManager::saveMacro(const MacroData& macro)
{
    bool result = macro.saveToFile(macroFilePath(macro.name));
    if (result) {
        emit macroListChanged();
    }
    return result;
}

bool MacroManager::deleteMacro(const QString& name)
{
    QFile file(macroFilePath(name));
    bool result = file.remove();
    if (result) {
        emit macroListChanged();
    }
    return result;
}

bool MacroManager::renameMacro(const QString& oldName, const QString& newName)
{
    QFile file(macroFilePath(oldName));
    bool result = file.rename(macroFilePath(newName));
    if (result) {
        emit macroListChanged();
    }
    return result;
}

bool MacroManager::importMacro(const QString& filePath)
{
    MacroData macro;
    QString errorMsg;
    if (!MacroData::loadFromFile(filePath, macro, errorMsg)) {
        return false;
    }

    // 确保名称唯一
    QString baseName = macro.name;
    int counter = 1;
    while (QFile::exists(macroFilePath(macro.name))) {
        macro.name = QString("%1_%2").arg(baseName).arg(counter++);
    }

    return saveMacro(macro);
}

bool MacroManager::exportMacro(const QString& name, const QString& filePath)
{
    MacroData macro;
    if (!loadMacro(name, macro)) {
        return false;
    }
    return macro.saveToFile(filePath);
}

} // namespace ComAssistant
