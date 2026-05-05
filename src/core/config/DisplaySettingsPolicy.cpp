/**
 * @file DisplaySettingsPolicy.cpp
 * @brief 显示相关设置的统一默认值与兼容迁移策略实现
 * @author ComAssistant Team
 * @date 2026-05-06
 */

#include "DisplaySettingsPolicy.h"

#include <QtGlobal>

namespace ComAssistant {

namespace {

/*
 * 使用固定键名把“当前值”和“迁移是否完成”绑在一起管理。
 * 这样主窗口读取显示设置时，就能顺手把历史遗留的 8MB 默认值纠正掉。
 */
const char* const kHexBufferSettingKey = "Display/HexBufferMB";
const char* const kHexBufferMigrationDoneKey = "Display/HexBufferMBMigrationDone";

} // namespace

int sanitizeHexBufferMb(int value)
{
    /*
     * 这里统一做上下限保护：
     * - 下限至少为 1MB，避免异常配置导致缓冲区不可用；
     * - 上限控制在 64MB，与设置对话框可选范围一致，避免手工改配置后
     *   把接收区缓存拉到明显超出当前产品预期的体量。
     */
    return qBound(kMinHexBufferMb, value, kMaxRecommendedHexBufferMb);
}

int loadHexBufferMbSetting(QSettings& settings)
{
    const bool migrationDone = settings.value(kHexBufferMigrationDoneKey, false).toBool();
    const bool hasStoredValue = settings.contains(kHexBufferSettingKey);

    int hexBufferMb = hasStoredValue
        ? settings.value(kHexBufferSettingKey, kDefaultHexBufferMb).toInt()
        : kDefaultHexBufferMb;

    bool needWriteBack = false;

    if (!hasStoredValue) {
        /*
         * 没有旧值时直接落新默认值，保证首次进入设置页看到的就是 2MB，
         * 同时也让运行时逻辑和界面逻辑天然一致。
         */
        hexBufferMb = kDefaultHexBufferMb;
        needWriteBack = true;
    }

    if (!migrationDone && hasStoredValue && hexBufferMb == kLegacyDefaultHexBufferMb) {
        /*
         * 只在“尚未迁移”且“值正好命中旧默认 8MB”时自动改成 2MB。
         * 这样既能修复旧版本默认值残留，又不会在迁移完成后反复覆盖用户手动设置。
         */
        hexBufferMb = kDefaultHexBufferMb;
        needWriteBack = true;
    }

    const int sanitizedValue = sanitizeHexBufferMb(hexBufferMb);
    if (sanitizedValue != hexBufferMb) {
        hexBufferMb = sanitizedValue;
        needWriteBack = true;
    }

    if (!migrationDone) {
        settings.setValue(kHexBufferMigrationDoneKey, true);
        needWriteBack = true;
    }

    if (needWriteBack) {
        settings.setValue(kHexBufferSettingKey, hexBufferMb);
        settings.sync();
    }

    return hexBufferMb;
}

void saveHexBufferMbSetting(QSettings& settings, int value)
{
    const int sanitizedValue = sanitizeHexBufferMb(value);

    /*
     * 用户明确点击“应用/确定”后，说明这已经是一个显式选择。
     * 因此这里同步写入“迁移完成”标记，避免未来再次把用户选择误判成旧默认值。
     */
    settings.setValue(kHexBufferSettingKey, sanitizedValue);
    settings.setValue(kHexBufferMigrationDoneKey, true);
}

} // namespace ComAssistant
