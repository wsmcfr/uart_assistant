/**
 * @file ModbusAnalyzerWidget.h
 * @brief Modbus协议分析组件
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_MODBUSANALYZERWIDGET_H
#define COMASSISTANT_MODBUSANALYZERWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QSplitter>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QDateTime>
#include <QGroupBox>
#include <QEvent>

namespace ComAssistant {

/**
 * @brief Modbus帧类型
 */
enum class ModbusFrameType {
    Request,
    Response,
    Exception,
    Unknown
};

/**
 * @brief Modbus功能码
 */
enum class ModbusFunctionCode : quint8 {
    ReadCoils = 0x01,
    ReadDiscreteInputs = 0x02,
    ReadHoldingRegisters = 0x03,
    ReadInputRegisters = 0x04,
    WriteSingleCoil = 0x05,
    WriteSingleRegister = 0x06,
    WriteMultipleCoils = 0x0F,
    WriteMultipleRegisters = 0x10,
    ReadWriteMultipleRegisters = 0x17,
    MaskWriteRegister = 0x16,
    ReadFIFOQueue = 0x18,
    // 异常响应 = 功能码 + 0x80
};

/**
 * @brief Modbus解析结果
 */
struct ModbusFrame {
    QDateTime timestamp;
    ModbusFrameType type = ModbusFrameType::Unknown;
    QByteArray rawData;

    // 公共字段
    quint8 slaveAddress = 0;
    quint8 functionCode = 0;
    quint16 crc = 0;
    bool crcValid = false;

    // 请求/响应特定字段
    quint16 startAddress = 0;
    quint16 quantity = 0;
    quint8 byteCount = 0;
    QByteArray dataBytes;

    // 异常
    quint8 exceptionCode = 0;

    // 解析后的寄存器值
    QVector<quint16> registerValues;
    QVector<bool> coilValues;

    QString functionName() const;
    QString exceptionName() const;
    QString description() const;
};

/**
 * @brief Modbus协议分析组件
 */
class ModbusAnalyzerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModbusAnalyzerWidget(QWidget* parent = nullptr);
    ~ModbusAnalyzerWidget() override = default;

    /**
     * @brief 添加接收数据
     */
    void appendData(const QByteArray& data, bool isRequest = false);

    /**
     * @brief 解析Modbus帧
     */
    ModbusFrame parseFrame(const QByteArray& data, bool isRequest = false);

    /**
     * @brief 清空所有记录
     */
    void clear();

    /**
     * @brief 设置从站地址过滤
     */
    void setSlaveFilter(int address);

    /**
     * @brief 导出分析报告
     */
    bool exportReport(const QString& filePath);

signals:
    /**
     * @brief 帧解析完成
     */
    void frameParsed(const ModbusFrame& frame);

    /**
     * @brief 请求发送Modbus请求
     */
    void sendModbusRequest(const QByteArray& data);

private slots:
    void onFrameSelected();
    void onGenerateRequest();
    void onClearClicked();
    void onExportClicked();
    void onAutoScrollToggled(bool checked);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void addFrameToTable(const ModbusFrame& frame);
    void showFrameDetails(const ModbusFrame& frame);
    quint16 calculateCRC16(const QByteArray& data);
    QString formatRegisterValue(quint16 value, int format);
    void retranslateUi();

private:
    // 帧列表
    QTableWidget* m_frameTable = nullptr;

    // 详情视图
    QTreeWidget* m_detailTree = nullptr;
    QTextEdit* m_rawDataEdit = nullptr;
    QGroupBox* m_rawGroup = nullptr;

    // 工具栏
    QLabel* m_slaveFilterLabel = nullptr;
    QLabel* m_functionFilterLabel = nullptr;
    QComboBox* m_slaveFilterCombo = nullptr;
    QComboBox* m_functionFilterCombo = nullptr;
    QCheckBox* m_autoScrollCheck = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;

    // 请求生成器
    QGroupBox* m_genGroup = nullptr;
    QLabel* m_slaveLabel = nullptr;
    QLabel* m_functionLabel = nullptr;
    QLabel* m_startAddrLabel = nullptr;
    QLabel* m_quantityLabel = nullptr;
    QSpinBox* m_slaveAddrSpin = nullptr;
    QComboBox* m_functionCombo = nullptr;
    QSpinBox* m_startAddrSpin = nullptr;
    QSpinBox* m_quantitySpin = nullptr;
    QPushButton* m_generateBtn = nullptr;

    // 数据
    QVector<ModbusFrame> m_frames;
    QByteArray m_receiveBuffer;
    int m_slaveFilter = -1;  // -1表示不过滤

    // 显示格式
    enum class ValueFormat { Decimal, Hex, Binary, Float };
    ValueFormat m_valueFormat = ValueFormat::Decimal;
};

} // namespace ComAssistant

#endif // COMASSISTANT_MODBUSANALYZERWIDGET_H
