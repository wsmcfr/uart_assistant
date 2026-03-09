/**
 * @file ReceiveWidget.cpp
 * @brief 接收区组件实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "ReceiveWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollBar>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>

namespace ComAssistant {

ReceiveWidget::ReceiveWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void ReceiveWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_receiveGroup = new QGroupBox(tr("Receive"));
    QVBoxLayout* groupLayout = new QVBoxLayout(m_receiveGroup);

    // 工具栏
    QHBoxLayout* toolLayout = new QHBoxLayout;

    // 显示模式
    m_modeLabel = new QLabel(tr("Mode:"));
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("ASCII", static_cast<int>(DataDisplayFormat::Ascii));
    m_modeCombo->addItem("HEX", static_cast<int>(DataDisplayFormat::Hex));
    m_modeCombo->addItem(tr("Mixed"), static_cast<int>(DataDisplayFormat::Mixed));
    toolLayout->addWidget(m_modeLabel);
    toolLayout->addWidget(m_modeCombo);

    toolLayout->addSpacing(20);

    // 选项
    m_timestampCheck = new QCheckBox(tr("Timestamp"));
    m_autoScrollCheck = new QCheckBox(tr("Auto Scroll"));
    m_autoScrollCheck->setChecked(true);
    m_wordWrapCheck = new QCheckBox(tr("Word Wrap"));
    m_wordWrapCheck->setChecked(true);

    toolLayout->addWidget(m_timestampCheck);
    toolLayout->addWidget(m_autoScrollCheck);
    toolLayout->addWidget(m_wordWrapCheck);

    toolLayout->addStretch();

    // 按钮
    m_clearBtn = new QPushButton(tr("Clear"));
    m_clearBtn->setFixedWidth(60);
    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setFixedWidth(60);

    toolLayout->addWidget(m_clearBtn);
    toolLayout->addWidget(m_exportBtn);

    groupLayout->addLayout(toolLayout);

    // 显示区
    m_displayEdit = new QTextEdit;
    m_displayEdit->setReadOnly(true);
    m_displayEdit->setFont(QFont("Consolas", 10));
    m_displayEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    groupLayout->addWidget(m_displayEdit);

    mainLayout->addWidget(m_receiveGroup);

    // 连接信号
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ReceiveWidget::onModeChanged);
    connect(m_clearBtn, &QPushButton::clicked, this, &ReceiveWidget::onClearClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &ReceiveWidget::onExportClicked);
    connect(m_timestampCheck, &QCheckBox::toggled, this, &ReceiveWidget::onTimestampToggled);
    connect(m_autoScrollCheck, &QCheckBox::toggled, this, &ReceiveWidget::onAutoScrollToggled);
    connect(m_wordWrapCheck, &QCheckBox::toggled, [this](bool checked) {
        m_displayEdit->setLineWrapMode(checked ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    });
}

void ReceiveWidget::appendData(const QByteArray& data)
{
    if (data.isEmpty()) {
        return;
    }

    // 保存原始数据
    m_rawData.append(data);

    // 限制数据大小
    if (m_rawData.size() > MAX_DISPLAY_SIZE) {
        m_rawData = m_rawData.right(MAX_DISPLAY_SIZE / 2);
        m_displayEdit->clear();
    }

    // 格式化并显示
    QString text;
    if (m_showTimestamp) {
        text = timestamp() + " ";
    }
    text += formatData(data);

    m_displayEdit->moveCursor(QTextCursor::End);
    m_displayEdit->insertPlainText(text);

    // 自动滚动
    if (m_autoScroll) {
        QScrollBar* scrollBar = m_displayEdit->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void ReceiveWidget::clear()
{
    m_displayEdit->clear();
    m_rawData.clear();
}

void ReceiveWidget::setDataDisplayFormat(DataDisplayFormat format)
{
    m_displayFormat = format;
    int index = m_modeCombo->findData(static_cast<int>(format));
    if (index >= 0) {
        m_modeCombo->setCurrentIndex(index);
    }
    refreshDisplay();
}

bool ReceiveWidget::exportToFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << m_displayEdit->toPlainText();
    file.close();

    return true;
}

void ReceiveWidget::onModeChanged(int index)
{
    m_displayFormat = static_cast<DataDisplayFormat>(m_modeCombo->itemData(index).toInt());
    refreshDisplay();
}

void ReceiveWidget::onClearClicked()
{
    clear();
}

void ReceiveWidget::onExportClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Data"),
        QString(), tr("Text Files (*.txt);;All Files (*)"));

    if (!fileName.isEmpty()) {
        if (exportToFile(fileName)) {
            QMessageBox::information(this, tr("Export"), tr("Data exported successfully."));
        } else {
            QMessageBox::warning(this, tr("Export"), tr("Failed to export data."));
        }
    }
}

void ReceiveWidget::onTimestampToggled(bool checked)
{
    m_showTimestamp = checked;
}

void ReceiveWidget::onAutoScrollToggled(bool checked)
{
    m_autoScroll = checked;
}

QString ReceiveWidget::formatData(const QByteArray& data) const
{
    switch (m_displayFormat) {
        case DataDisplayFormat::Ascii:
            return QString::fromUtf8(data);
        case DataDisplayFormat::Hex:
            return formatHex(data);
        case DataDisplayFormat::Mixed:
            return formatMixed(data);
        default:
            return QString::fromUtf8(data);
    }
}

QString ReceiveWidget::formatHex(const QByteArray& data) const
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        result += QString("%1 ").arg(static_cast<uchar>(data[i]), 2, 16, QChar('0')).toUpper();
    }
    return result;
}

QString ReceiveWidget::formatMixed(const QByteArray& data) const
{
    QString result;

    for (char c : data) {
        uchar uc = static_cast<uchar>(c);
        if (uc >= 32 && uc < 127) {
            // 可打印字符
            result += c;
        } else if (c == '\r') {
            result += "\\r";
        } else if (c == '\n') {
            result += "\\n\n";
        } else if (c == '\t') {
            result += "\\t";
        } else {
            // 非打印字符显示为十六进制
            result += QString("[%1]").arg(uc, 2, 16, QChar('0')).toUpper();
        }
    }

    return result;
}

QString ReceiveWidget::timestamp() const
{
    return QDateTime::currentDateTime().toString("[hh:mm:ss.zzz]");
}

void ReceiveWidget::refreshDisplay()
{
    if (m_rawData.isEmpty()) {
        return;
    }

    m_displayEdit->clear();
    m_displayEdit->setPlainText(formatData(m_rawData));

    if (m_autoScroll) {
        QScrollBar* scrollBar = m_displayEdit->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }
}

void ReceiveWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ReceiveWidget::retranslateUi()
{
    m_receiveGroup->setTitle(tr("Receive"));
    m_modeLabel->setText(tr("Mode:"));
    m_modeCombo->setItemText(2, tr("Mixed"));
    m_timestampCheck->setText(tr("Timestamp"));
    m_autoScrollCheck->setText(tr("Auto Scroll"));
    m_wordWrapCheck->setText(tr("Word Wrap"));
    m_clearBtn->setText(tr("Clear"));
    m_exportBtn->setText(tr("Export"));
}

} // namespace ComAssistant
