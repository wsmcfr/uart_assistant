/**
 * @file TestExportDialog.cpp
 * @brief 增强导出对话框单元测试实现
 * @author ComAssistant Team
 * @date 2026-06-03
 */

#include "TestExportDialog.h"

#include "core/export/DataExporter.h"
#include "ui/dialogs/ExportDialog.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>

using namespace ComAssistant;

namespace {

/**
 * @brief 按 objectName 查找必需子控件。
 * @tparam T 子控件类型。
 * @param root 对话框根对象。
 * @param objectName 稳定对象名。
 * @return 找到的子控件；缺失时让测试失败。
 */
template <typename T>
T* requireChild(QObject& root, const char* objectName)
{
    T* child = root.findChild<T*>(QString::fromLatin1(objectName));
    if (!child) {
        qFatal("Missing child widget: %s", objectName);
    }
    return child;
}

} // namespace

void TestExportDialog::testFilteredStatisticsReflectCurrentOptions()
{
    ExportDialog dialog;

    QVector<DataRecord> records;
    DataRecord first = DataRecord::fromReceive(QByteArray("KEEP first"));
    DataRecord second = DataRecord::fromSend(QByteArray("DROP second"));
    DataRecord third = DataRecord::fromReceive(QByteArray("KEEP third"));
    records << first << second << third;

    dialog.setRecords(records);

    QCheckBox* filterContentCheck =
        requireChild<QCheckBox>(dialog, "exportFilterContentCheck");
    QLineEdit* contentFilterEdit =
        requireChild<QLineEdit>(dialog, "exportContentFilterEdit");
    QLabel* filteredRecordsLabel =
        requireChild<QLabel>(dialog, "exportFilteredRecordsLabel");

    /*
     * 先启用内容过滤，再输入过滤规则。对话框应立即刷新统计，显示当前
     * 过滤条件下真正会导出的记录数，而不是继续显示总记录数。
     */
    filterContentCheck->setChecked(true);
    contentFilterEdit->setText(QStringLiteral("KEEP"));

    QCOMPARE(filteredRecordsLabel->text(), QStringLiteral("过滤后: 2"));
}
