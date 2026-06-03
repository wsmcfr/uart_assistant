/**
 * @file DataExporter.cpp
 * @brief 增强数据导出模块实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "DataExporter.h"
#include <QFile>
#include <QFileInfo>
#include <QTextCodec>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QXmlStreamWriter>
#include <QRegularExpression>
#include <QElapsedTimer>

namespace ComAssistant {

namespace {

/**
 * @brief 判断单条记录是否满足当前导出过滤选项。
 * @param record 待检查的原始收发记录。
 * @param options 导出对话框或调用方设置的过滤选项。
 * @param contentRegex 已编译的内容过滤正则。
 * @param contentFilterActive 内容过滤是否应生效。
 * @return true 表示该记录应进入导出、预览或过滤统计。
 */
bool recordMatchesExportOptions(const DataRecord& record,
                                const ExportOptions& options,
                                const QRegularExpression& contentRegex,
                                bool contentFilterActive)
{
    // 方向过滤：根据用户选择只保留 RX 或 TX。
    if (options.filterByDirection) {
        if (options.receiveOnly && !record.isReceive) {
            return false;
        }
        if (!options.receiveOnly && record.isReceive) {
            return false;
        }
    }

    // 时间过滤：边界包含起止时间，便于用户精确导出某一段抓包。
    if (options.filterByTime) {
        if (record.timestamp < options.startTime) {
            return false;
        }
        if (record.timestamp > options.endTime) {
            return false;
        }
    }

    /*
     * 内容过滤只在正则合法时生效。非法正则不丢弃数据，避免用户输入
     * 半截表达式时统计突然变成 0，真实导出路径也保持同样语义。
     */
    if (contentFilterActive) {
        const QString dataStr = QString::fromUtf8(record.data);
        bool matches = contentRegex.match(dataStr).hasMatch();
        if (options.invertContentFilter) {
            matches = !matches;
        }
        if (!matches) {
            return false;
        }
    }

    return true;
}

/**
 * @brief 为内容过滤准备正则对象和生效标记。
 * @param options 导出选项。
 * @param contentRegex 输出的正则对象。
 * @return true 表示内容过滤应参与匹配。
 */
bool prepareContentFilter(const ExportOptions& options,
                          QRegularExpression& contentRegex)
{
    if (!options.filterByContent || options.contentFilter.isEmpty()) {
        return false;
    }

    contentRegex = QRegularExpression(options.contentFilter);
    return contentRegex.isValid();
}

/**
 * @brief 统计一次流式导出所需的过滤结果。
 *
 * 真实导出需要提前知道进度条总数和 HTML/JSON/XML 头部中的记录数。
 * 这里仅做计数和字节累计，不复制 DataRecord，也不生成导出内容。
 */
struct StreamExportPlan {
    QRegularExpression contentRegex; ///< 已编译的内容过滤正则，非法正则时不参与匹配。
    bool contentFilterActive = false; ///< true 表示内容过滤可用于匹配。
    int exportedRecords = 0;          ///< 过滤后真正写出的记录数量。
    qint64 totalBytes = 0;            ///< 原始记录载荷总字节数，用于统计显示。
};

/**
 * @brief 创建流式导出的过滤计划。
 * @param records 全量原始收发记录。
 * @param options 当前导出选项。
 * @return 只包含计数、字节数和已编译过滤条件的导出计划。
 */
StreamExportPlan buildStreamExportPlan(const QVector<DataRecord>& records,
                                       const ExportOptions& options)
{
    StreamExportPlan plan;
    plan.contentFilterActive = prepareContentFilter(options, plan.contentRegex);

    for (const auto& record : records) {
        plan.totalBytes += record.data.size();
        if (recordMatchesExportOptions(record,
                                      options,
                                      plan.contentRegex,
                                      plan.contentFilterActive)) {
            ++plan.exportedRecords;
        }
    }

    return plan;
}

/**
 * @brief 将字符串编码成 JSON 字符串字面量。
 * @param text 待转义文本。
 * @return 包含双引号的 JSON 字符串。
 */
QString toJsonStringLiteral(const QString& text)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact))
        .mid(1)
        .chopped(1);
}

/**
 * @brief 把文本按用户配置编码后写入 QIODevice，并累计真实写入字节数。
 *
 * QTextStream 不方便精确统计编码后的每次写入字节数，也容易让测试无法
 * 区分“完整拼接后一次写入”和“逐条写入”。该类用 QTextCodec 的状态化
 * 转换器手动编码，再通过 writeAll 处理部分写入。
 */
class EncodedDeviceWriter
{
public:
    /**
     * @brief 构造一个编码写入器。
     * @param device 已打开且可写的目标设备。
     * @param options 当前导出选项，用于解析文本编码。
     * @param bytesWritten 成功写入的字节数累计位置。
     * @param errorString 写入失败时填充的错误说明。
     */
    EncodedDeviceWriter(QIODevice* device,
                        const ExportOptions& options,
                        qint64* bytesWritten,
                        QString* errorString)
        : m_device(device)
        , m_codec(QTextCodec::codecForName(options.encoding.toUtf8()))
        , m_bytesWritten(bytesWritten)
        , m_errorString(errorString)
    {
        // 编码名称不存在时回退 UTF-8，保持导出不中断且与旧 QTextStream 默认行为接近。
        if (!m_codec) {
            m_codec = QTextCodec::codecForName("UTF-8");
        }
        /*
         * QTextCodec 的状态化转换默认可能为 UTF-8/UTF-16 写入 BOM。
         * 旧导出路径未主动写 BOM；流式导出保持这一行为，避免导出文件
         * 开头多出隐藏字节，也让统计值对应用户实际看到的内容。
         */
        m_encodingState.flags = QTextCodec::IgnoreHeader;
    }

