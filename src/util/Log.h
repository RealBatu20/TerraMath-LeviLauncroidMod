// TerraMath-LeviLauncroidMod — logging shim.
// Uses Android's liblog directly (always available in the NDK) so the mod logs
// even if the Levi Launchroid pl::Logger header layout differs across SDK
// revisions. Tag is greppable via `adb logcat -s TerraMath`.
#pragma once

#if defined(__ANDROID__)
#include <android/log.h>
#define TM_LOG_TAG "TerraMath"
#define TM_LOGI(...) __android_log_print(ANDROID_LOG_INFO, TM_LOG_TAG, __VA_ARGS__)
#define TM_LOGW(...) __android_log_print(ANDROID_LOG_WARN, TM_LOG_TAG, __VA_ARGS__)
#define TM_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TM_LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define TM_LOGI(...)                \
    do {                            \
        std::printf("[I] ");        \
        std::printf(__VA_ARGS__);   \
        std::printf("\n");          \
    } while (0)
#define TM_LOGW(...)                \
    do {                            \
        std::printf("[W] ");        \
        std::printf(__VA_ARGS__);   \
        std::printf("\n");          \
    } while (0)
#define TM_LOGE(...)                \
    do {                            \
        std::printf("[E] ");        \
        std::printf(__VA_ARGS__);   \
        std::printf("\n");          \
    } while (0)
#endif
