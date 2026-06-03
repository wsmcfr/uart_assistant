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
#include <QCheckBox>
#include <QSpinBox>
#include <QStackedWidget>

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

    /**
     * @brief 通知当前文件传输对象本地发送结果。
     * @param success 当前包是否已进入/写出主窗口发送队列。
     * @param errorMessage 失败原因。
     */
    void notifyLocalSendResult(bool success, const QString& errorMessage);

signals:
    /**
     * @brief 请求发送数据
     */
    void sendData(const QByteArray& data);

private slots:
    void onBrowseClicked();
    void onStartClicked();
    void onPauseClicked();
    void onCancelClicked();
    void onProgressUpdated(const TransferProgress& progress);
    void onTransferCompleted(bool success, const QString& message);
    void onModeChanged(int index);
    void onFilePathChanged(const QString& path);

protected:
    void changeEvent(QEvent* event) override;

private:
    enum class DialogTransferMode {
        RawStream,
        StandardProtocol,
        CustomOta
    };

    void setupUi();
    void updateUI();
    void updateFileInfo();
    void resetFileInfo();
    void updateEstimatedInfo();
    void setTransferControlsEnabled(bool enabled);
    DialogTransferMode currentMode() const;
    int currentBlockSize() const;
    int currentIntervalMs() const;
    bool calculateSelectedFileCrc32(quint32& crc32, QString& errorMessage) const;
    void appendLog(const QString& message);
    void retranslateUi();

    /**
     * @brief 收集并校验自定义 OTA 参数。
     * @param options 输出可直接传给 OtaFileTransfer 的参数。
     * @param errorMessage 参数非法时输出给用户看的错误文本。
     * @return 所有字段可用时返回 true。
     *
     * 该函数把 Magic 文本解析集中在启动前处理，避免 UI 输入、核心
     * OtaTransferOptions 和后续测试各自理解不同格式。
     */
    bool collectOtaOptions(OtaTransferOptions& options, QString& errorMessage) const;

private:
    // 模式选择
    QComboBox* m_modeCombo = nullptr;

    // 传输方向
    QGroupBox* m_directionGroup = nullptr;
    QRadioButton* m_sendRadio = nullptr;
    QRadioButton* m_receiveRadio = nullptr;

    // 协议选择
    QLabel* m_protocolLabel = nullptr;
    QComboBox* m_protocolCombo = nullptr;
    QWidget* m_standardOptionsWidget = nullptr;
    QStackedWidget* m_optionsStack = nullptr;

    // 裸流参数
    QWidget* m_rawOptionsWidget = nullptr;
    QSpinBox* m_rawBlockSizeSpin = nullptr;
    QSpinBox* m_rawIntervalSpin = nullptr;

    // OTA 参数
    QWidget* m_otaOptionsWidget = nullptr;
    QLineEdit* m_otaMagicEdit = nullptr;
    QSpinBox* m_otaBlockSizeSpin = nullptr;
    QSpinBox* m_otaIntervalSpin = nullptr;
    QCheckBox* m_otaWaitAckCheck = nullptr;
    QLineEdit* m_otaAckEdit = nullptr;
    QSpinBox* m_otaTimeoutSpin = nullptr;
    QSpinBox* m_otaRetrySpin = nullptr;

    // 文件选择
    QLineEdit* m_filePathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;

    // 文件信息
    QGroupBox* m_fileInfoGroup = nullptr;
    QLabel* m_fileNameValueLabel = nullptr;
    QLabel* m_fileSizeValueLabel = nullptr;
    QLabel* m_crc32ValueLabel = nullptr;
    QLabel* m_blockCountValueLabel = nullptr;
    QLabel* m_estimatedValueLabel = nullptr;

    // 控制按钮
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
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
    bool m_transferPaused = false;
    quint32 m_selectedFileCrc32 = 0;
    qint64 m_selectedFileSize = 0;
};

} // namespace ComAssistant

#endif // COMASSISTANT_FILETRANSFERDIALOG_H