    /**
     * @brief 写入一段文本。
     * @param text 待写入文本。
     * @return true 表示完整写入；false 表示底层设备写入失败。
     */
    bool writeString(const QString& text)
    {
        if (text.isEmpty()) {
            return true;
        }

        const QByteArray encoded =
            m_codec->fromUnicode(text.constData(), text.size(), &m_encodingState);
        return writeBytes(encoded);
    }

    /**
     * @brief 写入一段原始字节。
     * @param bytes 待写入字节。
     * @return true 表示完整写入；false 表示底层设备写入失败。
     */
    bool writeBytes(const QByteArray& bytes)
    {
        if (bytes.isEmpty()) {
            return true;
        }

        qint64 offset = 0;
        while (offset < bytes.size()) {
            const qint64 written = m_device->write(bytes.constData() + offset,
                                                   bytes.size() - offset);
            if (written <= 0) {
                if (m_errorString) {
                    *m_errorString = QObject::tr("写入文件失败: %1")
                                         .arg(m_device ? m_device->errorString() : QString());
                }
                return false;
            }
            offset += written;
        }

        if (m_bytesWritten) {
            *m_bytesWritten += bytes.size();
        }
        return true;
    }

    /**
     * @brief 返回真实使用的文本编码名称。
     * @return Qt 编码器的规范名称；编码器缺失时返回 UTF-8。
     */
    QString encodingName() const
    {
        if (!m_codec) {
            return QStringLiteral("UTF-8");
        }
        return QString::fromLatin1(m_codec->name());
    }

private:
    QIODevice* m_device = nullptr;                         ///< 目标设备，不拥有生命周期。
    QTextCodec* m_codec = nullptr;                         ///< 文本编码器，由 Qt 管理生命周期。
    QTextCodec::ConverterState m_encodingState;            ///< 保持 UTF-16 等状态化编码连续性。
    qint64* m_bytesWritten = nullptr;                       ///< 导出统计中的真实写入字节数。
    QString* m_errorString = nullptr;                       ///< 失败时返回给调用方的错误文本。
};

} // namespace

// ============== DataRecord ==============

DataRecord DataRecord::fromReceive(const QByteArray& data, const QString& source)
{
    DataRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.data = data;
    record.isReceive = true;
    record.source = source;
    return record;
}

DataRecord DataRecord::fromSend(const QByteArray& data, const QString& source)
{
    DataRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.data = data;
    record.isReceive = false;
    record.source = source;
    return record;
}

// ============== DataExporter ==============

DataExporter::DataExporter(QObject* parent)
    : QObject(parent)
{
}

void DataExporter::setOptions(const ExportOptions& options)
{
    m_options = options;
}

void DataExporter::addRecord(const DataRecord& record)
{
    m_records.append(record);
}

void DataExporter::addRecords(const QVector<DataRecord>& records)
{
    m_records.append(records);
}

void DataExporter::clearRecords()
{
    m_records.clear();
    m_records.squeeze();
}

QVector<DataRecord> DataExporter::filteredRecords(int maxRecords) const
{
    /*
     * 过滤逻辑集中在 DataExporter 中，ExportDialog、文件导出和未来流式导出
     * 都复用同一套规则，避免预览/统计与真实导出结果出现分叉。
     */
    QVector<DataRecord> filtered;
    if (maxRecords != 0) {
        const int reserveCount = maxRecords > 0
            ? qMin(maxRecords, m_records.size())
            : m_records.size();
        filtered.reserve(reserveCount);
    }

    QRegularExpression contentRegex;
    const bool contentFilterActive = prepareContentFilter(m_options, contentRegex);

    for (const auto& record : m_records) {
        if (!recordMatchesExportOptions(record,
                                        m_options,
                                        contentRegex,
                                        contentFilterActive)) {
            continue;
        }

        if (maxRecords != 0) {
            filtered.append(record);
            if (maxRecords > 0 && filtered.size() >= maxRecords) {
                break;
            }
        }
    }

    return filtered;
}

int DataExporter::filteredRecordCount() const
{
    /*
     * 统计路径只计数不生成记录副本，也不拼接导出字符串。这样导出对话框
     * 刷新过滤条件时不会因为大量历史记录产生额外峰值内存。
     */
    QRegularExpression contentRegex;
    const bool contentFilterActive = prepareContentFilter(m_options, contentRegex);

    int count = 0;
    for (const auto& record : m_records) {
        if (recordMatchesExportOptions(record,
                                      m_options,
                                      contentRegex,
                                      contentFilterActive)) {
            ++count;
        }
    }
    return count;
}

