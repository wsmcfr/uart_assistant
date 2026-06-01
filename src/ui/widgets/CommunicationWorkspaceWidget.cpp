/**
 * @file CommunicationWorkspaceWidget.cpp
 * @brief 通信类型工作台基类实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "CommunicationWorkspaceWidget.h"

#include <QDateTime>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringList>

namespace ComAssistant {

CommunicationWorkspaceWidget::CommunicationWorkspaceWidget(QWidget* parent)
    : QWidget(parent)
{
}

void CommunicationWorkspaceWidget::setConnected(bool connected)
{
    m_connected = connected;
}

void CommunicationWorkspaceWidget::appendReceivedData(const QByteArray& data)
{
    Q_UNUSED(data)
}

void CommunicationWorkspaceWidget::appendSentData(const QByteArray& data)
{
    Q_UNUSED(data)
}

void CommunicationWorkspaceWidget::clear()
{
}

QString CommunicationWorkspaceWidget::bytesToHex(const QByteArray& data) const
{
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

QString CommunicationWorkspaceWidget::normalizeHexText(const QString& text) const
{
    /*
     * HEX 规范化按用户输入片段处理，而不是把所有字符拼成一串后整体补 0。
     * 这样 "0x0a bb c" 会被理解为 0A、BB、0C，符合手工输入短字节的直觉。
     */
    const QRegularExpression tokenPattern(
        QStringLiteral("(?:0[xX][0-9A-Fa-f]+)|(?:[0-9A-Fa-f]+)"));
    QRegularExpressionMatchIterator iterator = tokenPattern.globalMatch(text);
    QStringList groups;
    while (iterator.hasNext()) {
        QString token = iterator.next().captured(0);
        token.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
        if (token.size() % 2 != 0) {
            token.prepend(QLatin1Char('0'));
        }

        for (int index = 0; index < token.size(); index += 2) {
            groups << token.mid(index, 2).toUpper();
        }
    }
    return groups.join(QLatin1Char(' '));
}

QString CommunicationWorkspaceWidget::payloadPreview(const QByteArray& data, int maxChars) const
{
    const int safeMaxChars = qMax(0, maxChars);
    QString preview = QString::fromUtf8(data);
    for (int index = 0; index < preview.size(); ++index) {
        /*
         * 控制字符在日志里会破坏单行结构，统一替换成点号；普通中文或英文
         * 字符保持原样，方便用户从日志里快速识别文本协议内容。
         */
        const ushort code = preview.at(index).unicode();
        if (code < 0x20 || code == 0x7F) {
            preview[index] = QLatin1Char('.');
        }
    }

    if (safeMaxChars > 0 && preview.size() > safeMaxChars) {
        preview = preview.left(safeMaxChars) + QStringLiteral("...");
    }
    return preview;
}

QString CommunicationWorkspaceWidget::formatLogLine(const QString& direction,
                                                    const QString& type,
                                                    const QByteArray& data) const
{
    return QStringLiteral("[%1] %2 %3 %4 字节  HEX:%5  文本:%6")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             direction,
             type,
             QString::number(data.size()),
             bytesToHex(data),
             payloadPreview(data));
}

void CommunicationWorkspaceWidget::appendLogLine(QPlainTextEdit* logEdit,
                                                 const QString& direction,
                                                 const QString& type,
                                                 const QByteArray& data,
                                                 bool autoScroll) const
{
    if (!logEdit) {
        return;
    }

    /*
     * autoScroll 关闭时先记录滚动条位置，追加日志后恢复，避免用户查看历史时
     * 新数据把视图强行拉到底部。
     */
    QScrollBar* scrollBar = logEdit->verticalScrollBar();
    const int oldScrollValue = scrollBar ? scrollBar->value() : 0;
    logEdit->appendPlainText(formatLogLine(direction, type, data));
    if (autoScroll && scrollBar) {
        scrollBar->setValue(scrollBar->maximum());
    } else if (scrollBar) {
        scrollBar->setValue(oldScrollValue);
    }
}

QByteArray CommunicationWorkspaceWidget::parsePayload(const QString& text, bool hexMode) const
{
    if (!hexMode) {
        return text.toUtf8();
    }

    const QString hex = normalizeHexText(text);
    return QByteArray::fromHex(hex.toLatin1());
}

} // namespace ComAssistant
