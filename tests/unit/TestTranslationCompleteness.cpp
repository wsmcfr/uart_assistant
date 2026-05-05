/**
 * @file TestTranslationCompleteness.cpp
 * @brief 关键翻译词条完整性测试
 * @author ComAssistant Team
 * @date 2026-03-09
 */

#include "TestTranslationCompleteness.h"

#include <QFile>
#include <QDir>
#include <QXmlStreamReader>
#include <QDebug>

#ifndef COMASSISTANT_SOURCE_DIR
#error "COMASSISTANT_SOURCE_DIR is not defined"
#endif

void TestTranslationCompleteness::initTestCase()
{
    qDebug() << "Starting translation completeness tests...";
}

void TestTranslationCompleteness::cleanupTestCase()
{
    qDebug() << "Translation completeness tests completed.";
}

void TestTranslationCompleteness::testSideNavigationBarTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::SideNavigationBar"),
        {
            {QStringLiteral("连接"), QStringLiteral("Connection")},
            {QStringLiteral("数据"), QStringLiteral("Data")},
            {QStringLiteral("绘图"), QStringLiteral("Plot")},
            {QStringLiteral("工具"), QStringLiteral("Tools")},
            {QStringLiteral("设置"), QStringLiteral("Settings")},
            {QStringLiteral("切换主题"), QStringLiteral("Toggle Theme")},
            {QStringLiteral("帮助"), QStringLiteral("Help")}
        });
}

void TestTranslationCompleteness::testPlotControlPanelTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::PlotControlPanel"),
        {
            {QStringLiteral("控制面板"), QStringLiteral("Control Panel")},
            {QStringLiteral("曲线管理"), QStringLiteral("Curve Management")},
            {QStringLiteral("修改颜色"), QStringLiteral("Change Color")},
            {QStringLiteral("选择曲线颜色"), QStringLiteral("Select Curve Color")},
            {QStringLiteral("坐标轴"), QStringLiteral("Axes")},
            {QStringLiteral("自动缩放"), QStringLiteral("Auto Scale")},
            {QStringLiteral("应用范围"), QStringLiteral("Apply Range")},
            {QStringLiteral("显示"), QStringLiteral("Display")},
            {QStringLiteral("显示网格"), QStringLiteral("Show Grid")},
            {QStringLiteral("显示图例"), QStringLiteral("Show Legend")},
            {QStringLiteral("抗锯齿"), QStringLiteral("Antialiasing")},
            {QStringLiteral("最大点数:"), QStringLiteral("Max Points:")},
            {QStringLiteral("清空数据"), QStringLiteral("Clear Data")},
            {QStringLiteral("曲线 %1"), QStringLiteral("Curve %1")}
        });
}

void TestTranslationCompleteness::testPlotterRenderQualityTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::PlotterWindow"),
        {
            {QStringLiteral("  渲染:"), QStringLiteral("  Render:")},
            {QStringLiteral("切换波形渲染质量档位"), QStringLiteral("Switch waveform rendering quality profile")},
            {QStringLiteral("高质量"), QStringLiteral("High Quality")},
            {QStringLiteral("高性能"), QStringLiteral("High Performance")},
            {QStringLiteral("高质量（细腻）"), QStringLiteral("High Quality (Fine)")},
            {QStringLiteral("高性能（流畅）"), QStringLiteral("High Performance (Smooth)")},
            {QStringLiteral("波形渲染质量(&R)"), QStringLiteral("Waveform Render Quality(&R)")},
            {QStringLiteral("  显示点数:"), QStringLiteral("  Visible Points:")},
            {QStringLiteral("设置可见的数据点数量"), QStringLiteral("Set the number of visible data points")}
        });
}

void TestTranslationCompleteness::testSettingsDialogTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::SettingsDialog"),
        {
            {QStringLiteral("恢复默认"), QStringLiteral("Reset Defaults")},
            {QStringLiteral("确定"), QStringLiteral("OK")},
            {QStringLiteral("取消"), QStringLiteral("Cancel")},
            {QStringLiteral("应用"), QStringLiteral("Apply")},
            {QStringLiteral("自动保存"), QStringLiteral("Auto Save")},
            {QStringLiteral("启用自动保存"), QStringLiteral("Enable Auto Save")},
            {QStringLiteral("保存间隔:"), QStringLiteral("Save Interval:")},
            {QStringLiteral("界面语言:"), QStringLiteral("UI Language:")},
            {QStringLiteral("默认串口参数"), QStringLiteral("Default Serial Parameters")},
            {QStringLiteral("断开时自动重连"), QStringLiteral("Auto reconnect on disconnect")},
            {QStringLiteral("显示"), QStringLiteral("Display")}
        });
}

