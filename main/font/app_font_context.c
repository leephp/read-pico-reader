/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * 用户修订：异常启动暂用内建字体，避免自动重载故障资源；手动选择成功加载后才保存并允许本次启动使用。
 * User revision: recovery starts avoid automatic faulty-face reloads; explicit selections are saved and allowed for this session only after loading succeeds.
 * 中文：系统默认使用内建思源黑体子集，TF 卡黑体补足缺字；用户选择的字体覆盖默认外观。
 * English: The built-in Source Han Sans face is the default; the SD face fills missing glyphs and explicit choices override it.
 * 用户修订：票根始终借用系统字体，浅睡解锁恢复锁屏前的实际字体和上下文，不覆盖保存的选择。
 * User revision: tickets always borrow the system face; light-sleep wake restores the actual previous face/context without altering saved choices.
 */
#include "app_font_context.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "settings.h"
#include "boot_state.h"
#ifdef ESP_PLATFORM
#include "read_pico_sd.h"
#endif
#include "ttf_font.h"
#include "ui_kit.h"
#include "ui_nav.h"

static bool s_reading;
// 手动加载成功的系统字体可用于本次恢复启动，不放开其他资源。/ Only the manually validated system face bypasses recovery for this session.
static char s_confirmed_system[TTF_FONT_PATH_MAX];
// 自动加载跳过未变化的失败文件，手动选择始终重试。/ Automatic activation skips unchanged failed files; explicit selection always retries.
static char s_rejected[TTF_FONT_PATH_MAX];
static time_t s_rejected_mtime;
static off_t s_rejected_size;
static bool font_file(const char *path, struct stat *st) {
    return path && stat(path, st) == 0 && S_ISREG(st->st_mode) && st->st_size > 0;
}

static bool system_recovery(void) {
    return pico_boot_recovery() && (!s_confirmed_system[0] ||
        strcmp(s_confirmed_system, app_settings_system_font_path()));
}

static const char* system_path(char fallback[TTF_FONT_PATH_MAX]) {
    if (system_recovery()) return TTF_FONT_BUILTIN;
#ifdef ESP_PLATFORM
    if (!read_pico_sd_io_ready() &&
        (!app_settings_system_font_path()[0] || !strncmp(app_settings_system_font_path(),"/sdcard/",8)))
        return TTF_FONT_BUILTIN;
#endif
    const char* chosen = app_settings_system_font_path();
    if (chosen[0]) return chosen;
    if (snprintf(fallback, TTF_FONT_PATH_MAX, "%s/Hei.ttf", app_settings_fonts_dir()) >= TTF_FONT_PATH_MAX)
        return TTF_FONT_BUILTIN;
    struct stat st;
    if (stat(fallback, &st) == 0 && S_ISREG(st.st_mode)) return fallback;
    if (!ttf_font_count()) ttf_font_scan();
    const ttf_font_item_t *item = ttf_font_item(0);
    return item && font_file(item->path, &st) ? item->path : TTF_FONT_BUILTIN;
}

static bool activate(const char* path) {
    struct stat st = {0};
    bool builtin = ttf_font_path_is_builtin(path);
    bool valid = builtin || font_file(path, &st);
    bool rejected = !builtin && !strcmp(s_rejected, path) &&
        s_rejected_mtime == st.st_mtime && s_rejected_size == st.st_size;
    if (!valid || rejected) path = TTF_FONT_BUILTIN;
    if (ttf_font_ready() && !strcmp(ttf_font_path(), path)) return false;
    if (ttf_font_open(path) == ESP_OK) {
        if (!ttf_font_path_is_builtin(path)) s_rejected[0] = 0;
        return true;
    }
    if (!ttf_font_path_is_builtin(path)) {
        snprintf(s_rejected, sizeof(s_rejected), "%s", path);
        s_rejected_mtime = st.st_mtime;
        s_rejected_size = st.st_size;
    }
    if (ttf_font_ready() && ttf_font_is_builtin()) return false;
    return ttf_font_open_builtin() == ESP_OK;
}

