/* SPDX-License-Identifier: Apache-2.0
 * 中文：Pico 可重建缓存共用增长边界，给字体、插图和无线留出 PSRAM 与连续块。
 * English: Pico rebuildable caches share growth admission, preserving PSRAM/contiguous space.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_heap_caps.h"
#include "../kiiko_board_config.h"
#define CACHE_PSRAM (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define CACHE_MEMORY_RESERVE (KIIKO_BOARD_CACHE_PSRAM_RESERVE_KB * 1024u)
#define CACHE_MEMORY_BLOCK_RESERVE (KIIKO_BOARD_CACHE_BLOCK_RESERVE_KB * 1024u)

static inline bool cache_memory_can_grow(size_t bytes) {
    const size_t free=heap_caps_get_free_size(CACHE_PSRAM);
    const size_t largest=heap_caps_get_largest_free_block(CACHE_PSRAM);
    return bytes<=free && free-bytes>=CACHE_MEMORY_RESERVE &&
        bytes<=largest && largest-bytes>=CACHE_MEMORY_BLOCK_RESERVE;
}
static inline size_t cache_memory_budget(size_t used,size_t cap) {
    const size_t free=heap_caps_get_free_size(CACHE_PSRAM);
    size_t room=free>CACHE_MEMORY_RESERVE?free-CACHE_MEMORY_RESERVE:0;
    if(heap_caps_get_largest_free_block(CACHE_PSRAM)<CACHE_MEMORY_BLOCK_RESERVE)room=0;
    if(used>=cap)return cap;
    return room<cap-used?used+room:cap;
}
