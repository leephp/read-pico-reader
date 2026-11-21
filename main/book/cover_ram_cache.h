/* SPDX-License-Identifier: Apache-2.0
 * 中文：Pico 共用封面缓存，键包含文件版本和绘制参数，不向调用方借出指针。
 * English: Shared Pico cover cache uses versioned raster keys and copy-out ownership.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint64_t field[8]; } cover_ram_key_t;
typedef struct { unsigned width,height; bool white_edge; } cover_ram_meta_t;
typedef struct { size_t bytes,entries; uint64_t hits,misses,evictions; } cover_ram_stats_t;
/// out 可为 NULL 以只检查命中并读元数据。/ Pass NULL for out to check residency and read metadata only.
bool cover_ram_cache_get(const cover_ram_key_t *key,void *out,size_t capacity,cover_ram_meta_t *meta);
bool cover_ram_cache_put(const cover_ram_key_t *key,const void *pixels,size_t bytes,cover_ram_meta_t meta);
/// 在同一LRU预算内复制已处理的物理像素行，不借出缓存指针。/ Copy finished physical raster rows within the same LRU budget, without lending pointers.
bool cover_ram_cache_get_rows(const cover_ram_key_t*,void*,size_t stride,size_t span,size_t rows);
bool cover_ram_cache_put_rows(const cover_ram_key_t*,const void*,size_t stride,size_t span,size_t rows);
void cover_ram_cache_trim(size_t budget);
void cover_ram_cache_clear(void);
cover_ram_stats_t cover_ram_cache_stats(void);
