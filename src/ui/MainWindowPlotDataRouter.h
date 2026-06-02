/**
 * @file MainWindowPlotDataRouter.h
 * @brief 主窗口绘图数据路由器
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef COMASSISTANT_MAINWINDOWPLOTDATAROUTER_H
#define COMASSISTANT_MAINWINDOWPLOTDATAROUTER_H

#include "protocol/IProtocol.h"

#include <QByteArray>
#include <QVector>

namespace ComAssistant {

/**
 * @brief 主窗口绘图数据路由器。
 *
 * 该类只处理接收数据中的绘图协议部分：Raw 模式时提示调用方做自动
 * 检测；手动选择绘图协议后维护行缓冲、按换行提交给协议解析，并把
 * 协议结果转换为 PlotterManager 能消费的路由描述。它不直接依赖
 * MainWindow、PlotterManager 或任何 QWidget，便于单元测试覆盖。
 */
class MainWindowPlotDataRouter
{
public:
    /**
     * @brief 绘图路由描述。
     *
     * `xMode` 用来区分自动 X、时间戳 X 和自定义 X，调用方据此选择
     * PlotterManager 的不同重载，避免路由器直接创建绘图窗口。
     */
    struct PlotRoute
    {
        /**
         * @brief X 轴取值模式。
         */
        enum XMode {
            AutomaticX, ///< 不指定 X 值，由 PlotterWindow 自动递增。
            Timestamp,  ///< 使用协议中的时间戳作为 X 值。
            CustomX     ///< 使用协议中的自定义 X 值。
        };

        QString windowId;           ///< 目标绘图窗口 ID。
        QVector<double> values;     ///< 需要写入的 Y 值列表。
        double x = 0.0;             ///< 时间戳或自定义 X 值。
        XMode xMode = AutomaticX;   ///< 当前路由的 X 轴模式。
    };

    /**
     * @brief 接收数据处理结果。
     */
    struct ProcessResult
    {
        bool shouldFeedDetector = false; ///< Raw 模式下是否应把原始数据喂给自动检测器。
        bool bufferCleared = false;      ///< 是否因为残留缓冲过大而清空。
        QVector<PlotRoute> routes;       ///< 本次解析出的绘图路由结果。
    };

    /**
     * @brief 处理一段接收数据。
     *
     * @param data 新收到的数据块。
     * @param protocolType 当前主窗口选择的协议类型。
     * @param protocol 当前协议对象；Raw 或非绘图协议时可以为空。
     * @return 自动检测标志、缓冲清理标志以及绘图路由列表。
     */
    ProcessResult processReceivedData(const QByteArray& data,
                                      ProtocolType protocolType,
                                      IProtocol* protocol);

    /**
     * @brief 清空内部未完成行缓冲。
     *
     * 协议切换、自动检测结果变化和断开连接时都应调用，避免旧协议残留
     * 数据污染下一次解析。
     */
    void reset();

    /**
     * @brief 获取当前未完成行缓冲。
     * @return 缓冲中的残留数据，主要用于单元测试和调试。
     */
    QByteArray pendingBuffer() const;

private:
    static constexpr int MaxPendingBufferSize = 4096; ///< 残留缓冲最大字节数，超过即清空。

    QByteArray m_pendingBuffer; ///< 绘图协议按行解析前的残留数据缓冲。
};

} // namespace ComAssistant

#endif // COMASSISTANT_MAINWINDOWPLOTDATAROUTER_H
