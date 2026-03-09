/**
 * @file DataWindowManager.cpp
 * @brief 数据分窗管理器实现
 * @author ComAssistant Team
 * @date 2026-01-21
 */

#include "DataWindowManager.h"
#include "DataWindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace ComAssistant {

DataWindowManager::DataWindowManager(QWidget* parent)
    : QObject(parent)
    , m_parentWidget(parent)
{
}

DataWindowManager::~DataWindowManager()
{
    // 关闭并删除所有窗口
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            it.value()->close();
            delete it.value();
        }
    }
    m_windows.clear();
}

bool DataWindowManager::addWindow(const QString& name)
{
    if (name.isEmpty() || m_windows.contains(name)) {
        return false;
    }

    DataWindow* window = new DataWindow(name, m_parentWidget);
    window->setWindowFlags(Qt::Window);  // 独立窗口
    m_windows.insert(name, window);

    // 连接窗口关闭信号
    connect(window, &DataWindow::windowClosed, this, [this](const QString& windowName) {
        // 窗口关闭时从管理器中移除
        if (m_windows.contains(windowName)) {
            m_windows.remove(windowName);
            emit windowRemoved(windowName);
        }
    });

    emit windowAdded(name);
    return true;
}

void DataWindowManager::removeWindow(const QString& name)
{
    if (m_windows.contains(name)) {
        DataWindow* window = m_windows.take(name);
        if (window) {
            window->close();
            window->deleteLater();
        }
        emit windowRemoved(name);
    }
}

DataWindow* DataWindowManager::window(const QString& name) const
{
    return m_windows.value(name, nullptr);
}

QStringList DataWindowManager::windowNames() const
{
    return m_windows.keys();
}

void DataWindowManager::addRule(const WindowRule& rule)
{
    m_rules.append(rule);
}

void DataWindowManager::removeRule(int index)
{
    if (index >= 0 && index < m_rules.size()) {
        m_rules.removeAt(index);
    }
}

void DataWindowManager::clearRules()
{
    m_rules.clear();
}

void DataWindowManager::setRules(const QVector<WindowRule>& rules)
{
    m_rules = rules;
    // 重置编译状态
    for (auto& rule : m_rules) {
        rule.regexCompiled = false;
    }
}

void DataWindowManager::compileRegex(const WindowRule& rule) const
{
    if (rule.isRegex && !rule.regexCompiled) {
        QRegularExpression::PatternOptions options = QRegularExpression::NoPatternOption;
        if (!rule.caseSensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        rule.regex = QRegularExpression(rule.pattern, options);
        rule.regexCompiled = true;
    }
}

bool DataWindowManager::matchRule(const WindowRule& rule, const QString& data)
{
    if (!rule.enabled) {
        return false;
    }

    if (rule.isRegex) {
        compileRegex(rule);
        if (rule.regex.isValid()) {
            return rule.regex.match(data).hasMatch();
        }
        return false;
    } else {
        // 关键字匹配
        Qt::CaseSensitivity cs = rule.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        return data.contains(rule.pattern, cs);
    }
}

bool DataWindowManager::routeData(const QString& data)
{
    bool matched = false;

    for (const WindowRule& rule : m_rules) {
        if (matchRule(rule, data)) {
            // 确保目标窗口存在
            if (!m_windows.contains(rule.windowName)) {
                addWindow(rule.windowName);
            }

            DataWindow* targetWindow = m_windows.value(rule.windowName);
            if (targetWindow) {
                targetWindow->appendData(data);
                targetWindow->show();
                targetWindow->raise();
                emit dataRouted(rule.windowName, data);
                matched = true;
            }
        }
    }

    return matched;
}

void DataWindowManager::showAllWindows()
{
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            it.value()->show();
            it.value()->raise();
        }
    }
}

void DataWindowManager::hideAllWindows()
{
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            it.value()->hide();
        }
    }
}

void DataWindowManager::clearAllData()
{
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        if (it.value()) {
            it.value()->clear();
        }
    }
}

QByteArray DataWindowManager::saveConfig() const
{
    QJsonObject root;

    // 保存窗口列表
    QJsonArray windowsArray;
    for (const QString& name : m_windows.keys()) {
        windowsArray.append(name);
    }
    root["windows"] = windowsArray;

    // 保存规则列表
    QJsonArray rulesArray;
    for (const WindowRule& rule : m_rules) {
        QJsonObject ruleObj;
        ruleObj["windowName"] = rule.windowName;
        ruleObj["pattern"] = rule.pattern;
        ruleObj["isRegex"] = rule.isRegex;
        ruleObj["caseSensitive"] = rule.caseSensitive;
        ruleObj["enabled"] = rule.enabled;
        rulesArray.append(ruleObj);
    }
    root["rules"] = rulesArray;

    QJsonDocument doc(root);
    return doc.toJson();
}

bool DataWindowManager::loadConfig(const QByteArray& data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    QJsonObject root = doc.object();

    // 加载窗口列表
    QJsonArray windowsArray = root["windows"].toArray();
    for (const QJsonValue& val : windowsArray) {
        QString name = val.toString();
        if (!name.isEmpty() && !m_windows.contains(name)) {
            addWindow(name);
        }
    }

    // 加载规则列表
    m_rules.clear();
    QJsonArray rulesArray = root["rules"].toArray();
    for (const QJsonValue& val : rulesArray) {
        QJsonObject ruleObj = val.toObject();
        WindowRule rule;
        rule.windowName = ruleObj["windowName"].toString();
        rule.pattern = ruleObj["pattern"].toString();
        rule.isRegex = ruleObj["isRegex"].toBool(false);
        rule.caseSensitive = ruleObj["caseSensitive"].toBool(false);
        rule.enabled = ruleObj["enabled"].toBool(true);
        rule.regexCompiled = false;
        m_rules.append(rule);
    }

    return true;
}

} // namespace ComAssistant
