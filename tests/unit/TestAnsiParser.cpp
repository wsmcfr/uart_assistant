/**
 * @file TestAnsiParser.cpp
 * @brief ANSI 终端解析器边界行为测试
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#include "TestAnsiParser.h"
#include "ui/modes/AnsiParser.h"
#include "ui/modes/TerminalBuffer.h"

using namespace ComAssistant;

void TestAnsiParser::testLfReturnsToColumnZero()
{
    /*
     * 串口设备常只输出 LF 作为换行符。接收终端显示时用户期望每条新行
     * 从最左侧开始，否则连续日志会呈阶梯状偏移。该测试固定这个行为：
     * LF 既换到下一行，也回到第 0 列。
     */
    TerminalBuffer buffer(8, 4);
    AnsiParser parser(&buffer);

    parser.process("A\nB");

    QCOMPARE(buffer.cellAt(0, 0).character, QChar('A'));
    QCOMPARE(buffer.cellAt(1, 0).character, QChar('B'));
    QCOMPARE(buffer.cellAt(1, 1).character, QChar(' '));
    QCOMPARE(buffer.cursorRow(), 1);
    QCOMPARE(buffer.cursorCol(), 1);
}
