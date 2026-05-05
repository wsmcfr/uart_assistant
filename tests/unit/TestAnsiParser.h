/**
 * @file TestAnsiParser.h
 * @brief ANSI 终端解析器边界行为测试头文件
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#ifndef TESTANSIPARSER_H
#define TESTANSIPARSER_H

#include <QObject>
#include <QTest>

class TestAnsiParser : public QObject {
    Q_OBJECT

private slots:
    void testLfReturnsToColumnZero();
};

#endif // TESTANSIPARSER_H
