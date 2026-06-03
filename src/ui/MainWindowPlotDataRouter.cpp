/**
 * @file MainWindowPlotDataRouter.cpp
 * @brief 主窗口绘图数据路由器实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "MainWindowPlotDataRouter.h"

namespace ComAssistant {

MainWindowPlotDataRouter::ProcessResult MainWindowPlotDataRouter::processReceivedData(
    const QByteArray& data,
    ProtocolType protocolType,
    IProtocol* protocol)
{
    ProcessResult result;

    /*
     * Raw 模式仍由 MainWindow 持有的 PlotProtocolDetector 做自动检测。
     * 路由器只返回“应检测”信号，不直接依赖检测器，避免把 QObject
     * 信号生命周期和纯路由逻辑混在一起。
     */
    if (protocolType == ProtocolType::Raw) {
        result.shouldFeedDetector = true;
        return result;
    }

    /*
     * 没有协议对象或当前协议不支持绘图时，接收数据只进入普通显示、
     * 分窗和数据表路径；绘图路由器不做任何处理。
     */
    if (!protocol || !protocol->isPlotProtocol()) {
        return result;
    }

    m_pendingBuffer.append(data);

    /*
     * 协议实现按“完整一行”解析，因此这里消费所有换行前的数据。
     * 未完成的尾部行保留在 m_pendingBuffer 中，等待下一次接收补齐。
     */
    int newlinePos = -1;
    while ((newlinePos = m_pendingBuffer.indexOf('\n')) >= 0) {
        QByteArray line = m_pendingBuffer.left(newlinePos);
        m_pendingBuffer.remove(0, newlinePos + 1);

        // 串口设备常见 CRLF 行尾，提交协议解析前去掉 '\r'。
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        if (line.isEmpty()) {
            continue;
        }

        const PlotData plotData = protocol->parsePlotData(line);
        if (!plotData.valid || plotData.windowId.isEmpty() || plotData.yValues.isEmpty()) {
            continue;
        }

        PlotRoute route;
        route.windowId = plotData.windowId;
        route.values = plotData.yValues;
        if (plotData.useTimestamp) {
            route.x = plotData.timestamp;
            route.xMode = PlotRoute::Timestamp;
        } else if (plotData.useCustomX) {
            route.x = plotData.xValue;
            route.xMode = PlotRoute::CustomX;
        } else {
            route.xMode = PlotRoute::AutomaticX;
        }
        result.routes.append(route);
    }

    /*
     * 如果设备持续输出没有换行的坏数据，缓冲会无限增长。保持原
     * MainWindow 的 4096 字节上限，超限时丢弃残留，下一帧重新同步。
     */
    if (m_pendingBuffer.size() > MaxPendingBufferSize) {
        m_pendingBuffer.clear();
        result.bufferCleared = true;
    }

    return result;
}

void MainWindowPlotDataRouter::reset()
{
    // 协议状态切换时主动清空残留，避免上一协议的半行污染新协议。
    m_pendingBuffer.clear();
    m_pendingBuffer.squeeze();
}

QByteArray MainWindowPlotDataRouter::pendingBuffer() const
{
    return m_pendingBuffer;
}

} // namespace ComAssistant
