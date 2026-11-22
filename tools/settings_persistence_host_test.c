/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：真实设置模块的写入故障、重试与重载测试。
 * English: Real settings-module write failures, retries and reload tests.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef __APPLE__
size_t strlcpy(char *dst, const char *src, size_t cap) {
    size_t n = strlen(src);
    if (cap) { size_t copy = n < cap - 1 ? n : cap - 1; memcpy(dst, src, copy); dst[copy] = 0; }
    return n;
}
#endif
#include "../main/settings.c"

static struct { char key[16], text[300]; uint32_t number; int type; } entries[128];
static unsigned used, opens, closes, writes, commits, fail_write;
static bool fail_open, fail_commit;
static int entry(const char *key, bool create) {
    for (unsigned i = 0; i < used; ++i) if (!strcmp(entries[i].key, key)) return (int)i;
    if (!create) return -1;
    assert(used < 128 && strlen(key) < sizeof(entries[0].key));
    strlcpy(entries[used].key, key, sizeof(entries[0].key));
    return (int)used++;
}
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h) {
    (void)mode; assert(!strcmp(ns, NVS_NS)); ++opens;
    if (fail_open) return ESP_FAIL;
    *h = 1; return ESP_OK;
}
void nvs_close(nvs_handle_t h) { assert(h == 1); ++closes; }
esp_err_t nvs_commit(nvs_handle_t h) { assert(h == 1); ++commits; return fail_commit ? ESP_FAIL : ESP_OK; }
esp_err_t nvs_flash_init(void) { return ESP_OK; }
void app_setup_init(void) {}
static int write_entry(const char *key) { return ++writes == fail_write ? -1 : entry(key, true); }
// 模拟 set 即可落盘的 NVS；提交失败不能当作磁盘事务回滚。
// Model NVS writes taking effect at set time: commit failure is not a disk transaction rollback.
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v) {
    (void)h; int i = write_entry(key); if (i < 0) return ESP_FAIL;
    entries[i].type = 1; entries[i].number = v; return ESP_OK;
}
esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v) {
    (void)h; int i = write_entry(key); if (i < 0) return ESP_FAIL;
    entries[i].type = 2; entries[i].number = v; return ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *v) {
    (void)h; int i = write_entry(key); if (i < 0) return ESP_FAIL;
    entries[i].type = 3; strlcpy(entries[i].text, v, sizeof(entries[i].text)); return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *v) {
    (void)h; int i = entry(key, false); if (i < 0 || entries[i].type != 1) return ESP_FAIL;
    *v = entries[i].number; return ESP_OK;
}
esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v) {
    (void)h; int i = entry(key, false); if (i < 0 || entries[i].type != 2) return ESP_FAIL;
    *v = entries[i].number; return ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *v, size_t *n) {
    (void)h; int i = entry(key, false); if (i < 0 || entries[i].type != 3) return ESP_FAIL;
    size_t bytes = strlen(entries[i].text) + 1;
    if (!v) { *n = bytes; return ESP_OK; }
    if (*n < bytes) return ESP_FAIL;
    memcpy(v, entries[i].text, bytes); *n = bytes; return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char *k, void *v, size_t *n) {
    (void)h; (void)k; (void)v; (void)n; return ESP_FAIL;
}
static void failure(unsigned stage) {
    opens = closes = writes = commits = 0;
    fail_open = stage == 1; fail_write = stage == 2 ? 1 : 0; fail_commit = stage == 3;
}
static void check_failure(unsigned stage) {
    assert(opens == 1 && closes == (stage != 1));
    assert(writes == (stage != 1) && commits == (stage == 3));
}
#define CHECK(name, field, desired) do { \
    for (unsigned stage = 1; stage <= 3; ++stage) { \
        int old = field; failure(stage); app_settings_set_##name(desired); \
        assert((int)field == old); check_failure(stage); \
        failure(0); app_settings_set_##name(desired); assert((int)field != old); \
        int saved = field; field = old; app_settings_init(); assert((int)field == saved); \
        failure(0); app_settings_set_##name(old); \
    } \
} while (0)
int main(void) {
    CHECK(staged_shutdown, s_staged_shutdown, true);
    CHECK(wake_presses, s_wake_presses, 2);
    CHECK(pin_grace_code, s_pin_grace_code_v, 2);
    CHECK(main_refresh_mode, s_main_refresh, APP_MAIN_REFRESH_FAST);
    CHECK(auto_lock_minutes, s_auto_lock_minutes, 10);
    CHECK(home_full_refresh, s_home_full_refresh, false);
    CHECK(system_contrast, s_system_contrast, 120);
    CHECK(lock_style, s_lock_style, 1);
    CHECK(pickup_wake, s_pickup_wake, true);
    CHECK(book_px, s_book_px, 52);
    CHECK(reader_full_pages, s_reader_full_pages, 10);
    CHECK(reader_turn_effect, s_reader_turn_effect, 1);
    CHECK(reader_power_turn, s_reader_power_turn, true);
    CHECK(reader_immersive, s_reader_immersive, true);
    CHECK(reader_hide_images, s_reader_hide_images, true);
    CHECK(book_fonts, s_book_fonts, false);
    CHECK(book_vertical, s_book_vertical, true);
    CHECK(reader_vertical_turn, s_reader_vertical_turn, true);
    CHECK(reader_hold_refresh, s_reader_hold_refresh, true);
    CHECK(ble_turner, s_ble_turner, true);
    CHECK(book_tracking, s_book_tracking, 4);
    CHECK(book_indent, s_book_indent, 1);
    CHECK(book_reading_line, s_book_reading_line, 1);
    CHECK(book_line_spacing, s_book_line, 140);
    CHECK(book_margin, s_book_margin, 30);
    CHECK(book_paragraph_spacing, s_book_para, 25);
    CHECK(sleep_mode, s_sleep, APP_SLEEP_OFF);
    // 编码型 API 与字符串分别覆盖失败后的相同输入重试。
    // Encoded APIs and strings also retry the identical input after failure.
    for (unsigned stage = 1; stage <= 3; ++stage) {
        failure(0); app_settings_set_book_indent_adjust(0); app_settings_set_book_reading_line_offset(0);
        failure(stage); app_settings_set_book_indent_adjust(-50); assert(app_settings_book_indent_adjust() == 0); check_failure(stage);
        failure(0); app_settings_set_book_indent_adjust(-50); assert(app_settings_book_indent_adjust() == -50);
        s_book_indent_adjust = 20; app_settings_init(); assert(app_settings_book_indent_adjust() == -50);
        failure(stage); app_settings_set_book_reading_line_offset(6); assert(app_settings_book_reading_line_offset() == 0); check_failure(stage);
        failure(0); app_settings_set_book_reading_line_offset(6); assert(app_settings_book_reading_line_offset() == 6);
        failure(0); app_settings_set_wallpaper_path("");
        failure(stage); app_settings_set_wallpaper_path("/sdcard/pictures/test.png"); assert(!s_wallpaper[0]); check_failure(stage);
        failure(0); app_settings_set_wallpaper_path("/sdcard/pictures/test.png");
        s_wallpaper[0] = 0; app_settings_init(); assert(!strcmp(s_wallpaper, "/sdcard/pictures/test.png"));
    }
    for (unsigned stage = 1; stage <= 6; ++stage) {
        failure(0); app_settings_set_book_layout(48, 24, 130, 50);
        failure(0); fail_open = stage == 1; fail_write = stage >= 2 && stage <= 5 ? stage - 1 : 0; fail_commit = stage == 6;
        app_settings_set_book_layout(52, 30, 140, 25);
        assert(s_book_px == 48 && s_book_margin == 24 && s_book_line == 130 && s_book_para == 50);
        assert(closes == (stage != 1));
        failure(0); app_settings_set_book_layout(52, 30, 140, 25);
        assert(writes == 4 && commits == 1);
        s_book_px = 48; s_book_margin = 24; s_book_line = 130; s_book_para = 50;
        app_settings_init(); assert(s_book_px == 52 && s_book_margin == 30 && s_book_line == 140 && s_book_para == 25);
    }
    failure(2); assert(!nvs_put_u32("test32", 42)); assert(!commits && closes == 1);
    failure(3); assert(!nvs_put_u32("test32", 42)); assert(commits == 1 && closes == 1);
    failure(0); assert(nvs_put_u32("test32", 42));
    puts("PASS: 30 preferences, open/set/commit failures, unchanged RAM, identical-input retry, reload, multi-key partial writes");
}
