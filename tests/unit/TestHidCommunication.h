/**
 * @file TestHidCommunication.h
 * @brief HID 通信工厂与会话配置回归测试头文件
 * @author ComAssistant Team
 * @date 2026-06-01
 */

#ifndef TESTHIDCOMMUNICATION_H
#define TESTHIDCOMMUNICATION_H

#include <QObject>
#include <QTest>

class TestHidCommunication : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证通信工厂把 HID 纳入受支持类型，并能创建 HID 通信实例。
     */
    void testFactoryCreatesHidCommunication();

    /**
     * @brief 验证缺少设备标识时 HID 打开失败，并返回可读的错误信息。
     */
    void testOpenWithoutDeviceIdentityFailsClearly();

    /**
     * @brief 验证会话文件能保存并恢复 HID 配置，避免重新打开会话后丢失设备选择。
     */
    void testSessionPersistsHidConfig();
};

#endif // TESTHIDCOMMUNICATION_H
