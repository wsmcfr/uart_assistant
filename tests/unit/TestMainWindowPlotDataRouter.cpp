/**
 * @file TestMainWindowPlotDataRouter.cpp
 * @brief 主窗口绘图数据路由器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestMainWindowPlotDataRouter.h"

#include "ui/MainWindowPlotDataRouter.h"

using namespace ComAssistant;

namespace {

/**
 * @brief 用于测试的简单绘图协议。
 *
 * 输入行格式为 `windowId:y1,y2`、`windowId@x:y1,y2` 或
 * `windowId#timestamp:y1,y2`。测试只关心 MainWindowPlotDataRouter
 * 是否正确做缓冲和路由决策，因此协议本身保持极小实现。
 */
class FakePlotProtocol : public IProtocol
{
    Q_OBJECT

public:
    /**
     * @brief 返回协议类型。
     * @return 使用 TextPlot 类型代表普通绘图协议。
     */
    ProtocolType type() const override { return ProtocolType::TextPlot; }

    /**
     * @brief 返回协议名称。
     * @return 固定测试名称。
     */
    QString name() const override { return QStringLiteral("FakePlot"); }

    /**
     * @brief 返回协议描述。
     * @return 固定测试描述。
     */
    QString description() const override { return QStringLiteral("Fake plot protocol"); }

    /**
     * @brief 测试不覆盖通用帧解析。
     * @param data 输入数据。
     * @return 默认无效帧。
     */
    FrameResult parse(const QByteArray& data) override
    {
        Q_UNUSED(data);
        return FrameResult();
    }

    /**
     * @brief 测试不覆盖发送帧构造。
     * @param payload 有效载荷。
     * @param metadata 元数据。
     * @return 原样返回有效载荷。
     */
    QByteArray build(const QByteArray& payload, const QVariantMap& metadata = QVariantMap()) override
    {
        Q_UNUSED(metadata);
        return payload;
    }

    /**
     * @brief 测试协议始终认为帧有效。
     * @param frame 待验证帧。
     * @return true。
     */
    bool validate(const QByteArray& frame) override
    {
        Q_UNUSED(frame);
        return true;
    }

    /**
     * @brief 测试协议不需要校验。
     * @param data 待校验数据。
     * @return 空校验值。
     */
    QByteArray calculateChecksum(const QByteArray& data) override
    {
        Q_UNUSED(data);
        return QByteArray();
    }

    /**
     * @brief 声明这是绘图协议。
     * @return true。
     */
    bool isPlotProtocol() const override { return true; }

    /**
     * @brief 解析测试绘图行。
     * @param data 单行绘图数据。
     * @return 可供路由器转发的绘图数据。
     */
    PlotData parsePlotData(const QByteArray& data) override
    {
        parsedLines.append(data);

        PlotData result;
        const QString line = QString::fromLatin1(data);
        const int separator = line.indexOf(':');
        if (separator <= 0) {
            return result;
        }

        QString header = line.left(separator);
        const QStringList valueTexts = line.mid(separator + 1).split(',');
        for (const QString& valueText : valueTexts) {
            bool ok = false;
            const double value = valueText.toDouble(&ok);
            if (!ok) {
                return PlotData();
            }
            result.yValues.append(value);
        }

        const int timestampPos = header.indexOf('#');
        const int customXPos = header.indexOf('@');
        if (timestampPos > 0) {
            result.windowId = header.left(timestampPos);
            result.timestamp = header.mid(timestampPos + 1).toDouble();
            result.useTimestamp = true;
        } else if (customXPos > 0) {
            result.windowId = header.left(customXPos);
            result.xValue = header.mid(customXPos + 1).toDouble();
            result.useCustomX = true;
        } else {
            result.windowId = header;
        }

        result.valid = !result.windowId.isEmpty() && !result.yValues.isEmpty();
        return result;
    }

    QVector<QByteArray> parsedLines; ///< 记录路由器实际提交给协议的行数据。
};

} // namespace

