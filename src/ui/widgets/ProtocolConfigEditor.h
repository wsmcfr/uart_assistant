/**
 * @file ProtocolConfigEditor.h
 * @brief 协议配置 Schema 编辑器
 */

#ifndef COMASSISTANT_PROTOCOLCONFIGEDITOR_H
#define COMASSISTANT_PROTOCOLCONFIGEDITOR_H

#include "core/protocol/ProtocolConfigSchema.h"

#include <QWidget>

class QLabel;
class QFormLayout;

namespace ComAssistant {

/**
 * @brief 根据 ProtocolConfigSchema 自动生成表单控件的编辑器。
 *
 * 该控件只负责 Schema 到 Qt 控件的映射、配置加载、配置读取和错误提示；
 * 不负责创建协议实例，也不直接修改 MainWindow 状态。
 */
class ProtocolConfigEditor : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 创建协议配置编辑器。
     * @param parent 父级 QWidget，负责 Qt 对象树生命周期管理。
     */
    explicit ProtocolConfigEditor(QWidget* parent = nullptr);

    /**
     * @brief 设置配置 Schema 并重建表单。
     * @param schema 协议配置 Schema，字段顺序决定表单行顺序。
     */
    void setSchema(const ProtocolConfigSchema& schema);

private:
    /**
     * @brief 清空当前表单中的旧控件。
     *
     * setSchema() 每次都会重建表单，因此需要释放旧行，避免旧协议字段残留。
     */
    void clearForm();

    /**
     * @brief 根据单个 Schema 字段创建对应 Qt 编辑控件。
     * @param field 字段定义，包含类型、范围、枚举值和说明文本。
     * @return 已设置 objectName 与 tooltip 的字段编辑控件。
     */
    QWidget* createFieldWidget(const ProtocolConfigField& field);

    ProtocolConfigSchema m_schema;       ///< 当前正在展示的配置 Schema。
    QFormLayout* m_formLayout = nullptr; ///< Schema 字段表单布局。
    QLabel* m_emptyLabel = nullptr;      ///< 空 Schema 时显示的提示文本。
};

} // namespace ComAssistant

#endif // COMASSISTANT_PROTOCOLCONFIGEDITOR_H
