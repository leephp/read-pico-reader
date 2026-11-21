#pragma once
#include "common.h"
bool test_usb_storage_active(void);
static inline bool usb_storage_active(void) { return test_usb_storage_active(); }
static inline bool usb_storage_connected(void) { return true; }
static inline esp_err_t usb_storage_reattach(void) { return ESP_OK; }
static inline esp_err_t usb_storage_stop(void) { return ESP_OK; }

static inline void usb_storage_phy_init(void) {}
