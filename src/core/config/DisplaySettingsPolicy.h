/**
 * @file DisplaySettingsPolicy.h
 * @brief 显示相关设置的统一默认值与兼容迁移策略
 * @author ComAssistant Team
 * @date 2026-05-06
 */

#ifndef COMASSISTANT_DISPLAYSETTINGSPOLICY_H
#define COMASSISTANT_DISPLAYSETTINGSPOLICY_H

#include <QSettings>

namespace ComAssistant {

/*
 * 统一管理 HEX 缓冲区的显示设置常量，避免主窗口、设置页和启动迁移逻辑
 * 分别维护一套数字，导致“界面默认值”和“运行时实际值”不一致。
 */
constexpr int kDefaultHexBufferMb = 2;
constexpr int kLegacyDefaultHexBufferMb = 8;
constexpr int kMinHexBufferMb = 1;
constexpr int kMaxRecommendedHexBufferMb = 64;

/**
 * @brief 对 HEX 缓冲区大小做范围保护。
 * @param value 待校正的缓冲区大小，单位为 MB。
 * @return 位于允许范围内的缓冲区大小，避免异常配置把运行时撑得过大或变成无效值。
 */
int sanitizeHexBufferMb(int value);

/**
 * @brief 读取 HEX 缓冲区设置，并在首次升级时把旧默认值 8MB 迁移为新默认值 2MB。
 * @param settings 应用当前使用的 QSettings 对象。
 * @return 可直接用于运行时的缓冲区大小，单位为 MB。
 *
 * 说明：
 * 1. 如果用户此前从未保存过该项，则自动写入新的 2MB 默认值。
 * 2. 如果命中历史默认值 8MB，且尚未执行过迁移，则只迁移一次到 2MB。
 * 3. 如果用户在迁移完成后手动再改回 8MB，本函数会尊重用户选择，不会反复覆盖。
 */
int loadHexBufferMbSetting(QSettings& settings);

/**
 * @brief 保存 HEX 缓冲区设置，并标记该项已经完成默认值迁移。
 * @param settings 应用当前使用的 QSettings 对象。
 * @param value 用户希望保存的缓冲区大小，单位为 MB。
 *
 * 保存时会先做范围校正，再写入迁移完成标记，避免用户手动改成 8MB 后，
 * 下一次启动又被“旧默认值迁移逻辑”误判并改回 2MB。
 */
void saveHexBufferMbSetting(QSettings& settings, int value);

} // namespace ComAssistant

#endif // COMASSISTANT_DISPLAYSETTINGSPOLICY_H