bool DataExporter::exportToFile(const QString& filePath)
{
    /*
     * 文件导出是用户导出大量历史记录时的关键路径，因此这里不再先
     * filteredRecords() 复制匹配记录，也不再 exportRecordsToString()
     * 拼接完整文件内容，而是打开文件后委托流式核心逐条写出。
     */
    QElapsedTimer timer;
    timer.start();

    QFile file(filePath);
    /*
     * 这里不使用 QIODevice::Text。流式写入器会先按用户选择的编码生成
     * 字节，再直接写入设备；若让 QFile 在字节层面做换行转换，UTF-16LE
     * 等多字节编码会被破坏。
     */
    const QIODevice::OpenMode openMode = QIODevice::WriteOnly;
    if (!file.open(openMode)) {
        emit exportFinished(false, tr("无法打开文件: %1").arg(file.errorString()));
        return false;
    }

    QString errorString;
    const bool success = exportToDeviceInternal(&file, &errorString);
    file.close();

    m_statistics.fileSizeBytes = QFileInfo(filePath).size();
    m_statistics.duration = QString::number(timer.elapsed()) + " ms";

    emit exportFinished(success, success ? tr("导出完成") : errorString);
    return success;
}

bool DataExporter::exportToDevice(QIODevice* device)
{
    /*
     * QIODevice 入口用于测试和未来扩展。它与文件导出共享同一套流式
     * 写入逻辑，避免文件路径和内存设备路径出现过滤或格式差异。
     */
    QElapsedTimer timer;
    timer.start();

    QString errorString;
    const bool success = exportToDeviceInternal(device, &errorString);
    if (device && device->isOpen()) {
        m_statistics.fileSizeBytes = device->size();
    }
    m_statistics.duration = QString::number(timer.elapsed()) + " ms";

    emit exportFinished(success, success ? tr("导出完成") : errorString);
    return success;
}

QString DataExporter::exportToString()
{
    return exportRecordsToString(filteredRecords());
}

QString DataExporter::exportRecordsToString(const QVector<DataRecord>& records)
{
    /*
     * 文件导出、字符串导出和预览都走同一个格式分发函数。调用方可先完成
     * 过滤后再传入 records，避免同一次导出反复扫描完整历史。
     */
    switch (m_options.format) {
    case ExportFormat::PlainText:
        return exportPlainText(records);
    case ExportFormat::Csv:
        return exportCsv(records);
    case ExportFormat::Html:
        return exportHtml(records);
    case ExportFormat::Json:
        return exportJson(records);
    case ExportFormat::Xml:
        return exportXml(records);
    case ExportFormat::HexDump:
        return exportHexDump(records);
    default:
        return QString();
    }
}

QByteArray DataExporter::exportToBytes()
{
    if (m_options.format == ExportFormat::Binary) {
        return exportBinary(filteredRecords());
    }
    return exportToString().toUtf8();
}

