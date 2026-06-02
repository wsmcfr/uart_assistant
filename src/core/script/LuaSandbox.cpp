/**
 * @file LuaSandbox.cpp
 * @brief Lua 安全沙箱执行器实现
 */

#include "LuaSandbox.h"

#include "../utils/ChecksumUtils.h"
#include "../utils/ConversionUtils.h"

#include <QElapsedTimer>
#include <QVariant>

#include <cstdlib>

// Lua 头文件是 C API，需要使用 extern "C" 避免 C++ 名字修饰。
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ComAssistant {
namespace {

const char* kSandboxContextKey = "ComAssistant.LuaSandbox.Context";
const int kHookInstructionInterval = 1000;

/**
 * @brief 单次 Lua 执行的内部上下文。
 *
 * Lua C 回调、allocator 和 hook 都只能通过 lua_State 间接找回状态，
 * 因此把本次执行的配置、结果和资源统计集中放在该结构中。
 */
struct SandboxContext
{
    LuaSandboxOptions options;      ///< 本次执行选项
    LuaSandboxResult* result = nullptr; ///< 写回给调用方的执行结果
    QElapsedTimer timer;            ///< 超时检测使用的计时器
    qint64 currentBytes = 0;        ///< allocator 统计的当前 Lua 内存
    qint64 maxBytes = 0;            ///< allocator 允许的最大 Lua 内存
    bool memoryExceeded = false;    ///< allocator 是否已经拒绝过分配
    bool outputTruncated = false;   ///< print 输出是否已经追加过截断提示
};

/**
 * @brief 从 Lua registry 取回当前沙箱上下文。
 * @param L Lua 状态。
 * @return 当前执行上下文；如果 registry 尚未设置则返回 nullptr。
 */
SandboxContext* contextFromState(lua_State* L)
{
    lua_pushlightuserdata(L, const_cast<char*>(kSandboxContextKey));
    lua_gettable(L, LUA_REGISTRYINDEX);
    SandboxContext* context = static_cast<SandboxContext*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return context;
}

/**
 * @brief 把当前沙箱上下文写入 Lua registry。
 * @param L Lua 状态。
 * @param context 当前执行上下文。
 */
void storeContext(lua_State* L, SandboxContext* context)
{
    lua_pushlightuserdata(L, const_cast<char*>(kSandboxContextKey));
    lua_pushlightuserdata(L, context);
    lua_settable(L, LUA_REGISTRYINDEX);
}

/**
 * @brief Lua 自定义内存分配器。
 *
 * Lua 会把旧块大小和新块大小传入 allocator。这里用差值维护当前占用，
 * 超过预算时返回 nullptr，让 Lua 自己产生内存错误。
 */
void* sandboxAlloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    SandboxContext* context = static_cast<SandboxContext*>(ud);

    if (nsize == 0) {
        context->currentBytes -= static_cast<qint64>(osize);
        std::free(ptr);
        return nullptr;
    }

    const qint64 nextBytes =
        context->currentBytes - static_cast<qint64>(osize) + static_cast<qint64>(nsize);
    if (context->maxBytes > 0 && nextBytes > context->maxBytes) {
        context->memoryExceeded = true;
        if (context->result) {
            context->result->memoryExceeded = true;
        }
        return nullptr;
    }

    void* newPtr = std::realloc(ptr, nsize);
    if (newPtr) {
        context->currentBytes = nextBytes;
    }
    return newPtr;
}

/**
 * @brief 创建 Lua state。
 * @param context 当前执行上下文，会作为 allocator userdata 传给 Lua。
 * @return 新 Lua state，创建失败时返回 nullptr。
 *
 * vcpkg 当前提供的 Lua 5.5 在 lua_newstate 中新增 seed 参数，而 Lua 5.4
 * 仍是两个参数。这里集中封装版本差异，避免沙箱核心散落条件编译。
 */
lua_State* createLuaState(SandboxContext* context)
{
#if defined(LUA_VERSION_NUM) && LUA_VERSION_NUM >= 505
    return lua_newstate(sandboxAlloc, context, 0);
#else
    return lua_newstate(sandboxAlloc, context);
#endif
}

/**
 * @brief 将 Lua 栈上的值转换成 print 使用的可读文本。
 * @param L Lua 状态。
 * @param index 栈索引。
 * @return 转换后的文本。
 */
QString valueToString(lua_State* L, int index)
{
    switch (lua_type(L, index)) {
    case LUA_TSTRING:
        return QString::fromUtf8(lua_tostring(L, index));
    case LUA_TNUMBER:
        return QString::fromUtf8(luaL_tolstring(L, index, nullptr)).trimmed();
    case LUA_TBOOLEAN:
        return lua_toboolean(L, index) ? QStringLiteral("true") : QStringLiteral("false");
    case LUA_TNIL:
        return QStringLiteral("nil");
    default:
        luaL_tolstring(L, index, nullptr);
        {
            const QString text = QString::fromUtf8(lua_tostring(L, -1));
            lua_pop(L, 1);
            return text;
        }
    }
}

/**
 * @brief 沙箱版 print。
 *
 * 输出不会直接写 UI 或 stdout，而是进入 LuaSandboxResult。超过输出行数限制后
 * 只追加一条截断提示，避免恶意脚本或错误循环把 UI 日志撑爆。
 */
int sandboxPrint(lua_State* L)
{
    SandboxContext* context = contextFromState(L);
    if (!context || !context->result) {
        return 0;
    }

    if (context->options.maxOutputLines >= 0
        && context->result->outputLines.size() >= context->options.maxOutputLines) {
        if (!context->outputTruncated) {
            context->result->outputLines.append(
                QStringLiteral("[truncated] Lua output exceeded %1 lines")
                    .arg(context->options.maxOutputLines));
            context->outputTruncated = true;
        }
        return 0;
    }

    const int count = lua_gettop(L);
    QStringList parts;
    for (int i = 1; i <= count; ++i) {
        parts.append(valueToString(L, i));
        if (lua_type(L, -1) == LUA_TSTRING && lua_gettop(L) > count) {
            lua_pop(L, 1);
        }
    }

    context->result->outputLines.append(parts.join(QStringLiteral("\t")));
    return 0;
}

/**
 * @brief 沙箱版 hexToBytes。
 * @param L Lua 状态。
 * @return Lua 返回值数量。
 */
int sandboxHexToBytes(lua_State* L)
{
    const char* hex = luaL_checkstring(L, 1);
    const QByteArray bytes = ConversionUtils::hexStringToBytes(QString::fromUtf8(hex));
    lua_pushlstring(L, bytes.constData(), static_cast<size_t>(bytes.size()));
    return 1;
}

/**
 * @brief 沙箱版 bytesToHex。
 * @param L Lua 状态。
 * @return Lua 返回值数量。
 */
int sandboxBytesToHex(lua_State* L)
{
    size_t length = 0;
    const char* data = luaL_checklstring(L, 1, &length);
    const QByteArray bytes(data, static_cast<int>(length));
    const QString hex = ConversionUtils::bytesToHexString(bytes, QStringLiteral(" "));
    lua_pushstring(L, hex.toUtf8().constData());
    return 1;
}

/**
 * @brief 沙箱版 crc16，使用项目既有 Modbus CRC16 算法。
 * @param L Lua 状态。
 * @return Lua 返回值数量。
 */
int sandboxCrc16(lua_State* L)
{
    size_t length = 0;
    const char* data = luaL_checklstring(L, 1, &length);
    const QByteArray bytes(data, static_cast<int>(length));
    lua_pushinteger(L, ChecksumUtils::crc16Modbus(bytes));
    return 1;
}

/**
 * @brief 沙箱版 crc32，使用项目既有 CRC32 算法。
 * @param L Lua 状态。
 * @return Lua 返回值数量。
 */
int sandboxCrc32(lua_State* L)
{
    size_t length = 0;
    const char* data = luaL_checklstring(L, 1, &length);
    const QByteArray bytes(data, static_cast<int>(length));
    lua_pushinteger(L, ChecksumUtils::crc32(bytes));
    return 1;
}

/**
 * @brief Lua hook，用于周期性检查超时。
 *
 * Lua 没有安全的抢占式中断机制；hook 是第一版同步执行中最可控的边界。
 * 触发超时后通过 luaL_error 让 lua_pcall 返回错误。
 */
void sandboxHook(lua_State* L, lua_Debug*)
{
    SandboxContext* context = contextFromState(L);
    if (!context || !context->result) {
        return;
    }

    if (context->options.timeoutMs > 0
        && context->timer.elapsed() > context->options.timeoutMs) {
        context->result->timedOut = true;
        luaL_error(L, "Lua sandbox timeout");
    }
}

/**
 * @brief 从全局环境删除一个名字。
 * @param L Lua 状态。
 * @param name 要置空的全局变量名。
 */
void removeGlobal(lua_State* L, const char* name)
{
    lua_pushnil(L);
    lua_setglobal(L, name);
}

/**
 * @brief 打开一个 Lua 标准库并弹出返回表。
 * @param L Lua 状态。
 * @param name 库名。
 * @param openFunction Lua 标准库打开函数。
 */
void openLibrary(lua_State* L, const char* name, lua_CFunction openFunction)
{
    luaL_requiref(L, name, openFunction, 1);
    lua_pop(L, 1);
}

/**
 * @brief 打开白名单标准库并移除危险全局。
 * @param L Lua 状态。
 */
void openSafeLibraries(lua_State* L)
{
    openLibrary(L, "_G", luaopen_base);
    openLibrary(L, LUA_STRLIBNAME, luaopen_string);
    openLibrary(L, LUA_TABLIBNAME, luaopen_table);
    openLibrary(L, LUA_MATHLIBNAME, luaopen_math);
#ifdef LUA_UTF8LIBNAME
    openLibrary(L, LUA_UTF8LIBNAME, luaopen_utf8);
#endif

    /*
     * base 库里有一部分能力会破坏沙箱边界或让脚本二次动态加载。
     * 第一版宁可偏保守，后续确有需求再逐项放开。
     */
    removeGlobal(L, "collectgarbage");
    removeGlobal(L, "dofile");
    removeGlobal(L, "loadfile");
    removeGlobal(L, "load");
    removeGlobal(L, "require");
    removeGlobal(L, "rawequal");
    removeGlobal(L, "rawget");
    removeGlobal(L, "rawset");
    removeGlobal(L, "setmetatable");
    removeGlobal(L, "os");
    removeGlobal(L, "io");
    removeGlobal(L, "package");
    removeGlobal(L, "debug");
}

/**
 * @brief 注册沙箱允许的全局函数。
 * @param L Lua 状态。
 */
void registerSafeFunctions(lua_State* L)
{
    lua_pushcfunction(L, sandboxPrint);
    lua_setglobal(L, "print");

    lua_pushcfunction(L, sandboxHexToBytes);
    lua_setglobal(L, "hexToBytes");

    lua_pushcfunction(L, sandboxBytesToHex);
    lua_setglobal(L, "bytesToHex");

    lua_pushcfunction(L, sandboxCrc16);
    lua_setglobal(L, "crc16");

    lua_pushcfunction(L, sandboxCrc32);
    lua_setglobal(L, "crc32");
}

} // namespace

