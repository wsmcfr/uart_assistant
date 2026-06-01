/**
 * @file CommunicationWorkspaceWidget.cpp
 * @brief 通信类型工作台基类实现
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#include "CommunicationWorkspaceWidget.h"

#include <QRegularExpression>

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

QByteArray CommunicationWorkspaceWidget::parsePayload(const QString& text, bool hexMode) const
{
    if (!hexMode) {
        return text.toUtf8();
    }

    /*
     * HEX 输入允许用户带空格、换行或 0x 前缀。这里保留十六进制字符，
     * 奇数字符时在前面补 0，避免 QByteArray::fromHex 丢掉最后半字节。
     */
    QString hex = text;
    hex.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
    hex.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    if (hex.size() % 2 != 0) {
        hex.prepend(QLatin1Char('0'));
    }
    return QByteArray::fromHex(hex.toLatin1());
}

} // namespace ComAssistant
