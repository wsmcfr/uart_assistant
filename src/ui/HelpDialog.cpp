/**
 * @file HelpDialog.cpp
 * @brief 帮助文档对话框实现
 * @author ComAssistant Team
 * @date 2026-01-16
 */

#include "HelpDialog.h"
#include "core/utils/Logger.h"

#include <QFile>
#include <QMessageBox>

namespace ComAssistant {

HelpDialog::HelpDialog(QWidget *parent)
    : QDialog(parent)
    , m_tabWidget(nullptr)
    , m_quickStartBrowser(nullptr)
    , m_modesBrowser(nullptr)
    , m_protocolsBrowser(nullptr)
    , m_plottingBrowser(nullptr)
    , m_examplesBrowser(nullptr)
    , m_advancedBrowser(nullptr)
    , m_closeButton(nullptr)
    , m_isDarkTheme(true)  // 默认暗色主题
{
    setupUi();
    loadHelpContent();
}

HelpDialog::~HelpDialog()
{
}

void HelpDialog::setupUi()
{
    setWindowTitle(tr("帮助文档"));
    setMinimumSize(800, 600);
    resize(900, 700);

    // 创建主布局
    auto* mainLayout = new QVBoxLayout(this);

    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);

    // 创建各个浏览器
    m_quickStartBrowser = new QTextBrowser(this);
    m_quickStartBrowser->setOpenExternalLinks(true);

    m_modesBrowser = new QTextBrowser(this);
    m_modesBrowser->setOpenExternalLinks(true);

    m_protocolsBrowser = new QTextBrowser(this);
    m_protocolsBrowser->setOpenExternalLinks(true);

    m_plottingBrowser = new QTextBrowser(this);
    m_plottingBrowser->setOpenExternalLinks(true);

    m_examplesBrowser = new QTextBrowser(this);
    m_examplesBrowser->setOpenExternalLinks(true);

    m_advancedBrowser = new QTextBrowser(this);
    m_advancedBrowser->setOpenExternalLinks(true);

    // 添加标签页
    m_tabWidget->addTab(m_quickStartBrowser, tr("快速入门"));
    m_tabWidget->addTab(m_modesBrowser, tr("模式说明"));
    m_tabWidget->addTab(m_protocolsBrowser, tr("协议说明"));
    m_tabWidget->addTab(m_plottingBrowser, tr("绘图功能"));
    m_tabWidget->addTab(m_advancedBrowser, tr("高级功能"));
    m_tabWidget->addTab(m_examplesBrowser, tr("使用示例"));

    mainLayout->addWidget(m_tabWidget);