void TestTranslationCompleteness::testMainWindowLanguageCriticalTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::MainWindow"),
        {
            {QStringLiteral("文件传输(&F)..."), QStringLiteral("File Transfer(&F)...")},
            {QStringLiteral("IAP升级(&I)..."), QStringLiteral("IAP Upgrade(&I)...")},
            {QStringLiteral("宏录制/回放(&M)..."), QStringLiteral("Macro Record/Playback(&M)...")},
            {QStringLiteral("多端口管理(&P)..."), QStringLiteral("Multi-Port Manager(&P)...")},
            {QStringLiteral("Modbus分析(&A)..."), QStringLiteral("Modbus Analyzer(&A)...")},
            {QStringLiteral("数据分窗(&W)..."), QStringLiteral("Data Window Layout(&W)...")},
            {QStringLiteral("控件面板(&C)..."), QStringLiteral("Control Panel(&C)...")},
            {QStringLiteral("数据表格(&T)..."), QStringLiteral("Data Table(&T)...")},
            {QStringLiteral("类型:"), QStringLiteral("Type:")},
            {QStringLiteral("连接"), QStringLiteral("Connect")}
        });
}

void TestTranslationCompleteness::testMainWindowHamburgerMenuTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::MainWindow"),
        {
            /*
             * 这里覆盖汉堡菜单运行时真正使用的无快捷键文本。
             * Qt 翻译匹配要求 source 完全一致，旧菜单栏的“工具箱(&B)...”
             * 无法匹配汉堡菜单的“工具箱...”，因此这些词条需要单独测试。
             */
            {QStringLiteral("文件"), QStringLiteral("File")},
            {QStringLiteral("新建会话"), QStringLiteral("New Session")},
            {QStringLiteral("保存会话"), QStringLiteral("Save Session")},
            {QStringLiteral("加载会话"), QStringLiteral("Load Session")},
            {QStringLiteral("导出数据..."), QStringLiteral("Export Data...")},
            {QStringLiteral("退出"), QStringLiteral("Exit")},
            {QStringLiteral("编辑"), QStringLiteral("Edit")},
            {QStringLiteral("清空全部"), QStringLiteral("Clear All")},
            {QStringLiteral("数据搜索..."), QStringLiteral("Data Search...")},
            {QStringLiteral("视图"), QStringLiteral("View")},
            {QStringLiteral("置顶显示"), QStringLiteral("Always on Top")},
            {QStringLiteral("语言"), QStringLiteral("Language")},
            {QStringLiteral("简体中文"), QStringLiteral("Simplified Chinese")},
            {QStringLiteral("工具"), QStringLiteral("Tools")},
            {QStringLiteral("工具箱..."), QStringLiteral("Toolbox...")},
            {QStringLiteral("脚本编辑器..."), QStringLiteral("Script Editor...")},
            {QStringLiteral("文件传输..."), QStringLiteral("File Transfer...")},
            {QStringLiteral("IAP升级..."), QStringLiteral("IAP Upgrade...")},
            {QStringLiteral("宏录制/回放..."), QStringLiteral("Macro Record/Playback...")},
            {QStringLiteral("多端口管理..."), QStringLiteral("Multi-Port Manager...")},
            {QStringLiteral("Modbus分析..."), QStringLiteral("Modbus Analyzer...")},
            {QStringLiteral("数据分窗..."), QStringLiteral("Data Window Layout...")},
            {QStringLiteral("控件面板..."), QStringLiteral("Control Panel...")},
            {QStringLiteral("数据表格..."), QStringLiteral("Data Table...")},
            {QStringLiteral("设置..."), QStringLiteral("Settings...")},
            {QStringLiteral("绘图"), QStringLiteral("Plot")},
            {QStringLiteral("新建绘图窗口"), QStringLiteral("New Plot Window")},
            {QStringLiteral("关闭所有绘图窗口"), QStringLiteral("Close All Plot Windows")},
            {QStringLiteral("绘图协议"), QStringLiteral("Plot Protocol")},
            {QStringLiteral("无"), QStringLiteral("None")},
            {QStringLiteral("TEXT绘图"), QStringLiteral("TEXT Plot")},
            {QStringLiteral("STAMP绘图"), QStringLiteral("STAMP Plot")},
            {QStringLiteral("CSV绘图"), QStringLiteral("CSV Plot")},
            {QStringLiteral("JustFloat"), QStringLiteral("JustFloat")},
            {QStringLiteral("窗口同步"), QStringLiteral("Window Sync")},
            {QStringLiteral("启用后，所有绘图窗口的暂停状态将同步"), QStringLiteral("When enabled, the paused state of all drawing windows will be synchronized")},
            {QStringLiteral("帮助"), QStringLiteral("Help")},
            {QStringLiteral("帮助文档"), QStringLiteral("Help Documentation")},
            {QStringLiteral("检查更新..."), QStringLiteral("Check Updates...")},
            {QStringLiteral("启动时自动检查更新"), QStringLiteral("Automatically check for updates on startup")},
            {QStringLiteral("关于"), QStringLiteral("About")},
            {QStringLiteral("关于 Qt"), QStringLiteral("About Qt")}
        });
}

