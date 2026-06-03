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
#include <QIODevice>
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
     * @brief 获取当前导出选项过滤后的记录。
     * @param maxRecords 最多返回的记录数；小于 0 表示返回全部，0 表示只计算不返回。
     * @return 按方向、时间和内容过滤后的记录副本，顺序与原始记录一致。
     */
    QVector<DataRecord> filteredRecords(int maxRecords = -1) const;

    /**
     * @brief 计算当前导出选项过滤后的记录数量。
     * @return 真实会进入导出的记录条数，不生成导出文本，避免统计时额外拼接大字符串。
     */
    int filteredRecordCount() const;

    /**
     * @brief 导出到文件。
     *
     * 该函数用于真实文件导出，会按当前过滤规则逐条写入文件，避免先复制
     * 过滤后的完整记录集合或拼接完整导出文本造成额外峰值内存。
     *
     * @param filePath 目标文件路径。
     * @return true 表示导出成功；false 表示打开文件或写入过程中失败。
     */
    bool exportToFile(const QString& filePath);

    /**
     * @brief 导出到已打开的 QIODevice。
     *
     * 调用方负责打开并保持 device 可写。该接口是文件导出的核心流式实现，
     * 也便于测试使用 QBuffer 验证写入分块和过滤一致性。
     *
     * @param device 已打开且可写的导出目标。
     * @return true 表示导出成功；false 表示设备无效、不可写或写入失败。
     */
    bool exportToDevice(QIODevice* device);

    /**
     * @brief 导出到字符串。
     *
     * 该函数保留给预览和旧调用方使用，会生成完整字符串；大量历史记录
     * 的真实文件导出应使用 exportToFile() 或 exportToDevice()。
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
    QString exportPlainText(const QVector<DataRecord>& records);
    QString exportCsv(const QVector<DataRecord>& records);
    QString exportHtml(const QVector<DataRecord>& records);
    QString exportJson(const QVector<DataRecord>& records);
    QString exportXml(const QVector<DataRecord>& records);
    QByteArray exportBinary(const QVector<DataRecord>& records);
    QString exportHexDump(const QVector<DataRecord>& records);
    QString exportRecordsToString(const QVector<DataRecord>& records);

    QString formatData(const QByteArray& data) const;
    QString formatHexDump(const QByteArray& data, int offset = 0) const;
    QString escapeHtml(const QString& text) const;
    QString escapeCsv(const QString& text) const;
    QString escapeXml(const QString& text) const;

    /**
     * @brief 执行真正的流式导出，不负责打开/关闭设备和发送完成信号。
     * @param device 已打开且可写的目标设备。
     * @param errorString 写入失败时返回给调用方的错误说明。
     * @return true 表示所有匹配记录都已写入。
     */
    bool exportToDeviceInternal(QIODevice* device, QString* errorString);

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
