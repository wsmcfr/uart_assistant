/**
 * @file AnsiParser.cpp
 * @brief ANSI/VT100控制序列解析器实现
 * @author ComAssistant Team
 * @date 2026-01-20
 */

#include "AnsiParser.h"
#include <QDebug>

namespace ComAssistant {

AnsiParser::AnsiParser(TerminalBuffer* buffer, QObject* parent)
    : QObject(parent)
    , m_buffer(buffer)
{
}

void AnsiParser::process(const QByteArray& data)
{
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);

        switch (m_state) {
        case State::Normal:
            processNormal(byte);
            break;
        case State::Escape:
            processEscape(byte);
            break;
        case State::CSI:
            processCSI(byte);
            break;
        case State::OSC:
            processOSC(byte);
            break;
        case State::CharsetG0:
        case State::CharsetG1:
            // 忽略字符集选择
            m_state = State::Normal;
            break;
        default:
            m_state = State::Normal;
            break;
        }
    }
}

void AnsiParser::reset()
{
    m_state = State::Normal;
    m_escBuffer.clear();
    m_oscString.clear();
    m_csiPrefix = 0;
    m_csiSuffix = 0;
}

void AnsiParser::processNormal(unsigned char byte)
{
    switch (byte) {
    case 0x00:  // NUL - 忽略
        break;
    case 0x07:  // BEL - 响铃
        emit bell();
        break;
    case 0x08:  // BS - 退格
        m_buffer->backspace();
        break;
    case 0x09:  // HT - 水平制表符
        m_buffer->tab();
        break;
    case 0x0A:  // LF - 换行
    case 0x0B:  // VT - 垂直制表符（同LF）
    case 0x0C:  // FF - 换页（同LF）
        /*
         * 很多串口设备只输出 LF，而不会额外输出 CR。作为调试助手的
         * 接收终端，应当把这种常见日志流显示为“下一行从行首开始”，
         * 避免连续日志在屏幕上呈阶梯状偏移。
         */
        m_buffer->carriageReturn();
        m_buffer->newLine();
        break;
    case 0x0D:  // CR - 回车
        m_buffer->carriageReturn();
        break;
    case 0x1B:  // ESC - 转义
        m_state = State::Escape;
        m_escBuffer.clear();
        m_csiPrefix = 0;
        break;
    case 0x7F:  // DEL - 删除（忽略）
        break;
    default:
        if (byte >= 0x20) {  // 可打印字符
            m_buffer->putChar(QChar(byte));
        }
        break;
    }
}

void AnsiParser::processEscape(unsigned char byte)
{
    switch (byte) {
    case '[':  // CSI
        m_state = State::CSI;
        m_escBuffer.clear();
        break;
    case ']':  // OSC
        m_state = State::OSC;
        m_oscString.clear();
        break;
    case '(':  // G0 字符集
        m_state = State::CharsetG0;
        break;
    case ')':  // G1 字符集
        m_state = State::CharsetG1;
        break;
    case '7':  // DECSC - 保存光标
        m_buffer->saveCursor();
        m_state = State::Normal;
        break;
    case '8':  // DECRC - 恢复光标
        m_buffer->restoreCursor();
        m_state = State::Normal;
        break;
    case 'c':  // RIS - 重置
        m_buffer->clearScreen();
        m_buffer->setCursorPosition(0, 0);
        m_buffer->resetAttribute();
        m_state = State::Normal;
        break;
    case 'D':  // IND - 索引（向下移动一行）
        m_buffer->newLine();
        m_state = State::Normal;
        break;
    case 'E':  // NEL - 下一行
        m_buffer->carriageReturn();
        m_buffer->newLine();
        m_state = State::Normal;
        break;
    case 'H':  // HTS - 设置制表位
        m_state = State::Normal;
        break;
    case 'M':  // RI - 反向索引（向上移动一行）
        if (m_buffer->cursorRow() == m_buffer->scrollTop()) {
            m_buffer->scrollDown(1);
        } else {
            m_buffer->moveCursor(-1, 0);
        }
        m_state = State::Normal;
        break;
    case 'P':  // DCS
        m_state = State::DCS;
        break;
    case '\\':  // ST - 字符串终止符
        m_state = State::Normal;
        break;
    default:
        // 未知序列，返回普通模式
        m_state = State::Normal;
        break;
    }
}

