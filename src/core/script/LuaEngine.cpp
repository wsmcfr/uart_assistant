/**
 * @file LuaEngine.cpp
 * @brief Lua脚本引擎实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "LuaEngine.h"
#include "../utils/ChecksumUtils.h"
#include "../utils/ConversionUtils.h"
#include "../utils/Logger.h"

#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QCoreApplication>

namespace ComAssistant {

// 静态成员初始化
LuaEngine* LuaEngine::s_instance = nullptr;

LuaEngine* LuaEngine::instance()
{
    if (!s_instance) {
        s_instance = new LuaEngine();
    }
    return s_instance;
}

LuaEngine::LuaEngine(QObject* parent)
    : QObject(parent)
{
}

LuaEngine::~LuaEngine()
{
    shutdown();
}

bool LuaEngine::initialize()
{
    if (m_lua) {
        return true;  // 已初始化
    }

    m_lua = luaL_newstate();
    if (!m_lua) {
        m_lastError = tr("Failed to create Lua state");
        LOG_ERROR(m_lastError);
        return false;
    }

    // 打开标准库
    luaL_openlibs(m_lua);

    // 注册内置API
    registerBuiltinApis();

    LOG_INFO("Lua engine initialized");
    return true;
}

void LuaEngine::shutdown()
{
    if (m_lua) {
        lua_close(m_lua);
        m_lua = nullptr;
        LOG_INFO("Lua engine shutdown");
    }
}

bool LuaEngine::executeScript(const QString& script)
{
    if (!m_lua) {
        m_lastError = tr("Lua engine not initialized");
        emit scriptError(m_lastError);
        return false;
    }

    QByteArray scriptData = script.toUtf8();
    int result = luaL_dostring(m_lua, scriptData.constData());

    if (result != LUA_OK) {
        m_lastError = QString::fromUtf8(lua_tostring(m_lua, -1));
        lua_pop(m_lua, 1);
        emit scriptError(m_lastError);
        emit scriptFinished(false);
        return false;
    }

    emit scriptFinished(true);
    return true;
}

bool LuaEngine::executeFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = tr("Cannot open file: %1").arg(filePath);
        emit scriptError(m_lastError);
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    QString script = stream.readAll();
    file.close();

    return executeScript(script);
}

void LuaEngine::setGlobalString(const QString& name, const QString& value)
{
    if (!m_lua) return;
    lua_pushstring(m_lua, value.toUtf8().constData());
    lua_setglobal(m_lua, name.toUtf8().constData());
}

void LuaEngine::setGlobalNumber(const QString& name, double value)
{
    if (!m_lua) return;
    lua_pushnumber(m_lua, value);
    lua_setglobal(m_lua, name.toUtf8().constData());
}

void LuaEngine::setGlobalBoolean(const QString& name, bool value)
{
    if (!m_lua) return;
    lua_pushboolean(m_lua, value);
    lua_setglobal(m_lua, name.toUtf8().constData());
}

QString LuaEngine::getGlobalString(const QString& name)
{
    if (!m_lua) return QString();
    lua_getglobal(m_lua, name.toUtf8().constData());
    QString result;
    if (lua_isstring(m_lua, -1)) {
        result = QString::fromUtf8(lua_tostring(m_lua, -1));
    }
    lua_pop(m_lua, 1);
    return result;
}

double LuaEngine::getGlobalNumber(const QString& name)
{
    if (!m_lua) return 0.0;
    lua_getglobal(m_lua, name.toUtf8().constData());
    double result = 0.0;
    if (lua_isnumber(m_lua, -1)) {
        result = lua_tonumber(m_lua, -1);
    }
    lua_pop(m_lua, 1);
    return result;
}

bool LuaEngine::getGlobalBoolean(const QString& name)
{
    if (!m_lua) return false;
    lua_getglobal(m_lua, name.toUtf8().constData());
    bool result = lua_toboolean(m_lua, -1);
    lua_pop(m_lua, 1);
    return result;
}

void LuaEngine::registerFunction(const QString& name, lua_CFunction func)
{
    if (!m_lua) return;
    lua_pushcfunction(m_lua, func);
    lua_setglobal(m_lua, name.toUtf8().constData());
}

void LuaEngine::registerModule(const QString& moduleName, const luaL_Reg* functions)
{
    if (!m_lua) return;

    lua_newtable(m_lua);
    luaL_setfuncs(m_lua, functions, 0);
    lua_setglobal(m_lua, moduleName.toUtf8().constData());
}

void LuaEngine::doSend(const QByteArray& data)
{
    if (m_sendCallback) {
        m_sendCallback(data);
    }
}

QByteArray LuaEngine::doReceive(int timeout)
{
    if (m_receiveCallback) {
        return m_receiveCallback(timeout);
    }
    return QByteArray();
}

bool LuaEngine::doIsOpen()
{
    if (m_isOpenCallback) {
        return m_isOpenCallback();
    }
    return false;
}

void LuaEngine::registerBuiltinApis()
{
    // 重定向print函数
    registerFunction("print", lua_print);

    // 工具函数
    registerFunction("sleep", lua_sleep);
    registerFunction("hexToBytes", lua_hexToBytes);
    registerFunction("bytesToHex", lua_bytesToHex);
    registerFunction("crc16", lua_crc16);
    registerFunction("crc32", lua_crc32);

    // 串口模块
    static const luaL_Reg serialFuncs[] = {
        {"send", lua_send},
        {"sendHex", lua_sendHex},
        {"receive", lua_receive},
        {"isOpen", lua_isOpen},
        {nullptr, nullptr}
    };
    registerModule("serial", serialFuncs);
}

// ==================== Lua API 实现 ====================

int LuaEngine::lua_print(lua_State* L)
{
    int n = lua_gettop(L);
    QString output;

    for (int i = 1; i <= n; ++i) {
        if (lua_isstring(L, i)) {
            if (i > 1) output += "\t";
            output += QString::fromUtf8(lua_tostring(L, i));
        } else if (lua_isnil(L, i)) {
            if (i > 1) output += "\t";
            output += "nil";
        } else if (lua_isboolean(L, i)) {
            if (i > 1) output += "\t";
            output += lua_toboolean(L, i) ? "true" : "false";
        } else if (lua_isnumber(L, i)) {
            if (i > 1) output += "\t";
            output += QString::number(lua_tonumber(L, i));
        }
    }

    LuaEngine::instance()->emit scriptOutput(output);
    return 0;
}

int LuaEngine::lua_sleep(lua_State* L)
{
    int ms = static_cast<int>(luaL_checknumber(L, 1));
    if (ms > 0) {
        QThread::msleep(ms);
        QCoreApplication::processEvents();
    }
    return 0;
}

int LuaEngine::lua_send(lua_State* L)
{
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    QByteArray bytes(data, static_cast<int>(len));
    LuaEngine::instance()->doSend(bytes);
    return 0;
}

int LuaEngine::lua_sendHex(lua_State* L)
{
    const char* hexStr = luaL_checkstring(L, 1);
    QByteArray bytes = ConversionUtils::hexStringToBytes(QString::fromUtf8(hexStr));
    LuaEngine::instance()->doSend(bytes);
    return 0;
}

int LuaEngine::lua_receive(lua_State* L)
{
    int timeout = 1000;  // 默认1秒
    if (lua_gettop(L) >= 1) {
        timeout = static_cast<int>(luaL_checknumber(L, 1));
    }

    QByteArray data = LuaEngine::instance()->doReceive(timeout);

    if (data.isEmpty()) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, data.constData(), data.size());
    }
    return 1;
}

int LuaEngine::lua_isOpen(lua_State* L)
{
    bool isOpen = LuaEngine::instance()->doIsOpen();
    lua_pushboolean(L, isOpen);
    return 1;
}

int LuaEngine::lua_hexToBytes(lua_State* L)
{
    const char* hexStr = luaL_checkstring(L, 1);
    QByteArray bytes = ConversionUtils::hexStringToBytes(QString::fromUtf8(hexStr));
    lua_pushlstring(L, bytes.constData(), bytes.size());
    return 1;
}

int LuaEngine::lua_bytesToHex(lua_State* L)
{
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    QByteArray bytes(data, static_cast<int>(len));
    QString hex = ConversionUtils::bytesToHexString(bytes, " ");
    lua_pushstring(L, hex.toUtf8().constData());
    return 1;
}

int LuaEngine::lua_crc16(lua_State* L)
{
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    QByteArray bytes(data, static_cast<int>(len));
    quint16 crc = ChecksumUtils::crc16Modbus(bytes);
    lua_pushinteger(L, crc);
    return 1;
}

int LuaEngine::lua_crc32(lua_State* L)
{
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    QByteArray bytes(data, static_cast<int>(len));
    quint32 crc = ChecksumUtils::crc32(bytes);
    lua_pushinteger(L, crc);
    return 1;
}

} // namespace ComAssistant