bool DataExporter::exportToDeviceInternal(QIODevice* device, QString* errorString)
{
    /*
     * 真实导出统一走该函数。实现分两次线性扫描：第一次只构建统计和进度
     * 总数，第二次逐条写出匹配记录。这样既能保留准确统计，又不会在内存
     * 中复制过滤结果或聚合完整文件内容。
     */
    m_statistics = ExportStatistics();
    m_statistics.totalRecords = m_records.size();

    if (!device) {
        if (errorString) {
            *errorString = tr("导出目标无效");
        }
        return false;
    }

    if (!device->isOpen() || !device->isWritable()) {
        if (errorString) {
            *errorString = tr("导出目标未打开或不可写");
        }
        return false;
    }

    const StreamExportPlan plan = buildStreamExportPlan(m_records, m_options);
    m_statistics.totalBytes = plan.totalBytes;
    m_statistics.exportedRecords = plan.exportedRecords;
    m_statistics.filteredRecords = m_statistics.totalRecords - m_statistics.exportedRecords;

    EncodedDeviceWriter writer(device, m_options, &m_statistics.exportedBytes, errorString);

    int writtenRecords = 0;
    int globalOffset = 0;

    auto emitProgressForRecord = [&]() {
        ++writtenRecords;
        emit exportProgress(writtenRecords, plan.exportedRecords);
    };

    switch (m_options.format) {
    case ExportFormat::PlainText: {
        for (const auto& record : m_records) {
            if (!recordMatchesExportOptions(record,
                                            m_options,
                                            plan.contentRegex,
                                            plan.contentFilterActive)) {
                continue;
            }

            QString line;
            if (m_options.includeLineNumber) {
                line += QString("%1: ").arg(writtenRecords + 1, 6);
            }
            if (m_options.includeTimestamp) {
                line += "[" + record.timestamp.toString(m_options.timestampFormat) + "] ";
            }
            if (m_options.includeDirection) {
                line += record.isReceive ? "[RX] " : "[TX] ";
            }
            if (m_options.includeSource && !record.source.isEmpty()) {
                line += "[" + record.source + "] ";
            }
            line += formatData(record.data);
            line += m_options.lineSeparator;

            if (!writer.writeString(line)) {
                return false;
            }
            emitProgressForRecord();
        }
        return true;
    }
    case ExportFormat::Csv: {
        QStringList headers;
        if (m_options.includeLineNumber) headers << "序号";
        if (m_options.includeTimestamp) headers << "时间戳";
        if (m_options.includeDirection) headers << "方向";
        if (m_options.includeSource) headers << "来源";
        headers << "数据";
        if (m_options.hexFormat) headers << "HEX";
        headers << "长度";

        if (!writer.writeString(headers.join(m_options.csvSeparator) + m_options.lineSeparator)) {
            return false;
        }

        for (const auto& record : m_records) {
            if (!recordMatchesExportOptions(record,
                                            m_options,
                                            plan.contentRegex,
                                            plan.contentFilterActive)) {
                continue;
            }

            QStringList fields;
            if (m_options.includeLineNumber) {
                fields << QString::number(writtenRecords + 1);
            }
            if (m_options.includeTimestamp) {
                fields << escapeCsv(record.timestamp.toString(m_options.timestampFormat));
            }
            if (m_options.includeDirection) {
                fields << (record.isReceive ? "RX" : "TX");
            }
            if (m_options.includeSource) {
                fields << escapeCsv(record.source);
            }
            fields << escapeCsv(QString::fromUtf8(record.data));
            if (m_options.hexFormat) {
                fields << escapeCsv(record.data.toHex(' ').toUpper());
            }
            fields << QString::number(record.data.size());

            if (!writer.writeString(fields.join(m_options.csvSeparator) + m_options.lineSeparator)) {
                return false;
            }
            emitProgressForRecord();
        }
        return true;
    }
    case ExportFormat::Html: {
        const bool isDark = (m_options.htmlTheme == "dark");
        QString header;
        header += "<!DOCTYPE html>\n<html>\n<head>\n";
        header += "    <meta charset=\"" + escapeHtml(writer.encodingName()) + "\">\n";
        header += "    <title>" + escapeHtml(m_options.htmlTitle) + "</title>\n";
        header += "    <style>\n";
        if (isDark) {
            header += "        body { font-family: 'Consolas', monospace; margin: 20px; "
                      "background: #1e1e1e; color: #d4d4d4; }\n";
            header += "        table { border-collapse: collapse; width: 100%; }\n";
            header += "        th, td { border: 1px solid #444; padding: 8px; text-align: left; }\n";
            header += "        th { background-color: #2d2d2d; }\n";
            header += "        tr:nth-child(even) { background-color: #252526; }\n";
            header += "        .tx { color: #569cd6; }\n";
            header += "        .rx { color: #4ec9b0; }\n";
        } else {
            header += "        body { font-family: 'Consolas', monospace; margin: 20px; }\n";
            header += "        table { border-collapse: collapse; width: 100%; }\n";
            header += "        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
            header += "        th { background-color: #4CAF50; color: white; }\n";
            header += "        tr:nth-child(even) { background-color: #f2f2f2; }\n";
            header += "        .tx { color: #2196F3; }\n";
            header += "        .rx { color: #4CAF50; }\n";
        }
        header += "        .hex { font-family: 'Consolas', monospace; font-size: 12px; }\n";
        header += "        .timestamp { font-size: 12px; opacity: 0.8; }\n";
        header += "    </style>\n</head>\n<body>\n";
        header += "    <h1>" + escapeHtml(m_options.htmlTitle) + "</h1>\n";
        header += "    <p>导出时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "</p>\n";
        header += "    <p>记录数量: " + QString::number(plan.exportedRecords) + "</p>\n";
        header += "    <table>\n        <tr>\n";
        if (m_options.includeLineNumber) header += "            <th>#</th>\n";
        if (m_options.includeTimestamp) header += "            <th>时间戳</th>\n";
        if (m_options.includeDirection) header += "            <th>方向</th>\n";
        if (m_options.includeSource) header += "            <th>来源</th>\n";
        header += "            <th>数据</th>\n";
        if (m_options.hexFormat) header += "            <th>HEX</th>\n";
        header += "            <th>长度</th>\n";
        header += "        </tr>\n";

        if (!writer.writeString(header)) {
            return false;
        }

        for (const auto& record : m_records) {
            if (!recordMatchesExportOptions(record,
                                            m_options,
                                            plan.contentRegex,
                                            plan.contentFilterActive)) {
                continue;
            }

            QString row;
            row += "        <tr>\n";
            if (m_options.includeLineNumber) {
                row += "            <td>" + QString::number(writtenRecords + 1) + "</td>\n";
            }
            if (m_options.includeTimestamp) {
                row += "            <td class=\"timestamp\">" +
                       escapeHtml(record.timestamp.toString(m_options.timestampFormat)) + "</td>\n";
            }
            if (m_options.includeDirection) {
                const QString cls = record.isReceive ? "rx" : "tx";
                const QString dir = record.isReceive ? "RX" : "TX";
                row += "            <td class=\"" + cls + "\">" + dir + "</td>\n";
            }
            if (m_options.includeSource) {
                row += "            <td>" + escapeHtml(record.source) + "</td>\n";
            }
            row += "            <td>" + escapeHtml(QString::fromUtf8(record.data)) + "</td>\n";
            if (m_options.hexFormat) {
                row += "            <td class=\"hex\">" +
                       escapeHtml(QString::fromLatin1(record.data.toHex(' ').toUpper())) + "</td>\n";
            }
            row += "            <td>" + QString::number(record.data.size()) + "</td>\n";
            row += "        </tr>\n";

            if (!writer.writeString(row)) {
                return false;
            }
            emitProgressForRecord();
        }

        return writer.writeString("    </table>\n</body>\n</html>\n");
    }
    case ExportFormat::Json: {
        QString header;
        header += "{\n";
        header += "    \"exportTime\": " +
                  toJsonStringLiteral(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)) +
                  ",\n";
        header += "    \"recordCount\": " + QString::number(plan.exportedRecords) + ",\n";
        header += "    \"options\": {\n";
        header += "        \"encoding\": " + toJsonStringLiteral(writer.encodingName()) + ",\n";
        header += QString("        \"hexFormat\": %1\n")
                      .arg(m_options.hexFormat ? "true" : "false");
        header += "    },\n";
        header += "    \"records\": [\n";
        if (!writer.writeString(header)) {
            return false;
        }

        bool first = true;
        for (const auto& record : m_records) {
            if (!recordMatchesExportOptions(record,
                                            m_options,
                                            plan.contentRegex,
                                            plan.contentFilterActive)) {
                continue;
            }

            QJsonObject obj;
            if (m_options.includeTimestamp) {
                obj["timestamp"] = record.timestamp.toString(Qt::ISODateWithMs);
            }
            if (m_options.includeDirection) {
                obj["direction"] = record.isReceive ? "RX" : "TX";
            }
            if (m_options.includeSource && !record.source.isEmpty()) {
                obj["source"] = record.source;
            }
            obj["data"] = QString::fromUtf8(record.data);
            if (m_options.hexFormat) {
                obj["hex"] = QString::fromLatin1(record.data.toHex(' ').toUpper());
            }
            obj["length"] = record.data.size();
            if (!record.note.isEmpty()) {
                obj["note"] = record.note;
            }

            QJsonDocument doc(obj);
            QString recordJson = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
            recordJson.replace("\n", "\n        ");
            QString chunk = first ? "        " : ",\n        ";
            chunk += recordJson.trimmed();
            if (!writer.writeString(chunk)) {
                return false;
            }

            first = false;
            emitProgressForRecord();
        }

        return writer.writeString("\n    ]\n}\n");
    }
    case ExportFormat::Xml: {
        QString header;
        header += "<?xml version=\"1.0\" encoding=\"" +
                  escapeXml(writer.encodingName()) +
                  "\"?>\n";
        header += "<SerialDataExport exportTime=\"" +
                  escapeXml(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)) +
                  "\" recordCount=\"" + QString::number(plan.exportedRecords) + "\">\n";
        if (!writer.writeString(header)) {
            return false;
        }

        for (const auto& record : m_records) {
            if (!recordMatchesExportOptions(record,
                                            m_options,
                                            plan.contentRegex,
                                            plan.contentFilterActive)) {
                continue;
            }

            QString chunk;
            chunk += "    <Record index=\"" + QString::number(writtenRecords + 1) + "\">\n";
            if (m_options.includeTimestamp) {
                chunk += "        <Timestamp>" +
                         escapeXml(record.timestamp.toString(Qt::ISODateWithMs)) +
                         "</Timestamp>\n";
            }
            if (m_options.includeDirection) {
                chunk += "        <Direction>";
                chunk += record.isReceive ? "RX" : "TX";
                chunk += "</Direction>\n";
            }
            if (m_options.includeSource && !record.source.isEmpty()) {
                chunk += "        <Source>" + escapeXml(record.source) + "</Source>\n";
            }
            chunk += "        <Data>" + escapeXml(QString::fromUtf8(record.data)) + "</Data>\n";
            if (m_options.hexFormat) {
                chunk += "        <Hex>" +
                         escapeXml(QString::fromLatin1(record.data.toHex(' ').toUpper())) +
                         "</Hex>\n";
            }
            chunk += "        <Length>" + QString::number(record.data.size()) + "</Length>\n";
            chunk += "    </Record>\n";

            if (!writer.writeString(chunk)) {
                return false;
            }
            emitProgressForRecord();
        }

        return writer.writeString("</SerialDataExport>\n");
    }
    case ExportFormat::Binary: {
        for (const auto& record : m_records) {
            if (!recordMatchesExportOptions(record,
                                            m_options,
                                            plan.contentRegex,
                                            plan.contentFilterActive)) {
                continue;
            }
            if (!writer.writeBytes(record.data)) {
                return false;
            }
            emitProgressForRecord();
        }
        return true;
    }
    case ExportFormat::HexDump: {
        for (const auto& record : m_records) {
            if (!recordMatchesExportOptions(record,
                                            m_options,
                                            plan.contentRegex,
                                            plan.contentFilterActive)) {
                continue;
            }

            QString chunk;
            if (m_options.includeTimestamp || m_options.includeDirection) {
                chunk += "--- ";
                if (m_options.includeTimestamp) {
                    chunk += "[" + record.timestamp.toString(m_options.timestampFormat) + "] ";
                }
                if (m_options.includeDirection) {
                    chunk += record.isReceive ? "[RX]" : "[TX]";
                }
                chunk += " ---" + m_options.lineSeparator;
            }
            chunk += formatHexDump(record.data, globalOffset);
            globalOffset += record.data.size();

            if (!writer.writeString(chunk)) {
                return false;
            }
            emitProgressForRecord();
        }
        return true;
    }
    }

    if (errorString) {
        *errorString = tr("不支持的导出格式");
    }
    return false;
}

