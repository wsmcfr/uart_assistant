/**
 * @file FileTransferDialog.h
 * @brief 文件传输对话框
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef COMASSISTANT_FILETRANSFERDIALOG_H
#define COMASSISTANT_FILETRANSFERDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QGroupBox>
#include <QTextEdit>
#include <QEvent>

#include "transfer/FileTransfer.h"

namespace ComAssistant {

/**
 * @brief 文件传输对话框
 */
class FileTransferDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileTransferDialog(QWidget* parent = nullptr);
    ~FileTransferDialog() override;

    /**
     * @brief 处理接收到的数据
     */
    void processReceivedData(const QByteArray& data);

    /**
     * @brief 设置连接状态
     */
    void setConnected(bool connected);

    /**
     * @brief 设置IAP模式
     */
    void setIAPMode(bool iapMode);

signals:
    /**
     * @brief 请求发送数据
     */
    void sendData(const QByteArray& data);

private slots:
    void onBrowseClicked();
    void onStartClicked();
    void onCancelClicked();
    void onProgressUpdated(const TransferProgress& progress);
    void onTransferCompleted(bool success, const QString& message);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void updateUI();
    void appendLog(const QString& message);
    void retranslateUi();

private:
    // 传输方向
    QRadioButton* m_sendRadio = nullptr;
    QRadioButton* m_receiveRadio = nullptr;

    // 协议选择
    QComboBox* m_protocolCombo = nullptr;

    // 文件选择
    QLineEdit* m_filePathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;

    // 控制按钮
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    // 进度显示
    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_speedLabel = nullptr;
    QLabel* m_packetLabel = nullptr;

    // 日志
    QTextEdit* m_logEdit = nullptr;

    // 传输对象
    FileTransfer* m_transfer = nullptr;

    // 状态
    bool m_connected = false;
    bool m_iapMode = false;
};

} // namespace ComAssistant

#endif // COMASSISTANT_FILETRANSFERDIALOG_H
