/**
 * @file SendWidget.cpp
 * @brief 发送区组件实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "SendWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QEvent>

namespace ComAssistant {

SendWidget::SendWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();

    m_autoSendTimer = new QTimer(this);
    connect(m_autoSendTimer, &QTimer::timeout, this, &SendWidget::onAutoSendTimeout);
}

void SendWidget::setupUi()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // 左侧：输入框
    m_inputEdit = new QTextEdit;
    m_inputEdit->setPlaceholderText(tr("请输入要发送的数据..."));
    m_inputEdit->setFont(QFont("Consolas", 10));
    mainLayout->addWidget(m_inputEdit, 1);

    // 行数标签（隐藏，保留变量避免编译错误）
    m_lineNumberLabel = new QLabel;
    m_lineNumberLabel->hide();

    // 右侧：选项面板
    QWidget* optionsWidget = new QWidget;
    optionsWidget->setFixedWidth(280);
    QVBoxLayout* optionsLayout = new QVBoxLayout(optionsWidget);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(6);

    // 时间戳选项
    QHBoxLayout* timestampLayout = new QHBoxLayout;
    m_timestampCheck = new QCheckBox(tr("时间戳"));
    m_timestampSpin = new QSpinBox;
    m_timestampSpin->setRange(1, 9999);
    m_timestampSpin->setValue(20);
    m_timestampSpin->setSuffix(" ms");
    m_timestampSpin->setFixedWidth(80);
    m_timeoutLabel = new QLabel(tr("超时"));
    timestampLayout->addWidget(m_timestampCheck);
    timestampLayout->addWidget(m_timestampSpin);
    timestampLayout->addWidget(m_timeoutLabel);
    timestampLayout->addStretch();
    optionsLayout->addLayout(timestampLayout);

    // 循环发送选项
    QHBoxLayout* autoSendLayout = new QHBoxLayout;
    m_autoSendCheck = new QCheckBox(tr("循环发送"));
    m_intervalSpin = new QSpinBox;
    m_intervalSpin->setRange(10, 60000);
    m_intervalSpin->setValue(100);
    m_intervalSpin->setFixedWidth(80);
    m_intervalLabel = new QLabel(tr("ms/次"));
    autoSendLayout->addWidget(m_autoSendCheck);
    autoSendLayout->addWidget(m_intervalSpin);
    autoSendLayout->addWidget(m_intervalLabel);
    autoSendLayout->addStretch();
    optionsLayout->addLayout(autoSendLayout);

    // HEX 选项
    QHBoxLayout* hexLayout = new QHBoxLayout;
    m_hexDisplayCheck = new QCheckBox(tr("HEX显示"));
    m_hexSendCheck = new QCheckBox(tr("HEX发送"));
    hexLayout->addWidget(m_hexDisplayCheck);
    hexLayout->addWidget(m_hexSendCheck);
    hexLayout->addStretch();
    optionsLayout->addLayout(hexLayout);

    // 回车发送和追加新行
    QHBoxLayout* enterLayout = new QHBoxLayout;
    m_enterSendCheck = new QCheckBox(tr("回车发送"));
    m_appendNewlineCheck = new QCheckBox(tr("追加新行"));
    m_appendNewlineCheck->setChecked(true);
    enterLayout->addWidget(m_enterSendCheck);
    enterLayout->addWidget(m_appendNewlineCheck);
    enterLayout->addStretch();
    optionsLayout->addLayout(enterLayout);

    optionsLayout->addStretch();

    // 发送和清空按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    m_sendBtn = new QPushButton(tr("发 送"));
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setMinimumHeight(36);

    m_clearBtn = new QPushButton(tr("清 空"));
    m_clearBtn->setMinimumHeight(36);

    buttonLayout->addWidget(m_clearBtn);
    buttonLayout->addWidget(m_sendBtn);
    optionsLayout->addLayout(buttonLayout);

    mainLayout->addWidget(optionsWidget);

    // 连接信号
    connect(m_sendBtn, &QPushButton::clicked, this, &SendWidget::onSendClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &SendWidget::onClearClicked);
    connect(m_autoSendCheck, &QCheckBox::toggled, this, &SendWidget::onAutoSendToggled);
    connect(m_hexSendCheck, &QCheckBox::toggled, [this](bool checked) {
        m_sendMode = checked ? SendMode::Hex : SendMode::Ascii;
    });

    // 快捷键发送
    m_inputEdit->installEventFilter(this);
}

void SendWidget::updateLineNumbers()
{
    // 行号功能已移除
}

QByteArray SendWidget::getData() const
{
    QString text = m_inputEdit->toPlainText();
    QByteArray data = textToBytes(text);

    // 追加换行符
    if (m_appendNewlineCheck->isChecked() && m_sendMode == SendMode::Ascii) {
        data.append(m_appendNewlineSequence.toUtf8());
    }

    return data;
}

void SendWidget::setSendMode(SendMode mode)
{
    m_sendMode = mode;
    m_hexSendCheck->setChecked(mode == SendMode::Hex);
}

void SendWidget::setAppendNewlineEnabled(bool enabled)
{
    if (m_appendNewlineCheck) {
        m_appendNewlineCheck->setChecked(enabled);
    }
}

void SendWidget::setAppendNewlineSequence(const QString& newline)
{
    if (newline == "\r\n" || newline == "\n" || newline == "\r") {
        m_appendNewlineSequence = newline;
    } else {
        m_appendNewlineSequence = "\r\n";
    }
}

void SendWidget::clearHistory()
{
    m_history.clear();
}

void SendWidget::onSendClicked()
{
    QByteArray data = getData();
    if (data.isEmpty()) {
        return;
    }

    // 添加到历史
    QString text = m_inputEdit->toPlainText();
    addToHistory(text);

    emit sendRequested(data);
}

void SendWidget::onClearClicked()
{
    m_inputEdit->clear();
}

void SendWidget::onModeChanged(int index)
{
    Q_UNUSED(index)
}

void SendWidget::onAutoSendToggled(bool checked)
{
    if (checked) {
        m_autoSendTimer->start(m_intervalSpin->value());
    } else {
        m_autoSendTimer->stop();
    }
}

void SendWidget::onAutoSendTimeout()
{
    onSendClicked();
}

void SendWidget::onHistorySelected(int index)
{
    if (index >= 0 && index < m_history.size()) {
        m_inputEdit->setPlainText(m_history[index]);
    }
}

void SendWidget::addToHistory(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }

    // 移除重复项
    m_history.removeAll(text);

    // 添加到开头
    m_history.prepend(text);

    // 限制历史数量
    while (m_history.size() > MAX_HISTORY) {
        m_history.removeLast();
    }
}

QByteArray SendWidget::textToBytes(const QString& text) const
{
    if (m_sendMode == SendMode::Ascii) {
        return text.toUtf8();
    } else {
        // HEX模式 - 只保留有效的十六进制字符
        QString hex = text;
        hex.remove(QRegularExpression("[^0-9A-Fa-f]"));

        // 如果长度为奇数，前面补0
        if (hex.length() % 2 != 0) {
            hex.prepend('0');
        }

        return QByteArray::fromHex(hex.toLatin1());
    }
}

QString SendWidget::bytesToText(const QByteArray& data) const
{
    if (m_sendMode == SendMode::Ascii) {
        return QString::fromUtf8(data);
    } else {
        return data.toHex(' ').toUpper();
    }
}

bool SendWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_inputEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        // 回车发送（如果启用）
        if (m_enterSendCheck && m_enterSendCheck->isChecked()) {
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (!(keyEvent->modifiers() & Qt::ShiftModifier)) {
                    onSendClicked();
                    return true;
                }
            }
        }

        // Ctrl+Enter 发送数据
        if ((keyEvent->modifiers() & Qt::ControlModifier) &&
            (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)) {
            onSendClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SendWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SendWidget::retranslateUi()
{
    // 更新占位文本
    if (m_inputEdit) {
        m_inputEdit->setPlaceholderText(tr("请输入要发送的数据..."));
    }

    // 更新复选框文字
    if (m_timestampCheck) {
        m_timestampCheck->setText(tr("时间戳"));
    }
    if (m_autoSendCheck) {
        m_autoSendCheck->setText(tr("循环发送"));
    }
    if (m_hexDisplayCheck) {
        m_hexDisplayCheck->setText(tr("HEX显示"));
    }
    if (m_hexSendCheck) {
        m_hexSendCheck->setText(tr("HEX发送"));
    }
    if (m_enterSendCheck) {
        m_enterSendCheck->setText(tr("回车发送"));
    }
    if (m_appendNewlineCheck) {
        m_appendNewlineCheck->setText(tr("追加新行"));
    }

    // 更新标签
    if (m_timeoutLabel) {
        m_timeoutLabel->setText(tr("超时"));
    }
    if (m_intervalLabel) {
        m_intervalLabel->setText(tr("ms/次"));
    }

    // 更新按钮
    if (m_sendBtn) {
        m_sendBtn->setText(tr("发 送"));
    }
    if (m_clearBtn) {
        m_clearBtn->setText(tr("清 空"));
    }
}

} // namespace ComAssistant
