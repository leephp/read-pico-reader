/*
 * SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：有界图标字节游程解码，不分配堆内存。
 * English: Bounded icon byte-run decoding without heap allocation.
 * 冻结：解码保留原始 4bpp 字节。/ Frozen: Preserve the original 4bpp bytes.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// 高位为重复游程，否则为原样字节；低七位加一为长度。
// High bit selects a repeat run, otherwise literal bytes; low seven bits plus one give the length.
static inline bool ui_icon_rle_decode(const uint8_t *src, size_t length,
                                      uint8_t *out, size_t capacity) {
    if (!src || !out) return false;
    size_t input = 0, output = 0;
    while (input < length) {
        unsigned control = src[input++];
        size_t count = (control & 127u) + 1u;
        if (count > capacity - output) return false;
        if (control & 128u) {
            if (input == length) return false;
            memset(out + output, src[input++], count);
        } else {
            if (count > length - input) return false;
            memcpy(out + output, src + input, count);
            input += count;
        }
        output += count;
    }
    return output == capacity;
}
