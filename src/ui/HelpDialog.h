/**
 * @file HelpDialog.h
 * @brief 帮助文档对话框
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#ifndef HELPDIALOG_H
#define HELPDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>

namespace ComAssistant {

/**
 * @brief 帮助文档对话框类
 *
 * 显示应用程序的帮助文档，包括快速入门、协议说明、绘图功能和使用示例。
 */
class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit HelpDialog(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~HelpDialog() override;

    /**
     * @brief 显示特定主题
     * @param topic 主题名称（"quickstart", "protocols", "plotting", "examples"）
     */
    void showTopic(const QString& topic);

    /**
     * @brief 更新帮助文档的显示主题
     * @param isDarkTheme 是否为暗色主题
     */
    void updateTheme(bool isDarkTheme);

private:
    /**
     * @brief 初始化UI
     */
    void setupUi();

    /**
     * @brief 加载帮助内容
     */
    void loadHelpContent();

    /**
     * @brief 加载HTML文件
     * @param browser 文本浏览器
     * @param resourcePath 资源路径
     * @return 是否成功加载
     */
    bool loadHtmlFile(QTextBrowser* browser, const QString& resourcePath);

    /**
     * @brief 应用主题到HTML内容
     * @param content 原始HTML内容
     * @return 应用主题后的HTML内容
     */
    QString applyThemeToHtml(const QString& content);

    /**
     * @brief 重新加载所有帮助内容（带主题）
     */
    void reloadHelpContent();

private slots:
    /**
     * @brief 关闭对话框槽函数
     */
    void onClose();

protected:
    /**
     * @brief 事件处理
     */
    void changeEvent(QEvent* event) override;

private:
    /**
     * @brief 重新翻译UI
     */
    void retranslateUi();

private:
    QTabWidget* m_tabWidget;           ///< 标签页控件
    QTextBrowser* m_quickStartBrowser; ///< 快速入门浏览器
    QTextBrowser* m_modesBrowser;      ///< 模式说明浏览器
    QTextBrowser* m_protocolsBrowser;  ///< 协议说明浏览器
    QTextBrowser* m_plottingBrowser;   ///< 绘图功能浏览器
    QTextBrowser* m_examplesBrowser;   ///< 使用示例浏览器
    QTextBrowser* m_advancedBrowser;   ///< 高级功能浏览器
    QPushButton* m_closeButton;        ///< 关闭按钮
    bool m_isDarkTheme;                ///< 当前是否为暗色主题
};

} // namespace ComAssistant

#endif // HELPDIALOG_H