void TestTranslationCompleteness::testMainWindowToolbarTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::MainWindow"),
        {
            {QStringLiteral("菜单"), QStringLiteral("Menu")},
            {QStringLiteral("类型:"), QStringLiteral("Type:")},
            {QStringLiteral("串口"), QStringLiteral("Serial")},
            {QStringLiteral("TCP客户端"), QStringLiteral("TCP Client")},
            {QStringLiteral("TCP服务器"), QStringLiteral("TCP Server")},
            {QStringLiteral("选择通信类型"), QStringLiteral("Select communication type")},
            {QStringLiteral("端口:"), QStringLiteral("Port:")},
            {QStringLiteral("模式:"), QStringLiteral("Mode:")},
            {QStringLiteral("选择显示模式"), QStringLiteral("Select display mode")},
            {QStringLiteral("终端"), QStringLiteral("Terminal")},
            {QStringLiteral("帧模式"), QStringLiteral("Frame Mode")},
            {QStringLiteral("调试"), QStringLiteral("Debug")},
            {QStringLiteral("断开"), QStringLiteral("Disconnect")},
            {QStringLiteral("停止服务"), QStringLiteral("Stop Service")},
            {QStringLiteral("解绑"), QStringLiteral("Unbind")}
        });
}

void TestTranslationCompleteness::testQuickSendTranslations()
{
    const ContextTranslations translations = loadTranslations();
    verifyTranslations(
        translations,
        QStringLiteral("ComAssistant::QuickSendWidget"),
        {
            {QStringLiteral("快捷发送"), QStringLiteral("Quick Send")},
            {QStringLiteral("添加快捷指令"), QStringLiteral("Add Quick Command")},
            {QStringLiteral("查询版本"), QStringLiteral("Query Version")}
        });
}

TestTranslationCompleteness::ContextTranslations TestTranslationCompleteness::loadTranslations() const
{
    const QString tsPath = QDir::cleanPath(
        QStringLiteral(COMASSISTANT_SOURCE_DIR) + QStringLiteral("/resources/translations/en_US.ts"));

    QFile file(tsPath);
    if (!file.exists()) {
        qWarning() << "Translation file does not exist:" << tsPath;
        return {};
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open translation file:" << tsPath;
        return {};
    }

    QXmlStreamReader xml(&file);
    ContextTranslations translations;

    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("context")) {
            continue;
        }

        QString contextName;
        QHash<QString, TranslationInfo> contextMessages;

        while (!(xml.isEndElement() && xml.name() == QStringLiteral("context")) && !xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) {
                continue;
            }

            if (xml.name() == QStringLiteral("name")) {
                contextName = xml.readElementText();
                continue;
            }

            if (xml.name() != QStringLiteral("message")) {
                continue;
            }

            QString sourceText;
            TranslationInfo translationInfo;

            while (!(xml.isEndElement() && xml.name() == QStringLiteral("message")) && !xml.atEnd()) {
                xml.readNext();
                if (!xml.isStartElement()) {
                    continue;
                }

                if (xml.name() == QStringLiteral("source")) {
                    sourceText = xml.readElementText();
                    continue;
                }

                if (xml.name() == QStringLiteral("translation")) {
                    translationInfo.unfinished =
                        xml.attributes().value(QStringLiteral("type")) == QStringLiteral("unfinished");
                    translationInfo.text = xml.readElementText();
                    continue;
                }

                xml.skipCurrentElement();
            }

            if (!sourceText.isEmpty()) {
                contextMessages.insert(sourceText, translationInfo);
            }
        }

        if (!contextName.isEmpty() && !contextMessages.isEmpty()) {
            auto& existingContext = translations[contextName];
            for (auto it = contextMessages.cbegin(); it != contextMessages.cend(); ++it) {
                existingContext.insert(it.key(), it.value());
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "Failed to parse translation file:" << xml.errorString();
        return {};
    }

    return translations;
}

void TestTranslationCompleteness::verifyTranslations(
    const ContextTranslations& translations,
    const QString& contextName,
    const QHash<QString, QString>& expectedTranslations) const
{
    QVERIFY2(translations.contains(contextName),
             qPrintable(QStringLiteral("Missing translation context: %1").arg(contextName)));

    const QHash<QString, TranslationInfo>& messages = translations.value(contextName);
    for (auto it = expectedTranslations.cbegin(); it != expectedTranslations.cend(); ++it) {
        const QString& sourceText = it.key();
        const QString& expectedText = it.value();

        QVERIFY2(messages.contains(sourceText),
                 qPrintable(QStringLiteral("Missing source text in %1: %2").arg(contextName, sourceText)));

        const TranslationInfo& actualInfo = messages.value(sourceText);
        QVERIFY2(!actualInfo.unfinished,
                 qPrintable(QStringLiteral("Translation is unfinished in %1: %2").arg(contextName, sourceText)));
        QCOMPARE(actualInfo.text, expectedText);
    }
}
