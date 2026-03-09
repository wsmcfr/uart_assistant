/**
 * @file ToolboxDialog.cpp
 * @brief 工具箱对话框实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "ToolboxDialog.h"
#include "../../core/utils/ChecksumUtils.h"
#include "../../core/utils/ConversionUtils.h"
#include "../../core/utils/EncodingUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>

namespace ComAssistant {

ToolboxDialog::ToolboxDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("工具箱"));
    setMinimumSize(600, 500);
    setupUi();
}

void ToolboxDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tabWidget = new QTabWidget;
    mainLayout->addWidget(m_tabWidget);

    setupChecksumTab();
    setupConversionTab();
    setupEncodingTab();

    // 关闭按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    QPushButton* closeBtn = new QPushButton(tr("关闭"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);
}

void ToolboxDialog::setupChecksumTab()
{
    m_checksumTab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_checksumTab);

    // 输入区
    QGroupBox* inputGroup = new QGroupBox(tr("输入数据"));
    QVBoxLayout* inputLayout = new QVBoxLayout(inputGroup);

    m_checksumInput = new QTextEdit;
    m_checksumInput->setPlaceholderText(tr("输入十六进制数据，如: AA BB CC DD 或 AABBCCDD"));
    m_checksumInput->setMaximumHeight(100);
    inputLayout->addWidget(m_checksumInput);

    QHBoxLayout* optLayout = new QHBoxLayout;
    m_checksumHexInput = new QCheckBox(tr("输入为HEX格式"));
    m_checksumHexInput->setChecked(true);
    optLayout->addWidget(m_checksumHexInput);
    optLayout->addStretch();
    inputLayout->addLayout(optLayout);

    layout->addWidget(inputGroup);

    // 校验类型和计算
    QGroupBox* calcGroup = new QGroupBox(tr("校验计算"));
    QGridLayout* calcLayout = new QGridLayout(calcGroup);

    calcLayout->addWidget(new QLabel(tr("校验类型:")), 0, 0);
    m_checksumTypeCombo = new QComboBox;
    m_checksumTypeCombo->addItem("CRC-16 Modbus", "crc16modbus");
    m_checksumTypeCombo->addItem("CRC-16 CCITT", "crc16ccitt");
    m_checksumTypeCombo->addItem("CRC-16 CCITT-FALSE", "crc16ccittfalse");
    m_checksumTypeCombo->addItem("CRC-16 XMODEM", "crc16xmodem");
    m_checksumTypeCombo->addItem("CRC-32", "crc32");
    m_checksumTypeCombo->addItem("CRC-8", "crc8");
    m_checksumTypeCombo->addItem("CRC-8 MAXIM", "crc8maxim");
    m_checksumTypeCombo->addItem("XOR", "xor");
    m_checksumTypeCombo->addItem("SUM (8-bit)", "sum8");
    m_checksumTypeCombo->addItem("SUM (16-bit)", "sum16");
    m_checksumTypeCombo->addItem("LRC", "lrc");
    m_checksumTypeCombo->addItem("BCC", "bcc");
    calcLayout->addWidget(m_checksumTypeCombo, 0, 1);

    QPushButton* calcBtn = new QPushButton(tr("计算"));
    connect(calcBtn, &QPushButton::clicked, this, &ToolboxDialog::onCalculateChecksum);
    calcLayout->addWidget(calcBtn, 0, 2);

    QPushButton* clearBtn = new QPushButton(tr("清空"));
    connect(clearBtn, &QPushButton::clicked, this, &ToolboxDialog::onClearChecksum);
    calcLayout->addWidget(clearBtn, 0, 3);

    layout->addWidget(calcGroup);

    // 结果区
    QGroupBox* resultGroup = new QGroupBox(tr("计算结果"));
    QVBoxLayout* resultLayout = new QVBoxLayout(resultGroup);

    m_checksumResult = new QLabel;
    m_checksumResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_checksumResult->setStyleSheet("QLabel { font-size: 14px; font-family: Consolas; padding: 10px; background: #f0f0f0; }");
    m_checksumResult->setMinimumHeight(60);
    resultLayout->addWidget(m_checksumResult);

    layout->addWidget(resultGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_checksumTab, tr("校验和"));
}

void ToolboxDialog::setupConversionTab()
{
    m_conversionTab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_conversionTab);

    // 进制转换
    QGroupBox* baseGroup = new QGroupBox(tr("进制转换"));
    QGridLayout* baseLayout = new QGridLayout(baseGroup);

    baseLayout->addWidget(new QLabel(tr("输入:")), 0, 0);
    m_baseInput = new QLineEdit;
    m_baseInput->setPlaceholderText(tr("输入数值"));
    baseLayout->addWidget(m_baseInput, 0, 1, 1, 2);

    baseLayout->addWidget(new QLabel(tr("从:")), 1, 0);
    m_baseFromCombo = new QComboBox;
    m_baseFromCombo->addItem(tr("十进制"), 10);
    m_baseFromCombo->addItem(tr("十六进制"), 16);
    m_baseFromCombo->addItem(tr("二进制"), 2);
    m_baseFromCombo->addItem(tr("八进制"), 8);
    baseLayout->addWidget(m_baseFromCombo, 1, 1);

    baseLayout->addWidget(new QLabel(tr("到:")), 1, 2);
    m_baseToCombo = new QComboBox;
    m_baseToCombo->addItem(tr("十进制"), 10);
    m_baseToCombo->addItem(tr("十六进制"), 16);
    m_baseToCombo->addItem(tr("二进制"), 2);
    m_baseToCombo->addItem(tr("八进制"), 8);
    m_baseToCombo->setCurrentIndex(1);
    baseLayout->addWidget(m_baseToCombo, 1, 3);

    QPushButton* convertBaseBtn = new QPushButton(tr("转换"));
    connect(convertBaseBtn, &QPushButton::clicked, this, &ToolboxDialog::onConvertBase);
    baseLayout->addWidget(convertBaseBtn, 1, 4);

    baseLayout->addWidget(new QLabel(tr("结果:")), 2, 0);
    m_baseOutput = new QLineEdit;
    m_baseOutput->setReadOnly(true);
    baseLayout->addWidget(m_baseOutput, 2, 1, 1, 4);

    layout->addWidget(baseGroup);

    // 字节序转换
    QGroupBox* endianGroup = new QGroupBox(tr("字节序转换"));
    QGridLayout* endianLayout = new QGridLayout(endianGroup);

    endianLayout->addWidget(new QLabel(tr("HEX输入:")), 0, 0);
    m_endianInput = new QLineEdit;
    m_endianInput->setPlaceholderText(tr("如: 01 02 03 04"));
    endianLayout->addWidget(m_endianInput, 0, 1, 1, 2);

    endianLayout->addWidget(new QLabel(tr("类型:")), 1, 0);
    m_endianTypeCombo = new QComboBox;
    m_endianTypeCombo->addItem(tr("反转字节"), "reverse");
    m_endianTypeCombo->addItem(tr("16位交换"), "swap16");
    m_endianTypeCombo->addItem(tr("32位交换"), "swap32");
    endianLayout->addWidget(m_endianTypeCombo, 1, 1);

    QPushButton* swapBtn = new QPushButton(tr("转换"));
    connect(swapBtn, &QPushButton::clicked, this, &ToolboxDialog::onSwapEndian);
    endianLayout->addWidget(swapBtn, 1, 2);

    endianLayout->addWidget(new QLabel(tr("结果:")), 2, 0);
    m_endianOutput = new QLineEdit;
    m_endianOutput->setReadOnly(true);
    endianLayout->addWidget(m_endianOutput, 2, 1, 1, 2);

    layout->addWidget(endianGroup);

    // IEEE754浮点数
    QGroupBox* floatGroup = new QGroupBox(tr("IEEE754 浮点数"));
    QGridLayout* floatLayout = new QGridLayout(floatGroup);

    floatLayout->addWidget(new QLabel(tr("HEX → Float")), 0, 0);
    m_floatHexInput = new QLineEdit;
    m_floatHexInput->setPlaceholderText(tr("8字符HEX，如: 41200000"));
    floatLayout->addWidget(m_floatHexInput, 0, 1);
    m_floatBigEndian = new QCheckBox(tr("大端序"));
    floatLayout->addWidget(m_floatBigEndian, 0, 2);
    m_floatValueOutput = new QLineEdit;
    m_floatValueOutput->setReadOnly(true);
    floatLayout->addWidget(m_floatValueOutput, 0, 3);

    floatLayout->addWidget(new QLabel(tr("Float → HEX")), 1, 0);
    m_floatValueInput = new QLineEdit;
    m_floatValueInput->setPlaceholderText(tr("浮点数，如: 10.0"));
    floatLayout->addWidget(m_floatValueInput, 1, 1);
    floatLayout->addWidget(new QLabel(""), 1, 2);
    m_floatHexOutput = new QLineEdit;
    m_floatHexOutput->setReadOnly(true);
    floatLayout->addWidget(m_floatHexOutput, 1, 3);

    QPushButton* convertFloatBtn = new QPushButton(tr("转换"));
    connect(convertFloatBtn, &QPushButton::clicked, this, &ToolboxDialog::onConvertFloat);
    floatLayout->addWidget(convertFloatBtn, 2, 1, 1, 2);

    layout->addWidget(floatGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_conversionTab, tr("进制转换"));
}

void ToolboxDialog::setupEncodingTab()
{
    m_encodingTab = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(m_encodingTab);

    // Base64编码
    QGroupBox* base64Group = new QGroupBox(tr("Base64 编码"));
    QGridLayout* base64Layout = new QGridLayout(base64Group);

    base64Layout->addWidget(new QLabel(tr("原文:")), 0, 0);
    m_base64Input = new QTextEdit;
    m_base64Input->setMaximumHeight(60);
    base64Layout->addWidget(m_base64Input, 0, 1);

    QPushButton* toBase64Btn = new QPushButton(tr("编码 →"));
    QPushButton* fromBase64Btn = new QPushButton(tr("← 解码"));
    QVBoxLayout* base64BtnLayout = new QVBoxLayout;
    base64BtnLayout->addWidget(toBase64Btn);
    base64BtnLayout->addWidget(fromBase64Btn);
    base64Layout->addLayout(base64BtnLayout, 0, 2);

    base64Layout->addWidget(new QLabel(tr("Base64:")), 0, 3);
    m_base64Output = new QTextEdit;
    m_base64Output->setMaximumHeight(60);
    base64Layout->addWidget(m_base64Output, 0, 4);

    connect(toBase64Btn, &QPushButton::clicked, [this]() {
        QString input = m_base64Input->toPlainText();
        m_base64Output->setPlainText(EncodingUtils::toBase64(input.toUtf8()));
    });
    connect(fromBase64Btn, &QPushButton::clicked, [this]() {
        QString input = m_base64Output->toPlainText();
        QByteArray decoded = EncodingUtils::fromBase64(input);
        m_base64Input->setPlainText(QString::fromUtf8(decoded));
    });

    layout->addWidget(base64Group);

    // URL编码
    QGroupBox* urlGroup = new QGroupBox(tr("URL 编码"));
    QGridLayout* urlLayout = new QGridLayout(urlGroup);

    urlLayout->addWidget(new QLabel(tr("原文:")), 0, 0);
    m_urlInput = new QLineEdit;
    urlLayout->addWidget(m_urlInput, 0, 1);

    QPushButton* toUrlBtn = new QPushButton(tr("编码 →"));
    QPushButton* fromUrlBtn = new QPushButton(tr("← 解码"));
    urlLayout->addWidget(toUrlBtn, 0, 2);
    urlLayout->addWidget(fromUrlBtn, 0, 3);

    urlLayout->addWidget(new QLabel(tr("URL:")), 1, 0);
    m_urlOutput = new QLineEdit;
    urlLayout->addWidget(m_urlOutput, 1, 1, 1, 3);

    connect(toUrlBtn, &QPushButton::clicked, [this]() {
        m_urlOutput->setText(EncodingUtils::urlEncode(m_urlInput->text()));
    });
    connect(fromUrlBtn, &QPushButton::clicked, [this]() {
        m_urlInput->setText(EncodingUtils::urlDecode(m_urlOutput->text()));
    });

    layout->addWidget(urlGroup);

    // 转义序列
    QGroupBox* escapeGroup = new QGroupBox(tr("转义序列"));
    QGridLayout* escapeLayout = new QGridLayout(escapeGroup);

    escapeLayout->addWidget(new QLabel(tr("带转义:")), 0, 0);
    m_escapeInput = new QLineEdit;
    m_escapeInput->setPlaceholderText(tr("如: Hello\\nWorld\\x00"));
    escapeLayout->addWidget(m_escapeInput, 0, 1);

    QPushButton* processEscapeBtn = new QPushButton(tr("处理 →"));
    QPushButton* toEscapeBtn = new QPushButton(tr("← 转义"));
    escapeLayout->addWidget(processEscapeBtn, 0, 2);
    escapeLayout->addWidget(toEscapeBtn, 0, 3);

    escapeLayout->addWidget(new QLabel(tr("HEX:")), 1, 0);
    m_escapeOutput = new QLineEdit;
    m_escapeOutput->setReadOnly(true);
    escapeLayout->addWidget(m_escapeOutput, 1, 1, 1, 3);

    connect(processEscapeBtn, &QPushButton::clicked, [this]() {
        QByteArray data = EncodingUtils::processEscapeSequences(m_escapeInput->text());
        m_escapeOutput->setText(ConversionUtils::bytesToHexString(data));
    });
    connect(toEscapeBtn, &QPushButton::clicked, [this]() {
        QByteArray data = ConversionUtils::hexStringToBytes(m_escapeOutput->text());
        m_escapeInput->setText(EncodingUtils::toEscapeString(data));
    });

    layout->addWidget(escapeGroup);

    // 字符编码转换
    QGroupBox* codecGroup = new QGroupBox(tr("字符编码转换"));
    QGridLayout* codecLayout = new QGridLayout(codecGroup);

    codecLayout->addWidget(new QLabel(tr("输入:")), 0, 0);
    m_encodingInput = new QTextEdit;
    m_encodingInput->setMaximumHeight(60);
    codecLayout->addWidget(m_encodingInput, 0, 1, 1, 2);

    codecLayout->addWidget(new QLabel(tr("从:")), 1, 0);
    m_encodingFromCombo = new QComboBox;
    for (const QString& codec : EncodingUtils::supportedCodecs()) {
        m_encodingFromCombo->addItem(codec);
    }
    codecLayout->addWidget(m_encodingFromCombo, 1, 1);

    codecLayout->addWidget(new QLabel(tr("到:")), 1, 2);
    m_encodingToCombo = new QComboBox;
    for (const QString& codec : EncodingUtils::supportedCodecs()) {
        m_encodingToCombo->addItem(codec);
    }
    m_encodingToCombo->setCurrentText("GBK");
    codecLayout->addWidget(m_encodingToCombo, 1, 3);

    QPushButton* convertEncodingBtn = new QPushButton(tr("转换"));
    connect(convertEncodingBtn, &QPushButton::clicked, this, &ToolboxDialog::onConvertEncoding);
    codecLayout->addWidget(convertEncodingBtn, 1, 4);

    codecLayout->addWidget(new QLabel(tr("输出 (HEX):")), 2, 0);
    m_encodingOutput = new QTextEdit;
    m_encodingOutput->setMaximumHeight(60);
    m_encodingOutput->setReadOnly(true);
    codecLayout->addWidget(m_encodingOutput, 2, 1, 1, 4);

    layout->addWidget(codecGroup);
    layout->addStretch();

    m_tabWidget->addTab(m_encodingTab, tr("编码转换"));
}

void ToolboxDialog::onCalculateChecksum()
{
    QString input = m_checksumInput->toPlainText().trimmed();
    if (input.isEmpty()) {
        return;
    }

    QByteArray data;
    if (m_checksumHexInput->isChecked()) {
        data = ConversionUtils::hexStringToBytes(input);
    } else {
        data = input.toUtf8();
    }

    if (data.isEmpty()) {
        m_checksumResult->setText(tr("输入数据无效"));
        return;
    }

    QString type = m_checksumTypeCombo->currentData().toString();
    QString result;

    if (type == "crc16modbus") {
        quint16 crc = ChecksumUtils::crc16Modbus(data);
        result = QString("CRC-16 Modbus: 0x%1\n低位在前: %2 %3")
                     .arg(crc, 4, 16, QChar('0')).toUpper()
                     .arg(crc & 0xFF, 2, 16, QChar('0')).toUpper()
                     .arg((crc >> 8) & 0xFF, 2, 16, QChar('0')).toUpper();
    } else if (type == "crc16ccitt") {
        quint16 crc = ChecksumUtils::crc16CCITT(data);
        result = QString("CRC-16 CCITT: 0x%1").arg(crc, 4, 16, QChar('0')).toUpper();
    } else if (type == "crc16ccittfalse") {
        quint16 crc = ChecksumUtils::crc16CCITT_FALSE(data);
        result = QString("CRC-16 CCITT-FALSE: 0x%1").arg(crc, 4, 16, QChar('0')).toUpper();
    } else if (type == "crc16xmodem") {
        quint16 crc = ChecksumUtils::crc16XMODEM(data);
        result = QString("CRC-16 XMODEM: 0x%1").arg(crc, 4, 16, QChar('0')).toUpper();
    } else if (type == "crc32") {
        quint32 crc = ChecksumUtils::crc32(data);
        result = QString("CRC-32: 0x%1").arg(crc, 8, 16, QChar('0')).toUpper();
    } else if (type == "crc8") {
        quint8 crc = ChecksumUtils::crc8(data);
        result = QString("CRC-8: 0x%1").arg(crc, 2, 16, QChar('0')).toUpper();
    } else if (type == "crc8maxim") {
        quint8 crc = ChecksumUtils::crc8Maxim(data);
        result = QString("CRC-8 MAXIM: 0x%1").arg(crc, 2, 16, QChar('0')).toUpper();
    } else if (type == "xor") {
        quint8 xorVal = ChecksumUtils::xorChecksum(data);
        result = QString("XOR: 0x%1").arg(xorVal, 2, 16, QChar('0')).toUpper();
    } else if (type == "sum8") {
        quint8 sum = ChecksumUtils::sumChecksum(data);
        result = QString("SUM (8-bit): 0x%1").arg(sum, 2, 16, QChar('0')).toUpper();
    } else if (type == "sum16") {
        quint16 sum = ChecksumUtils::sum16Checksum(data);
        result = QString("SUM (16-bit): 0x%1").arg(sum, 4, 16, QChar('0')).toUpper();
    } else if (type == "lrc") {
        quint8 lrc = ChecksumUtils::lrcChecksum(data);
        result = QString("LRC: 0x%1").arg(lrc, 2, 16, QChar('0')).toUpper();
    } else if (type == "bcc") {
        quint8 bcc = ChecksumUtils::bcc(data);
        result = QString("BCC: 0x%1").arg(bcc, 2, 16, QChar('0')).toUpper();
    }

    m_checksumResult->setText(result);
}

void ToolboxDialog::onClearChecksum()
{
    m_checksumInput->clear();
    m_checksumResult->clear();
}

void ToolboxDialog::onConvertBase()
{
    QString input = m_baseInput->text().trimmed();
    if (input.isEmpty()) {
        return;
    }

    int fromBase = m_baseFromCombo->currentData().toInt();
    int toBase = m_baseToCombo->currentData().toInt();

    bool ok;
    qint64 value = 0;

    switch (fromBase) {
    case 10:
        value = ConversionUtils::hexToDec(input, &ok);
        if (!ok) value = input.toLongLong(&ok, 10);
        break;
    case 16:
        value = ConversionUtils::hexToDec(input, &ok);
        break;
    case 2:
        value = ConversionUtils::binToDec(input, &ok);
        break;
    case 8:
        value = ConversionUtils::octToDec(input, &ok);
        break;
    }

    if (!ok) {
        m_baseOutput->setText(tr("输入格式错误"));
        return;
    }

    QString result;
    switch (toBase) {
    case 10:
        result = QString::number(value);
        break;
    case 16:
        result = ConversionUtils::decToHex(value);
        break;
    case 2:
        result = ConversionUtils::decToBin(value);
        break;
    case 8:
        result = ConversionUtils::decToOct(value);
        break;
    }

    m_baseOutput->setText(result);
}

void ToolboxDialog::onSwapEndian()
{
    QString input = m_endianInput->text().trimmed();
    if (input.isEmpty()) {
        return;
    }

    QByteArray data = ConversionUtils::hexStringToBytes(input);
    if (data.isEmpty()) {
        m_endianOutput->setText(tr("输入格式错误"));
        return;
    }

    QString type = m_endianTypeCombo->currentData().toString();
    QByteArray result;

    if (type == "reverse") {
        result = ConversionUtils::reverseBytes(data);
    } else if (type == "swap16" && data.size() >= 2) {
        quint16 val = ConversionUtils::bytesToUint16(data, false);
        val = ConversionUtils::swapBytes16(val);
        result = ConversionUtils::uint16ToBytes(val, false);
    } else if (type == "swap32" && data.size() >= 4) {
        quint32 val = ConversionUtils::bytesToUint32(data, false);
        val = ConversionUtils::swapBytes32(val);
        result = ConversionUtils::uint32ToBytes(val, false);
    } else {
        m_endianOutput->setText(tr("数据长度不足"));
        return;
    }

    m_endianOutput->setText(ConversionUtils::bytesToHexString(result));
}

void ToolboxDialog::onConvertFloat()
{
    // HEX -> Float
    QString hexInput = m_floatHexInput->text().trimmed();
    if (!hexInput.isEmpty()) {
        bool ok;
        float val = ConversionUtils::hexToFloat(hexInput, m_floatBigEndian->isChecked(), &ok);
        if (ok) {
            m_floatValueOutput->setText(QString::number(val, 'g', 9));
        } else {
            m_floatValueOutput->setText(tr("转换失败"));
        }
    }

    // Float -> HEX
    QString floatInput = m_floatValueInput->text().trimmed();
    if (!floatInput.isEmpty()) {
        bool ok;
        float val = floatInput.toFloat(&ok);
        if (ok) {
            m_floatHexOutput->setText(ConversionUtils::floatToHex(val, m_floatBigEndian->isChecked()));
        } else {
            m_floatHexOutput->setText(tr("转换失败"));
        }
    }
}

void ToolboxDialog::onConvertEncoding()
{
    QString input = m_encodingInput->toPlainText();
    if (input.isEmpty()) {
        return;
    }

    QString fromCodec = m_encodingFromCombo->currentText();
    QString toCodec = m_encodingToCombo->currentText();

    QByteArray inputData = input.toUtf8();
    QByteArray result = EncodingUtils::convertEncoding(inputData, fromCodec, toCodec);

    m_encodingOutput->setPlainText(ConversionUtils::bytesToHexString(result));
}

void ToolboxDialog::onConvertBase64()
{
    // 由lambda处理
}

void ToolboxDialog::onConvertUrl()
{
    // 由lambda处理
}

void ToolboxDialog::onConvertEscape()
{
    // 由lambda处理
}

} // namespace ComAssistant
