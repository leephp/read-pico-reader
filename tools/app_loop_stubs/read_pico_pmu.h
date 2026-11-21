/* 中文：调度测试替身。/ English: Scheduler test shim. */
#pragma once
#include "common.h"

void read_pico_pmu_drain_events(void);

typedef struct { bool config_ok; struct { bool wake_on_charge; } config; } pmu_snapshot_t;
#define PMU_CMD_CONFIG_SET_WAKE_ON_CHARGE 1
static inline const pmu_snapshot_t *read_pico_pmu_get(void) { return NULL; }
static inline esp_err_t read_pico_pmu_cmd(int cmd, const void *data, size_t size) { (void)cmd; (void)data; (void)size; return ESP_OK; }
