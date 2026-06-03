/**
 * @file TestDataWindow.cpp
 * @brief 数据分窗单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestDataWindow.h"

#include "ui/widgets/DataWindow.h"

#include <QTextEdit>
#include <QTemporaryDir>
#include <QFile>

using namespace ComAssistant;

namespace {

/**
 * @brief 获取数据分窗内部显示文本。
 * @param window 待检查的数据分窗。
 * @return 当前可见文本内容。
 */
QString dataWindowText(DataWindow& window)
{
    QTextEdit* textEdit = window.findChild<QTextEdit*>();
    if (!textEdit) {
        qFatal("DataWindow missing QTextEdit child");
    }
    return textEdit->toPlainText();
}

} // namespace

void TestDataWindow::testLongUnbrokenTextDropsOldestContent()
{
    DataWindow window(QStringLiteral("memory"));
    window.resize(600, 400);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    /*
     * 旧实现只设置 maximumBlockCount，连续无换行数据会停留在一个
     * QTextDocument block 内，行数上限无法触发。这里追加超过默认
     * 字符上限的数据，期望分窗主动裁剪最早内容。
     */
    window.appendData(QStringLiteral("EARLIEST_MARK"));
    window.appendData(QString(2 * 1024 * 1024 + 128 * 1024, QLatin1Char('A')));
    window.appendData(QStringLiteral("LATEST_MARK"));

    const QString text = dataWindowText(window);
    QVERIFY2(text.size() <= 2 * 1024 * 1024 + 4096,
             "数据分窗应按字符上限裁剪无换行长流，不能只依赖最大行数。");
    QVERIFY2(text.contains(QStringLiteral("LATEST_MARK")),
             "裁剪必须保留最新收到的数据。");
    QVERIFY2(!text.contains(QStringLiteral("EARLIEST_MARK")),
             "超过上限后最早内容应被删除。");
}

void TestDataWindow::testExportUsesTrimmedVisibleContent()
{
    DataWindow window(QStringLiteral("export"));
    window.resize(600, 400);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    window.appendData(QStringLiteral("EARLIEST_MARK"));
    window.appendData(QString(2 * 1024 * 1024 + 128 * 1024, QLatin1Char('B')));
    window.appendData(QStringLiteral("LATEST_MARK"));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString filePath = tempDir.filePath(QStringLiteral("data-window.txt"));

    QVERIFY(window.exportToFile(filePath));

    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString exportedText = QString::fromUtf8(file.readAll());
    file.close();

    QVERIFY2(exportedText.contains(QStringLiteral("LATEST_MARK")),
             "导出文件应包含裁剪后仍可见的最新数据。");
    QVERIFY2(!exportedText.contains(QStringLiteral("EARLIEST_MARK")),
             "导出文件不应重新包含已经从分窗裁剪掉的旧数据。");
    QVERIFY2(exportedText.size() <= 2 * 1024 * 1024 + 4096,
             "导出文件大小应与裁剪后的可见内容一致。");
}
