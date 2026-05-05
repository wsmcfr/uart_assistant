/**
 * @file TestFilterUtils.h
 * @brief 数字滤波工具边界测试头文件
 * @author ComAssistant Team
 * @date 2026-05-05
 */

#ifndef TESTFILTERUTILS_H
#define TESTFILTERUTILS_H

#include <QObject>
#include <QTest>

class TestFilterUtils : public QObject {
    Q_OBJECT

private slots:
    void testFilterPointClampsInvalidWindowSize();
    void testFilterPointMedianWithInvalidWindowSize();
};

#endif // TESTFILTERUTILS_H