    // 创建底部按钮布局
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_closeButton = new QPushButton(tr("关闭"), this);
    m_closeButton->setMinimumWidth(100);
    connect(m_closeButton, &QPushButton::clicked, this, &HelpDialog::onClose);

    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void HelpDialog::loadHelpContent()
{
    // 加载快速入门
    if (!loadHtmlFile(m_quickStartBrowser, ":/help/quickstart.html")) {
        m_quickStartBrowser->setHtml(tr("<h1>加载失败</h1><p>无法加载快速入门文档。</p>"));
    }

    // 加载模式说明
    if (!loadHtmlFile(m_modesBrowser, ":/help/modes.html")) {
        m_modesBrowser->setHtml(tr("<h1>加载失败</h1><p>无法加载模式说明文档。</p>"));
    }

    // 加载协议说明
    if (!loadHtmlFile(m_protocolsBrowser, ":/help/protocols.html")) {
        m_protocolsBrowser->setHtml(tr("<h1>加载失败</h1><p>无法加载协议说明文档。</p>"));
    }

    // 加载绘图功能
    if (!loadHtmlFile(m_plottingBrowser, ":/help/plotting.html")) {
        m_plottingBrowser->setHtml(tr("<h1>加载失败</h1><p>无法加载绘图功能文档。</p>"));
    }

    // 加载使用示例
    if (!loadHtmlFile(m_examplesBrowser, ":/help/examples.html")) {
        m_examplesBrowser->setHtml(tr("<h1>加载失败</h1><p>无法加载使用示例文档。</p>"));
    }

    // 加载高级功能
    if (!loadHtmlFile(m_advancedBrowser, ":/help/advanced.html")) {
        m_advancedBrowser->setHtml(tr("<h1>加载失败</h1><p>无法加载高级功能文档。</p>"));
    }
}

bool HelpDialog::loadHtmlFile(QTextBrowser* browser, const QString& resourcePath)
{
    if (!browser) {
        LOG_ERROR("Browser is null");
        return false;
    }

    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR(QString("Failed to open help file: %1").arg(resourcePath));
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    if (content.isEmpty()) {
        LOG_WARN(QString("Help file is empty: %1").arg(resourcePath));
        return false;
    }

    // 应用主题到HTML内容
    content = applyThemeToHtml(content);

    browser->setHtml(content);
    LOG_INFO(QString("Loaded help file: %1").arg(resourcePath));
    return true;
}

QString HelpDialog::applyThemeToHtml(const QString& content)
{
    QString result = content;
    QString themeAttr = m_isDarkTheme ? "dark" : "light";

    // 在<html>标签中添加data-theme属性
    if (result.contains("<html>")) {
        result.replace("<html>", QString("<html data-theme=\"%1\">").arg(themeAttr));
    } else if (result.contains("<html ")) {
        // 如果已有属性，在其中添加data-theme
        int pos = result.indexOf("<html ");
        int endPos = result.indexOf(">", pos);
        if (endPos > pos) {
            result.insert(endPos, QString(" data-theme=\"%1\"").arg(themeAttr));
        }
    }

    return result;
}

void HelpDialog::reloadHelpContent()
{
    // 保存当前标签页索引
    int currentIndex = m_tabWidget->currentIndex();

    // 重新加载所有帮助内容
    loadHelpContent();

    // 恢复标签页索引
    m_tabWidget->setCurrentIndex(currentIndex);
}

void HelpDialog::updateTheme(bool isDarkTheme)
{
    if (m_isDarkTheme == isDarkTheme) {
        return;  // 主题没有变化
    }

    m_isDarkTheme = isDarkTheme;
    reloadHelpContent();

    LOG_INFO(QString("Help dialog theme updated to: %1").arg(isDarkTheme ? "dark" : "light"));
}

void HelpDialog::showTopic(const QString& topic)
{
    if (topic == "quickstart") {
        m_tabWidget->setCurrentIndex(0);
    } else if (topic == "modes") {
        m_tabWidget->setCurrentIndex(1);
    } else if (topic == "protocols") {
        m_tabWidget->setCurrentIndex(2);
    } else if (topic == "plotting") {
        m_tabWidget->setCurrentIndex(3);
    } else if (topic == "advanced") {
        m_tabWidget->setCurrentIndex(4);
    } else if (topic == "examples") {
        m_tabWidget->setCurrentIndex(5);
    } else {
        LOG_WARN(QString("Unknown help topic: %1").arg(topic));
    }

    show();
    raise();
    activateWindow();
}

void HelpDialog::onClose()
{
    accept();
}

void HelpDialog::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QDialog::changeEvent(event);
}

void HelpDialog::retranslateUi()
{
    setWindowTitle(tr("帮助文档"));
    m_closeButton->setText(tr("关闭"));

    // 更新标签页标题
    m_tabWidget->setTabText(0, tr("快速入门"));
    m_tabWidget->setTabText(1, tr("模式说明"));
    m_tabWidget->setTabText(2, tr("协议说明"));
    m_tabWidget->setTabText(3, tr("绘图功能"));
    m_tabWidget->setTabText(4, tr("高级功能"));
    m_tabWidget->setTabText(5, tr("使用示例"));
}

} // namespace ComAssistant
