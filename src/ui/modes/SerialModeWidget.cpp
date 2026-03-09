/**
 * @file SerialModeWidget.cpp
 * @brief 串口模式组件实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "SerialModeWidget.h"
#include <QVBoxLayout>

namespace ComAssistant {

SerialModeWidget::SerialModeWidget(QWidget* parent)
    : IModeWidget(parent)
{
    setupUi();
}

void SerialModeWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 主分割器（垂直：接收区在上，发送区在下）
    m_splitter = new QSplitter(Qt::Vertical);

    // 接收区
    m_receiveWidget = new TabbedReceiveWidget;
    m_splitter->addWidget(m_receiveWidget);

    // 发送区
    m_sendWidget = new SendWidget;
    m_splitter->addWidget(m_sendWidget);

    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter);

    // 转发发送请求
    connect(m_sendWidget, &SendWidget::sendRequested,
            this, &SerialModeWidget::sendDataRequested);

    // 转发数据接收信号
    connect(m_receiveWidget, &TabbedReceiveWidget::dataReceived,
            this, &SerialModeWidget::dataReceived);
}

void SerialModeWidget::appendReceivedData(const QByteArray& data)
{
    m_receiveWidget->appendData(data);
}

void SerialModeWidget::appendSentData(const QByteArray& data)
{
    m_receiveWidget->appendSentData(data);
}

void SerialModeWidget::clear()
{
    m_receiveWidget->clear();
}

bool SerialModeWidget::exportToFile(const QString& fileName)
{
    m_receiveWidget->exportToFile(fileName);
    return true;
}

void SerialModeWidget::onActivated()
{
    m_sendWidget->setFocus();
}

void SerialModeWidget::onDeactivated()
{
    // 保存状态
}

} // namespace ComAssistant
