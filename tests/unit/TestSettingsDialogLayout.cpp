/**
 * @file TestSettingsDialogLayout.cpp
 * @brief 设置窗口布局回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestSettingsDialogLayout.h"

#include "ui/dialogs/SettingsDialog.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QSettings>
#include <QTemporaryDir>

using namespace ComAssistant;

void TestSettingsDialogLayout::testDisplayControlsHaveEnoughRoom()
{
    /*
     * 设置窗口会读取 QSettings。测试使用临时路径和独立组织名，避免污染
     * 用户真实配置，也避免历史配置影响控件默认值。
     */
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, tempDir.path());
    QCoreApplication::setOrganizationName(QStringLiteral("ComAssistantTest"));
    QCoreApplication::setApplicationName(QStringLiteral("SettingsDialogLayoutTest"));

    SettingsDialog dialog;

    /*
     * 截图中设置窗口宽高偏紧，数值框后缀和下拉内容被裁剪。这里先锁住
     * 窗口自身的保底尺寸，保证显示页有足够空间容纳中文标签和控件。
     */
    QVERIFY(dialog.minimumWidth() >= 620);
    QVERIFY(dialog.minimumHeight() >= 540);
    QVERIFY(dialog.size().width() >= 680);
    QVERIFY(dialog.size().height() >= 580);

    const QList<QAbstractSpinBox*> spinBoxes = dialog.findChildren<QAbstractSpinBox*>();
    QVERIFY(!spinBoxes.isEmpty());
    for (const QAbstractSpinBox* spinBox : spinBoxes) {
        QVERIFY2(spinBox->minimumHeight() >= 34,
                 qPrintable(QStringLiteral("Spin box is too short: %1").arg(spinBox->objectName())));
        QVERIFY2(spinBox->minimumWidth() >= 180,
                 qPrintable(QStringLiteral("Spin box is too narrow: %1").arg(spinBox->objectName())));
    }

    const QList<QComboBox*> comboBoxes = dialog.findChildren<QComboBox*>();
    QVERIFY(!comboBoxes.isEmpty());
    for (const QComboBox* comboBox : comboBoxes) {
        QVERIFY2(comboBox->minimumHeight() >= 34,
                 qPrintable(QStringLiteral("Combo box is too short: %1").arg(comboBox->objectName())));
        QVERIFY2(comboBox->minimumWidth() >= 180,
                 qPrintable(QStringLiteral("Combo box is too narrow: %1").arg(comboBox->objectName())));
    }

    const QList<QFormLayout*> formLayouts = dialog.findChildren<QFormLayout*>();
    QVERIFY(!formLayouts.isEmpty());
    for (const QFormLayout* formLayout : formLayouts) {
        QCOMPARE(formLayout->fieldGrowthPolicy(), QFormLayout::ExpandingFieldsGrow);
    }
}
