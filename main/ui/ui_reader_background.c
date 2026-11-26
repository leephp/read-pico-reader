/* SPDX-License-Identifier: Apache-2.0 */
#ifdef ESP_PLATFORM
#include "ui_locale_format.h"
#endif
#include "ui_reader_background.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "book_cover.h"
#include "esp_heap_caps.h"
#include "ui_kit.h"
#include "ui_image_dither.h"
#include "settings.h"
#include "reader_background_photos.inc"
static const uint8_t *builtin_photo(const char *path) {
    if (!path) return NULL;
    if (!strcmp(path, BOOK_BG_MOUNTAIN)) return mountain_photo;
    if (!strcmp(path, BOOK_BG_BAMBOO)) return bamboo_photo;
    return NULL;
}
static void draw_photo(uint8_t *fb, EpdRect rect, const uint8_t *photo) {
    for (int y=0; y<rect.height; ++y) for (int x=0; x<rect.width; ++x) {
        unsigned i = (y*96/rect.height)*144 + x*144/rect.width;
        uint8_t v = photo[i/2];
        epd_draw_pixel(rect.x+x, rect.y+y, i&1 ? v<<4 : v&0xf0, fb);
    }
}
void ui_reader_background_sample(uint8_t *fb, EpdRect rect, const char *builtin) {
    const uint8_t *photo = builtin_photo(builtin);
    if (photo) draw_photo(fb, rect, photo);
}
static uint8_t *s_gray;
static const uint8_t *s_photo;
// Optional native-frame copy. Text prefetch frames remain ink-only.
static uint8_t *s_paper;
static int s_paper_rotation = -1;
static unsigned s_paper_strength;
static unsigned s_width, s_height;
static char s_path[288];
static off_t s_size;
static time_t s_mtime;
void ui_reader_background_release(void) {
    free(s_paper); s_paper = NULL; s_paper_rotation = -1;
    s_photo = NULL; free(s_gray); s_gray = NULL; s_path[0] = 0; s_width = s_height = 0;
}
bool ui_reader_background_prepare(const char *path) {
    const uint8_t *photo = builtin_photo(path);
    if (photo) {
        if (s_photo != photo || s_width != (unsigned)epd_rotated_display_width() ||
            s_height != (unsigned)epd_rotated_display_height()) ui_reader_background_release();
        s_photo = photo;
        s_width = epd_rotated_display_width();
        s_height = epd_rotated_display_height();
        return true;
    }
    struct stat st;
    if (!path || !path[0] || strncmp(path, "/sdcard/pictures/", 17) ||
        strnlen(path, sizeof(s_path)) >= sizeof(s_path) || stat(path, &st) ||
        !S_ISREG(st.st_mode) || st.st_size <= 0 || (uint64_t)st.st_size > BOOK_IMAGE_FILE_MAX) {
        ui_reader_background_release(); return false;
    }
    const unsigned w = epd_rotated_display_width(), h = epd_rotated_display_height();
    if (!w || !h || w > 1216 || h > 1216 || (uint64_t)w * h > 684u * 1216u) {
        ui_reader_background_release(); return false;
    }
    if (!strcmp(path, s_path) && s_gray && st.st_size == s_size && st.st_mtime == s_mtime &&
        s_width == w && s_height == h) return true;
    ui_reader_background_release(); // 仅一个选中背景，不保留两份图片。/ One selected wallpaper, never two image caches.
    const char *ext = strrchr(path, '.');
    const bool png = ext && !strcasecmp(ext, ".png");
    // 星瞳云壁纸为 XTH，和 JPG/PNG 一样可作为阅读背景。
    // Xteink cloud wallpapers are XTH and work as reader backgrounds like JPG/PNG.
    const bool xth = ext && !strcasecmp(ext, ".xth");
    unsigned source_w = 0, source_h = 0;
    if (!ext || (!png && !xth && strcasecmp(ext, ".jpg") && strcasecmp(ext, ".jpeg")) ||
        !(xth ? book_image_xth_dimensions(path, &source_w, &source_h)
              : book_image_file_dimensions(path, png, &source_w, &source_h)) || !source_w || !source_h) return false;
    const size_t pixels = (size_t)w * h, packed = (pixels + 1) / 2;
    const unsigned caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    // 按屏幕尺寸解码一次，再原位压缩为16灰阶，保留细线和原图层次。
    // Decode once at display resolution, then compact in place to native 16-gray pixels.
    // 临时8位平面最多831744B，常驻4位背景最多415872B。
    // The temporary byte plane is <=831744B; the sole resident wallpaper is <=415872B.
    if (heap_caps_get_free_size(caps) < pixels + 1024u * 1024u ||
        heap_caps_get_largest_free_block(caps) < pixels) return false;
    s_gray = heap_caps_malloc(pixels, caps);
    if (!s_gray || !(xth ? book_image_file_xth_grayscale(path, w, h, s_gray)
                         : book_image_file_grayscale(path, png, w, h, s_gray))) {
        ui_reader_background_release(); return false;
    }
    for (size_t i = 0; i < pixels; i += 2) {
        const uint8_t first = ui_image_dither_gray(s_gray[i], i % w, i / w);
        const uint8_t second = i + 1 < pixels
            ? ui_image_dither_gray(s_gray[i+1], (i+1) % w, (i+1) / w) >> 4 : 15;
        s_gray[i/2] = (first & 0xf0) | second;
    }
    uint8_t *compact = heap_caps_realloc(s_gray, packed, caps);
    if (!compact) { ui_reader_background_release(); return false; }
    s_gray = compact;
    s_width = w; s_height = h; s_size = st.st_size; s_mtime = st.st_mtime;
    strcpy(s_path, path); return true;
}

