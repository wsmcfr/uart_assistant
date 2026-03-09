/**
 * @file Logger.cpp
 * @brief 日志系统实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "Logger.h"
#include <QDateTime>
#include <QStandardPaths>
#include <spdlog/sinks/basic_file_sink.h>

namespace ComAssistant {

Logger* Logger::instance()
{
    static Logger instance;
    return &instance;
}

bool Logger::initialize(const QString& logDir, size_t maxFileSize, size_t maxFiles)
{
    if (m_initialized) {
        return true;
    }

    try {
        // 创建日志目录
        m_logDir = logDir;
        QDir dir(m_logDir);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                return false;
            }
        }

        // 生成日志文件名
        QString logFileName = QString("%1/comassistant.log")
                                  .arg(m_logDir);

        // 创建 sinks
        std::vector<spdlog::sink_ptr> sinks;

        // 文件 sink (滚动日志)
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logFileName.toStdString(),
            maxFileSize,
            maxFiles
        );
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
        sinks.push_back(fileSink);

        // 控制台 sink
        if (m_consoleEnabled) {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            sinks.push_back(consoleSink);
        }

        // 创建 logger
        m_logger = std::make_shared<spdlog::logger>("ComAssistant", sinks.begin(), sinks.end());
        m_logger->set_level(spdlog::level::info);
        m_logger->flush_on(spdlog::level::warn);  // warn 及以上级别自动刷新

        // 注册为默认 logger
        spdlog::set_default_logger(m_logger);

        m_initialized = true;

        // 记录启动日志
        info(QString("========================================"));
        info(QString("ComAssistant Logger Initialized"));
        info(QString("Log Directory: %1").arg(m_logDir));
        info(QString("========================================"));

        return true;
    }
    catch (const spdlog::spdlog_ex& ex) {
        // 日志初始化失败
        return false;
    }
}

void Logger::setLevel(Level level)
{
    m_level = level;
    if (m_logger) {
        m_logger->set_level(static_cast<spdlog::level::level_enum>(level));
    }
}

Logger::Level Logger::level() const
{
    return m_level;
}

void Logger::setConsoleEnabled(bool enabled)
{
    m_consoleEnabled = enabled;
}

void Logger::trace(const QString& message)
{
    if (m_logger) {
        m_logger->trace(message.toStdString());
    }
}

void Logger::debug(const QString& message)
{
    if (m_logger) {
        m_logger->debug(message.toStdString());
    }
}

void Logger::info(const QString& message)
{
    if (m_logger) {
        m_logger->info(message.toStdString());
    }
}

void Logger::warn(const QString& message)
{
    if (m_logger) {
        m_logger->warn(message.toStdString());
    }
}

void Logger::error(const QString& message)
{
    if (m_logger) {
        m_logger->error(message.toStdString());
    }
}

void Logger::critical(const QString& message)
{
    if (m_logger) {
        m_logger->critical(message.toStdString());
    }
}

void Logger::flush()
{
    if (m_logger) {
        m_logger->flush();
    }
}

QString Logger::logDirectory() const
{
    return m_logDir;
}

} // namespace ComAssistant
