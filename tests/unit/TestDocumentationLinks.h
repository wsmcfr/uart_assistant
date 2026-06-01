/**
 * @file TestDocumentationLinks.h
 * @brief 用户指南文档链接检查测试头文件
 * @author ComAssistant Team
 * @date 2026-06-02
 */

#ifndef TESTDOCUMENTATIONLINKS_H
#define TESTDOCUMENTATIONLINKS_H

#include <QObject>
#include <QTest>

/**
 * @brief 用户指南文档链接检查测试
 *
 * 该测试用于在本地和 CI 中提前发现 Markdown 用户指南引用了不存在
 * 的本地章节，避免 README、docs 与内置帮助长期漂移。
 */
class TestDocumentationLinks : public QObject
{
    Q_OBJECT

private slots:
    /**
     * @brief 验证 docs/user-guide 下的相对 Markdown 链接都指向真实文件。
     */
    void testUserGuideMarkdownLinksResolve();
};

#endif // TESTDOCUMENTATIONLINKS_H