QString DataExporter::exportPlainText(const QVector<DataRecord>& records)
{
    QString result;
    int lineNum = 0;

    for (const auto& record : records) {
        QString line;
        lineNum++;

        if (m_options.includeLineNumber) {
            line += QString("%1: ").arg(lineNum, 6);
        }

        if (m_options.includeTimestamp) {
            line += "[" + record.timestamp.toString(m_options.timestampFormat) + "] ";
        }

        if (m_options.includeDirection) {
            line += record.isReceive ? "[RX] " : "[TX] ";
        }

        if (m_options.includeSource && !record.source.isEmpty()) {
            line += "[" + record.source + "] ";
        }

        line += formatData(record.data);
        result += line + m_options.lineSeparator;

        emit exportProgress(lineNum, records.size());
    }

    return result;
}

QString DataExporter::exportCsv(const QVector<DataRecord>& records)
{
    QString result;
    QString sep = m_options.csvSeparator;

    // 表头
    QStringList headers;
    if (m_options.includeLineNumber) headers << "序号";
    if (m_options.includeTimestamp) headers << "时间戳";
    if (m_options.includeDirection) headers << "方向";
    if (m_options.includeSource) headers << "来源";
    headers << "数据";
    if (m_options.hexFormat) headers << "HEX";
    headers << "长度";

    result += headers.join(sep) + m_options.lineSeparator;

    // 数据
    int lineNum = 0;
    for (const auto& record : records) {
        QStringList fields;
        lineNum++;

        if (m_options.includeLineNumber) {
            fields << QString::number(lineNum);
        }

        if (m_options.includeTimestamp) {
            fields << escapeCsv(record.timestamp.toString(m_options.timestampFormat));
        }

        if (m_options.includeDirection) {
            fields << (record.isReceive ? "RX" : "TX");
        }

        if (m_options.includeSource) {
            fields << escapeCsv(record.source);
        }

        fields << escapeCsv(QString::fromUtf8(record.data));

        if (m_options.hexFormat) {
            fields << escapeCsv(record.data.toHex(' ').toUpper());
        }

        fields << QString::number(record.data.size());

        result += fields.join(sep) + m_options.lineSeparator;
        emit exportProgress(lineNum, records.size());
    }

    return result;
}

