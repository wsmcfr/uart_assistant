/**
 * @file TestFrameModeWidget.cpp
 * @brief 帧模式校验与内存回收回归测试
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestFrameModeWidget.h"

#include "ui/modes/FrameModeWidget.h"
#include "utils/ChecksumUtils.h"

#include <QPushButton>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTableWidget>

using namespace ComAssistant;

namespace {

/**
 * @brief 构造标准测试帧。
 * @param checksumType 校验类型：0=无，1=XOR，2=SUM，3=CRC16。
 * @param payload 帧内有效载荷。
 * @param corrupt 是否故意破坏校验字节。
 * @return 按 AA + payload + checksum + 55 格式生成的完整帧。
 *
 * 测试统一采用默认帧头 AA、帧尾 55。启用 CRC16 时使用项目现有
 * Modbus CRC16，低字节在前，和接收侧期望保持一致。
 */
QByteArray buildFrameForTest(int checksumType, const QByteArray& payload, bool corrupt = false)
{
    QByteArray frame;
    frame.append(static_cast<char>(0xAA));
    frame.append(payload);

    if (checksumType == 1) {
        quint8 checksum = ChecksumUtils::xorChecksum(payload);
        if (corrupt) {
            checksum ^= 0xFF;
        }
        frame.append(static_cast<char>(checksum));
    } else if (checksumType == 2) {
        quint8 checksum = ChecksumUtils::sumChecksum(payload);
        if (corrupt) {
            checksum = static_cast<quint8>(checksum + 1);
        }
        frame.append(static_cast<char>(checksum));
    } else if (checksumType == 3) {
        quint16 checksum = ChecksumUtils::crc16Modbus(payload);
        if (corrupt) {
            checksum ^= 0xFFFF;
        }
        frame.append(static_cast<char>(checksum & 0xFF));
        frame.append(static_cast<char>((checksum >> 8) & 0xFF));
    }

    frame.append(static_cast<char>(0x55));
    return frame;
}

/**
 * @brief 构造启用指定校验类型的帧模式控件。
 * @param checksumType 校验类型：0=无，1=XOR，2=SUM，3=CRC16。
 * @return 已设置默认 AA/55 帧格式的控件。
 *
 * 测试通过公开 setFrameConfig() 配置控件，避免直接调用私有校验函数，
 * 这样覆盖的是用户实际接收数据时走到的完整路径。
 */
void configureWidgetForChecksum(FrameModeWidget& widget, int checksumType)
{
    ModeFrameConfig config = widget.frameConfig();
    config.header = QByteArray::fromHex("AA");
    config.footer = QByteArray::fromHex("55");
    config.checksumType = checksumType;
    widget.setFrameConfig(config);
}

/**
 * @brief 取帧表格中的状态文本。
 * @param widget 已接收测试帧的帧模式控件。
 * @return 第 0 行状态列文本；表格或单元格不存在时返回空字符串。
 */
QString firstFrameStatusText(FrameModeWidget& widget)
{
    widget.flushPendingFramesForTest();
    QTableWidget* table = widget.findChild<QTableWidget*>();
    if (!table || table->rowCount() == 0 || !table->item(0, 3)) {
        return QString();
    }
    return table->item(0, 3)->text();
}

} // namespace

void TestFrameModeWidget::testChecksumFailuresAreMarkedInvalid()
{
    const QByteArray payload = QByteArray::fromHex("01 02 03 04");

    /*
     * 逐项覆盖三种 UI 暴露的校验方式。旧实现只检查帧头/帧尾，会把这些
     * 故意破坏校验字节的帧误判为有效；测试应先在旧实现下红灯失败。
     */
    for (int checksumType : {1, 2, 3}) {
        FrameModeWidget widget;
        configureWidgetForChecksum(widget, checksumType);
        widget.appendReceivedData(buildFrameForTest(checksumType, payload, true));

        QCOMPARE(firstFrameStatusText(widget), QStringLiteral("✗"));
        QCOMPARE(widget.validFrameCountForTest(), 0);
        QCOMPARE(widget.invalidFrameCountForTest(), 1);
        QVERIFY2(widget.firstFrameErrorForTest().contains(QStringLiteral("校验")),
                 "坏校验帧应在错误信息中明确提示校验失败。");
    }
}

void TestFrameModeWidget::testChecksumSuccessesAreMarkedValid()
{
    const QByteArray payload = QByteArray::fromHex("10 20 30 40");

    /*
     * 正确校验帧仍必须被识别为有效，证明修复没有把所有启用校验的帧都
     * 保守地判成无效。
     */
    for (int checksumType : {1, 2, 3}) {
        FrameModeWidget widget;
        configureWidgetForChecksum(widget, checksumType);
        widget.appendReceivedData(buildFrameForTest(checksumType, payload, false));

        QCOMPARE(firstFrameStatusText(widget), QStringLiteral("✓"));
        QCOMPARE(widget.validFrameCountForTest(), 1);
        QCOMPARE(widget.invalidFrameCountForTest(), 0);
    }
}

