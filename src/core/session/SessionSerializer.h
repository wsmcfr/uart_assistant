/**
 * @file SessionSerializer.h
 * @brief 会话序列化器
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef COMASSISTANT_SESSIONSERIALIZER_H
#define COMASSISTANT_SESSIONSERIALIZER_H

#include "SessionData.h"
#include <QString>

namespace ComAssistant {

/**
 * @brief 会话序列化器
 * 支持保存和加载会话文件(.cas格式)
 */
class SessionSerializer {
public:
    /**
     * @brief 文件格式版本
     */
    static constexpr int FILE_VERSION = 1;

    /**
     * @brief 文件魔数
     */
    static constexpr char FILE_MAGIC[4] = {'C', 'A', 'S', '\0'};

    /**
     * @brief 保存会话到文件
     * @param data 会话数据
     * @param filePath 文件路径
     * @param compress 是否压缩（默认true）
     * @return 是否成功
     */
    static bool saveToFile(const SessionData& data, const QString& filePath, bool compress = true);

    /**
     * @brief 从文件加载会话
     * @param filePath 文件路径
     * @param data 输出会话数据
     * @return 是否成功
     */
    static bool loadFromFile(const QString& filePath, SessionData& data);

    /**
     * @brief 序列化为JSON字符串
     * @param data 会话数据
     * @return JSON字符串
     */
    static QByteArray toJson(const SessionData& data);

    /**
     * @brief 从JSON字符串解析
     * @param json JSON字符串
     * @param data 输出会话数据
     * @return 是否成功
     */
    static bool fromJson(const QByteArray& json, SessionData& data);

    /**
     * @brief 获取最后的错误信息
     */
    static QString lastError();

private:
    static QString s_lastError;
};

} // namespace ComAssistant

#endif // COMASSISTANT_SESSIONSERIALIZER_H
