/**
 * @file LuaEngine.h
 * @brief Lua脚本引擎
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_LUAENGINE_H
#define COMASSISTANT_LUAENGINE_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QStringList>
#include <functional>

// Lua头文件
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ComAssistant {

/**
 * @brief Lua脚本引擎
 * 提供Lua脚本执行和API绑定功能
 */
class LuaEngine : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static LuaEngine* instance();

    /**
     * @brief 初始化引擎
     * @return 是否成功
     */
    bool initialize();

    /**
     * @brief 关闭引擎
     */
    void shutdown();

    /**
     * @brief 检查引擎是否已初始化
     */
    bool isInitialized() const { return m_lua != nullptr; }

    /**
     * @brief 执行Lua脚本字符串
     * @param script Lua脚本代码
     * @return 是否执行成功
     */
    bool executeScript(const QString& script);

    /**
     * @brief 执行Lua脚本文件
     * @param filePath 文件路径
     * @return 是否执行成功
     */
    bool executeFile(const QString& filePath);

    /**
     * @brief 获取最后的错误信息
     */
    QString lastError() const { return m_lastError; }

    /**
     * @brief 设置全局变量
     */
    void setGlobalString(const QString& name, const QString& value);
    void setGlobalNumber(const QString& name, double value);
    void setGlobalBoolean(const QString& name, bool value);

    /**
     * @brief 获取全局变量
     */
    QString getGlobalString(const QString& name);
    double getGlobalNumber(const QString& name);
    bool getGlobalBoolean(const QString& name);

    /**
     * @brief 注册C函数到Lua
     * @param name 函数名
     * @param func 函数指针
     */
    void registerFunction(const QString& name, lua_CFunction func);

    /**
     * @brief 注册模块到Lua
     * @param moduleName 模块名
     * @param functions 函数表
     */
    void registerModule(const QString& moduleName, const luaL_Reg* functions);

    /**
     * @brief 获取Lua状态
     */
    lua_State* luaState() { return m_lua; }

    // ==================== 串口API回调 ====================
    using SendCallback = std::function<void(const QByteArray&)>;
    using ReceiveCallback = std::function<QByteArray(int)>;
    using IsOpenCallback = std::function<bool()>;

    void setSendCallback(SendCallback callback) { m_sendCallback = callback; }
    void setReceiveCallback(ReceiveCallback callback) { m_receiveCallback = callback; }
    void setIsOpenCallback(IsOpenCallback callback) { m_isOpenCallback = callback; }

    // 供Lua API使用
    void doSend(const QByteArray& data);
    QByteArray doReceive(int timeout);
    bool doIsOpen();

signals:
    /**
     * @brief 脚本输出信号
     */
    void scriptOutput(const QString& text);

    /**
     * @brief 脚本错误信号
     */
    void scriptError(const QString& error);

    /**
     * @brief 脚本执行完成信号
     */
    void scriptFinished(bool success);

private:
    explicit LuaEngine(QObject* parent = nullptr);
    ~LuaEngine() override;

    // 禁止复制
    LuaEngine(const LuaEngine&) = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

    // 注册内置API
    void registerBuiltinApis();

    // 静态API函数
    static int lua_print(lua_State* L);
    static int lua_sleep(lua_State* L);
    static int lua_send(lua_State* L);
    static int lua_sendHex(lua_State* L);
    static int lua_receive(lua_State* L);
    static int lua_isOpen(lua_State* L);
    static int lua_hexToBytes(lua_State* L);
    static int lua_bytesToHex(lua_State* L);
    static int lua_crc16(lua_State* L);
    static int lua_crc32(lua_State* L);

private:
    static LuaEngine* s_instance;
    lua_State* m_lua = nullptr;
    QString m_lastError;

    // 回调函数
    SendCallback m_sendCallback;
    ReceiveCallback m_receiveCallback;
    IsOpenCallback m_isOpenCallback;
};

} // namespace ComAssistant

#endif // COMASSISTANT_LUAENGINE_H