QString DataExporter::exportHtml(const QVector<DataRecord>& records)
{
    QString result;
    QString theme = m_options.htmlTheme;
    bool isDark = (theme == "dark");

    // HTML头部
    result += "<!DOCTYPE html>\n<html>\n<head>\n";
    result += "    <meta charset=\"UTF-8\">\n";
    result += "    <title>" + escapeHtml(m_options.htmlTitle) + "</title>\n";
    result += "    <style>\n";

    if (isDark) {
        result += "        body { font-family: 'Consolas', monospace; margin: 20px; "
                  "background: #1e1e1e; color: #d4d4d4; }\n";
        result += "        table { border-collapse: collapse; width: 100%; }\n";
        result += "        th, td { border: 1px solid #444; padding: 8px; text-align: left; }\n";
        result += "        th { background-color: #2d2d2d; }\n";
        result += "        tr:nth-child(even) { background-color: #252526; }\n";
        result += "        .tx { color: #569cd6; }\n";
        result += "        .rx { color: #4ec9b0; }\n";
    } else {
        result += "        body { font-family: 'Consolas', monospace; margin: 20px; }\n";
        result += "        table { border-collapse: collapse; width: 100%; }\n";
        result += "        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
        result += "        th { background-color: #4CAF50; color: white; }\n";
        result += "        tr:nth-child(even) { background-color: #f2f2f2; }\n";
        result += "        .tx { color: #2196F3; }\n";
        result += "        .rx { color: #4CAF50; }\n";
    }

    result += "        .hex { font-family: 'Consolas', monospace; font-size: 12px; }\n";
    result += "        .timestamp { font-size: 12px; opacity: 0.8; }\n";
    result += "    </style>\n</head>\n<body>\n";

    result += "    <h1>" + escapeHtml(m_options.htmlTitle) + "</h1>\n";
    result += "    <p>导出时间: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + "</p>\n";
    result += "    <p>记录数量: " + QString::number(records.size()) + "</p>\n";

    // 表格
    result += "    <table>\n        <tr>\n";
    if (m_options.includeLineNumber) result += "            <th>#</th>\n";
    if (m_options.includeTimestamp) result += "            <th>时间戳</th>\n";
    if (m_options.includeDirection) result += "            <th>方向</th>\n";
    if (m_options.includeSource) result += "            <th>来源</th>\n";
    result += "            <th>数据</th>\n";
    if (m_options.hexFormat) result += "            <th>HEX</th>\n";
    result += "            <th>长度</th>\n";
    result += "        </tr>\n";

    int lineNum = 0;
    for (const auto& record : records) {
        lineNum++;
        result += "        <tr>\n";

        if (m_options.includeLineNumber) {
            result += "            <td>" + QString::number(lineNum) + "</td>\n";
        }

        if (m_options.includeTimestamp) {
            result += "            <td class=\"timestamp\">" +
                      escapeHtml(record.timestamp.toString(m_options.timestampFormat)) + "</td>\n";
        }

        if (m_options.includeDirection) {
            QString cls = record.isReceive ? "rx" : "tx";
            QString dir = record.isReceive ? "RX" : "TX";
            result += "            <td class=\"" + cls + "\">" + dir + "</td>\n";
        }

        if (m_options.includeSource) {
            result += "            <td>" + escapeHtml(record.source) + "</td>\n";
        }

        result += "            <td>" + escapeHtml(QString::fromUtf8(record.data)) + "</td>\n";

        if (m_options.hexFormat) {
            result += "            <td class=\"hex\">" +
                      escapeHtml(QString::fromLatin1(record.data.toHex(' ').toUpper())) + "</td>\n";
        }

        result += "            <td>" + QString::number(record.data.size()) + "</td>\n";
        result += "        </tr>\n";

        emit exportProgress(lineNum, records.size());
    }

    result += "    </table>\n</body>\n</html>\n";
    return result;
}

