/**
 * @file TestFileTransfer.h
 * @brief 文件传输核心逻辑单元测试
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef COMASSISTANT_TESTFILETRANSFER_H
#define COMASSISTANT_TESTFILETRANSFER_H

#include <QObject>

/**
 * @brief 文件传输核心逻辑测试类
 *
 * 该测试类只覆盖不依赖真实串口和 UI 的核心行为，例如裸流分块、
 * OTA 文件头和数据包编码。这样可以在没有硬件的环境下快速验证
 * 协议输出是否符合约定。
 */
class TestFileTransfer : public QObject {
    Q_OBJECT

private slots:
    /**
     * @brief 验证裸流发送器会按指定块大小切分数据
     *
     * 主要流程：构造 10 字节测试数据，按 4 字节分块，检查输出为
     * 4/4/2 三个数据块。该测试用于保护后续定时发送逻辑的基础切片规则。
     */
    void testRawChunkerBuildsExpectedChunks();

    /**
     * @brief 验证 OTA 文件头使用约定的小端元数据格式
     *
     * 主要流程：构造固定文件名、大小、CRC32 和块大小，检查 magic、
     * 文件大小、CRC32、块大小和文件名长度均按协议写入。
     */
    void testOtaHeaderUsesLittleEndianMetadata();

    /**
     * @brief 验证 OTA 文件头支持 uint32_t 形式的 magic。
     *
     * 主要流程：使用 0x474F5441UL 这类 MCU 侧常见的 uint32_t magic
     * 构造文件头，检查包头按小端数值写入，避免继续把 magic 当任意字符串处理。
     */
    void testOtaHeaderAcceptsUint32Magic();

    /**
     * @brief 验证 OTA magic 文本能解析为 uint32_t。
     *
     * 主要流程：分别解析 ASCII 默认值和 0x474F5441UL 十六进制常量，
     * 检查输出的 32 位数值符合小端包头约定，并拒绝超过 uint32_t 的输入。
     */
    void testOtaMagicTextParsesUint32();

    /**
     * @brief 文件传输对话框的 OTA magic 输入框应允许十六进制 uint32_t。
     *
     * 主要流程：切换到自定义 OTA 模式，输入 0x474F5441UL，确认输入不会
     * 被旧的 4 字符限制截断，便于用户直接填写固件侧常量。
     */
    void testFileTransferDialogAcceptsUint32OtaMagicInput();

    /**
     * @brief 验证 OTA 数据包包含块序号、长度、载荷和 CRC16
     *
     * 主要流程：构造固定块序号和 payload，检查包头字段、payload
     * 位置以及尾部 CRC16 是否等于对前面字段计算出的 CRC16-XMODEM。
     */
    void testOtaDataPacketIncludesIndexLengthPayloadAndCrc();

    /**
     * @brief 验证 OTA 结束包携带 DONE 标识和整文件 CRC32
     *
     * 主要流程：构造固定 CRC32，检查结束包以 DONE 开头，并以小端
     * 格式携带整文件 CRC32，供下位机最终确认。
     */
    void testOtaEndPacketCarriesDoneAndCrc32();

    /**
     * @brief 裸流发送应等待本地发送确认后再读取下一块
     *
     * 主要流程：启动两块裸流文件发送，检查第一块发出后不会自动读取
     * 第二块；调用发送成功确认后才继续推进。
     */
    void testRawTransferWaitsForLocalSendResultBeforeNextChunk();

    /**
     * @brief 裸流本地发送失败时应停止传输并保留错误
     *
     * 主要流程：第一块发出后模拟主窗口发送失败，验证传输进入错误状态，
     * 不继续读取后续文件块。
     */
    void testRawTransferFailsWhenLocalSendFails();

    /**
     * @brief OTA 发送应等待本地发送确认后再推进数据包
     *
     * 主要流程：启动无 ACK OTA 发送，确认文件头发出后不会自动读取数据块；
     * 每次调用本地发送成功确认后才进入下一阶段。
     */
    void testOtaTransferWaitsForLocalSendResultBeforeNextPacket();

    /**
     * @brief 裸流暂停和继续应使用统一 Running/Paused 状态
     *
     * 主要流程：启动裸流发送后检查 Running，暂停后检查 Paused，继续后
     * 检查 Running，确保 UI 可以用统一状态控制按钮。
     */
    void testRawTransferUsesRunningPausedStates();

    /**
     * @brief 裸流取消应经过 Cancelling 并最终进入 Cancelled
     *
     * 主要流程：启动发送后取消，检查状态变化信号包含 Cancelling 和
     * Cancelled，避免重复取消和资源释放状态不可见。
     */
    void testRawTransferCancelTransitionsThroughCancelling();

    /**
     * @brief 本地发送失败应进入统一 Failed 状态并保留错误
     *
     * 主要流程：发送第一块后模拟本地失败，检查状态为 Failed 且
     * progress.errorMessage 保留失败原因。
     */
    void testRawTransferLocalFailureUsesFailedState();

    /**
     * @brief XMODEM 发送启动后不能把整文件缓存到内存。
     *
     * 主要流程：创建超过单个协议块的文件，启动发送但不完成握手，检查
     * 传输对象没有把整个文件内容放入 m_fileData。随后模拟接收端发起
     * CRC 模式，验证首包仍按文件内容正确发送。
     */
    void testXModemSendStreamsFileWithoutWholeFileCache();

    /**
     * @brief XMODEM EOT 阶段超时应重发 EOT 而不是上一数据包。
     *
     * 主要流程：发送一个小文件到数据包 ACK 后进入 EOT 阶段，手动触发
     * 超时回调，验证最后一次 sendData 是 EOT 控制字节。
     */
    void testXModemSendRetransmitsEotOnTimeout();