void AnsiParser::processCSI(unsigned char byte)
{
    // 检查前缀字符
    if (m_escBuffer.isEmpty() && (byte == '?' || byte == '>' || byte == '=' || byte == '!')) {
        m_csiPrefix = byte;
        return;
    }

    // 参数字符: 0-9 ; :
    if ((byte >= '0' && byte <= '9') || byte == ';' || byte == ':') {
        m_escBuffer.append(byte);
        return;
    }

    // 命令字符
    m_csiSuffix = byte;
    executeCSI();
    m_state = State::Normal;
}

void AnsiParser::processOSC(unsigned char byte)
{
    // OSC 以 BEL (0x07) 或 ST (ESC \) 结束
    if (byte == 0x07) {
        executeOSC();
        m_state = State::Normal;
    } else if (byte == 0x1B) {
        // 可能是 ST (ESC \)
        // 简化处理：直接执行
        executeOSC();
        m_state = State::Escape;
    } else {
        m_oscString.append(QChar(byte));
    }
}

void AnsiParser::executeCSI()
{
    QVector<int> params = parseParams();

    switch (m_csiSuffix) {
    // 光标移动
    case 'A':  // CUU - 光标上移
        m_buffer->moveCursor(-qMax(1, getParam(0, 1)), 0);
        break;
    case 'B':  // CUD - 光标下移
    case 'e':  // VPR - 同CUD
        m_buffer->moveCursor(qMax(1, getParam(0, 1)), 0);
        break;
    case 'C':  // CUF - 光标右移
    case 'a':  // HPR - 同CUF
        m_buffer->moveCursor(0, qMax(1, getParam(0, 1)));
        break;
    case 'D':  // CUB - 光标左移
        m_buffer->moveCursor(0, -qMax(1, getParam(0, 1)));
        break;
    case 'E':  // CNL - 光标下移并回车
        m_buffer->moveCursor(qMax(1, getParam(0, 1)), 0);
        m_buffer->carriageReturn();
        break;
    case 'F':  // CPL - 光标上移并回车
        m_buffer->moveCursor(-qMax(1, getParam(0, 1)), 0);
        m_buffer->carriageReturn();
        break;
    case 'G':  // CHA - 光标水平绝对位置
    case '`':  // HPA - 同CHA
        m_buffer->setCursorPosition(m_buffer->cursorRow(), qMax(1, getParam(0, 1)) - 1);
        break;
    case 'H':  // CUP - 光标定位
    case 'f':  // HVP - 同CUP
        m_buffer->setCursorPosition(qMax(1, getParam(0, 1)) - 1, qMax(1, getParam(1, 1)) - 1);
        break;
    case 'd':  // VPA - 光标垂直绝对位置
        m_buffer->setCursorPosition(qMax(1, getParam(0, 1)) - 1, m_buffer->cursorCol());
        break;

    // 屏幕擦除
    case 'J':  // ED - 擦除显示
        switch (getParam(0, 0)) {
        case 0:  // 从光标到屏幕末尾
            m_buffer->clearToEndOfScreen();
            break;
        case 1:  // 从屏幕开始到光标
            m_buffer->clearToStartOfScreen();
            break;
        case 2:  // 整个屏幕
        case 3:  // 整个屏幕（包括滚动缓冲）
            m_buffer->clearScreen();
            break;
        }
        break;

    // 行擦除
    case 'K':  // EL - 擦除行
        switch (getParam(0, 0)) {
        case 0:  // 从光标到行末
            m_buffer->clearToEndOfLine();
            break;
        case 1:  // 从行首到光标
            m_buffer->clearToStartOfLine();
            break;
        case 2:  // 整行
            m_buffer->clearLine();
            break;
        }
        break;

    // 插入/删除
    case 'L':  // IL - 插入行
        m_buffer->insertLines(qMax(1, getParam(0, 1)));
        break;
    case 'M':  // DL - 删除行
        m_buffer->deleteLines(qMax(1, getParam(0, 1)));
        break;
    case '@':  // ICH - 插入字符
        m_buffer->insertChars(qMax(1, getParam(0, 1)));
        break;
    case 'P':  // DCH - 删除字符
        m_buffer->deleteChars(qMax(1, getParam(0, 1)));
        break;
    case 'X':  // ECH - 擦除字符
        for (int i = 0; i < qMax(1, getParam(0, 1)); ++i) {
            if (m_buffer->cursorCol() + i < m_buffer->cols()) {
                m_buffer->cellAt(m_buffer->cursorRow(), m_buffer->cursorCol() + i).character = ' ';
            }
        }
        break;

    // 滚动
    case 'S':  // SU - 向上滚动
        m_buffer->scrollUp(qMax(1, getParam(0, 1)));
        break;
    case 'T':  // SD - 向下滚动
        m_buffer->scrollDown(qMax(1, getParam(0, 1)));
        break;

    // 图形属性
    case 'm':  // SGR - 设置图形属性
        executeSGR(params);
        break;

    // 滚动区域
    case 'r':  // DECSTBM - 设置滚动区域
        if (m_csiPrefix == 0) {
            int top = qMax(1, getParam(0, 1)) - 1;
            int bottom = getParam(1, m_buffer->rows()) - 1;
            m_buffer->setScrollRegion(top, bottom);
            m_buffer->setCursorPosition(0, 0);
        }
        break;

    // 光标显示
    case 'h':  // SM - 设置模式
        if (m_csiPrefix == '?' && getParam(0, 0) == 25) {
            m_buffer->setCursorVisible(true);
        }
        break;
    case 'l':  // RM - 重置模式
        if (m_csiPrefix == '?' && getParam(0, 0) == 25) {
            m_buffer->setCursorVisible(false);
        }
        break;

    // 保存/恢复光标
    case 's':  // SCP - 保存光标位置
        m_buffer->saveCursor();
        break;
    case 'u':  // RCP - 恢复光标位置
        m_buffer->restoreCursor();
        break;

    // 设备状态报告等（忽略）
    case 'n':  // DSR
    case 'c':  // DA
        break;

    default:
        // 未知序列
        break;
    }
}

