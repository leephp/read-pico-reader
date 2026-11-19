/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 *
 * 宿主模拟器对未编译固件模块的最小替身。
 * Minimal stand-ins for firmware modules the simulator does not compile.
 */
#include "sim_stubs.h"

void app_setup_init(void) {}
bool app_setup_pending(void) { return false; }
bool app_setup_english(void) { return false; }
esp_err_t app_setup_set_english(bool english) {
    (void)english;
    return ESP_OK;
}
esp_err_t app_setup_finish(void) { return ESP_OK; }

/* 模拟器没有独立宿主进程可重启；保持当前会话。/ The simulator has no separate host to restart. */
void app_restart_host(void) {}

bool app_touch_input_pending(app_ctx_t *ctx) {
    (void)ctx;
    return false;
}

esp_err_t wl_read(wl_handle_t handle, size_t offset, void *dst, size_t size) {
    (void)handle;
    (void)offset;
    (void)dst;
    (void)size;
    return ESP_FAIL;
}

esp_err_t wl_unmount(wl_handle_t handle) {
    (void)handle;
    return ESP_OK;
}

size_t wl_size(wl_handle_t handle) {
    (void)handle;
    /* 0 让 flash_data_blank() 直接判否，模拟器绝不因“空白判断”去格式化宿主目录。 */
    /* 0 makes flash_data_blank() decline, so the host folder is never formatted. */
    return 0;
}
