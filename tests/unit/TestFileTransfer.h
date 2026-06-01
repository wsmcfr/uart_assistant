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
};

#endif // COMASSISTANT_TESTFILETRANSFER_H