QString DataExporter::exportJson(const QVector<DataRecord>& records)
{
    QJsonObject root;
    root["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    root["recordCount"] = records.size();
    root["options"] = QJsonObject{
        {"hexFormat", m_options.hexFormat},
        {"encoding", m_options.encoding}
    };

    QJsonArray dataArray;
    int lineNum = 0;
    for (const auto& record : records) {
        lineNum++;
        QJsonObject obj;

        if (m_options.includeTimestamp) {
            obj["timestamp"] = record.timestamp.toString(Qt::ISODateWithMs);
        }

        if (m_options.includeDirection) {
            obj["direction"] = record.isReceive ? "RX" : "TX";
        }

        if (m_options.includeSource && !record.source.isEmpty()) {
            obj["source"] = record.source;
        }

        obj["data"] = QString::fromUtf8(record.data);

        if (m_options.hexFormat) {
            obj["hex"] = QString::fromLatin1(record.data.toHex(' ').toUpper());
        }

        obj["length"] = record.data.size();

        if (!record.note.isEmpty()) {
            obj["note"] = record.note;
        }

        dataArray.append(obj);
        emit exportProgress(lineNum, records.size());
    }

    root["records"] = dataArray;

    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString DataExporter::exportXml(const QVector<DataRecord>& records)
{
    QString result;
    QXmlStreamWriter xml(&result);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(4);

    xml.writeStartDocument();
    xml.writeStartElement("SerialDataExport");
    xml.writeAttribute("exportTime", QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    xml.writeAttribute("recordCount", QString::number(records.size()));

    int lineNum = 0;
    for (const auto& record : records) {
        lineNum++;
        xml.writeStartElement("Record");
        xml.writeAttribute("index", QString::number(lineNum));

        if (m_options.includeTimestamp) {
            xml.writeTextElement("Timestamp", record.timestamp.toString(Qt::ISODateWithMs));
        }

        if (m_options.includeDirection) {
            xml.writeTextElement("Direction", record.isReceive ? "RX" : "TX");
        }

        if (m_options.includeSource && !record.source.isEmpty()) {
            xml.writeTextElement("Source", record.source);
        }

        xml.writeTextElement("Data", QString::fromUtf8(record.data));

        if (m_options.hexFormat) {
            xml.writeTextElement("Hex", QString::fromLatin1(record.data.toHex(' ').toUpper()));
        }

        xml.writeTextElement("Length", QString::number(record.data.size()));

        xml.writeEndElement(); // Record

        emit exportProgress(lineNum, records.size());
    }

    xml.writeEndElement(); // SerialDataExport
    xml.writeEndDocument();

    return result;
}

QByteArray DataExporter::exportBinary(const QVector<DataRecord>& records)
{
    QByteArray result;

    for (const auto& record : records) {
        result.append(record.data);
    }

    return result;
}

QString DataExporter::exportHexDump(const QVector<DataRecord>& records)
{
    QString result;
    int globalOffset = 0;
    int lineNum = 0;

    for (const auto& record : records) {
        lineNum++;

        if (m_options.includeTimestamp || m_options.includeDirection) {
            result += "--- ";
            if (m_options.includeTimestamp) {
                result += "[" + record.timestamp.toString(m_options.timestampFormat) + "] ";
            }
            if (m_options.includeDirection) {
                result += record.isReceive ? "[RX]" : "[TX]";
            }
            result += " ---" + m_options.lineSeparator;
        }

        result += formatHexDump(record.data, globalOffset);
        globalOffset += record.data.size();

        emit exportProgress(lineNum, records.size());
    }

    return result;
}

QString DataExporter::formatData(const QByteArray& data) const
{
    if (m_options.hexFormat) {
        return QString::fromLatin1(data.toHex(' ').toUpper());
    }
    return QString::fromUtf8(data);
}

QString DataExporter::formatHexDump(const QByteArray& data, int offset) const
{
    QString result;
    int bytesPerLine = m_options.hexBytesPerLine;

    for (int i = 0; i < data.size(); i += bytesPerLine) {
        // 地址
        result += QString("%1  ").arg(offset + i, 8, 16, QChar('0')).toUpper();

        // 十六进制
        QString hexPart;
        QString asciiPart;

        for (int j = 0; j < bytesPerLine; ++j) {
            if (i + j < data.size()) {
                unsigned char byte = static_cast<unsigned char>(data[i + j]);
                hexPart += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();

                if (m_options.showPrintable && byte >= 0x20 && byte <= 0x7E) {
                    asciiPart += QChar(byte);
                } else {
                    asciiPart += '.';
                }
            } else {
                hexPart += "   ";
                asciiPart += ' ';
            }

            // 中间分隔
            if (j == bytesPerLine / 2 - 1) {
                hexPart += " ";
            }
        }

        result += hexPart;

        if (m_options.showPrintable) {
            result += " |" + asciiPart + "|";
        }

        result += m_options.lineSeparator;
    }

    return result;
}

QString DataExporter::escapeHtml(const QString& text) const
{
    QString result = text;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    result.replace("'", "&#39;");
    return result;
}

QString DataExporter::escapeCsv(const QString& text) const
{
    if (!m_options.csvQuoteStrings) {
        return text;
    }

    QString result = text;
    result.replace("\"", "\"\"");
    result.replace("\r", "\\r");
    result.replace("\n", "\\n");
    return "\"" + result + "\"";
}

QString DataExporter::escapeXml(const QString& text) const
{
    QString result = text;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    result.replace("'", "&apos;");
    return result;
}

QStringList DataExporter::supportedFormats()
{
    return {"TXT", "CSV", "HTML", "JSON", "XML", "BIN", "HEX"};
}

QString DataExporter::formatExtension(ExportFormat format)
{
    switch (format) {
    case ExportFormat::PlainText: return ".txt";
    case ExportFormat::Csv: return ".csv";
    case ExportFormat::Html: return ".html";
    case ExportFormat::Json: return ".json";
    case ExportFormat::Xml: return ".xml";
    case ExportFormat::Binary: return ".bin";
    case ExportFormat::HexDump: return ".hex";
    default: return ".txt";
    }
}

QString DataExporter::formatFilter(ExportFormat format)
{
    switch (format) {
    case ExportFormat::PlainText: return QObject::tr("文本文件 (*.txt)");
    case ExportFormat::Csv: return QObject::tr("CSV文件 (*.csv)");
    case ExportFormat::Html: return QObject::tr("HTML文件 (*.html)");
    case ExportFormat::Json: return QObject::tr("JSON文件 (*.json)");
    case ExportFormat::Xml: return QObject::tr("XML文件 (*.xml)");
    case ExportFormat::Binary: return QObject::tr("二进制文件 (*.bin)");
    case ExportFormat::HexDump: return QObject::tr("十六进制转储 (*.hex)");
    default: return QObject::tr("所有文件 (*)");
    }
}

QString DataExporter::allFormatsFilter()
{
    QStringList filters;
    filters << formatFilter(ExportFormat::PlainText);
    filters << formatFilter(ExportFormat::Csv);
    filters << formatFilter(ExportFormat::Html);
    filters << formatFilter(ExportFormat::Json);
    filters << formatFilter(ExportFormat::Xml);
    filters << formatFilter(ExportFormat::Binary);
    filters << formatFilter(ExportFormat::HexDump);
    filters << QObject::tr("所有文件 (*)");
    return filters.join(";;");
}

// ============== ExportTemplate ==============

bool ExportTemplate::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_errorString = QObject::tr("无法打开模板文件");
        return false;
    }

    m_template = QString::fromUtf8(file.readAll());
    return true;
}

bool ExportTemplate::loadFromString(const QString& templateStr)
{
    m_template = templateStr;
    return true;
}

QString ExportTemplate::render(const QVector<DataRecord>& records,
                              const ExportOptions& options) const
{
    QString result = m_template;

    // 处理全局变量
    QVariantMap globalVars;
    globalVars["EXPORT_TIME"] = QDateTime::currentDateTime().toString(options.timestampFormat);
    globalVars["RECORD_COUNT"] = records.size();

    result = processVariables(result, globalVars);
    result = processLoops(result, records, options);

    return result;
}

QString ExportTemplate::processVariables(const QString& text, const QVariantMap& variables) const
{
    QString result = text;
    for (auto it = variables.constBegin(); it != variables.constEnd(); ++it) {
        result.replace("{{" + it.key() + "}}", it.value().toString());
    }
    return result;
}

QString ExportTemplate::processLoops(const QString& text, const QVector<DataRecord>& records,
                                    const ExportOptions& options) const
{
    QString result = text;

    // 查找 {{#each records}} ... {{/each}} 块
    QRegularExpression loopRegex(R"(\{\{#each records\}\}(.*?)\{\{/each\}\})",
                                 QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatch match = loopRegex.match(result);
    if (match.hasMatch()) {
        QString loopTemplate = match.captured(1);
        QString loopResult;

        int index = 0;
        for (const auto& record : records) {
            QString itemResult = loopTemplate;
            itemResult.replace("{{INDEX}}", QString::number(++index));
            itemResult.replace("{{TIMESTAMP}}", record.timestamp.toString(options.timestampFormat));
            itemResult.replace("{{DIRECTION}}", record.isReceive ? "RX" : "TX");
            itemResult.replace("{{SOURCE}}", record.source);
            itemResult.replace("{{DATA}}", QString::fromUtf8(record.data));
            itemResult.replace("{{HEX}}", QString::fromLatin1(record.data.toHex(' ').toUpper()));
            itemResult.replace("{{LENGTH}}", QString::number(record.data.size()));
            loopResult += itemResult;
        }

        result.replace(match.captured(0), loopResult);
    }

    return result;
}

QString ExportTemplate::processConditions(const QString& text, const QVariantMap& variables) const
{
    QString result = text;

    // 简单条件处理: {{#if VAR}} ... {{/if}}
    QRegularExpression ifRegex(R"(\{\{#if (\w+)\}\}(.*?)\{\{/if\}\})",
                              QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatchIterator it = ifRegex.globalMatch(result);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        QString content = match.captured(2);

        if (variables.contains(varName) && variables[varName].toBool()) {
            result.replace(match.captured(0), content);
        } else {
            result.replace(match.captured(0), QString());
        }
    }

    return result;
}

} // namespace ComAssistant
