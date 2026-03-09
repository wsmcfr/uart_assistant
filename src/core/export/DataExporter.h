/**
 * @file DataExporter.h
 * @brief 增强数据导出模块
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_DATAEXPORTER_H
#define COMASSISTANT_DATAEXPORTER_H

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QVector>
#include <QVariantMap>

namespace ComAssistant {

/**
 * @brief 导出格式
 */
enum class ExportFormat {
    PlainText,      ///< 纯文本 (.txt)
    Csv,            ///< CSV表格 (.csv)
    Html,           ///< HTML网页 (.html)
    Json,           ///< JSON格式 (.json)
    Xml,            ///< XML格式 (.xml)
    Binary,         ///< 二进制 (.bin)
    HexDump         ///< 十六进制转储 (.hex)
};

/**
 * @brief 数据记录
 */
struct DataRecord {
    QDateTime timestamp;
    QByteArray data;
    bool isReceive = true;      ///< true=接收, false=发送
    QString source;             ///< 数据来源（如串口名）
    QString note;               ///< 备注

    static DataRecord fromReceive(const QByteArray& data, const QString& source = QString());
    static DataRecord fromSend(const QByteArray& data, const QString& source = QString());
};

/**
 * @brief 导出选项
 */
struct ExportOptions {
    // 格式选项
    ExportFormat format = ExportFormat::PlainText;
    QString encoding = "UTF-8";
    QString lineSeparator = "\r\n";

    // 内容选项
    bool includeTimestamp = true;
    bool includeDirection = true;
    bool includeSource = false;
    bool includeLineNumber = false;

    // 显示格式
    bool hexFormat = false;
    bool showPrintable = false;     ///< HEX模式下显示可打印字符
    int hexBytesPerLine = 16;       ///< HEX每行字节数

    // 时间格式
    QString timestampFormat = "yyyy-MM-dd hh:mm:ss.zzz";

    // 过滤选项
    bool filterByDirection = false;
    bool receiveOnly = true;        ///< filterByDirection时生效
    bool filterByTime = false;
    QDateTime startTime;
    QDateTime endTime;
    bool filterByContent = false;
    QString contentFilter;          ///< 正则表达式
    bool invertContentFilter = false;

    // CSV选项
    QString csvSeparator = ",";
    bool csvQuoteStrings = true;

    // HTML选项
    QString htmlTitle = "Serial Data Export";
    bool htmlSyntaxHighlight = true;
    QString htmlTheme = "light";    ///< light/dark

    // 模板选项
    bool useTemplate = false;
    QString templatePath;
};

/**
 * @brief 导出统计
 */
struct ExportStatistics {
    int totalRecords = 0;
    int exportedRecords = 0;
    int filteredRecords = 0;
    qint64 totalBytes = 0;
    qint64 exportedBytes = 0;
    qint64 fileSizeBytes = 0;
    QString duration;
};

/**
 * @brief 数据导出器
 */
class DataExporter : public QObject {
    Q_OBJECT

public:
    explicit DataExporter(QObject* parent = nullptr);
    ~DataExporter() override = default;

    /**
     * @brief 设置导出选项
     */
    void setOptions(const ExportOptions& options);
    ExportOptions options() const { return m_options; }

    /**
     * @brief 添加数据记录
     */
    void addRecord(const DataRecord& record);
    void addRecords(const QVector<DataRecord>& records);

    /**
     * @brief 清空数据
     */
    void clearRecords();

    /**
     * @brief 获取记录数量
     */
    int recordCount() const { return m_records.size(); }

    /**
     * @brief 导出到文件
     */
    bool exportToFile(const QString& filePath);

    /**
     * @brief 导出到字符串
     */
    QString exportToString();

    /**
     * @brief 导出到字节数组
     */
    QByteArray exportToBytes();

    /**
     * @brief 获取导出统计
     */
    ExportStatistics statistics() const { return m_statistics; }

    /**
     * @brief 获取支持的格式列表
     */
    static QStringList supportedFormats();

    /**
     * @brief 获取格式的文件扩展名
     */
    static QString formatExtension(ExportFormat format);

    /**
     * @brief 获取格式的过滤器字符串
     */
    static QString formatFilter(ExportFormat format);

    /**
     * @brief 获取所有格式的过滤器字符串
     */
    static QString allFormatsFilter();

signals:
    /**
     * @brief 导出进度
     */
    void exportProgress(int current, int total);

    /**
     * @brief 导出完成
     */
    void exportFinished(bool success, const QString& message);

private:
    QVector<DataRecord> filterRecords() const;

    QString exportPlainText(const QVector<DataRecord>& records);
    QString exportCsv(const QVector<DataRecord>& records);
    QString exportHtml(const QVector<DataRecord>& records);
    QString exportJson(const QVector<DataRecord>& records);
    QString exportXml(const QVector<DataRecord>& records);
    QByteArray exportBinary(const QVector<DataRecord>& records);
    QString exportHexDump(const QVector<DataRecord>& records);

    QString formatData(const QByteArray& data) const;
    QString formatHexDump(const QByteArray& data, int offset = 0) const;
    QString escapeHtml(const QString& text) const;
    QString escapeCsv(const QString& text) const;
    QString escapeXml(const QString& text) const;

private:
    ExportOptions m_options;
    QVector<DataRecord> m_records;
    ExportStatistics m_statistics;
};

/**
 * @brief 导出模板引擎
 */
class ExportTemplate {
public:
    ExportTemplate() = default;

    /**
     * @brief 加载模板
     */
    bool loadFromFile(const QString& filePath);
    bool loadFromString(const QString& templateStr);

    /**
     * @brief 渲染模板
     */
    QString render(const QVector<DataRecord>& records, const ExportOptions& options) const;

    /**
     * @brief 获取错误信息
     */
    QString errorString() const { return m_errorString; }

private:
    QString processVariables(const QString& text, const QVariantMap& variables) const;
    QString processLoops(const QString& text, const QVector<DataRecord>& records,
                        const ExportOptions& options) const;
    QString processConditions(const QString& text, const QVariantMap& variables) const;

private:
    QString m_template;
    QString m_errorString;
};

} // namespace ComAssistant

#endif // COMASSISTANT_DATAEXPORTER_H
