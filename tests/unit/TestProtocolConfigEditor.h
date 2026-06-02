/**
 * @file TestProtocolConfigEditor.h
 * @brief 协议配置编辑器单元测试头文件
 */

#ifndef TESTPROTOCOLCONFIGEDITOR_H
#define TESTPROTOCOLCONFIGEDITOR_H

#include <QObject>
#include <QTest>

/**
 * @brief 协议配置编辑器测试类
 *
 * 该测试类验证 Schema 字段可以生成对应 Qt 控件，并且控件 objectName 稳定，
 * 便于后续对话框、翻译和回归测试定位。
 */
class TestProtocolConfigEditor : public QObject
{
    Q_OBJECT

private slots:
    void buildsWidgetsFromSchema();
    void loadsAndReadsConfig();
    void restoresDefaults();
    void reportsValidationErrors();
    void dialogAcceptsNormalizedConfig();
};

#endif // TESTPROTOCOLCONFIGEDITOR_H