static uint8_t read_pixel(const uint8_t *fb, int x, int y) {
    int px = x, py = y;
    switch (epd_get_rotation()) {
        case EPD_ROT_PORTRAIT: px = epd_width() - y - 1; py = x; break;
        case EPD_ROT_INVERTED_LANDSCAPE: px = epd_width() - x - 1; py = epd_height() - y - 1; break;
        case EPD_ROT_INVERTED_PORTRAIT: px = y; py = epd_height() - x - 1; break;
        default: break;
    }
    return epd_get_pixel(px, py, epd_width(), epd_height(), fb);
}
void ui_reader_background_paint(uint8_t *fb, EpdRect area, bool blend) {
    ui_reader_background_paint_level(fb, area, blend, app_settings_background_strength());
}
static void paint_pixels(uint8_t *fb, EpdRect area, bool blend, unsigned strength) {
    for (int y = 0; y < area.height; ++y) for (int x = 0; x < area.width; ++x) {
        // 正文按真实屏幕坐标叠加，只有小预览缩小；不随边距重新缩放。
        // Reading uses exact screen pixels. Only the thumbnail preview is reduced.
        const unsigned sx = blend ? (unsigned)(area.x+x) : (uint64_t)x * s_width / area.width;
        const unsigned sy = blend ? (unsigned)(area.y+y) : (uint64_t)y * s_height / area.height;
        const size_t pixel = s_photo ? (sy*96/s_height)*144 + sx*144/s_width : (size_t)sy*s_width+sx;
        const uint8_t byte = (s_photo ? s_photo : s_gray)[pixel/2];
        unsigned gray = pixel & 1 ? byte & 15 : byte >> 4;
        if (strength < 2) gray = 15 - ((15-gray)*(strength ? 3 : 1)+2)/5;
        if (blend) {
            // 保留原图灰度并叠加正文，不强制洗白或制造字形白边。
            // Keep the source tones; composite black/antialiased ink without a white wash or halo.
            const unsigned ink = read_pixel(fb, area.x+x, area.y+y) >> 4;
            if (!ink) continue;
            gray = (gray * ink + 7) / 15;
        }
        epd_draw_pixel(area.x+x, area.y+y, gray << 4, fb);
    }
}

void ui_reader_background_paint_level(uint8_t *fb, EpdRect area, bool blend, unsigned strength) {
    if ((!s_photo && !s_gray) || !fb || area.x < 0 || area.y < 0 || area.width <= 0 || area.height <= 0 ||
        area.x + area.width > epd_rotated_display_width() || area.y + area.height > epd_rotated_display_height()) return;
    if (blend && (s_width != (unsigned)epd_rotated_display_width() ||
                  s_height != (unsigned)epd_rotated_display_height())) return;
    const bool whole_page = blend && area.x == 0 && area.y == 0 &&
        (unsigned)area.width == s_width && (unsigned)area.height == s_height;
    if (!whole_page || (epd_width() & 1)) {
        paint_pixels(fb, area, blend, strength);
        return;
    }
    const size_t bytes = (size_t)s_width * s_height / 2;
    if (!s_paper) {
        const unsigned caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        if (heap_caps_get_free_size(caps) >= bytes + 1024u * 1024u)
            s_paper = heap_caps_malloc(bytes, caps);
        if (!s_paper) {
            paint_pixels(fb, area, true, strength);
            return;
        }
    }
    const int rotation = epd_get_rotation();
    if (rotation != s_paper_rotation || strength != s_paper_strength) {
        memset(s_paper, 0xff, bytes);
        paint_pixels(s_paper, area, false, strength);
        s_paper_rotation = rotation;
        s_paper_strength = strength;
    }
    for (size_t i = 0; i < bytes; ++i) {
        const unsigned ink = fb[i];
        if (!ink) continue;
        const unsigned paper = s_paper[i];
        if (ink == 0xff) fb[i] = paper;
        else {
            unsigned low = ((ink & 15) * (paper & 15) + 7) / 15;
            unsigned high = ((ink >> 4) * (paper >> 4) + 7) / 15;
            fb[i] = (high << 4) | low;
        }
    }
}
