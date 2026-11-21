/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：在 TF 卡和电脑之间独占切换 USB 磁盘访问。
 * English: Switch exclusive SD access between firmware and USB host.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t usb_storage_start(void);
esp_err_t usb_storage_stop(void);
bool usb_storage_active(void);
bool usb_storage_connected(void);

/// 强制主机看到一次 USB 断开/重连，用于首次进入时主机仍绑定串口设备。
/// Force a host-visible USB detach/attach when the host may still be bound to serial.
esp_err_t usb_storage_reattach(void);

/// 无损恢复串口 PHY 和上拉；TinyUSB 占用期间不操作。
/// Restore the serial PHY and pull-up without detaching; leave TinyUSB ownership intact.
void usb_storage_phy_init(void);

/// 浅睡退出后重新枚举串口，不在普通轮询中调用。
/// Re-enumerate after light sleep, never from ordinary polling.
void usb_storage_serial_resume(void);
