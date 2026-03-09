/**
 * @file DataExporter.cpp
 * @brief 增强数据导出模块实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "DataExporter.h"
#include "utils/Logger.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QXmlStreamWriter>
#include <QRegularExpression>
#include <QElapsedTimer>

namespace ComAssistant {

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
}

QVector<DataRecord> DataExporter::filterRecords() const
{
    QVector<DataRecord> filtered;
    QRegularExpression contentRegex;

    if (m_options.filterByContent && !m_options.contentFilter.isEmpty()) {
        contentRegex = QRegularExpression(m_options.contentFilter);
    }

    for (const auto& record : m_records) {
        // 方向过滤
        if (m_options.filterByDirection) {
            if (m_options.receiveOnly && !record.isReceive) continue;
            if (!m_options.receiveOnly && record.isReceive) continue;
        }

        // 时间过滤
        if (m_options.filterByTime) {
            if (record.timestamp < m_options.startTime) continue;
            if (record.timestamp > m_options.endTime) continue;
        }

        // 内容过滤
        if (m_options.filterByContent && contentRegex.isValid()) {
            QString dataStr = QString::fromUtf8(record.data);
            bool matches = contentRegex.match(dataStr).hasMatch();
            if (m_options.invertContentFilter) matches = !matches;
            if (!matches) continue;
        }

        filtered.append(record);
    }

    return filtered;
}

bool DataExporter::exportToFile(const QString& filePath)
{
    QElapsedTimer timer;
    timer.start();

    m_statistics = ExportStatistics();
    m_statistics.totalRecords = m_records.size();

    QVector<DataRecord> records = filterRecords();
    m_statistics.exportedRecords = records.size();
    m_statistics.filteredRecords = m_statistics.totalRecords - m_statistics.exportedRecords;

    for (const auto& record : m_records) {
        m_statistics.totalBytes += record.data.size();
    }

    QFile file(filePath);
    bool success = false;

    if (m_options.format == ExportFormat::Binary) {
        if (!file.open(QIODevice::WriteOnly)) {
            emit exportFinished(false, tr("无法打开文件: %1").arg(file.errorString()));
            return false;
        }
        QByteArray data = exportBinary(records);
        file.write(data);
        m_statistics.exportedBytes = data.size();
        success = true;
    } else {
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit exportFinished(false, tr("无法打开文件: %1").arg(file.errorString()));
            return false;
        }

        QString content = exportToString();
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::encodingForName(
            m_options.encoding.toUtf8()).value_or(QStringConverter::Utf8));
        stream << content;
        m_statistics.exportedBytes = content.toUtf8().size();
        success = true;
    }

    file.close();
    m_statistics.fileSizeBytes = QFileInfo(filePath).size();
    m_statistics.duration = QString::number(timer.elapsed()) + " ms";

    emit exportFinished(success, tr("导出完成"));
    return success;
}

QString DataExporter::exportToString()
{
    QVector<DataRecord> records = filterRecords();

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
        return exportBinary(filterRecords());
    }
    return exportToString().toUtf8();
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