LuaSandboxResult LuaSandbox::execute(const QString& script,
                                     const LuaSandboxOptions& options)
{
    LuaSandboxResult result;
    SandboxContext context;
    context.options = options;
    context.result = &result;
    context.maxBytes = options.memoryLimitKb > 0
        ? static_cast<qint64>(options.memoryLimitKb) * 1024
        : 0;
    context.timer.start();

    lua_State* L = createLuaState(&context);
    if (!L) {
        result.memoryExceeded = context.memoryExceeded;
        result.errorMessage = result.memoryExceeded
            ? QStringLiteral("Lua sandbox memory limit exceeded")
            : QStringLiteral("Failed to create Lua state");
        result.elapsedMs = context.timer.elapsed();
        return result;
    }

    storeContext(L, &context);
    openSafeLibraries(L);
    registerSafeFunctions(L);
    lua_sethook(L, sandboxHook, LUA_MASKCOUNT, kHookInstructionInterval);

    const QByteArray scriptData = script.toUtf8();
    int status = luaL_loadbuffer(L,
                                 scriptData.constData(),
                                 static_cast<size_t>(scriptData.size()),
                                 "sandbox");
    if (status == LUA_OK) {
        status = lua_pcall(L, 0, 0, 0);
    }

    if (status == LUA_OK) {
        result.success = true;
    } else {
        const char* error = lua_tostring(L, -1);
        result.errorMessage = error ? QString::fromUtf8(error) : QStringLiteral("Lua error");
        lua_pop(L, 1);
    }

    if (context.memoryExceeded) {
        result.memoryExceeded = true;
        if (result.errorMessage.isEmpty()
            || !result.errorMessage.contains(QStringLiteral("memory"), Qt::CaseInsensitive)) {
            result.errorMessage = QStringLiteral("Lua sandbox memory limit exceeded");
        }
    }

    if (result.timedOut
        && !result.errorMessage.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)) {
        result.errorMessage = QStringLiteral("Lua sandbox timeout");
    }

    result.elapsedMs = context.timer.elapsed();
    lua_close(L);
    return result;
}

} // namespace ComAssistant
