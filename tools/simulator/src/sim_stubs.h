/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 *
 * 宿主模拟器的补声明：模拟器不编译的固件模块（首次引导、重启、触摸待处理、磨损均衡）。
 * Host-side gaps for firmware modules the simulator does not compile: setup lifecycle, restart,
 * pending input and wear levelling. Never part of the device build.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "app.h"
#include "esp_err.h"
#include "wear_levelling.h"

/* 模拟器构建的版本标签；设备构建从不包含本文件。/ Version label for host builds; device builds never include this. */
#define SIM_FIRMWARE_VERSION "0.3.3-dev-sim"

/* main/setup_state.c */
void app_setup_init(void);
bool app_setup_pending(void);
bool app_setup_english(void);
esp_err_t app_setup_set_english(bool english);
esp_err_t app_setup_finish(void);

/* main/sleep.c */
void app_restart_host(void);

/* main/app/app_loop.c */
bool app_touch_input_pending(app_ctx_t *ctx);

/* IDF wear_levelling：模拟器只挂载宿主目录。/ Host folders only; no real flash. */
esp_err_t wl_read(wl_handle_t handle, size_t offset, void *dst, size_t size);
esp_err_t wl_unmount(wl_handle_t handle);
size_t wl_size(wl_handle_t handle);
