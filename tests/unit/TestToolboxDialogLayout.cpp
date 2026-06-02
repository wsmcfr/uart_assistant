/**
 * @file TestToolboxDialogLayout.cpp
 * @brief 工具箱窗口布局回归测试实现
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#include "TestToolboxDialogLayout.h"

#include "ui/dialogs/ToolboxDialog.h"

#include <QComboBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>

using namespace ComAssistant;

void TestToolboxDialogLayout::testEncodingTabKeepsControlsReadable()
{
    /*
     * 用户截图显示工具箱编码转换页在暗色主题和较紧窗口下，底部
     * “字符编码转换”区域的输入框、下拉框、输出框和转换按钮互相遮挡。
     * 这里锁住工具箱的保底尺寸，并要求编码页具备滚动容器，避免内容
     * 高度超过标签页时被 Qt 5.12 压缩到不可读。
     */
    ToolboxDialog dialog;
    QVERIFY(dialog.minimumWidth() >= 720);
    QVERIFY(dialog.minimumHeight() >= 620);

    const QList<QScrollArea*> scrollAreas = dialog.findChildren<QScrollArea*>();
    QVERIFY2(!scrollAreas.isEmpty(), "Encoding tab must use a scroll area instead of vertically compressing groups.");

    /*
     * 字符编码转换区的控件后续通过对象名精确定位。保底高度和宽度
     * 可以抵消主题边框、字体缩放、中文文案带来的额外占位。
     */
    QTextEdit* encodingInput = dialog.findChild<QTextEdit*>(QStringLiteral("encodingInputEdit"));
    QTextEdit* encodingOutput = dialog.findChild<QTextEdit*>(QStringLiteral("encodingOutputEdit"));
    QComboBox* fromCombo = dialog.findChild<QComboBox*>(QStringLiteral("encodingFromCombo"));
    QComboBox* toCombo = dialog.findChild<QComboBox*>(QStringLiteral("encodingToCombo"));
    QPushButton* convertButton = dialog.findChild<QPushButton*>(QStringLiteral("encodingConvertButton"));

    QVERIFY(encodingInput);
    QVERIFY(encodingOutput);
    QVERIFY(fromCombo);
    QVERIFY(toCombo);
    QVERIFY(convertButton);

    QVERIFY(encodingInput->minimumHeight() >= 64);
    QVERIFY(encodingOutput->minimumHeight() >= 64);
    QVERIFY(fromCombo->minimumWidth() >= 180);
    QVERIFY(toCombo->minimumWidth() >= 180);
    QVERIFY(convertButton->minimumHeight() >= 34);
    QVERIFY(convertButton->minimumWidth() >= 96);
}
