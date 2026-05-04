/**
 * @file ModbusAnalyzerWidget.cpp
 * @brief Modbus协议分析组件实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "ModbusAnalyzerWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QTextStream>

namespace ComAssistant {

QString ModbusFrame::functionName() const
{
    switch (static_cast<ModbusFunctionCode>(functionCode & 0x7F)) {
    case ModbusFunctionCode::ReadCoils: return "读线圈 (0x01)";
    case ModbusFunctionCode::ReadDiscreteInputs: return "读离散输入 (0x02)";
    case ModbusFunctionCode::ReadHoldingRegisters: return "读保持寄存器 (0x03)";
    case ModbusFunctionCode::ReadInputRegisters: return "读输入寄存器 (0x04)";
    case ModbusFunctionCode::WriteSingleCoil: return "写单个线圈 (0x05)";
    case ModbusFunctionCode::WriteSingleRegister: return "写单个寄存器 (0x06)";
    case ModbusFunctionCode::WriteMultipleCoils: return "写多个线圈 (0x0F)";
    case ModbusFunctionCode::WriteMultipleRegisters: return "写多个寄存器 (0x10)";
    default: return QString("未知功能码 (0x%1)").arg(functionCode, 2, 16, QChar('0'));
    }
}

QString ModbusFrame::exceptionName() const
{
    switch (exceptionCode) {
    case 0x01: return "非法功能码";
    case 0x02: return "非法数据地址";
    case 0x03: return "非法数据值";
    case 0x04: return "从站设备故障";
    case 0x05: return "确认";
    case 0x06: return "从站设备忙";
    case 0x08: return "存储奇偶性差错";
    case 0x0A: return "不可用网关路径";
    case 0x0B: return "网关目标设备响应失败";
    default: return QString("未知异常 (0x%1)").arg(exceptionCode, 2, 16, QChar('0'));
    }
}

QString ModbusFrame::description() const
{
    QString desc;
    if (type == ModbusFrameType::Exception) {
        desc = QString("异常: %1").arg(exceptionName());
    } else {
        desc = functionName();
        if (startAddress > 0 || quantity > 0) {
            desc += QString(" 地址:%1 数量:%2").arg(startAddress).arg(quantity);
        }
    }
    return desc;
}

ModbusAnalyzerWidget::ModbusAnalyzerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ModbusAnalyzerWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 工具栏
    auto* toolLayout = new QHBoxLayout();

    m_slaveFilterLabel = new QLabel(tr("从站过滤:"), this);
    toolLayout->addWidget(m_slaveFilterLabel);
    m_slaveFilterCombo = new QComboBox(this);
    m_slaveFilterCombo->addItem(tr("全部"), -1);
    for (int i = 1; i <= 247; ++i) {
        m_slaveFilterCombo->addItem(QString::number(i), i);
    }
    toolLayout->addWidget(m_slaveFilterCombo);

    m_functionFilterLabel = new QLabel(tr("功能码:"), this);
    toolLayout->addWidget(m_functionFilterLabel);
    m_functionFilterCombo = new QComboBox(this);
    m_functionFilterCombo->addItem(tr("全部"), -1);
    m_functionFilterCombo->addItem(tr("0x01 读线圈"), 0x01);
    m_functionFilterCombo->addItem(tr("0x02 读离散输入"), 0x02);
    m_functionFilterCombo->addItem(tr("0x03 读保持寄存器"), 0x03);
    m_functionFilterCombo->addItem(tr("0x04 读输入寄存器"), 0x04);
    m_functionFilterCombo->addItem(tr("0x05 写单线圈"), 0x05);
    m_functionFilterCombo->addItem(tr("0x06 写单寄存器"), 0x06);
    m_functionFilterCombo->addItem(tr("0x0F 写多线圈"), 0x0F);
    m_functionFilterCombo->addItem(tr("0x10 写多寄存器"), 0x10);
    toolLayout->addWidget(m_functionFilterCombo);

    m_autoScrollCheck = new QCheckBox(tr("自动滚动"), this);
    m_autoScrollCheck->setChecked(true);
    toolLayout->addWidget(m_autoScrollCheck);

    toolLayout->addStretch();

    m_clearBtn = new QPushButton(tr("清空"), this);
    connect(m_clearBtn, &QPushButton::clicked, this, &ModbusAnalyzerWidget::onClearClicked);
    toolLayout->addWidget(m_clearBtn);

    m_exportBtn = new QPushButton(tr("导出"), this);
    connect(m_exportBtn, &QPushButton::clicked, this, &ModbusAnalyzerWidget::onExportClicked);
    toolLayout->addWidget(m_exportBtn);

    mainLayout->addLayout(toolLayout);

    // 主分割器
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // 帧列表
    m_frameTable = new QTableWidget(this);
    m_frameTable->setColumnCount(7);
    m_frameTable->setHorizontalHeaderLabels({
        tr("时间"), tr("方向"), tr("从站"), tr("功能码"),
        tr("描述"), tr("CRC"), tr("原始数据")
    });
    m_frameTable->horizontalHeader()->setStretchLastSection(true);
    m_frameTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_frameTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_frameTable->verticalHeader()->setVisible(false);
    connect(m_frameTable, &QTableWidget::itemSelectionChanged,
            this, &ModbusAnalyzerWidget::onFrameSelected);
    splitter->addWidget(m_frameTable);

    // 详情面板
    auto* detailWidget = new QWidget(this);
    auto* detailLayout = new QHBoxLayout(detailWidget);

    // 解析树
    m_detailTree = new QTreeWidget(this);
    m_detailTree->setHeaderLabels({tr("字段"), tr("值"), tr("说明")});
    m_detailTree->setColumnWidth(0, 120);
    m_detailTree->setColumnWidth(1, 100);
    detailLayout->addWidget(m_detailTree, 2);

    // 原始数据
    m_rawGroup = new QGroupBox(tr("原始数据"), this);
    auto* rawLayout = new QVBoxLayout(m_rawGroup);
    m_rawDataEdit = new QTextEdit(this);
    m_rawDataEdit->setReadOnly(true);
    m_rawDataEdit->setFont(QFont("Consolas", 10));
    m_rawDataEdit->setMaximumWidth(300);
    rawLayout->addWidget(m_rawDataEdit);
    detailLayout->addWidget(m_rawGroup, 1);

    splitter->addWidget(detailWidget);
    splitter->setSizes({300, 200});

    mainLayout->addWidget(splitter);

    // 请求生成器
    m_genGroup = new QGroupBox(tr("Modbus请求生成"), this);
    auto* genLayout = new QHBoxLayout(m_genGroup);

    m_slaveLabel = new QLabel(tr("从站:"), this);
    genLayout->addWidget(m_slaveLabel);
    m_slaveAddrSpin = new QSpinBox(this);
    m_slaveAddrSpin->setRange(1, 247);
    m_slaveAddrSpin->setValue(1);
    genLayout->addWidget(m_slaveAddrSpin);

    m_functionLabel = new QLabel(tr("功能:"), this);
    genLayout->addWidget(m_functionLabel);
    m_functionCombo = new QComboBox(this);
    m_functionCombo->addItem(tr("0x03 读保持寄存器"), 0x03);
    m_functionCombo->addItem(tr("0x04 读输入寄存器"), 0x04);
    m_functionCombo->addItem(tr("0x01 读线圈"), 0x01);
    m_functionCombo->addItem(tr("0x02 读离散输入"), 0x02);
    m_functionCombo->addItem(tr("0x06 写单寄存器"), 0x06);
    m_functionCombo->addItem(tr("0x05 写单线圈"), 0x05);
    genLayout->addWidget(m_functionCombo);

    m_startAddrLabel = new QLabel(tr("起始地址:"), this);
    genLayout->addWidget(m_startAddrLabel);
    m_startAddrSpin = new QSpinBox(this);
    m_startAddrSpin->setRange(0, 65535);
    m_startAddrSpin->setValue(0);
    genLayout->addWidget(m_startAddrSpin);

    m_quantityLabel = new QLabel(tr("数量:"), this);
    genLayout->addWidget(m_quantityLabel);
    m_quantitySpin = new QSpinBox(this);
    m_quantitySpin->setRange(1, 125);
    m_quantitySpin->setValue(10);
    genLayout->addWidget(m_quantitySpin);

    m_generateBtn = new QPushButton(tr("生成并发送"), this);
    connect(m_generateBtn, &QPushButton::clicked, this, &ModbusAnalyzerWidget::onGenerateRequest);
    genLayout->addWidget(m_generateBtn);

    genLayout->addStretch();

    mainLayout->addWidget(m_genGroup);
}

void ModbusAnalyzerWidget::appendData(const QByteArray& data, bool isRequest)
{
    m_receiveBuffer.append(data);

    // 尝试解析完整帧（最小长度4字节：地址+功能码+CRC）
    while (m_receiveBuffer.size() >= 4) {
        ModbusFrame frame = parseFrame(m_receiveBuffer, isRequest);

        if (frame.type != ModbusFrameType::Unknown && frame.rawData.size() > 0) {
            m_frames.append(frame);
            addFrameToTable(frame);
            emit frameParsed(frame);

            m_receiveBuffer.remove(0, frame.rawData.size());
        } else {
            // 无法解析，移除第一个字节继续尝试
            m_receiveBuffer.remove(0, 1);
        }
    }
}

ModbusFrame ModbusAnalyzerWidget::parseFrame(const QByteArray& data, bool isRequest)
{
    ModbusFrame frame;
    frame.timestamp = QDateTime::currentDateTime();

    if (data.size() < 4) {
        return frame;
    }

    frame.slaveAddress = static_cast<quint8>(data[0]);
    frame.functionCode = static_cast<quint8>(data[1]);

    // 检查是否为异常响应
    if (frame.functionCode & 0x80) {
        if (data.size() >= 5) {
            frame.type = ModbusFrameType::Exception;
            frame.exceptionCode = static_cast<quint8>(data[2]);
            frame.rawData = data.left(5);

            // 验证CRC
            frame.crc = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[3]);
            quint16 calcCrc = calculateCRC16(data.left(3));
            frame.crcValid = (frame.crc == calcCrc);
        }
        return frame;
    }

    // 根据功能码确定帧长度
    int frameLength = 0;
    quint8 fc = frame.functionCode;

    if (isRequest) {
        frame.type = ModbusFrameType::Request;

        switch (fc) {
        case 0x01: case 0x02: case 0x03: case 0x04:
            // 读请求: 地址(1) + 功能码(1) + 起始地址(2) + 数量(2) + CRC(2) = 8
            frameLength = 8;
            if (data.size() >= frameLength) {
                frame.startAddress = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
                frame.quantity = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);
            }
            break;

        case 0x05: case 0x06:
            // 写单个: 地址(1) + 功能码(1) + 地址(2) + 值(2) + CRC(2) = 8
            frameLength = 8;
            if (data.size() >= frameLength) {
                frame.startAddress = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
                frame.dataBytes = data.mid(4, 2);
            }
            break;

        case 0x0F: case 0x10:
            // 写多个: 地址(1) + 功能码(1) + 起始地址(2) + 数量(2) + 字节数(1) + 数据(N) + CRC(2)
            if (data.size() >= 7) {
                frame.startAddress = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
                frame.quantity = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);
                frame.byteCount = static_cast<quint8>(data[6]);
                frameLength = 7 + frame.byteCount + 2;
                if (data.size() >= frameLength) {
                    frame.dataBytes = data.mid(7, frame.byteCount);
                }
            }
            break;

        default:
            frame.type = ModbusFrameType::Unknown;
            return frame;
        }
    } else {
        frame.type = ModbusFrameType::Response;

        switch (fc) {
        case 0x01: case 0x02:
            // 读线圈响应: 地址(1) + 功能码(1) + 字节数(1) + 数据(N) + CRC(2)
            if (data.size() >= 3) {
                frame.byteCount = static_cast<quint8>(data[2]);
                frameLength = 3 + frame.byteCount + 2;
                if (data.size() >= frameLength) {
                    frame.dataBytes = data.mid(3, frame.byteCount);
                    // 解析线圈值
                    for (int i = 0; i < frame.byteCount; ++i) {
                        quint8 byte = static_cast<quint8>(frame.dataBytes[i]);
                        for (int bit = 0; bit < 8; ++bit) {
                            frame.coilValues.append((byte >> bit) & 0x01);
                        }
                    }
                }
            }
            break;

        case 0x03: case 0x04:
            // 读寄存器响应: 地址(1) + 功能码(1) + 字节数(1) + 数据(N) + CRC(2)
            if (data.size() >= 3) {
                frame.byteCount = static_cast<quint8>(data[2]);
                frameLength = 3 + frame.byteCount + 2;
                if (data.size() >= frameLength) {
                    frame.dataBytes = data.mid(3, frame.byteCount);
                    // 解析寄存器值
                    for (int i = 0; i + 1 < frame.byteCount; i += 2) {
                        quint16 value = (static_cast<quint8>(frame.dataBytes[i]) << 8) |
                                       static_cast<quint8>(frame.dataBytes[i + 1]);
                        frame.registerValues.append(value);
                    }
                }
            }
            break;

        case 0x05: case 0x06:
            // 写单个响应: 回显请求
            frameLength = 8;
            if (data.size() >= frameLength) {
                frame.startAddress = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
                frame.dataBytes = data.mid(4, 2);
            }
            break;

        case 0x0F: case 0x10:
            // 写多个响应: 地址(1) + 功能码(1) + 起始地址(2) + 数量(2) + CRC(2) = 8
            frameLength = 8;
            if (data.size() >= frameLength) {
                frame.startAddress = (static_cast<quint8>(data[2]) << 8) | static_cast<quint8>(data[3]);
                frame.quantity = (static_cast<quint8>(data[4]) << 8) | static_cast<quint8>(data[5]);
            }
            break;

        default:
            frame.type = ModbusFrameType::Unknown;
            return frame;
        }
    }

    if (frameLength == 0 || data.size() < frameLength) {
        frame.type = ModbusFrameType::Unknown;
        return frame;
    }

    frame.rawData = data.left(frameLength);

    // 验证CRC
    frame.crc = (static_cast<quint8>(data[frameLength - 1]) << 8) |
                static_cast<quint8>(data[frameLength - 2]);
    quint16 calcCrc = calculateCRC16(data.left(frameLength - 2));
    frame.crcValid = (frame.crc == calcCrc);

    return frame;
}

void ModbusAnalyzerWidget::addFrameToTable(const ModbusFrame& frame)
{
    // 检查过滤
    if (m_slaveFilter > 0 && frame.slaveAddress != m_slaveFilter) {
        return;
    }

    int row = m_frameTable->rowCount();
    m_frameTable->insertRow(row);

    // 时间
    m_frameTable->setItem(row, 0, new QTableWidgetItem(
        frame.timestamp.toString("hh:mm:ss.zzz")));

    // 方向
    QString direction;
    switch (frame.type) {
    case ModbusFrameType::Request: direction = "TX →"; break;
    case ModbusFrameType::Response: direction = "← RX"; break;
    case ModbusFrameType::Exception: direction = "← ERR"; break;
    default: direction = "???"; break;
    }
    auto* dirItem = new QTableWidgetItem(direction);
    dirItem->setForeground(frame.type == ModbusFrameType::Request ?
                          Qt::darkBlue : (frame.type == ModbusFrameType::Exception ?
                                         Qt::red : Qt::darkGreen));
    m_frameTable->setItem(row, 1, dirItem);

    // 从站地址
    m_frameTable->setItem(row, 2, new QTableWidgetItem(
        QString::number(frame.slaveAddress)));

    // 功能码
    m_frameTable->setItem(row, 3, new QTableWidgetItem(
        QString("0x%1").arg(frame.functionCode, 2, 16, QChar('0')).toUpper()));

    // 描述
    m_frameTable->setItem(row, 4, new QTableWidgetItem(frame.description()));

    // CRC
    auto* crcItem = new QTableWidgetItem(frame.crcValid ? "✓" : "✗");
    crcItem->setForeground(frame.crcValid ? Qt::darkGreen : Qt::red);
    m_frameTable->setItem(row, 5, crcItem);

    // 原始数据
    m_frameTable->setItem(row, 6, new QTableWidgetItem(
        QString::fromLatin1(frame.rawData.toHex(' ').toUpper())));

    // 自动滚动
    if (m_autoScrollCheck->isChecked()) {
        m_frameTable->scrollToBottom();
    }
}

void ModbusAnalyzerWidget::onFrameSelected()
{
    int row = m_frameTable->currentRow();
    if (row >= 0 && row < m_frames.size()) {
        showFrameDetails(m_frames[row]);
    }
}

void ModbusAnalyzerWidget::showFrameDetails(const ModbusFrame& frame)
{
    m_detailTree->clear();

    // 基本信息
    auto* basicItem = new QTreeWidgetItem(m_detailTree, {tr("基本信息")});
    new QTreeWidgetItem(basicItem, {tr("从站地址"),
        QString::number(frame.slaveAddress),
        QString("0x%1").arg(frame.slaveAddress, 2, 16, QChar('0'))});
    new QTreeWidgetItem(basicItem, {tr("功能码"),
        QString("0x%1").arg(frame.functionCode, 2, 16, QChar('0')),
        frame.functionName()});
    new QTreeWidgetItem(basicItem, {tr("CRC校验"),
        frame.crcValid ? tr("正确") : tr("错误"),
        QString("0x%1").arg(frame.crc, 4, 16, QChar('0'))});
    basicItem->setExpanded(true);

    // 地址和数量
    if (frame.startAddress > 0 || frame.quantity > 0) {
        auto* addrItem = new QTreeWidgetItem(m_detailTree, {tr("地址信息")});
        new QTreeWidgetItem(addrItem, {tr("起始地址"),
            QString::number(frame.startAddress),
            QString("0x%1").arg(frame.startAddress, 4, 16, QChar('0'))});
        if (frame.quantity > 0) {
            new QTreeWidgetItem(addrItem, {tr("数量"),
                QString::number(frame.quantity), ""});
        }
        addrItem->setExpanded(true);
    }

    // 寄存器值
    if (!frame.registerValues.isEmpty()) {
        auto* regItem = new QTreeWidgetItem(m_detailTree, {tr("寄存器值")});
        for (int i = 0; i < frame.registerValues.size(); ++i) {
            quint16 value = frame.registerValues[i];
            new QTreeWidgetItem(regItem, {
                QString("[%1]").arg(frame.startAddress + i),
                QString::number(value),
                QString("0x%1").arg(value, 4, 16, QChar('0'))
            });
        }
        regItem->setExpanded(true);
    }

    // 线圈值
    if (!frame.coilValues.isEmpty()) {
        auto* coilItem = new QTreeWidgetItem(m_detailTree, {tr("线圈值")});
        for (int i = 0; i < frame.coilValues.size(); ++i) {
            new QTreeWidgetItem(coilItem, {
                QString("[%1]").arg(frame.startAddress + i),
                frame.coilValues[i] ? "ON" : "OFF",
                frame.coilValues[i] ? "1" : "0"
            });
        }
        coilItem->setExpanded(true);
    }

    // 异常
    if (frame.type == ModbusFrameType::Exception) {
        auto* excItem = new QTreeWidgetItem(m_detailTree, {tr("异常信息")});
        new QTreeWidgetItem(excItem, {tr("异常码"),
            QString("0x%1").arg(frame.exceptionCode, 2, 16, QChar('0')),
            frame.exceptionName()});
        excItem->setExpanded(true);
    }

    // 原始数据
    m_rawDataEdit->setPlainText(frame.rawData.toHex(' ').toUpper());
}

void ModbusAnalyzerWidget::onGenerateRequest()
{
    QByteArray request;

    quint8 slave = m_slaveAddrSpin->value();
    quint8 fc = m_functionCombo->currentData().toInt();
    quint16 startAddr = m_startAddrSpin->value();
    quint16 quantity = m_quantitySpin->value();

    request.append(static_cast<char>(slave));
    request.append(static_cast<char>(fc));
    request.append(static_cast<char>((startAddr >> 8) & 0xFF));
    request.append(static_cast<char>(startAddr & 0xFF));
    request.append(static_cast<char>((quantity >> 8) & 0xFF));
    request.append(static_cast<char>(quantity & 0xFF));

    quint16 crc = calculateCRC16(request);
    request.append(static_cast<char>(crc & 0xFF));
    request.append(static_cast<char>((crc >> 8) & 0xFF));

    emit sendModbusRequest(request);

    // 添加到分析列表
    appendData(request, true);
}

void ModbusAnalyzerWidget::onClearClicked()
{
    m_frames.clear();
    m_frameTable->setRowCount(0);
    m_detailTree->clear();
    m_rawDataEdit->clear();
    m_receiveBuffer.clear();
}

void ModbusAnalyzerWidget::onExportClicked()
{
    QString path = QFileDialog::getSaveFileName(this, tr("导出报告"),
        "", tr("CSV文件 (*.csv);;文本文件 (*.txt)"));

    if (!path.isEmpty()) {
        exportReport(path);
    }
}

void ModbusAnalyzerWidget::onAutoScrollToggled(bool checked)
{
    Q_UNUSED(checked)
}

bool ModbusAnalyzerWidget::exportReport(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "时间,方向,从站,功能码,描述,CRC,原始数据\n";

    for (const ModbusFrame& frame : m_frames) {
        QString direction;
        switch (frame.type) {
        case ModbusFrameType::Request: direction = "TX"; break;
        case ModbusFrameType::Response: direction = "RX"; break;
        case ModbusFrameType::Exception: direction = "ERR"; break;
        default: direction = "???"; break;
        }

        stream << frame.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << ","
               << direction << ","
               << frame.slaveAddress << ","
               << QString("0x%1").arg(frame.functionCode, 2, 16, QChar('0')) << ","
               << frame.description() << ","
               << (frame.crcValid ? "OK" : "ERR") << ","
               << frame.rawData.toHex(' ').toUpper() << "\n";
    }

    return true;
}

void ModbusAnalyzerWidget::clear()
{
    onClearClicked();
}

void ModbusAnalyzerWidget::setSlaveFilter(int address)
{
    m_slaveFilter = address;
    m_slaveFilterCombo->setCurrentIndex(
        m_slaveFilterCombo->findData(address));
}

quint16 ModbusAnalyzerWidget::calculateCRC16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint8>(data[i]);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

QString ModbusAnalyzerWidget::formatRegisterValue(quint16 value, int format)
{
    switch (static_cast<ValueFormat>(format)) {
    case ValueFormat::Hex:
        return QString("0x%1").arg(value, 4, 16, QChar('0'));
    case ValueFormat::Binary:
        return QString("0b%1").arg(value, 16, 2, QChar('0'));
    case ValueFormat::Float:
        // 需要两个寄存器组合
        return QString::number(value);
    default:
        return QString::number(value);
    }
}

void ModbusAnalyzerWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ModbusAnalyzerWidget::retranslateUi()
{
    // 工具栏
    m_slaveFilterLabel->setText(tr("从站过滤:"));
    m_slaveFilterCombo->setItemText(0, tr("全部"));
    m_functionFilterLabel->setText(tr("功能码:"));
    m_functionFilterCombo->setItemText(0, tr("全部"));
    m_autoScrollCheck->setText(tr("自动滚动"));
    m_clearBtn->setText(tr("清空"));
    m_exportBtn->setText(tr("导出"));

    // 表格列头
    m_frameTable->setHorizontalHeaderLabels({
        tr("时间"), tr("方向"), tr("从站"), tr("功能码"),
        tr("描述"), tr("CRC"), tr("原始数据")
    });

    // 详情树列头
    m_detailTree->setHeaderLabels({tr("字段"), tr("值"), tr("说明")});

    // 分组框
    m_rawGroup->setTitle(tr("原始数据"));
    m_genGroup->setTitle(tr("Modbus请求生成"));

    // 请求生成器标签
    m_slaveLabel->setText(tr("从站:"));
    m_functionLabel->setText(tr("功能:"));
    m_startAddrLabel->setText(tr("起始地址:"));
    m_quantityLabel->setText(tr("数量:"));
    m_generateBtn->setText(tr("生成并发送"));
}

} // namespace ComAssistant
