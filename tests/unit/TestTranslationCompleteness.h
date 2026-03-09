/**
 * @file TestTranslationCompleteness.h
 * @brief 关键翻译词条完整性测试头文件
 * @author ComAssistant Team
 * @date 2026-03-09
 */

#ifndef TESTTRANSLATIONCOMPLETENESS_H
#define TESTTRANSLATIONCOMPLETENESS_H

#include <QObject>
#include <QHash>
#include <QTest>

class TestTranslationCompleteness : public QObject {
    Q_OBJECT

private:
    struct TranslationInfo {
        QString text;
        bool unfinished = false;
    };

    using ContextTranslations = QHash<QString, QHash<QString, TranslationInfo>>;

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSideNavigationBarTranslations();
    void testPlotControlPanelTranslations();
    void testPlotterRenderQualityTranslations();
    void testSettingsDialogTranslations();
    void testMainWindowLanguageCriticalTranslations();
    void testQuickSendTranslations();

private:
    ContextTranslations loadTranslations() const;
    void verifyTranslations(
        const ContextTranslations& translations,
        const QString& contextName,
        const QHash<QString, QString>& expectedTranslations) const;
};

#endif // TESTTRANSLATIONCOMPLETENESS_H
