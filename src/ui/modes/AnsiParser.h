/**
 * @file AnsiParser.h
 * @brief ANSI/VT100控制序列解析器
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#ifndef ANSIPARSER_H
#define ANSIPARSER_H

#include <QObject>
#include <QByteArray>
#include <QVector>
#include <functional>
#include "TerminalBuffer.h"

namespace ComAssistant {

/**
 * @brief ANSI/VT100控制序列解析器
 *
 * 支持的序列：
 * - CSI (Control Sequence Introducer): ESC [ ...
 * - SGR (Select Graphic Rendition): 颜色和样式
 * - 光标控制: CUP, CUU, CUD, CUF, CUB, etc.
 * - 屏幕控制: ED, EL, etc.
 * - 滚动: SU, SD, DECSTBM
 */
class AnsiParser : public QObject {
    Q_OBJECT

public:
    explicit AnsiParser(TerminalBuffer* buffer, QObject* parent = nullptr);

    /**
     * @brief 处理输入数据
     * @param data 原始字节数据
     */
    void process(const QByteArray& data);

    /**
     * @brief 重置解析器状态
     */
    void reset();

signals:
    /**
     * @brief 响铃信号
     */
    void bell();

    /**
     * @brief 窗口标题变化
     */
    void titleChanged(const QString& title);

private:
    enum class State {
        Normal,         // 普通字符
        Escape,         // 收到 ESC
        CSI,            // 收到 ESC [
        OSC,            // 收到 ESC ]
        DCS,            // 收到 ESC P
        APC,            // 收到 ESC _
        PM,             // 收到 ESC ^
        SOS,            // 收到 ESC X
        CharsetG0,      // 收到 ESC (
        CharsetG1,      // 收到 ESC )
    };

    void processNormal(unsigned char byte);
    void processEscape(unsigned char byte);
    void processCSI(unsigned char byte);
    void processOSC(unsigned char byte);

    void executeCSI();
    void executeSGR(const QVector<int>& params);
    void executeOSC();

    // 辅助函数
    int getParam(int index, int defaultValue = 0) const;
    QVector<int> parseParams() const;

    // 颜色设置
    void setForegroundColor(int code);
    void setBackgroundColor(int code);
    void setForeground256(int index);
    void setBackground256(int index);
    void setForegroundRGB(int r, int g, int b);
    void setBackgroundRGB(int r, int g, int b);

    // 256色调色板
    static QColor color256(int index);

    TerminalBuffer* m_buffer;
    State m_state = State::Normal;
    QByteArray m_escBuffer;
    QString m_oscString;
    char m_csiPrefix = 0;    // ? > = 等前缀
    char m_csiSuffix = 0;    // 命令字符
};

} // namespace ComAssistant

#endif // ANSIPARSER_H
