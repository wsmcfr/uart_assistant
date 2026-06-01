/**
 * @file SendQueue.h
 * @brief 统一发送队列与背压策略
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_SENDQUEUE_H
#define COMASSISTANT_SENDQUEUE_H

#include <QByteArray>
#include <QQueue>
#include <QString>

namespace ComAssistant {

/**
 * @brief 发送队列容量选项。
 *
 * maxItems 限制待发送任务条数，maxBytes 限制队列中 payload 总字节数。
 * 两个限制共同构成背压边界，避免脚本、快捷发送或文件传输在连接变慢
 * 时无限堆积内存。
 */
struct SendQueueOptions
{
    int maxItems = 1024;             ///< 队列允许保存的最大任务条数。
    qint64 maxBytes = 8 * 1024 * 1024; ///< 队列允许保存的最大 payload 总字节数。
};

/**
 * @brief 单个发送任务。
 *
 * id 是队列内递增编号，用于日志和测试定位；payload 是真实写入底层
 * 通信对象的数据；source 记录来源，方便诊断队列满或失败来自哪个入口。
 */
struct SendItem
{
    qint64 id = 0;              ///< 队列分配的单调递增任务编号。
    QByteArray payload;         ///< 待发送原始字节。
    QString source;             ///< 发送来源，例如 manual/script/file。
    int attemptCount = 0;       ///< 当前任务已经失败重试或失败记录的次数。
    QString lastError;          ///< 当前任务最近一次失败原因。
};

/**
 * @brief 入队结果。
 *
 * accepted 为 true 时 itemId 表示成功入队任务编号；accepted 为 false 时
 * error 给出拒绝原因，上层可直接展示或写入诊断日志。
 */
struct SendEnqueueResult
{
    bool accepted = false;      ///< 是否成功入队。
    qint64 itemId = 0;          ///< 成功入队后的任务编号。
    QString error;              ///< 失败原因。
};

/**
 * @brief 队首任务完成结果。
 *
 * 调度器把底层 write() 结果转换为该结构。成功会弹出队首，失败会保留
 * 队首并记录错误，保证恢复连接后可以重试同一个 payload。
 */
struct SendCompletion
{
    bool ok = false;            ///< 底层写入是否成功。
    QString error;              ///< 失败原因，成功时为空。

    /**
     * @brief 构造成功完成结果。
     * @return ok 为 true 的完成结果。
     */
    static SendCompletion success();

    /**
     * @brief 构造失败完成结果。
     * @param error 失败原因，会写入队首任务的 lastError。
     * @return ok 为 false 的完成结果。
     */
    static SendCompletion failed(const QString& error);
};

/**
 * @brief 统一发送队列。
 *
 * SendQueue 只负责顺序、容量和失败保持，不直接触碰串口、网络或 HID。
 * 这种纯数据结构便于单元测试，也让 MainWindowCommunicationController
 * 可以把所有普通字节发送汇聚到同一背压策略下。
 */
class SendQueue
{
public:
    /**
     * @brief 构造发送队列。
     * @param options 队列容量选项，非法值会在构造时清洗为安全范围。
     */
    explicit SendQueue(const SendQueueOptions& options = SendQueueOptions());

    /**
     * @brief 添加发送任务。
     * @param payload 待发送原始字节，不能为空。
     * @param source 发送来源标签，用于诊断；可为空。
     * @return 入队结果，失败时队列保持不变。
     */
    SendEnqueueResult enqueue(const QByteArray& payload, const QString& source = QString());

    /**
     * @brief 完成当前队首任务。
     * @param completion 底层写入完成结果。
     * @return 成功弹出队首返回 true；失败或无队首返回 false。
     */
    bool completeHead(const SendCompletion& completion);

    /**
     * @brief 取消并清空全部待发送任务。
     * @param reason 取消原因，会保存为队列最近错误。
     * @return 被清空的任务数量。
     */
    int cancelAll(const QString& reason = QString());

    /**
     * @brief 判断是否存在待发送任务。
     * @return 队列非空返回 true。
     */
    bool hasPending() const;

    /**
     * @brief 获取队首任务。
     * @return 队首任务引用；调用前必须确保 hasPending() 为 true。
     */
    const SendItem& peek() const;

    /**
     * @brief 获取待发送任务条数。
     * @return 当前队列长度。
     */
    int size() const;

    /**
     * @brief 获取队列中待发送 payload 总字节数。
     * @return 所有待发送任务 payload 字节数之和。
     */
    qint64 queuedBytes() const;

    /**
     * @brief 获取最近一次队列错误。
     * @return 最近一次入队拒绝、发送失败或取消原因。
     */
    QString lastError() const;

    /**
     * @brief 获取队列容量选项。
     * @return 清洗后的容量选项。
     */
    SendQueueOptions options() const;

private:
    /**
     * @brief 生成下一条任务编号。
     * @return 单调递增的任务编号。
     */
    qint64 nextId();

private:
    SendQueueOptions m_options; ///< 清洗后的队列容量配置。
    QQueue<SendItem> m_items;   ///< FIFO 发送任务队列。
    qint64 m_queuedBytes = 0;   ///< 当前队列 payload 总字节数。
    qint64 m_nextId = 1;        ///< 下一条任务编号。
    QString m_lastError;        ///< 最近一次队列级错误。
};

} // namespace ComAssistant

#endif // COMASSISTANT_SENDQUEUE_H
