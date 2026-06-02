/**
 * @file LuaScriptProtocol.cpp
 * @brief Lua 脚本协议解析器实现
 */

#include "LuaScriptProtocol.h"

#include "../script/LuaSandbox.h"
#include "../utils/ConversionUtils.h"

#include <QtGlobal>

namespace ComAssistant {
namespace {

const char* kResultBeginMarker = "__COMASSISTANT_LUA_PROTOCOL_BEGIN__";
const char* kResultEndMarker = "__COMASSISTANT_LUA_PROTOCOL_END__";

/**
 * @brief Lua 脚本协议运行配置。
 *
 * 该结构把 QVariantMap 中的协议配置先收束成强类型字段，避免 parse()
 * 主流程里到处散落默认值和类型转换细节。
 */
struct LuaScriptProtocolOptions
{
    QString scriptSource;                ///< 内联 Lua 脚本源码。
    QString entryFunction = QStringLiteral("process"); ///< Lua 入口函数名。
    int timeoutMs = 1000;                ///< 单次沙箱执行超时。
    int memoryLimitKb = 1024;            ///< 单次 Lua state 内存预算。
    int maxOutputLines = 200;            ///< print 输出最大保留行数。
    bool allowCommunicationApi = false;  ///< 是否允许发送类通信 API。
};

/**
 * @brief 从 QVariantMap 读取整数配置。
 * @param config 协议配置表。
 * @param key 配置键名。
 * @param defaultValue 缺失或类型异常时使用的默认值。
 * @return 规范化后的整数值。
 *
 * 协议注册中心的 Schema 会在主流程中提供默认值和范围校验，但测试或外部
 * 调用方仍可能直接 setConfig()。这里做一层本地兜底，让解析器自身足够稳。
 */
int readIntegerOption(const QVariantMap& config,
                      const QString& key,
                      int defaultValue)
{
    bool ok = false;
    const int value = config.value(key, defaultValue).toInt(&ok);
    return ok ? value : defaultValue;
}

/**
 * @brief 从 QVariantMap 读取布尔配置。
 * @param config 协议配置表。
 * @param key 配置键名。
 * @param defaultValue 缺失时使用的默认值。
 * @return 规范化后的布尔值。
 */
bool readBooleanOption(const QVariantMap& config,
                       const QString& key,
                       bool defaultValue)
{
    if (!config.contains(key)) {
        return defaultValue;
    }
    return config.value(key).toBool();
}

/**
 * @brief 从协议配置中读取 Lua 解析器选项。
 * @param config 协议配置表。
 * @return 带默认值兜底的 Lua 脚本协议选项。
 */
LuaScriptProtocolOptions readOptions(const QVariantMap& config)
{
    LuaScriptProtocolOptions options;
    options.scriptSource = config.value(QStringLiteral("scriptSource")).toString();

    const QString entryFunction =
        config.value(QStringLiteral("entryFunction"),
                     options.entryFunction).toString().trimmed();
    if (!entryFunction.isEmpty()) {
        options.entryFunction = entryFunction;
    }

    options.timeoutMs = readIntegerOption(config, QStringLiteral("timeoutMs"), options.timeoutMs);
    options.memoryLimitKb =
        readIntegerOption(config, QStringLiteral("memoryLimitKb"), options.memoryLimitKb);
    options.maxOutputLines =
        readIntegerOption(config, QStringLiteral("maxOutputLines"), options.maxOutputLines);
    options.allowCommunicationApi =
        readBooleanOption(config,
                          QStringLiteral("allowCommunicationApi"),
                          options.allowCommunicationApi);
    return options;
}

/**
 * @brief 转义 Lua 单引号字符串。
 * @param value 待写入 wrapper 的文本。
 * @return 可安全放入 Lua 单引号字面量的文本。
 *
 * wrapper 会把入口函数名和输入 hex 文本直接拼入 Lua 源码。这里集中转义
 * 反斜杠、单引号和换行等字符，避免配置文本破坏 wrapper 结构。
 */
QString escapeLuaSingleQuotedString(const QString& value)
{
    QString escaped = value;
    escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escaped.replace(QStringLiteral("'"), QStringLiteral("\\'"));
    escaped.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
    escaped.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    escaped.replace(QStringLiteral("\t"), QStringLiteral("\\t"));
    return escaped;
}

/**
 * @brief 构造 Lua 协议调用 wrapper。
 * @param options Lua 脚本协议选项。
 * @param data 本次 parse() 输入数据。
 * @return 完整 Lua 源码，包含用户脚本和受控结果编码逻辑。
 *
 * wrapper 使用项目 LuaSandbox 已暴露的 hexToBytes()/bytesToHex() 处理二进制
 * 数据，避免把任意字节直接嵌入 Lua 源码。结果通过固定哨兵和 key=value
 * 输出回 C++，C++ 只解析哨兵区间，用户脚本自己的 print 不会干扰协议结果。
 */
QString buildWrapperScript(const LuaScriptProtocolOptions& options,
                           const QByteArray& data)
{
    const QString dataHex = QString::fromLatin1(data.toHex(' ')).toUpper();
    const QString entryFunction = escapeLuaSingleQuotedString(options.entryFunction);
    const int dataLength = data.size();

    return QStringLiteral(R"lua(
%1

local __comassistant_entry_name = '%2'
local __comassistant_input = hexToBytes('%3')
local __comassistant_context = {
    protocolId = 'lua.script',
    entryFunction = __comassistant_entry_name,
    dataLength = %4
}

local __comassistant_entry = _G[__comassistant_entry_name]
if type(__comassistant_entry) ~= 'function' then
    error('Lua protocol entry function not found: ' .. __comassistant_entry_name)
end

local __comassistant_result = __comassistant_entry(__comassistant_input, __comassistant_context)
if type(__comassistant_result) ~= 'table' then
    error('Lua protocol entry function must return a table')
end

local function __comassistant_print_text(prefix, value)
    if type(value) == 'string' then
        print(prefix .. '=' .. value)
    end
end

print('%5')
print('valid=' .. (__comassistant_result.valid and '1' or '0'))
if type(__comassistant_result.consumedBytes) == 'number' then
    print('consumedBytes=' .. tostring(math.floor(__comassistant_result.consumedBytes)))
end
if type(__comassistant_result.frame) == 'string' then
    print('frameHex=' .. bytesToHex(__comassistant_result.frame))
end
if type(__comassistant_result.payload) == 'string' then
    print('payloadHex=' .. bytesToHex(__comassistant_result.payload))
end
__comassistant_print_text('error', __comassistant_result.error)

if type(__comassistant_result.metadata) == 'table' then
    for key, value in pairs(__comassistant_result.metadata) do
        if type(key) == 'string' then
            if type(value) == 'string' then
                print('metadataHex:' .. key .. '=' .. bytesToHex(value))
            elseif type(value) == 'number' then
                print('metadataNumber:' .. key .. '=' .. tostring(value))
            elseif type(value) == 'boolean' then
                print('metadataBool:' .. key .. '=' .. (value and '1' or '0'))
            end
        end
    end
end
print('%6')
)lua").arg(options.scriptSource,
           entryFunction,
           dataHex,
           QString::number(dataLength),
           QString::fromLatin1(kResultBeginMarker),
           QString::fromLatin1(kResultEndMarker));
}

/**
 * @brief 查找 Lua 输出中的协议结果区间。
 * @param outputLines LuaSandbox 捕获到的 print 输出。
 * @param beginIndex 写回起始哨兵后的第一行索引。
 * @param endIndex 写回结束哨兵所在索引。
 * @return 找到完整哨兵区间返回 true。
 */
bool findResultBlock(const QStringList& outputLines,
                     int* beginIndex,
                     int* endIndex)
{
    int begin = -1;
    int end = -1;

    for (int i = 0; i < outputLines.size(); ++i) {
        if (outputLines.at(i) == QString::fromLatin1(kResultBeginMarker)) {
            begin = i + 1;
            continue;
        }

        if (begin >= 0 && outputLines.at(i) == QString::fromLatin1(kResultEndMarker)) {
            end = i;
            break;
        }
    }

    if (begin < 0 || end < begin) {
        return false;
    }

    if (beginIndex) {
        *beginIndex = begin;
    }
    if (endIndex) {
        *endIndex = end;
    }
    return true;
}

/**
 * @brief 读取 key=value 行中的 value 部分。
 * @param line 单行输出。
 * @param prefix 需要匹配的键名前缀，包含等号。
 * @param value 写回 value 文本。
 * @return 前缀匹配成功返回 true。
 */
bool readPrefixedValue(const QString& line,
                       const QString& prefix,
                       QString* value)
{
    if (!line.startsWith(prefix)) {
        return false;
    }

    if (value) {
        *value = line.mid(prefix.size());
    }
    return true;
}

/**
 * @brief 从 metadata 输出行读取键名和值。
 * @param line 单行输出。
 * @param prefix metadata 类型前缀，例如 metadataHex:。
 * @param key 写回 metadata 键名。
 * @param value 写回 metadata 值文本。
 * @return 成功解析键值对返回 true。
 */
bool readMetadataLine(const QString& line,
                      const QString& prefix,
                      QString* key,
                      QString* value)
{
    if (!line.startsWith(prefix)) {
        return false;
    }

    const QString body = line.mid(prefix.size());
    const int equalIndex = body.indexOf(QLatin1Char('='));
    if (equalIndex <= 0) {
        return false;
    }

    if (key) {
        *key = body.left(equalIndex);
    }
    if (value) {
        *value = body.mid(equalIndex + 1);
    }
    return true;
}

/**
 * @brief 把十六进制文本解码为字节数组。
 * @param hex Lua wrapper 输出的十六进制文本。
 * @return 解码后的原始字节。
 */
QByteArray decodeHexBytes(const QString& hex)
{
    return ConversionUtils::hexStringToBytes(hex);
}

/**
 * @brief 将脚本返回的 consumedBytes 限制到输入长度范围。
 * @param consumedBytes 脚本返回值。
 * @param inputSize 本次 parse() 输入长度。
 * @return 夹紧后的消耗字节数。
 */
int clampConsumedBytes(int consumedBytes, int inputSize)
{
    return qBound(0, consumedBytes, inputSize);
}

/**
 * @brief 将 Lua wrapper 输出映射为 FrameResult。
 * @param outputLines LuaSandbox 捕获的全部输出行。
 * @param inputSize 本次 parse() 输入长度，用于夹紧 consumedBytes。
 * @return 解析出的 FrameResult；缺少哨兵时返回错误结果。
 */
FrameResult parseFrameResultOutput(const QStringList& outputLines,
                                   int inputSize)
{
    FrameResult result;
    int beginIndex = -1;
    int endIndex = -1;
    if (!findResultBlock(outputLines, &beginIndex, &endIndex)) {
        result.errorMessage = QStringLiteral("Lua protocol result block missing");
        return result;
    }

    for (int i = beginIndex; i < endIndex; ++i) {
        const QString line = outputLines.at(i);
        QString value;

        if (readPrefixedValue(line, QStringLiteral("valid="), &value)) {
            result.valid = (value == QStringLiteral("1")
                            || value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
            continue;
        }

        if (readPrefixedValue(line, QStringLiteral("consumedBytes="), &value)) {
            bool ok = false;
            const int consumedBytes = value.toInt(&ok);
            if (ok) {
                result.consumedBytes = clampConsumedBytes(consumedBytes, inputSize);
            }
            continue;
        }

        if (readPrefixedValue(line, QStringLiteral("frameHex="), &value)) {
            result.frame = decodeHexBytes(value);
            continue;
        }

        if (readPrefixedValue(line, QStringLiteral("payloadHex="), &value)) {
            result.payload = decodeHexBytes(value);
            continue;
        }

        if (readPrefixedValue(line, QStringLiteral("error="), &value)) {
            result.errorMessage = value;
            continue;
        }

        QString key;
        if (readMetadataLine(line,
                             QStringLiteral("metadataHex:"),
                             &key,
                             &value)) {
            result.metadata.insert(key, QString::fromUtf8(decodeHexBytes(value)));
            continue;
        }

        if (readMetadataLine(line,
                             QStringLiteral("metadataNumber:"),
                             &key,
                             &value)) {
            bool ok = false;
            const double number = value.toDouble(&ok);
            if (ok) {
                result.metadata.insert(key, number);
            }
            continue;
        }

        if (readMetadataLine(line,
                             QStringLiteral("metadataBool:"),
                             &key,
                             &value)) {
            result.metadata.insert(key, value == QStringLiteral("1"));
            continue;
        }
    }

    return result;
}

/**
 * @brief 将协议配置转换为 LuaSandboxOptions。
 * @param options Lua 脚本协议强类型选项。
 * @return 可传给 LuaSandbox::execute() 的执行选项。
 */
LuaSandboxOptions makeSandboxOptions(const LuaScriptProtocolOptions& options)
{
    LuaSandboxOptions sandboxOptions;
    sandboxOptions.timeoutMs = options.timeoutMs;
    sandboxOptions.memoryLimitKb = options.memoryLimitKb;
    sandboxOptions.maxOutputLines = options.maxOutputLines;
    sandboxOptions.allowCommunicationApi = options.allowCommunicationApi;
    return sandboxOptions;
}

} // namespace

LuaScriptProtocol::LuaScriptProtocol(QObject* parent)
    : IProtocol(parent)
{
}

QString LuaScriptProtocol::description() const
{
    return tr("Lua script protocol parser");
}

QString LuaScriptProtocol::recentError() const
{
    return m_recentError;
}

FrameResult LuaScriptProtocol::parse(const QByteArray& data)
{
    FrameResult result;
    const LuaScriptProtocolOptions options = readOptions(m_config);
    if (options.scriptSource.trimmed().isEmpty()) {
        result.errorMessage = QStringLiteral("Lua protocol script source is empty");
        m_recentError = result.errorMessage;
        emit parseError(result.errorMessage);
        return result;
    }

    LuaSandbox sandbox;
    const LuaSandboxResult sandboxResult =
        sandbox.execute(buildWrapperScript(options, data), makeSandboxOptions(options));

    if (!sandboxResult.success) {
        result.errorMessage = sandboxResult.errorMessage;
        m_recentError = result.errorMessage;
        emit parseError(result.errorMessage);
        return result;
    }

    result = parseFrameResultOutput(sandboxResult.outputLines, data.size());
    if (result.valid) {
        /*
         * 有效帧解析成功说明当前脚本和资源边界已经恢复正常。清空最近错误，
         * 避免诊断包继续显示用户已经修复过的旧失败。
         */
        m_recentError.clear();
        emit frameReceived(result);
    } else if (!result.errorMessage.isEmpty()) {
        m_recentError = result.errorMessage;
        emit parseError(result.errorMessage);
    }
    return result;
}

QByteArray LuaScriptProtocol::build(const QByteArray& payload,
                                    const QVariantMap& metadata)
{
    Q_UNUSED(metadata)

    /*
     * 第一版 Lua 协议只承诺接收解析能力。发送构帧若也交给脚本，需要
     * 另行定义 build(payload, metadata) 入口、错误语义和测试。
     */
    return payload;
}

bool LuaScriptProtocol::validate(const QByteArray& frame)
{
    return parse(frame).valid;
}

QByteArray LuaScriptProtocol::calculateChecksum(const QByteArray& data)
{
    Q_UNUSED(data)

    /*
     * Lua 校验计算没有统一契约。返回空数组可以保持 IProtocol 接口完整，
     * 同时不提前承诺脚本校验 API。
     */
    return QByteArray();
}

void LuaScriptProtocol::reset()
{
    m_buffer.clear();
    m_recentError.clear();
}

} // namespace ComAssistant
