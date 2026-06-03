/**
 * @file TestMainWindowExportIntegration.cpp
 * @brief 主窗口增强导出入口集成测试实现
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestMainWindowExportIntegration.h"

#include "ui/MainWindow.h"
#include "ui/dialogs/ExportDialog.h"

#include <QApplication>
#include <QDialog>
#include <QMetaObject>
#include <QPointer>
#include <QTextEdit>
#include <QTimer>

using namespace ComAssistant;

namespace {

/**
 * @brief 在应用顶层窗口中查找当前显示的指定对话框。
 * @tparam T 对话框类型。
 * @return 找到的可见对话框；没有找到时返回 nullptr。
 */
template <typename T>
T* findVisibleDialog()
{
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget* widget : widgets) {
        T* dialog = qobject_cast<T*>(widget);
        if (dialog && dialog->isVisible()) {
            return dialog;
        }
    }
    return nullptr;
}

/**
 * @brief 关闭所有非目标测试对话框，避免旧实现弹出的文件保存对话框阻塞测试。
 */
void rejectVisibleDialogs()
{
    const QWidgetList widgets = QApplication::topLevelWidgets();
    for (QWidget* widget : widgets) {
        QDialog* dialog = qobject_cast<QDialog*>(widget);
        if (dialog && dialog->isVisible()) {
            dialog->reject();
        }
    }
}

/**
 * @brief 读取增强导出对话框的预览文本。
 * @param dialog 增强导出对话框。
 * @return 预览框当前文本；未找到预览框时返回空字符串。
 */
QString exportPreviewText(ExportDialog* dialog)
{
    if (!dialog) {
        return QString();
    }

    const QList<QTextEdit*> edits = dialog->findChildren<QTextEdit*>();
    for (QTextEdit* edit : edits) {
        if (edit && edit->isReadOnly()) {
            return edit->toPlainText();
        }
    }
    return QString();
}

} // namespace

void TestMainWindowExportIntegration::testExportActionOpensEnhancedDialogWithRxAndTxRecords()
{
    MainWindow window;
    window.resize(1200, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    /*
     * 直接调用私有槽，覆盖主窗口真实接收/发送记录路径。这里不依赖真实
     * 串口连接，避免测试把通信硬件状态引入导出入口验证。
     */
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "onDataReceived",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, QByteArray("rx-payload"))));
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "onDataSent",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, QByteArray("tx-payload"))));

    bool sawExportDialog = false;
    QString preview;
    QTimer::singleShot(0, &window, [&window]() {
        QMetaObject::invokeMethod(&window, "onExportData", Qt::DirectConnection);
    });
    QTimer::singleShot(200, &window, [&sawExportDialog, &preview]() {
        ExportDialog* exportDialog = findVisibleDialog<ExportDialog>();
        if (exportDialog) {
            sawExportDialog = true;
            preview = exportPreviewText(exportDialog);
            exportDialog->reject();
            return;
        }

        rejectVisibleDialogs();
    });

    QTRY_VERIFY_WITH_TIMEOUT(sawExportDialog, 1000);

    QVERIFY2(preview.contains(QStringLiteral("[RX]")),
             "增强导出预览应包含接收方向标记。");
    QVERIFY2(preview.contains(QStringLiteral("[TX]")),
             "增强导出预览应包含发送方向标记。");
    QVERIFY2(preview.contains(QStringLiteral("rx-payload")),
             "增强导出预览应包含主窗口接收历史。");
    QVERIFY2(preview.contains(QStringLiteral("tx-payload")),
             "增强导出预览应包含主窗口发送历史。");
}

void TestMainWindowExportIntegration::testExportHistoryMergesReceiveChunksIntoCompleteTextLine()
{
    MainWindow window;
    window.resize(1200, 760);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    /*
     * 复现用户截图中的场景：设备实际输出一条以换行结束的 PT100 日志，
     * 但串口底层 readyRead 可能按 32/32/4 字节分三次上报。导出历史
     * 应以完整文本行为粒度，而不是以底层分片为粒度。
     */
    const QByteArray line("PT100: Vout=0.0000V signal=-0.9617V R=-491.42ohm T=-50.00C valid=0\r\n");
    QCOMPARE(line.size(), 68);

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "onDataReceived",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, line.left(32))));
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "onDataReceived",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, line.mid(32, 32))));
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "onDataReceived",
                                      Qt::DirectConnection,
                                      Q_ARG(QByteArray, line.mid(64))));

    bool sawExportDialog = false;
    QString preview;
    QTimer::singleShot(0, &window, [&window]() {
        QMetaObject::invokeMethod(&window, "onExportData", Qt::DirectConnection);
    });
    QTimer::singleShot(200, &window, [&sawExportDialog, &preview]() {
        ExportDialog* exportDialog = findVisibleDialog<ExportDialog>();
        if (exportDialog) {
            sawExportDialog = true;
            preview = exportPreviewText(exportDialog);
            exportDialog->reject();
            return;
        }

        rejectVisibleDialogs();
    });

    QTRY_VERIFY_WITH_TIMEOUT(sawExportDialog, 1000);

    const int firstRx = preview.indexOf(QStringLiteral("[RX]"));
    QVERIFY2(firstRx >= 0, "导出预览应包含合并后的 RX 记录。");
    QCOMPARE(preview.indexOf(QStringLiteral("[RX]"), firstRx + 1), -1);
    QVERIFY2(preview.contains(QString::fromUtf8(line).trimmed()),
             "导出预览应包含完整的设备日志行，而不是串口底层分片。");
}
