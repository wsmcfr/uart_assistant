/**
 * @file CrashHandler.h
 * @brief 崩溃处理器 - 捕获程序崩溃并保存关键数据
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_CRASHHANDLER_H
#define COMASSISTANT_CRASHHANDLER_H

#include <QString>
#include <QStringList>
#include <functional>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

namespace ComAssistant {

/**
 * @brief 崩溃处理器 - 单例模式
 */
class CrashHandler {
public:
    /**
     * @brief 崩溃转储类型
     */
    enum class DumpType {
        Mini,       ///< 最小转储（仅调用栈）
        Normal,     ///< 普通转储（含线程信息）
        Full        ///< 完整转储（含堆内存）
    };

    /**
     * @brief 崩溃回调函数类型
     */
    using CrashCallback = std::function<void()>;

    /**
     * @brief 获取单例实例
     */
    static CrashHandler* instance();

    /**
     * @brief 初始化崩溃处理器
     * @note 应在 main 函数开始时调用
     */
    void initialize();

    /**
     * @brief 设置崩溃转储存放目录
     */
    void setDumpPath(const QString& path);

    /**
     * @brief 获取崩溃转储目录
     */
    QString dumpPath() const;

    /**
     * @brief 设置转储类型
     */
    void setDumpType(DumpType type);

    /**
     * @brief 获取转储类型
     */
    DumpType dumpType() const;

    /**
     * @brief 设置最多保留的转储文件数
     */
    void setMaxDumpFiles(int count);

    /**
     * @brief 获取最大转储文件数
     */
    int maxDumpFiles() const;

    /**
     * @brief 注册崩溃回调
     * @param callback 崩溃时调用的回调函数
     * @note 回调函数应尽量简单，避免分配内存
     */
    void registerCallback(CrashCallback callback);

    /**
     * @brief 清除所有回调
     */
    void clearCallbacks();

    /**
     * @brief 生成崩溃报告
     */
    QString generateCrashReport() const;

    /**
     * @brief 列出现有的崩溃转储文件
     */
    QStringList listDumpFiles() const;

    /**
     * @brief 清理旧的转储文件
     */
    void cleanOldDumps();

private:
    CrashHandler();
    ~CrashHandler() = default;
    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

    void executeCallbacks();

#ifdef _WIN32
    static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
    static void createMiniDump(EXCEPTION_POINTERS* exceptionInfo);
    static MINIDUMP_TYPE getMiniDumpType();
#else
    static void posixSignalHandler(int sig, siginfo_t* info, void* context);
#endif

    QString m_dumpPath;
    DumpType m_dumpType = DumpType::Normal;
    int m_maxDumpFiles = 10;
    std::vector<CrashCallback> m_callbacks;
    bool m_initialized = false;

    static CrashHandler* s_instance;
};

} // namespace ComAssistant

#endif // COMASSISTANT_CRASHHANDLER_H