void TestFrameModeWidget::testSendWithHeaderAppendsConfiguredChecksum()
{
    FrameModeWidget widget;
    configureWidgetForChecksum(widget, 1);
    QSignalSpy sendSpy(&widget, SIGNAL(sendDataRequested(QByteArray)));

    /*
     * 查找发送编辑框和“带帧头尾发送”按钮，走真实点击路径。普通“发送”
     * 保持原样发送语义，只有该按钮负责按当前帧格式构造完整帧。
     */
    QLineEdit* sendEdit = nullptr;
    for (QLineEdit* lineEdit : widget.findChildren<QLineEdit*>()) {
        if (lineEdit->placeholderText().contains(QStringLiteral("十六进制"))) {
            sendEdit = lineEdit;
            break;
        }
    }
    QVERIFY(sendEdit != nullptr);
    QList<QPushButton*> buttons = widget.findChildren<QPushButton*>();
    QPushButton* sendWithHeaderButton = nullptr;
    for (QPushButton* button : buttons) {
        if (button->text().contains(QStringLiteral("帧头尾"))) {
            sendWithHeaderButton = button;
            break;
        }
    }
    QVERIFY(sendWithHeaderButton != nullptr);

    sendEdit->setText(QStringLiteral("01 02 03"));
    QTest::mouseClick(sendWithHeaderButton, Qt::LeftButton);

    QCOMPARE(sendSpy.count(), 1);
    const QByteArray expected = buildFrameForTest(1, QByteArray::fromHex("01 02 03"));
    QCOMPARE(sendSpy.takeFirst().at(0).toByteArray(), expected);
}

void TestFrameModeWidget::testEmptyFrameMarkersAreRejected()
{
    FrameModeWidget widget;
    configureWidgetForChecksum(widget, 0);

    /*
     * 帧头/帧尾是解析循环前进的锚点。旧实现允许空配置覆盖 m_config，
     * 后续 indexOf(empty) 可能让 processBuffer() 反复提取空帧而不前进。
     * 因此空标记应被忽略，保留上一组可用配置。
     */
    ModeFrameConfig emptyHeader = widget.frameConfig();
    emptyHeader.header.clear();
    emptyHeader.footer = QByteArray::fromHex("55");
    widget.setFrameConfig(emptyHeader);
    QCOMPARE(widget.frameConfig().header, QByteArray::fromHex("AA"));
    QCOMPARE(widget.frameConfig().footer, QByteArray::fromHex("55"));

    ModeFrameConfig emptyFooter = widget.frameConfig();
    emptyFooter.header = QByteArray::fromHex("AA");
    emptyFooter.footer.clear();
    widget.setFrameConfig(emptyFooter);
    QCOMPARE(widget.frameConfig().header, QByteArray::fromHex("AA"));
    QCOMPARE(widget.frameConfig().footer, QByteArray::fromHex("55"));
}

void TestFrameModeWidget::testClearReleasesFrameBuffers()
{
    FrameModeWidget widget;
    configureWidgetForChecksum(widget, 1);

    /*
     * 先制造已落表记录、待刷新队列和接收缓冲容量，再清空。容量回收测试
     * 用测试专用访问器读取容器 capacity，避免依赖进程工作集这种不稳定指标。
     */
    for (int index = 0; index < 120; ++index) {
        widget.appendReceivedData(buildFrameForTest(1, QByteArray::number(index), false));
    }
    widget.flushPendingFramesForTest();
    QVERIFY(widget.frameCapacityForTest() > 0);

    for (int index = 0; index < 80; ++index) {
        widget.appendReceivedData(buildFrameForTest(1, QByteArray("pending-") + QByteArray::number(index), false));
    }
    QVERIFY(widget.pendingFrameCapacityForTest() > 0);

    widget.appendReceivedData(QByteArray(4096, static_cast<char>(0xAA)));
    QVERIFY(widget.receiveBufferCapacityForTest() > 0);

    widget.clear();

    QCOMPARE(widget.frameCountForTest(), 0);
    QCOMPARE(widget.pendingFrameCountForTest(), 0);
    QCOMPARE(widget.receiveBufferSizeForTest(), 0);
    QCOMPARE(widget.frameCapacityForTest(), 0);
    QCOMPARE(widget.pendingFrameCapacityForTest(), 0);
    QCOMPARE(widget.receiveBufferCapacityForTest(), 0);
}
