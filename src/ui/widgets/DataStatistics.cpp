/**
 * @file DataStatistics.cpp
 * @brief 数据统计组件实现
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#include "DataStatistics.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QPushButton>
#include <QTimer>

namespace ComAssistant {

DataStatistics::DataStatistics(QWidget* parent)
    : QWidget(parent)
    , m_lastTime(std::chrono::steady_clock::now())
{
    setupUi();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &DataStatistics::updateDisplay);
    connect(m_updateTimer, &QTimer::timeout, this, &DataStatistics::calculateRates);
    m_updateTimer->start(500);  // 500ms更新一次
}

void DataStatistics::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* statsGroup = new QGroupBox(tr("Statistics"));
    QGridLayout* gridLayout = new QGridLayout(statsGroup);

    // 接收统计
    gridLayout->addWidget(new QLabel(tr("RX:")), 0, 0);
    m_rxBytesLabel = new QLabel("0 B");
    m_rxBytesLabel->setObjectName("rxBytesLabel");
    gridLayout->addWidget(m_rxBytesLabel, 0, 1);

    gridLayout->addWidget(new QLabel(tr("Rate:")), 0, 2);
    m_rxRateLabel = new QLabel("0 B/s");
    m_rxRateLabel->setObjectName("rxBytesLabel");
    gridLayout->addWidget(m_rxRateLabel, 0, 3);

    // 发送统计
    gridLayout->addWidget(new QLabel(tr("TX:")), 1, 0);
    m_txBytesLabel = new QLabel("0 B");
    m_txBytesLabel->setObjectName("txBytesLabel");
    gridLayout->addWidget(m_txBytesLabel, 1, 1);

    gridLayout->addWidget(new QLabel(tr("Rate:")), 1, 2);
    m_txRateLabel = new QLabel("0 B/s");
    m_txRateLabel->setObjectName("txBytesLabel");
    gridLayout->addWidget(m_txRateLabel, 1, 3);

    // 重置按钮
    QPushButton* resetBtn = new QPushButton(tr("Reset"));
    resetBtn->setFixedWidth(60);
    connect(resetBtn, &QPushButton::clicked, this, &DataStatistics::reset);
    gridLayout->addWidget(resetBtn, 2, 0, 1, 4, Qt::AlignRight);

    mainLayout->addWidget(statsGroup);
}

void DataStatistics::addRxBytes(qint64 bytes)
{
    m_rxBytes.fetch_add(bytes, std::memory_order_relaxed);
}

void DataStatistics::addTxBytes(qint64 bytes)
{
    m_txBytes.fetch_add(bytes, std::memory_order_relaxed);
}

QString DataStatistics::rxBytesFormatted() const
{
    return formatBytes(m_rxBytes.load(std::memory_order_relaxed));
}

QString DataStatistics::txBytesFormatted() const
{
    return formatBytes(m_txBytes.load(std::memory_order_relaxed));
}

void DataStatistics::reset()
{
    m_rxBytes.store(0, std::memory_order_relaxed);
    m_txBytes.store(0, std::memory_order_relaxed);
    m_rxRate.store(0.0, std::memory_order_relaxed);
    m_txRate.store(0.0, std::memory_order_relaxed);
    m_lastRxBytes.store(0, std::memory_order_relaxed);
    m_lastTxBytes.store(0, std::memory_order_relaxed);
    m_lastTime = std::chrono::steady_clock::now();

    updateDisplay();
}

void DataStatistics::updateDisplay()
{
    m_rxBytesLabel->setText(rxBytesFormatted());
    m_txBytesLabel->setText(txBytesFormatted());
    m_rxRateLabel->setText(formatRate(m_rxRate.load(std::memory_order_relaxed)));
    m_txRateLabel->setText(formatRate(m_txRate.load(std::memory_order_relaxed)));
}

void DataStatistics::calculateRates()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastTime).count();

    if (elapsed > 0) {
        qint64 currentRx = m_rxBytes.load(std::memory_order_relaxed);
        qint64 currentTx = m_txBytes.load(std::memory_order_relaxed);
        qint64 lastRx = m_lastRxBytes.load(std::memory_order_relaxed);
        qint64 lastTx = m_lastTxBytes.load(std::memory_order_relaxed);

        double rxDelta = static_cast<double>(currentRx - lastRx);
        double txDelta = static_cast<double>(currentTx - lastTx);
        double seconds = elapsed / 1000.0;

        m_rxRate.store(rxDelta / seconds, std::memory_order_relaxed);
        m_txRate.store(txDelta / seconds, std::memory_order_relaxed);

        m_lastRxBytes.store(currentRx, std::memory_order_relaxed);
        m_lastTxBytes.store(currentTx, std::memory_order_relaxed);
        m_lastTime = now;
    }
}

QString DataStatistics::formatBytes(qint64 bytes)
{
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

QString DataStatistics::formatRate(double bytesPerSec)
{
    if (bytesPerSec < 1024) {
        return QString("%1 B/s").arg(bytesPerSec, 0, 'f', 0);
    } else if (bytesPerSec < 1024 * 1024) {
        return QString("%1 KB/s").arg(bytesPerSec / 1024.0, 0, 'f', 2);
    } else {
        return QString("%1 MB/s").arg(bytesPerSec / (1024.0 * 1024.0), 0, 'f', 2);
    }
}

} // namespace ComAssistant
