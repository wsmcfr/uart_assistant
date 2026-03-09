/**
 * @file Logger.h
 * @brief 日志系统 - 基于 spdlog 封装
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_LOGGER_H
#define COMASSISTANT_LOGGER_H

#include <QString>
#include <QDir>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace ComAssistant {

/**
 * @brief 日志管理器 - 单例模式
 */
class Logger {
public:
    /**
     * @brief 日志级别
     */
    enum class Level {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Critical = 5,
        Off = 6
    };

    /**
     * @brief 获取单例实例
     */
    static Logger* instance();

    /**
     * @brief 初始化日志系统
     * @param logDir 日志目录路径
     * @param maxFileSize 单个日志文件最大大小（字节）
     * @param maxFiles 最大日志文件数量
     * @return 是否初始化成功
     */
    bool initialize(const QString& logDir,
                    size_t maxFileSize = 5 * 1024 * 1024,  // 5MB
                    size_t maxFiles = 3);

    /**
     * @brief 设置日志级别
     */
    void setLevel(Level level);

    /**
     * @brief 获取当前日志级别
     */
    Level level() const;

    /**
     * @brief 启用/禁用控制台输出
     */
    void setConsoleEnabled(bool enabled);

    /**
     * @brief 记录跟踪日志
     */
    void trace(const QString& message);

    /**
     * @brief 记录调试日志
     */
    void debug(const QString& message);

    /**
     * @brief 记录信息日志
     */
    void info(const QString& message);

    /**
     * @brief 记录警告日志
     */
    void warn(const QString& message);

    /**
     * @brief 记录错误日志
     */
    void error(const QString& message);

    /**
     * @brief 记录严重错误日志
     */
    void critical(const QString& message);

    /**
     * @brief 刷新日志缓冲区
     */
    void flush();

    /**
     * @brief 获取日志目录路径
     */
    QString logDirectory() const;

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::shared_ptr<spdlog::logger> m_logger;
    QString m_logDir;
    Level m_level = Level::Info;
    bool m_consoleEnabled = true;
    bool m_initialized = false;
};

// 便捷宏定义
#define LOG_TRACE(msg) ComAssistant::Logger::instance()->trace(msg)
#define LOG_DEBUG(msg) ComAssistant::Logger::instance()->debug(msg)
#define LOG_INFO(msg)  ComAssistant::Logger::instance()->info(msg)
#define LOG_WARN(msg)  ComAssistant::Logger::instance()->warn(msg)
#define LOG_ERROR(msg) ComAssistant::Logger::instance()->error(msg)
#define LOG_CRITICAL(msg) ComAssistant::Logger::instance()->critical(msg)

} // namespace ComAssistant

#endif // COMASSISTANT_LOGGER_H
