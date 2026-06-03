/**
 * @file TestDataExporter.cpp
 * @brief 数据导出器单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestDataExporter.h"

#include "core/export/DataExporter.h"

#include <QBuffer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QTextCodec>

using namespace ComAssistant;

namespace {

/**
 * @brief 记录每次 writeData 调用大小的内存设备。
 *
 * 流式导出的核心回归风险是重新回到“先拼完整 QString/QByteArray，
 * 再一次性写文件”的实现。该设备会记录每次写入大小，测试可以用
 * 单次写入上限证明真实导出路径正在分块写出。
 */
class TrackingBuffer : public QBuffer
{
public:
    explicit TrackingBuffer(QByteArray* data)
        : QBuffer(data)
    {
    }

    /**
     * @brief 返回底层 QIODevice 收到的最大单次写入字节数。
     * @return 最大单次写入字节数；未写入时为 0。
     */
    qint64 maxWriteSize() const
    {
        qint64 maxSize = 0;
        for (const qint64 size : m_writeSizes) {
            maxSize = qMax(maxSize, size);
        }
        return maxSize;
    }

    /**
     * @brief 返回 writeData 被调用的次数。
     * @return 写入调用次数。
     */
    int writeCallCount() const
    {
        return m_writeSizes.size();
    }

protected:
    /**
     * @brief 记录写入大小后交给 QBuffer 保存数据。
     * @param data Qt 传入的待写入字节。
     * @param len 本次待写入长度。
     * @return 实际写入长度。
     */
    qint64 writeData(const char* data, qint64 len) override
    {
        m_writeSizes.append(len);
        return QBuffer::writeData(data, len);
    }

private:
    QVector<qint64> m_writeSizes; ///< 每次底层写入调用的字节数，用于验证分块写出。
};

/**
 * @brief 构造带固定时间戳的数据记录，避免测试受当前时间影响。
 * @param text 记录载荷。
 * @param receive true 表示 RX，false 表示 TX。
 * @return 初始化后的 DataRecord。
 */
DataRecord makeRecord(const QByteArray& text, bool receive = true)
{
    DataRecord record;
    record.timestamp = QDateTime(QDate(2026, 6, 3), QTime(12, 0, 0), Qt::LocalTime);
    record.data = text;
    record.isReceive = receive;
    record.source = QStringLiteral("COM1");
    return record;
}

} // namespace

void TestDataExporter::testPlainTextExportToDeviceWritesSmallChunksAndKeepsFiltering()
{
    DataExporter exporter;
    ExportOptions options;
    options.format = ExportFormat::PlainText;
    options.includeTimestamp = false;
    options.includeDirection = false;
    options.includeSource = false;
    options.filterByContent = true;
    options.contentFilter = QStringLiteral("KEEP");
    options.lineSeparator = QStringLiteral("\n");
    exporter.setOptions(options);

    const QByteArray firstPayload = QByteArray("KEEP ") + QByteArray(8192, 'A');
    const QByteArray secondPayload = QByteArray("DROP ") + QByteArray(8192, 'B');
    const QByteArray thirdPayload = QByteArray("KEEP ") + QByteArray(8192, 'C');
    exporter.addRecord(makeRecord(firstPayload));
    exporter.addRecord(makeRecord(secondPayload));
    exporter.addRecord(makeRecord(thirdPayload));

    QByteArray output;
    TrackingBuffer device(&output);
    QVERIFY(device.open(QIODevice::WriteOnly));

    QSignalSpy progressSpy(&exporter, &DataExporter::exportProgress);

    /*
     * 真实导出接口应接收 QIODevice，这样文件、内存缓冲和未来网络/压缩
     * 目标都能复用同一个逐条写入实现。
     */
    QVERIFY(exporter.exportToDevice(&device));
    device.close();

    const QByteArray expected = firstPayload + "\n" + thirdPayload + "\n";
    QCOMPARE(output, expected);
    QCOMPARE(exporter.statistics().totalRecords, 3);
    QCOMPARE(exporter.statistics().exportedRecords, 2);
    QCOMPARE(exporter.statistics().filteredRecords, 1);
    QCOMPARE(exporter.statistics().exportedBytes, static_cast<qint64>(expected.size()));
    QCOMPARE(progressSpy.size(), 2);
    QVERIFY2(device.writeCallCount() >= 2,
             "文本导出应随记录逐步写入，而不是只做一次完整内容写入");
    QVERIFY2(device.maxWriteSize() < expected.size(),
             "最大单次写入不应等于完整导出文件大小");
}

void TestDataExporter::testBinaryExportToDeviceStreamsAndReportsProgress()
{
    DataExporter exporter;
    ExportOptions options;
    options.format = ExportFormat::Binary;
    options.filterByDirection = true;
    options.receiveOnly = true;
    exporter.setOptions(options);

    const QByteArray rxOne = QByteArray(4096, '\x11');
    const QByteArray txPayload = QByteArray(4096, '\x22');
    const QByteArray rxTwo = QByteArray(4096, '\x33');
    exporter.addRecord(makeRecord(rxOne, true));
    exporter.addRecord(makeRecord(txPayload, false));
    exporter.addRecord(makeRecord(rxTwo, true));

    QByteArray output;
    TrackingBuffer device(&output);
    QVERIFY(device.open(QIODevice::WriteOnly));

    QSignalSpy progressSpy(&exporter, &DataExporter::exportProgress);

    QVERIFY(exporter.exportToDevice(&device));
    device.close();

    QCOMPARE(output, rxOne + rxTwo);
    QCOMPARE(progressSpy.size(), 2);
    QCOMPARE(progressSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(progressSpy.at(0).at(1).toInt(), 2);
    QCOMPARE(progressSpy.at(1).at(0).toInt(), 2);
    QCOMPARE(progressSpy.at(1).at(1).toInt(), 2);
    QCOMPARE(exporter.statistics().exportedBytes, static_cast<qint64>(rxOne.size() + rxTwo.size()));
    QVERIFY2(device.maxWriteSize() <= rxOne.size(),
             "二进制导出应按记录写入，不能先拼完整 QByteArray");
}

void TestDataExporter::testExportToFileReportsEncodedFileSize()
{
    DataExporter exporter;
    ExportOptions options;
    options.format = ExportFormat::PlainText;
    options.encoding = QStringLiteral("UTF-16LE");
    options.includeTimestamp = false;
    options.includeDirection = false;
    options.lineSeparator = QStringLiteral("\n");
    exporter.setOptions(options);
    exporter.addRecord(makeRecord(QByteArray("abc")));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString filePath = tempDir.filePath(QStringLiteral("export.txt"));

    QVERIFY(exporter.exportToFile(filePath));

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    file.close();

    /*
     * UTF-16LE 文件每个 ASCII 字符占两个字节。统计值必须来自真实
     * 写入字节数/文件大小，而不能固定用 UTF-8 重新估算。
     */
    QVERIFY(bytes.size() > QByteArray("abc\n").size());
    QCOMPARE(exporter.statistics().exportedBytes, static_cast<qint64>(bytes.size()));
    QCOMPARE(exporter.statistics().fileSizeBytes, static_cast<qint64>(bytes.size()));
}
