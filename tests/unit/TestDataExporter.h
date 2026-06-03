/**
 * @file TestDataExporter.h
 * @brief 数据导出器单元测试头文件
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#ifndef TESTDATAEXPORTER_H
#define TESTDATAEXPORTER_H

#include <QObject>
#include <QTest>

/**
 * @brief 验证 DataExporter 的真实导出路径、过滤一致性和流式写入策略。
 */
class TestDataExporter : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 文本导出到 QIODevice 时应逐条写入，不应先拼接完整导出文本。
     */
    void testPlainTextExportToDeviceWritesSmallChunksAndKeepsFiltering();

    /**
     * @brief 二进制导出应逐条写入匹配记录，并对每条已导出记录发送进度。
     */
    void testBinaryExportToDeviceStreamsAndReportsProgress();

    /**
     * @brief 文件导出的 exportedBytes 应按实际编码后的文件大小统计。
     */
    void testExportToFileReportsEncodedFileSize();
};

#endif // TESTDATAEXPORTER_H