    /**
     * @brief YMODEM 发送头包阶段不能把整文件缓存到内存。
     *
     * 主要流程：创建超过单个 1K 块的文件，启动 YMODEM 并模拟接收端
     * 请求头包，检查头包发送后 m_fileData 仍不持有整文件，同时首个
     * 数据包可以从文件读取正确内容。
     */
    void testYModemSendStreamsFileWithoutWholeFileCache();

    /**
     * @brief YMODEM 发送收到 NAK 时必须重发当前数据包。
     *
     * 主要流程：完成头包握手并发出第一个数据包后，模拟接收端返回 NAK，
     * 验证发送器不会读取下一包，而是重发当前待确认的数据包。
     */
    void testYModemSendRetransmitsCurrentPacketOnNak();

    /**
     * @brief YMODEM 每个数据包 ACK 后应重置重试计数。
     *
     * 主要流程：限制最大重试为 1，先让第一个数据包 NAK 后 ACK，再让
     * 第二个数据包 NAK；验证第二个包仍可重发，不会继承上一包的重试数
     * 而直接失败。
     */
    void testYModemSendResetsRetryCountAfterPacketAck();

    /**
     * @brief YMODEM-G 发送应由 G 握手启动并连续发送数据包。
     *
     * 主要流程：启动 YMODEM-G 发送，先用接收端的 G 请求文件头，再用
     * 第二个 G 请求数据流；验证发送端不等待每个数据包 ACK，而是连续
     * 发出所有数据包和 EOT，最后收到 ACK+G 后发送空头结束批次。
     */
    void testYModemGSendStreamsPacketsWithoutPerPacketAck();

    /**
     * @brief YMODEM-G 接收应使用 G 握手且不对每个数据包返回 ACK。
     *
     * 主要流程：接收端启动后先发送 G；收到文件头后再次发送 G；收到
     * 数据包时直接写入文件但不 ACK；收到 EOT 后才 ACK 并发送下一个 G。
     */
    void testYModemGReceiveUsesGHandshakeAndSkipsDataAcks();

    /**
     * @brief YMODEM-G 接收发现错误包时必须中止而不是请求重传。
     *
     * 主要流程：接收合法头包后输入 CRC 损坏的数据包，验证接收端发送
     * 多字节 CAN 中止序列、释放文件句柄并进入 Failed 状态。
     */
    void testYModemGReceiveAbortsOnCorruptDataPacket();

    /**
     * @brief 文件传输对话框标准协议列表必须暴露 YMODEM-G。
     *
     * 主要流程：构造对话框并切到 XMODEM/YMODEM 模式，检查协议下拉框
     * 包含 YMODEM-G，且接收方向仍可选，避免枚举存在但 UI 无入口。
     */
    void testFileTransferDialogExposesYModemGProtocol();

    /**
     * @brief XMODEM 接收应边校验边写入文件，不再累计整文件数据。
     *
     * 主要流程：启动 XMODEM-CRC 接收，喂入一个合法 128 字节包，验证
     * 接收端立即 ACK、`m_fileData` 不增长；收到 EOT 后输出文件包含
     * 完整协议块内容。
     */
    void testXModemReceiveWritesPacketsWithoutAccumulatingFileData();

    /**
     * @brief XMODEM 接收应保留半包直到后续数据补齐。
     *
     * 主要流程：把一个合法 XMODEM 包拆成两段输入，验证第一段不会被
     * 清空丢失，第二段到达后能完成校验并返回 ACK。
     */
    void testXModemReceiveKeepsPartialPacketUntilComplete();

    /**
     * @brief XMODEM 接收收到发送方取消时必须释放目标文件。
     *
     * 主要流程：开始接收后模拟发送方发送 CAN，验证状态为 Cancelled，
     * 完成信号为失败，且内部文件句柄已经释放。
     */
    void testXModemReceiveSenderCancelReleasesOpenFile();

    /**
     * @brief XMODEM 接收校验失败超限时必须释放目标文件。
     *
     * 主要流程：最大重试设为 0 后输入 CRC 错误包，验证传输进入 Failed，
     * 错误文本保留，并且目标文件句柄释放。
     */
    void testXModemReceiveFailureReleasesOpenFile();

    /**
     * @brief YMODEM 接收应完整处理文件头、数据包、EOT 和空头结束。
     *
     * 主要流程：接收端先发送 C 握手；收到 0 号文件头后 ACK 并再次发送 C；
     * 收到数据包后写入声明大小的数据；EOT 按 NAK/ACK+C 双阶段收尾；
     * 最后收到空文件头后 ACK 完成批次。
     */
    void testYModemReceiveCompletesSingleFileBatch();

    /**
     * @brief YMODEM 接收应把无效文件头 NAK 后继续等待正确头包。
     *
     * 主要流程：先发送 CRC 错误的 0 号头包，验证接收端返回 NAK 且不创建
     * 输出文件；随后发送正确头包，验证状态机可以继续推进。
     */
    void testYModemReceiveRejectsCorruptHeaderAndAcceptsRetry();

    /**
     * @brief YMODEM 接收必须阻止文件名路径穿越保存目录。
     *
     * 主要流程：发送包含上级目录的文件名，验证接收端失败并且不会在
     * 保存目录之外创建文件。
     */
    void testYModemReceiveRejectsUnsafeFileName();

    /**
     * @brief YMODEM 接收收到 CAN 后必须释放已打开的目标文件。
     *
     * 主要流程：先接收合法头包打开目标文件，再模拟发送方取消，验证状态
     * 进入 Cancelled，且 Windows 下可以立即删除目标文件。
     */
    void testYModemReceiveSenderCancelReleasesOpenFile();
};

#endif // COMASSISTANT_TESTFILETRANSFER_H
