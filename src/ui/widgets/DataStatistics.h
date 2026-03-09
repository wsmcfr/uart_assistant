/**
 * @file DataStatistics.h
 * @brief 数据统计组件（使用lock-free atomic实现）
 * @author ComAssistant Team
 * @date 2026-01-15
 */

#ifndef COMASSISTANT_DATASTATISTICS_H
#define COMASSISTANT_DATASTATISTICS_H

#include <QWidget>
#include <QLabel>
#include <atomic>
#include <chrono>

namespace ComAssistant {

/**
 * @brief 数据统计组件
 *
 * 使用std::atomic实现lock-free高性能统计
 */
class DataStatistics : public QWidget {
    Q_OBJECT

public:
    explicit DataStatistics(QWidget* parent = nullptr);
    ~DataStatistics() override = default;

    /**
     * @brief 添加接收字节数
     */
    void addRxBytes(qint64 bytes);

    /**
     * @brief 添加发送字节数
     */
    void addTxBytes(qint64 bytes);

    /**
     * @brief 获取接收字节数
     */
    qint64 rxBytes() const { return m_rxBytes.load(std::memory_order_relaxed); }

    /**
     * @brief 获取发送字节数
     */
    qint64 txBytes() const { return m_txBytes.load(std::memory_order_relaxed); }

    /**
     * @brief 获取格式化的接收字节数
     */
    QString rxBytesFormatted() const;

    /**
     * @brief 获取格式化的发送字节数
     */
    QString txBytesFormatted() const;

    /**
     * @brief 获取接收速率 (bytes/s)
     */
    double rxRate() const { return m_rxRate.load(std::memory_order_relaxed); }

    /**
     * @brief 获取发送速率 (bytes/s)
     */
    double txRate() const { return m_txRate.load(std::memory_order_relaxed); }

    /**
     * @brief 重置统计
     */
    void reset();

private slots:
    void updateDisplay();
    void calculateRates();

private:
    void setupUi();
    static QString formatBytes(qint64 bytes);
    static QString formatRate(double bytesPerSec);

private:
    // lock-free 统计计数器
    std::atomic<qint64> m_rxBytes{0};
    std::atomic<qint64> m_txBytes{0};
    std::atomic<double> m_rxRate{0.0};
    std::atomic<double> m_txRate{0.0};

    // 用于速率计算
    std::atomic<qint64> m_lastRxBytes{0};
    std::atomic<qint64> m_lastTxBytes{0};
    std::chrono::steady_clock::time_point m_lastTime;

    // UI组件
    QLabel* m_rxBytesLabel = nullptr;
    QLabel* m_txBytesLabel = nullptr;
    QLabel* m_rxRateLabel = nullptr;
    QLabel* m_txRateLabel = nullptr;

    QTimer* m_updateTimer = nullptr;
};

} // namespace ComAssistant

#endif // COMASSISTANT_DATASTATISTICS_H
