/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 *
 * 中文：把 4bpp 覆盖率直接写成面板半字节。旋转每次落笔前解析一次，内层不再调用
 * epd_draw_pixel（那里每像素都要再算旋转、两次边界判断和两次宽高查询）。
 * English: Write 4bpp coverage straight into the panel nibbles. Rotation is resolved once per
 * blit, and the inner loop never calls epd_draw_pixel (which recomputes rotation, two bounds and
 * two size queries per pixel).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "epdiy.h"

/// LUT 里的这个值表示“不落笔”，保留底下的图片或灰阶。/ "Paint nothing" in a LUT; keeps the pixel below.
#define FB_TEXT_SKIP 0xFFu

typedef struct {
    uint8_t *fb;
    int w, h, stride;
    enum EpdRotation rot;
} fb_text_target_t;

static inline fb_text_target_t fb_text_target(uint8_t *fb) {
    fb_text_target_t t;
    t.fb = fb;
    t.w = epd_width();
    t.h = epd_height();
    t.stride = t.w / 2;
    t.rot = epd_get_rotation();
    return t;
}

/// 写一行字模：逻辑坐标 (x, y) 起，沿逻辑 +x 前进 n 个像素。
/// One glyph row: starts at logical (x, y) and advances n pixels along logical +x.
static inline void fb_text_row(const fb_text_target_t *t, int x, int y,
                               const uint8_t *coverage, int n, const uint8_t *lut) {
    if (n <= 0) return;
    switch (t->rot) {
    case EPD_ROT_PORTRAIT: {
        // 逻辑列变成连续物理行，同一半字节位。 / A logical column becomes contiguous physical rows.
        const int px = t->w - 1 - y;
        if ((unsigned)px >= (unsigned)t->w) return;
        uint8_t *base = t->fb + (px >> 1);
        const bool high = (px & 1) != 0;
        // 有效像素满足 0 <= x+i < h：门槛按 x 偏移算，不能拿 x+n 直接比 h。
        // Valid pixels need 0 <= x+i < h; the limit is h-x, not h.
        int i0 = x < 0 ? -x : 0;
        int i1 = t->h - x > n ? n : t->h - x;
        for (int i = i0; i < i1; ++i) {
            const uint8_t v = lut[coverage[i]];
            if (v == FB_TEXT_SKIP) continue;
            uint8_t *p = base + (size_t)(x + i) * (size_t)t->stride;
            *p = high ? (uint8_t)((*p & 0x0F) | (uint8_t)(v << 4))
                      : (uint8_t)((*p & 0xF0) | v);
        }
        return;
    }
    case EPD_ROT_INVERTED_PORTRAIT: {
        const int px = y;
        if ((unsigned)px >= (unsigned)t->w) return;
        uint8_t *base = t->fb + (px >> 1);
        const bool high = (px & 1) != 0;
        int i0 = x < 0 ? -x : 0;
        int i1 = t->h - x > n ? n : t->h - x;
        for (int i = i0; i < i1; ++i) {
            const uint8_t v = lut[coverage[i]];
            if (v == FB_TEXT_SKIP) continue;
            uint8_t *p = base + (size_t)(t->h - 1 - (x + i)) * (size_t)t->stride;
            *p = high ? (uint8_t)((*p & 0x0F) | (uint8_t)(v << 4))
                      : (uint8_t)((*p & 0xF0) | v);
        }
        return;
    }
    case EPD_ROT_LANDSCAPE: {
        const int py = y;
        if ((unsigned)py >= (unsigned)t->h) return;
        uint8_t *row = t->fb + (size_t)py * (size_t)t->stride;
        int i0 = x < 0 ? -x : 0;
        int i1 = t->w - x > n ? n : t->w - x;
        for (int i = i0; i < i1; ++i) {
            const uint8_t v = lut[coverage[i]];
            if (v == FB_TEXT_SKIP) continue;
            const int px = x + i;
            uint8_t *p = row + (px >> 1);
            *p = (px & 1) ? (uint8_t)((*p & 0x0F) | (uint8_t)(v << 4))
                          : (uint8_t)((*p & 0xF0) | v);
        }
        return;
    }
    case EPD_ROT_INVERTED_LANDSCAPE: {
        const int py = t->h - 1 - y;
        if ((unsigned)py >= (unsigned)t->h) return;
        uint8_t *row = t->fb + (size_t)py * (size_t)t->stride;
        int i0 = x < 0 ? -x : 0;
        int i1 = t->w - x > n ? n : t->w - x;
        for (int i = i0; i < i1; ++i) {
            const uint8_t v = lut[coverage[i]];
            if (v == FB_TEXT_SKIP) continue;
            const int px = t->w - 1 - (x + i);
            uint8_t *p = row + (px >> 1);
            *p = (px & 1) ? (uint8_t)((*p & 0x0F) | (uint8_t)(v << 4))
                          : (uint8_t)((*p & 0xF0) | v);
        }
        return;
    }
    }
}

/// 整块字模：逐行调用 fb_text_row。/ Whole glyph: one fb_text_row per bitmap row.
static inline void fb_text_glyph(const fb_text_target_t *t, int left, int top,
                                 const uint8_t *bitmap, int width, int height,
                                 const uint8_t *lut) {
    for (int gy = 0; gy < height; ++gy)
        fb_text_row(t, left, top + gy, bitmap + (size_t)gy * (size_t)width, width, lut);
}