void AnsiParser::executeSGR(const QVector<int>& params)
{
    if (params.isEmpty()) {
        m_buffer->resetAttribute();
        return;
    }

    CharAttribute attr = m_buffer->currentAttribute();

    for (int i = 0; i < params.size(); ++i) {
        int code = params[i];

        switch (code) {
        case 0:  // 重置
            attr = CharAttribute();
            break;
        case 1:  // 粗体
            attr.bold = true;
            break;
        case 2:  // 暗淡
            attr.dim = true;
            break;
        case 3:  // 斜体
            attr.italic = true;
            break;
        case 4:  // 下划线
            attr.underline = true;
            break;
        case 5:  // 慢闪烁
        case 6:  // 快闪烁
            attr.blink = true;
            break;
        case 7:  // 反显
            attr.reverse = true;
            break;
        case 8:  // 隐藏
            attr.hidden = true;
            break;
        case 9:  // 删除线
            attr.strikethrough = true;
            break;
        case 22:  // 正常亮度
            attr.bold = false;
            attr.dim = false;
            break;
        case 23:  // 关闭斜体
            attr.italic = false;
            break;
        case 24:  // 关闭下划线
            attr.underline = false;
            break;
        case 25:  // 关闭闪烁
            attr.blink = false;
            break;
        case 27:  // 关闭反显
            attr.reverse = false;
            break;
        case 28:  // 关闭隐藏
            attr.hidden = false;
            break;
        case 29:  // 关闭删除线
            attr.strikethrough = false;
            break;

        // 标准前景色
        case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37:
            setForegroundColor(code - 30);
            attr.foreground = m_buffer->currentAttribute().foreground;
            break;

        // 256色/真彩前景
        case 38:
            if (i + 2 < params.size() && params[i + 1] == 5) {
                // 256色: 38;5;n
                setForeground256(params[i + 2]);
                attr.foreground = m_buffer->currentAttribute().foreground;
                i += 2;
            } else if (i + 4 < params.size() && params[i + 1] == 2) {
                // 真彩色: 38;2;r;g;b
                setForegroundRGB(params[i + 2], params[i + 3], params[i + 4]);
                attr.foreground = m_buffer->currentAttribute().foreground;
                i += 4;
            }
            break;

        case 39:  // 默认前景色
            attr.foreground = QColor(0xAA, 0xAA, 0xAA);
            break;

        // 标准背景色
        case 40: case 41: case 42: case 43:
        case 44: case 45: case 46: case 47:
            setBackgroundColor(code - 40);
            attr.background = m_buffer->currentAttribute().background;
            break;

        // 256色/真彩背景
        case 48:
            if (i + 2 < params.size() && params[i + 1] == 5) {
                setBackground256(params[i + 2]);
                attr.background = m_buffer->currentAttribute().background;
                i += 2;
            } else if (i + 4 < params.size() && params[i + 1] == 2) {
                setBackgroundRGB(params[i + 2], params[i + 3], params[i + 4]);
                attr.background = m_buffer->currentAttribute().background;
                i += 4;
            }
            break;

        case 49:  // 默认背景色
            attr.background = QColor(0x00, 0x00, 0x00);
            break;

        // 亮色前景
        case 90: case 91: case 92: case 93:
        case 94: case 95: case 96: case 97:
            setForegroundColor(code - 90 + 8);
            attr.foreground = m_buffer->currentAttribute().foreground;
            break;

        // 亮色背景
        case 100: case 101: case 102: case 103:
        case 104: case 105: case 106: case 107:
            setBackgroundColor(code - 100 + 8);
            attr.background = m_buffer->currentAttribute().background;
            break;
        }
    }

    m_buffer->setCurrentAttribute(attr);
}