bool app_font_activate_system(void) {
    s_reading = false;
    ttf_set_stroke_adjust(0);
    ttf_set_antialias(true);
    const char* chosen = app_settings_system_font_path();
    char fallback[TTF_FONT_PATH_MAX];
    const char* path = system_path(fallback);
    bool changed = activate(path);
    // 未选字体时由内建思源黑体绘制界面；TTF 仍保留完整字库供文件名缺字回退。
    // Built-in Source Han Sans draws default UI; the full TTF remains loaded for missing glyphs.
    ui_text_set_system_font(!system_recovery() && chosen[0] && ttf_font_ready() &&
                           !strcmp(chosen, ttf_font_path()) && !ttf_font_is_builtin());
    ui_text_set_system_scale(true);
    ui_nav_status_font_context(false, changed);
    return changed;
}

esp_err_t app_font_select_system(const char *path) {
    if (!path || strlen(path) >= TTF_FONT_PATH_MAX) return ESP_ERR_INVALID_ARG;
    char selected[TTF_FONT_PATH_MAX], previous[TTF_FONT_PATH_MAX];
    snprintf(selected, sizeof(selected), "%s", ttf_font_path_is_builtin(path) ? "" : path);
    snprintf(previous, sizeof(previous), "%s", ttf_font_path());
    const char *target = selected[0] ? selected : TTF_FONT_BUILTIN;
    // 先加载再保存，失败不能留下假的选中项。/ Load before saving so a failed face never becomes the selection.
    esp_err_t err = ttf_font_open(target);
    if (err == ESP_OK && (!ttf_font_ready() || strcmp(ttf_font_path(), target))) err = ESP_FAIL;
    if (err == ESP_OK) {
        app_settings_set_system_font_path(selected);
        if (strcmp(app_settings_system_font_path(), selected)) err = ESP_ERR_INVALID_STATE;
    }
    if (err != ESP_OK) {
        if (!ttf_font_ready() || strcmp(ttf_font_path(), previous)) (void)ttf_font_open(previous);
    } else {
        snprintf(s_confirmed_system, sizeof(s_confirmed_system), "%s", selected);
        if (!strcmp(s_rejected, selected)) s_rejected[0] = 0;
    }
    app_font_activate_system();
    ui_nav_status_font_context(false, true);
    return err;
}

bool app_font_activate_reading(void) {
    s_reading = true;
    ttf_set_stroke_adjust_fine(app_settings_book_weight_fine());
    ttf_set_antialias(app_settings_book_antialias());
    const char* saved = pico_boot_recovery() ? TTF_FONT_BUILTIN : app_settings_font_path();
    bool changed = activate(ttf_font_path_is_builtin(saved) ? TTF_FONT_BUILTIN : saved);
    ui_text_set_system_font(false);
    ui_text_set_system_scale(false);
    ui_nav_status_font_context(true, changed);
    return changed;
}

bool app_font_retry_active(void) {
    return s_reading ? app_font_activate_reading() : app_font_activate_system();
}

void app_font_begin_lock(app_lock_font_t *saved) {
    if (!saved) return;
    snprintf(saved->path, sizeof(saved->path), "%s", ttf_font_path());
    saved->reading = s_reading;
    app_font_activate_system();
}

void app_font_end_lock(const app_lock_font_t *saved) {
    if (!saved) return;
    (void)activate(saved->path);
    s_reading = saved->reading;
    ttf_set_stroke_adjust_fine(s_reading ? app_settings_book_weight_fine() : 0);
    ttf_set_antialias(!s_reading || app_settings_book_antialias());
    const char *chosen = app_settings_system_font_path();
    ui_text_set_system_font(!s_reading && !system_recovery() && chosen[0] &&
                           !strcmp(chosen, ttf_font_path()) && !ttf_font_is_builtin());
    ui_text_set_system_scale(!s_reading);
    ui_nav_status_font_context(s_reading, false);
}
