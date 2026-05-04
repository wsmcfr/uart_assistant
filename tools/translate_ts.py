#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt .ts 翻译文件自动翻译脚本
使用内置字典 + 在线 API 混合翻译

用法: python translate_ts.py <ts_file> [--dry-run]
"""

import sys
import os
import re
import time
import argparse
import xml.etree.ElementTree as ET
from urllib.request import urlopen, Request
from urllib.parse import quote
from urllib.error import URLError, HTTPError
import json

# 内置翻译字典：常见 UI 字符串
BUILTIN_DICT = {
    # 通用操作
    "确定": "OK",
    "取消": "Cancel",
    "是": "Yes",
    "否": "No",
    "关闭": "Close",
    "打开": "Open",
    "保存": "Save",
    "删除": "Delete",
    "编辑": "Edit",
    "新建": "New",
    "添加": "Add",
    "移除": "Remove",
    "清空": "Clear",
    "复制": "Copy",
    "粘贴": "Paste",
    "剪切": "Cut",
    "撤销": "Undo",
    "重做": "Redo",
    "刷新": "Refresh",
    "搜索": "Search",
    "替换": "Replace",
    "过滤": "Filter",
    "导入": "Import",
    "导出": "Export",
    "上移": "Move Up",
    "下移": "Move Down",
    "全选": "Select All",
    "帮助": "Help",
    "关于": "About",
    "设置": "Settings",
    "退出": "Exit",
    "最小化": "Minimize",
    "最大化": "Maximize",
    "还原": "Restore",
    "暂停": "Pause",
    "继续": "Continue",
    "停止": "Stop",
    "开始": "Start",
    "重置": "Reset",
    "应用": "Apply",
    "确认": "Confirm",
    "警告": "Warning",
    "错误": "Error",
    "提示": "Tip",
    "成功": "Success",
    "失败": "Failed",
    "正在加载...": "Loading...",
    "加载中...": "Loading...",
    "请稍候...": "Please wait...",
    "无标题": "Untitled",
    "未命名": "Untitled",
    "未知": "Unknown",
    "默认": "Default",
    "自定义": "Custom",
    "启用": "Enable",
    "禁用": "Disable",
    "显示": "Show",
    "隐藏": "Hide",
    "自动": "Auto",
    "手动": "Manual",
    "已连接": "Connected",
    "未连接": "Disconnected",
    "连接": "Connect",
    "断开": "Disconnect",
    "发送": "Send",
    "接收": "Receive",
    "刷新": "Refresh",
    "重试": "Retry",
    "忽略": "Ignore",
    "跳过": "Skip",
    "完成": "Done",
    "完成": "Finish",
    "下一步": "Next",
    "上一步": "Previous",
    "返回": "Back",
    "前进": "Forward",

    # 串口相关
    "串口": "Serial Port",
    "波特率": "Baud Rate",
    "数据位": "Data Bits",
    "停止位": "Stop Bits",
    "校验位": "Parity",
    "流控制": "Flow Control",
    "无": "None",
    "奇校验": "Odd",
    "偶校验": "Even",
    "标记": "Mark",
    "空格": "Space",
    "硬件": "Hardware",
    "软件": "Software",
    "打开串口": "Open Port",
    "关闭串口": "Close Port",
    "串口设置": "Port Settings",
    "串口列表": "Port List",
    "串口错误": "Port Error",
    "串口号": "Port Number",
    "可用串口": "Available Ports",
    "无可用串口": "No Available Ports",
    "串口已打开": "Port Opened",
    "串口已关闭": "Port Closed",
    "串口打开失败": "Failed to Open Port",
    "串口关闭失败": "Failed to Close Port",

    # 数据显示
    "发送": "Send",
    "接收": "Receive",
    "HEX": "HEX",
    "ASCII": "ASCII",
    "UTF-8": "UTF-8",
    "GBK": "GBK",
    "自动滚动": "Auto Scroll",
    "暂停显示": "Pause Display",
    "清空接收": "Clear Receive",
    "清空发送": "Clear Send",
    "接收设置": "Receive Settings",
    "发送设置": "Send Settings",
    "显示设置": "Display Settings",
    "时间戳": "Timestamp",
    "方向": "Direction",
    "全部": "All",
    "接收(RX)": "Receive (RX)",
    "发送(TX)": "Send (TX)",
    "共 %1 条记录": "Total %1 records",
    "序号": "Index",
    "时间": "Time",
    "协议": "Protocol",
    "描述": "Description",
    "数值": "Values",
    "原始数据": "Raw Data",
    "解析数值": "Parsed Values",
    "导出CSV": "Export CSV",
    "导出成功": "Export Success",
    "导出失败": "Export Failed",
    "数据已导出到: %1": "Data exported to: %1",
    "无法写入文件: %1": "Cannot write file: %1",
    "输入关键字过滤...": "Enter keyword to filter...",

    # 网络相关
    "TCP客户端": "TCP Client",
    "TCP服务器": "TCP Server",
    "UDP": "UDP",
    "IP地址": "IP Address",
    "端口": "Port",
    "连接": "Connect",
    "断开连接": "Disconnect",
    "监听": "Listen",
    "停止监听": "Stop Listen",
    "发送数据": "Send Data",
    "接收数据": "Receive Data",
    "网络设置": "Network Settings",
    "本地地址": "Local Address",
    "远程地址": "Remote Address",

    # 绘图相关
    "绘图": "Plot",
    "曲线": "Curve",
    "波形": "Waveform",
    "频谱": "Spectrum",
    "放大": "Zoom In",
    "缩小": "Zoom Out",
    "适应窗口": "Fit Window",
    "截图": "Screenshot",
    "保存图片": "Save Image",
    "图片已保存到: %1": "Image saved to: %1",
    "无法保存图片": "Cannot save image",
    "当前没有曲线": "No curves available",
    "当前没有曲线数据": "No curve data available",
    "所选曲线没有数据": "Selected curve has no data",
    "数据点数太少（至少需要8个点）": "Too few data points (at least 8 required)",
    "请选择两条不同的曲线": "Please select two different curves",
    "X轴": "X Axis",
    "Y轴": "Y Axis",
    "图例": "Legend",
    "网格": "Grid",
    "标题": "Title",
    "标签": "Label",
    "范围": "Range",
    "自动范围": "Auto Range",
    "固定范围": "Fixed Range",
    "采样率": "Sample Rate",
    "数据点": "Data Points",
    "曲线数量": "Curve Count",
    "显示图例": "Show Legend",
    "显示网格": "Show Grid",
    "抗锯齿": "Anti-aliasing",
    "渲染质量": "Render Quality",
    "高质量": "High Quality",
    "高性能": "High Performance",
    "启用 OpenGL 加速(&O)": "Enable OpenGL Acceleration (&O)",

    # Modbus 相关
    "功能码:": "Function Code:",
    "0x01 读线圈": "0x01 Read Coils",
    "0x02 读离散输入": "0x02 Read Discrete Inputs",
    "0x03 读保持寄存器": "0x03 Read Holding Registers",
    "0x04 读输入寄存器": "0x04 Read Input Registers",
    "0x05 写单线圈": "0x05 Write Single Coil",
    "0x06 写单寄存器": "0x06 Write Single Register",
    "0x0F 写多线圈": "0x0F Write Multiple Coils",
    "0x10 写多寄存器": "0x10 Write Multiple Registers",
    "从站地址:": "Slave Address:",
    "起始地址:": "Start Address:",
    "数量:": "Count:",
    "发送请求": "Send Request",
    "响应": "Response",
    "请求": "Request",
    "帧": "Frame",
    "CRC校验": "CRC Check",
    "有效": "Valid",
    "无效": "Invalid",

    # 脚本相关
    "脚本编辑器": "Script Editor",
    "脚本列表": "Script List",
    "代码编辑": "Code Editor",
    "输出": "Output",
    "运行": "Run",
    "停止": "Stop",
    "新建脚本": "New Script",
    "打开脚本": "Open Script",
    "保存脚本": "Save Script",
    "当前脚本已修改，是否保存？": "Current script has been modified. Save?",
    "脚本加载完成": "Script loaded",
    "开始执行脚本...": "Executing script...",
    "脚本执行完成": "Script execution completed",
    "脚本已停止": "Script stopped",
    "脚本已保存: %1": "Script saved: %1",
    "无法打开文件: %1": "Cannot open file: %1",
    "无法保存文件: %1": "Cannot save file: %1",
    "提示: Ctrl+Enter 运行脚本": "Tip: Ctrl+Enter to run script",
    "Lua脚本 (*.lua);;所有文件 (*)": "Lua Scripts (*.lua);;All Files (*)",
    "Lua脚本 (*.lua)": "Lua Scripts (*.lua)",

    # 宏相关
    "宏录制": "Macro Recording",
    "录制": "Record",
    "回放": "Playback",
    "停止录制": "Stop Recording",
    "宏名称": "Macro Name",
    "宏列表": "Macro List",
    "录制宏": "Record Macro",
    "回放宏": "Playback Macro",
    "保存宏": "Save Macro",
    "加载宏": "Load Macro",
    "删除宏": "Delete Macro",
    "导入宏": "Import Macro",
    "导出宏": "Export Macro",
    "确认删除": "Confirm Delete",
    "宏导入成功": "Macro imported successfully",
    "宏导出成功": "Macro exported successfully",
    "导入宏失败": "Failed to import macro",
    "导出宏失败": "Failed to export macro",
    "没有录制到任何事件": "No events recorded",

    # 设置相关
    "常规": "General",
    "外观": "Appearance",
    "主题": "Theme",
    "明亮": "Light",
    "暗黑": "Dark",
    "语言": "Language",
    "界面语言": "Interface Language",
    "字体": "Font",
    "字号": "Font Size",
    "颜色": "Color",
    "背景色": "Background Color",
    "前景色": "Foreground Color",
    "自动保存": "Auto Save",
    "自动保存间隔": "Auto Save Interval",
    "秒": "seconds",
    "分钟": "minutes",
    "小时": "hours",
    "毫秒": "milliseconds",

    # 文件传输相关
    "文件传输": "File Transfer",
    "选择文件": "Select File",
    "传输进度": "Transfer Progress",
    "传输完成": "Transfer Complete",
    "传输失败": "Transfer Failed",
    "请先选择文件": "Please select a file first",
    "创建传输对象失败": "Failed to create transfer object",
    "启动传输失败": "Failed to start transfer",

    # 多串口相关
    "多串口": "Multi Port",
    "添加串口": "Add Port",
    "删除串口": "Remove Port",
    "请选择有效的串口": "Please select a valid port",
    "创建串口失败，可能已被占用": "Failed to create port, may be in use",
    "打开串口失败": "Failed to open port",
    "至少保留一个分组": "At least one group must be kept",
    "无法打开文件": "Cannot open file",
    "无效的配置文件格式": "Invalid configuration file format",

    # 快捷发送相关
    "快捷发送": "Quick Send",
    "快捷按钮": "Quick Buttons",
    "添加按钮": "Add Button",
    "删除按钮": "Delete Button",
    "按钮名称": "Button Name",
    "发送内容": "Send Content",
    "确认删除": "Confirm Delete",
    "数据不能为空": "Data cannot be empty",

    # 数据窗口相关
    "数据窗口": "Data Window",
    "窗口规则": "Window Rules",
    "添加规则": "Add Rule",
    "删除规则": "Delete Rule",
    "规则名称": "Rule Name",
    "匹配模式": "Match Pattern",
    "显示方式": "Display Mode",

    # FFT 相关
    "FFT设置": "FFT Settings",
    "窗函数": "Window Function",
    "矩形窗": "Rectangular",
    "汉宁窗": "Hanning",
    "汉明窗": "Hamming",
    "布莱克曼窗": "Blackman",
    "频率范围": "Frequency Range",
    "幅度": "Amplitude",
    "相位": "Phase",
    "功率谱": "Power Spectrum",
    "实时FFT": "Real-time FFT",
    "频谱分析": "Spectrum Analysis",
    "请选择频谱图标签页": "Please select a spectrum tab",
    "频谱图已保存到: %1": "Spectrum saved to: %1",
    "无法保存频谱图": "Cannot save spectrum",
    "频谱数据已保存到: %1": "Spectrum data saved to: %1",

    # 统计相关
    "统计": "Statistics",
    "最大值": "Maximum",
    "最小值": "Minimum",
    "平均值": "Average",
    "标准差": "Standard Deviation",
    "方差": "Variance",
    "总和": "Sum",
    "计数": "Count",
    "均方根": "RMS",
    "统计报告已保存到:\n%1": "Statistics report saved to:\n%1",

    # 滤波相关
    "滤波": "Filter",
    "低通滤波": "Low Pass Filter",
    "高通滤波": "High Pass Filter",
    "带通滤波": "Band Pass Filter",
    "带阻滤波": "Band Stop Filter",
    "截止频率": "Cutoff Frequency",
    "滤波完成": "Filter Complete",
    "无可用曲线": "No Available Curves",
    "没有可用于触发的曲线": "No curves available for triggering",
    "没有可用于峰值检测的曲线": "No curves available for peak detection",
    "没有可用的曲线": "No available curves",

    # 其他
    "场景预设": "Scene Preset",
    "场景预设已应用，请按照预设配置发送数据。": "Scene preset applied. Please send data according to preset configuration.",
    "XY视图至少需要2条原始曲线": "XY view requires at least 2 raw curves",
    "至少需要2条曲线才能计算差值": "At least 2 curves needed to calculate difference",
    "请选择两条不同的曲线": "Please select two different curves",
    "确认退出": "Confirm Exit",
    "确认清空": "Confirm Clear",
    "保存成功": "Save Success",
    "保存失败": "Save Failed",
    "加载成功": "Load Success",
    "加载失败": "Load Failed",
    "检查更新": "Check Updates",
    "下载更新": "Download Update",
    "更新模块未初始化。": "Update module not initialized.",
    "未找到可用的下载链接。": "No available download links found.",
    "当前已是最新版本（%1）。": "Already the latest version (%1).",
    "关于 %1": "About %1",
    "恢复失败": "Recovery Failed",
    "命令列表": "Command List",
    "无法创建文件": "Cannot create file",
    "无法创建通信实例": "Cannot create communication instance",
    "无法创建文件": "Cannot create file",

    # 终端相关
    "终端": "Terminal",
    "终端模式": "Terminal Mode",
    "终端设置": "Terminal Settings",
    "光标": "Cursor",
    "闪烁": "Blink",
    "块状光标": "Block Cursor",
    "下划线光标": "Underline Cursor",
    "竖线光标": "Bar Cursor",
    "终端缓冲区": "Terminal Buffer",
    "缓冲区大小": "Buffer Size",
    "行数": "Line Count",
    "列数": "Column Count",

    # 调试相关
    "调试": "Debug",
    "调试模式": "Debug Mode",
    "日志": "Log",
    "信息": "Info",
    "警告": "Warning",
    "错误": "Error",
    "调试信息": "Debug Info",
    "性能": "Performance",
    "帧率": "Frame Rate",
    "CPU使用率": "CPU Usage",
    "内存使用": "Memory Usage",

    # 3D 姿态相关
    "姿态显示": "Attitude Display",
    "3D姿态显示 - %1": "3D Attitude Display - %1",
    "添加曲线...": "Add Curve...",
    "删除": "Delete",
    "俯仰": "Pitch",
    "滚转": "Roll",
    "偏航": "Yaw",
    "格式: ATT %r %p %y\\n": "Format: ATT %r %p %y\\n",
    "%r=滚转, %p=俯仰, %y=偏航": "%r=Roll, %p=Pitch, %y=Yaw",
    "显示模型:": "Show Model:",
    "红色通道:": "Red Channel:",
    "绿色通道:": "Green Channel:",
    "蓝色通道:": "Blue Channel:",
    "显示坐标轴": "Show Axes",
    "显示网格": "Show Grid",
    "未选中": "Not Selected",

    # 命令行相关
    "输入命令...": "Enter command...",
    "控制面板": "Control Panel",
    "快捷指令:": "Quick Commands:",
    "发送指令": "Send Command",

    # 搜索相关
    "搜索:": "Search:",
    "关键词不能为空": "Keyword cannot be empty",
    "上一个": "Previous",
    "下一个": "Next",
    "区分大小写": "Case Sensitive",
    "正则表达式": "Regular Expression",
    "匹配: %1/%2": "Matches: %1/%2",
    "未找到匹配": "No matches found",

    # 侧边栏相关
    "连接": "Connection",
    "数据": "Data",
    "绘图": "Plotter",
    "工具": "Tools",
    "设置": "Settings",
    "切换主题": "Toggle Theme",
    "帮助": "Help",

    # 升级相关
    "检查更新": "Check Updates",
    "发现新版本: %1": "New version found: %1",
    "当前已是最新版本": "Already the latest version",
    "更新检查失败": "Update check failed",
    "下载中...": "Downloading...",
    "下载完成": "Download Complete",
    "安装更新": "Install Update",

    # 错误消息
    "Error": "Error",
    "Failed to initialize core system.\nPlease check application permissions.": "Failed to initialize core system.\nPlease check application permissions.",

    # 方向
    "方向:": "Direction:",
    "搜索:": "Search:",
    "暂停": "Pause",
    "继续": "Continue",
    "复制": "Copy",
    "导出CSV": "Export CSV",
    "清空": "Clear",

    # 其他 UI 元素
    "功能:": "Function:",
    "确认": "Confirm",
    "请选择导出路径": "Please select export path",
    "没有数据可导出": "No data to export",
    "导出完成": "Export Complete",
    "请选择两条不同的曲线": "Please select two different curves",
    "XY视图至少需要2条原始曲线": "XY view requires at least 2 raw curves",
    "没有可用于直方图分析的原始曲线": "No raw curves available for histogram analysis",
    "统计报告已保存到:\n%1": "Statistics report saved to:\n%1",
    "没有可用的曲线": "No available curves",
    "场景预设": "Scene Preset",
    "场景预设已应用，请按照预设配置发送数据。": "Scene preset applied. Please send data according to preset configuration.",
}


def translate_from_dict(text):
    """从内置字典查找翻译"""
    if text in BUILTIN_DICT:
        return BUILTIN_DICT[text]

    # 尝试模糊匹配（处理带参数的字符串）
    for key, value in BUILTIN_DICT.items():
        if key in text or text in key:
            # 保留格式化参数
            result = text
            for k, v in BUILTIN_DICT.items():
                result = result.replace(k, v)
            if result != text:
                return result

    return None


def translate_online(text, retries=2, delay=1.0):
    """使用 MyMemory API 在线翻译"""
    if not text or not text.strip():
        return text

    encoded_text = quote(text)
    url = f"https://api.mymemory.translated.net/get?q={encoded_text}&langpair=zh-CN|en"

    for attempt in range(retries):
        try:
            req = Request(url, headers={
                'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
            })
            with urlopen(req, timeout=10) as response:
                data = json.loads(response.read().decode('utf-8'))
                if data.get('responseStatus') == 200:
                    translated = data['responseData']['translatedText']
                    if translated.isupper() and len(translated) > 5:
                        return None
                    return translated
        except Exception:
            if attempt < retries - 1:
                time.sleep(delay)
    return None


def is_cjk(text):
    """检查文本是否包含中文字符"""
    for char in text:
        if '一' <= char <= '鿿':
            return True
    return False


def process_ts_file(ts_path, use_online=False, dry_run=False):
    """处理 .ts 翻译文件"""
    print(f"Loading: {ts_path}")

    # 解析 XML
    tree = ET.parse(ts_path)
    root = tree.getroot()

    # 统计
    total = 0
    needs_translation = 0
    translated_count = 0
    online_count = 0
    skipped_count = 0

    # 收集需要翻译的消息
    messages_to_translate = []

    for context in root.findall('context'):
        context_name = context.find('name').text
        for message in context.findall('message'):
            source_elem = message.find('source')
            translation_elem = message.find('translation')

            if source_elem is None or translation_elem is None:
                continue

            source_text = source_elem.text or ''
            total += 1

            # 检查是否需要翻译
            is_unfinished = translation_elem.get('type') == 'unfinished'
            translation_text = translation_elem.text or ''
            is_empty = not translation_text.strip()

            if (is_unfinished or is_empty) and is_cjk(source_text):
                needs_translation += 1
                messages_to_translate.append({
                    'context': context_name,
                    'source': source_text,
                    'translation_elem': translation_elem
                })

    print(f"Total messages: {total}")
    print(f"Needs translation (Chinese, unfinished): {needs_translation}")

    if dry_run:
        print("\n[DRY RUN] No translations will be saved.")
        for msg in messages_to_translate[:30]:
            dict_result = translate_from_dict(msg['source'])
            status = "DICT" if dict_result else "API"
            print(f"  [{status}] [{msg['context']}] {msg['source']}")
        if len(messages_to_translate) > 30:
            print(f"  ... and {len(messages_to_translate) - 30} more")
        return

    # 翻译
    print(f"\nTranslating...")
    for i, msg in enumerate(messages_to_translate):
        source = msg['source']
        translated = None

        # 先查内置字典
        translated = translate_from_dict(source)

        # 如果字典没有且启用了在线翻译
        if not translated and use_online:
            translated = translate_online(source)
            if translated:
                online_count += 1
                time.sleep(0.3)  # API 限速

        if translated and translated != source:
            msg['translation_elem'].text = translated
            if msg['translation_elem'].get('type') == 'unfinished':
                del msg['translation_elem'].attrib['type']
            translated_count += 1
        else:
            skipped_count += 1

        if (i + 1) % 100 == 0:
            print(f"  Progress: {i+1}/{needs_translation} (translated: {translated_count}, online: {online_count}, skipped: {skipped_count})")

    # 保存
    print(f"\nSaving to: {ts_path}")
    tree.write(ts_path, encoding='utf-8', xml_declaration=True)

    print(f"\nDone!")
    print(f"  Total messages: {total}")
    print(f"  Translated: {translated_count} (dict: {translated_count - online_count}, online: {online_count})")
    print(f"  Skipped: {skipped_count}")
    print(f"  Remaining unfinished: {needs_translation - translated_count}")


def main():
    parser = argparse.ArgumentParser(description='Qt .ts 翻译文件自动翻译脚本')
    parser.add_argument('ts_file', help='.ts 翻译文件路径')
    parser.add_argument('--online', action='store_true', help='启用在线翻译 API（用于字典未覆盖的字符串）')
    parser.add_argument('--dry-run', action='store_true', help='仅显示需要翻译的内容，不实际翻译')
    args = parser.parse_args()

    if not os.path.exists(args.ts_file):
        print(f"Error: File not found: {args.ts_file}")
        sys.exit(1)

    process_ts_file(args.ts_file, args.online, args.dry_run)


if __name__ == '__main__':
    main()
