/**
 * @file CrashHandler.cpp
 * @brief 崩溃处理器实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "CrashHandler.h"
#include "Logger.h"
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>
#include <QSysInfo>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace ComAssistant {

CrashHandler* CrashHandler::s_instance = nullptr;

CrashHandler* CrashHandler::instance()
{
    static CrashHandler instance;
    s_instance = &instance;
    return &instance;
}

CrashHandler::CrashHandler()
{
}

void CrashHandler::initialize()
{
    if (m_initialized) {
        return;
    }

    // 确保转储目录存在
    if (m_dumpPath.isEmpty()) {
        m_dumpPath = QCoreApplication::applicationDirPath() + "/crashes";
    }

    QDir dir(m_dumpPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

#ifdef _WIN32
    // Windows: 设置未处理异常过滤器
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#else
    // POSIX: 设置信号处理器
    struct sigaction sa;
    sa.sa_sigaction = posixSignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);  // 段错误
    sigaction(SIGABRT, &sa, nullptr);  // abort
    sigaction(SIGFPE, &sa, nullptr);   // 浮点异常
    sigaction(SIGBUS, &sa, nullptr);   // 总线错误
    sigaction(SIGILL, &sa, nullptr);   // 非法指令
#endif

    m_initialized = true;
    LOG_INFO("CrashHandler initialized");
}

void CrashHandler::setDumpPath(const QString& path)
{
    m_dumpPath = path;
    QDir dir(m_dumpPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

QString CrashHandler::dumpPath() const
{
    return m_dumpPath;
}

void CrashHandler::setDumpType(DumpType type)
{
    m_dumpType = type;
}

CrashHandler::DumpType CrashHandler::dumpType() const
{
    return m_dumpType;
}

void CrashHandler::setMaxDumpFiles(int count)
{
    m_maxDumpFiles = count;
}

int CrashHandler::maxDumpFiles() const
{
    return m_maxDumpFiles;
}

void CrashHandler::registerCallback(CrashCallback callback)
{
    m_callbacks.push_back(std::move(callback));
}

void CrashHandler::clearCallbacks()
{
    m_callbacks.clear();
}

void CrashHandler::executeCallbacks()
{
    for (const auto& callback : m_callbacks) {
        try {
            callback();
        }
        catch (...) {
            // 忽略回调中的异常
        }
    }
}

QString CrashHandler::generateCrashReport() const
{
    QString report;
    report += "=== ComAssistant Crash Report ===\n";
    report += QString("Time: %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    report += QString("OS: %1 %2\n").arg(QSysInfo::productType(), QSysInfo::productVersion());
    report += QString("Architecture: %1\n").arg(QSysInfo::currentCpuArchitecture());
    report += QString("Kernel: %1\n").arg(QSysInfo::kernelVersion());
    report += "================================\n";
    return report;
}

QStringList CrashHandler::listDumpFiles() const
{
    QDir dir(m_dumpPath);
    return dir.entryList(QStringList() << "*.dmp" << "*.txt", QDir::Files, QDir::Time);
}

void CrashHandler::cleanOldDumps()
{
    QDir dir(m_dumpPath);
    QStringList files = dir.entryList(QStringList() << "*.dmp", QDir::Files, QDir::Time);

    while (files.size() > m_maxDumpFiles) {
        QString oldestFile = files.takeLast();
        QFile::remove(dir.filePath(oldestFile));
        LOG_INFO(QString("Removed old dump file: %1").arg(oldestFile));
    }
}

#ifdef _WIN32

LONG WINAPI CrashHandler::unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    if (s_instance) {
        // 执行回调
        s_instance->executeCallbacks();

        // 创建转储文件
        createMiniDump(exceptionInfo);

        // 生成崩溃报告
        QString report = s_instance->generateCrashReport();

        // 保存报告到文件
        QString reportPath = s_instance->m_dumpPath + "/" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_report.txt";
        QFile reportFile(reportPath);
        if (reportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            reportFile.write(report.toUtf8());
            reportFile.close();
        }

        // 清理旧转储
        s_instance->cleanOldDumps();
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void CrashHandler::createMiniDump(EXCEPTION_POINTERS* exceptionInfo)
{
    if (!s_instance) {
        return;
    }

    // 生成转储文件名
    QString dumpFileName = s_instance->m_dumpPath + "/" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".dmp";

    // 创建转储文件
    HANDLE hFile = CreateFileW(
        reinterpret_cast<LPCWSTR>(dumpFileName.utf16()),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    // 设置异常信息
    MINIDUMP_EXCEPTION_INFORMATION exceptionParam;
    exceptionParam.ThreadId = GetCurrentThreadId();
    exceptionParam.ExceptionPointers = exceptionInfo;
    exceptionParam.ClientPointers = FALSE;

    // 写入转储
    MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        getMiniDumpType(),
        &exceptionParam,
        nullptr,
        nullptr
    );

    CloseHandle(hFile);
}

MINIDUMP_TYPE CrashHandler::getMiniDumpType()
{
    if (!s_instance) {
        return MiniDumpNormal;
    }

    switch (s_instance->m_dumpType) {
        case DumpType::Mini:
            return MiniDumpNormal;
        case DumpType::Normal:
            return static_cast<MINIDUMP_TYPE>(
                MiniDumpWithDataSegs |
                MiniDumpWithHandleData |
                MiniDumpWithThreadInfo
            );
        case DumpType::Full:
            return static_cast<MINIDUMP_TYPE>(
                MiniDumpWithFullMemory |
                MiniDumpWithHandleData |
                MiniDumpWithThreadInfo |
                MiniDumpWithProcessThreadData
            );
        default:
            return MiniDumpNormal;
    }
}

#else // POSIX

void CrashHandler::posixSignalHandler(int sig, siginfo_t* info, void* context)
{
    if (s_instance) {
        // 执行回调
        s_instance->executeCallbacks();

        // 生成崩溃报告
        QString report = s_instance->generateCrashReport();
        report += QString("Signal: %1\n").arg(sig);

        // 保存报告
        QString reportPath = s_instance->m_dumpPath + "/" +
            QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_report.txt";
        QFile reportFile(reportPath);
        if (reportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            reportFile.write(report.toUtf8());
            reportFile.close();
        }
    }

    // 恢复默认处理并重新发送信号
    signal(sig, SIG_DFL);
    raise(sig);
}

#endif

} // namespace ComAssistant