void TestMainWindowPlotDataRouter::testRawProtocolRequestsAutoDetectionOnly()
{
    MainWindowPlotDataRouter router;
    FakePlotProtocol protocol;

    const MainWindowPlotDataRouter::ProcessResult result =
        router.processReceivedData(QByteArray("plot1:1.0\n"),
                                   ProtocolType::Raw,
                                   &protocol);

    QVERIFY(result.shouldFeedDetector);
    QVERIFY(result.routes.isEmpty());
    QVERIFY(protocol.parsedLines.isEmpty());
    QVERIFY(router.pendingBuffer().isEmpty());
}

void TestMainWindowPlotDataRouter::testChunkedPlotLinesAreBufferedUntilNewline()
{
    MainWindowPlotDataRouter router;
    FakePlotProtocol protocol;

    MainWindowPlotDataRouter::ProcessResult first =
        router.processReceivedData(QByteArray("plot1:1.0,2."),
                                   ProtocolType::TextPlot,
                                   &protocol);
    QVERIFY(!first.shouldFeedDetector);
    QVERIFY(first.routes.isEmpty());
    QCOMPARE(router.pendingBuffer(), QByteArray("plot1:1.0,2."));

    MainWindowPlotDataRouter::ProcessResult second =
        router.processReceivedData(QByteArray("5\nplot1@42:3,4\n"),
                                   ProtocolType::TextPlot,
                                   &protocol);

    QCOMPARE(protocol.parsedLines.size(), 2);
    QCOMPARE(protocol.parsedLines.at(0), QByteArray("plot1:1.0,2.5"));
    QCOMPARE(protocol.parsedLines.at(1), QByteArray("plot1@42:3,4"));
    QCOMPARE(second.routes.size(), 2);
    QCOMPARE(second.routes.at(0).windowId, QStringLiteral("plot1"));
    QCOMPARE(second.routes.at(0).values, QVector<double>({1.0, 2.5}));
    QCOMPARE(second.routes.at(0).xMode, MainWindowPlotDataRouter::PlotRoute::AutomaticX);
    QCOMPARE(second.routes.at(1).windowId, QStringLiteral("plot1"));
    QCOMPARE(second.routes.at(1).x, 42.0);
    QCOMPARE(second.routes.at(1).xMode, MainWindowPlotDataRouter::PlotRoute::CustomX);
    QCOMPARE(second.routes.at(1).values, QVector<double>({3.0, 4.0}));
    QVERIFY(router.pendingBuffer().isEmpty());
}

void TestMainWindowPlotDataRouter::testCarriageReturnIsRemovedBeforeParsing()
{
    MainWindowPlotDataRouter router;
    FakePlotProtocol protocol;

    const MainWindowPlotDataRouter::ProcessResult result =
        router.processReceivedData(QByteArray("plot2#12.5:7\r\n"),
                                   ProtocolType::StampPlot,
                                   &protocol);

    QCOMPARE(protocol.parsedLines.size(), 1);
    QCOMPARE(protocol.parsedLines.at(0), QByteArray("plot2#12.5:7"));
    QCOMPARE(result.routes.size(), 1);
    QCOMPARE(result.routes.at(0).windowId, QStringLiteral("plot2"));
    QCOMPARE(result.routes.at(0).xMode, MainWindowPlotDataRouter::PlotRoute::Timestamp);
    QCOMPARE(result.routes.at(0).x, 12.5);
    QCOMPARE(result.routes.at(0).values, QVector<double>({7.0}));
}

void TestMainWindowPlotDataRouter::testOversizedPartialBufferIsCleared()
{
    MainWindowPlotDataRouter router;
    FakePlotProtocol protocol;

    const QByteArray oversized(4097, 'x');
    const MainWindowPlotDataRouter::ProcessResult result =
        router.processReceivedData(oversized,
                                   ProtocolType::TextPlot,
                                   &protocol);

    QVERIFY(!result.shouldFeedDetector);
    QVERIFY(result.routes.isEmpty());
    QVERIFY(result.bufferCleared);
    QVERIFY(router.pendingBuffer().isEmpty());
}

#include "TestMainWindowPlotDataRouter.moc"