void AnsiParser::executeOSC()
{
    // OSC 0; title ST - 设置窗口标题
    // OSC 2; title ST - 设置窗口标题
    if (m_oscString.startsWith("0;") || m_oscString.startsWith("2;")) {
        QString title = m_oscString.mid(2);
        emit titleChanged(title);
    }
}

int AnsiParser::getParam(int index, int defaultValue) const
{
    QVector<int> params = parseParams();
    if (index < params.size() && params[index] != 0) {
        return params[index];
    }
    return defaultValue;
}

QVector<int> AnsiParser::parseParams() const
{
    QVector<int> result;
    QString str = QString::fromLatin1(m_escBuffer);
    QStringList parts = str.split(';');

    for (const QString& part : parts) {
        bool ok;
        int value = part.toInt(&ok);
        result.append(ok ? value : 0);
    }

    if (result.isEmpty()) {
        result.append(0);
    }

    return result;
}

// 标准8色 + 8亮色
static const QColor s_standardColors[16] = {
    QColor(0x00, 0x00, 0x00),  // 0: 黑
    QColor(0xCD, 0x00, 0x00),  // 1: 红
    QColor(0x00, 0xCD, 0x00),  // 2: 绿
    QColor(0xCD, 0xCD, 0x00),  // 3: 黄
    QColor(0x00, 0x00, 0xEE),  // 4: 蓝
    QColor(0xCD, 0x00, 0xCD),  // 5: 品红
    QColor(0x00, 0xCD, 0xCD),  // 6: 青
    QColor(0xE5, 0xE5, 0xE5),  // 7: 白
    QColor(0x7F, 0x7F, 0x7F),  // 8: 亮黑（灰）
    QColor(0xFF, 0x00, 0x00),  // 9: 亮红
    QColor(0x00, 0xFF, 0x00),  // 10: 亮绿
    QColor(0xFF, 0xFF, 0x00),  // 11: 亮黄
    QColor(0x5C, 0x5C, 0xFF),  // 12: 亮蓝
    QColor(0xFF, 0x00, 0xFF),  // 13: 亮品红
    QColor(0x00, 0xFF, 0xFF),  // 14: 亮青
    QColor(0xFF, 0xFF, 0xFF),  // 15: 亮白
};

void AnsiParser::setForegroundColor(int code)
{
    if (code >= 0 && code < 16) {
        CharAttribute attr = m_buffer->currentAttribute();
        attr.foreground = s_standardColors[code];
        m_buffer->setCurrentAttribute(attr);
    }
}

void AnsiParser::setBackgroundColor(int code)
{
    if (code >= 0 && code < 16) {
        CharAttribute attr = m_buffer->currentAttribute();
        attr.background = s_standardColors[code];
        m_buffer->setCurrentAttribute(attr);
    }
}

QColor AnsiParser::color256(int index)
{
    if (index < 16) {
        return s_standardColors[index];
    } else if (index < 232) {
        // 216色立方体: 6x6x6
        index -= 16;
        int r = (index / 36) * 51;
        int g = ((index / 6) % 6) * 51;
        int b = (index % 6) * 51;
        return QColor(r, g, b);
    } else {
        // 24级灰度
        int gray = (index - 232) * 10 + 8;
        return QColor(gray, gray, gray);
    }
}

void AnsiParser::setForeground256(int index)
{
    CharAttribute attr = m_buffer->currentAttribute();
    attr.foreground = color256(index);
    m_buffer->setCurrentAttribute(attr);
}

void AnsiParser::setBackground256(int index)
{
    CharAttribute attr = m_buffer->currentAttribute();
    attr.background = color256(index);
    m_buffer->setCurrentAttribute(attr);
}

void AnsiParser::setForegroundRGB(int r, int g, int b)
{
    CharAttribute attr = m_buffer->currentAttribute();
    attr.foreground = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
    m_buffer->setCurrentAttribute(attr);
}

void AnsiParser::setBackgroundRGB(int r, int g, int b)
{
    CharAttribute attr = m_buffer->currentAttribute();
    attr.background = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
    m_buffer->setCurrentAttribute(attr);
}

} // namespace ComAssistant
