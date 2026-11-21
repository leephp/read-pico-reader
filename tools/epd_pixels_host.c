/**
 * @brief Host-only scalar reference for Pico Xtensa diff.S / lut.S
 */
#include <esp_attr.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

/**
 * @brief 将 to/from 两行 4bpp 交织为差分行，并更新列脏标记
 * @param to 目标帧一行（2 像素/字节）
 * @param from 当前帧一行（2 像素/字节）
 * @param interlaced 输出差分（1 像素/字节）
 * @param col_dirtyness 列脏字节（与 to 同行宽）
 * @param fb_width 像素宽度
 * @return 非 0 表示本行有差异
 */
static inline uint32_t expand_nibbles(uint32_t value) {
    value=(value|(value<<8))&UINT32_C(0x00ff00ff);
    return (value|(value<<4))&UINT32_C(0x0f0f0f0f);
}
__attribute__((optimize("O3"))) uint32_t IRAM_ATTR epd_interlace_4bpp_line_c(
    const uint8_t* to,
    const uint8_t* from,
    uint8_t* interlaced,
    uint8_t* col_dirtyness,
    int fb_width
) {
    uint32_t dirty = 0;
    if (to == NULL || from == NULL || interlaced == NULL || col_dirtyness == NULL || fb_width <= 0) {
        return 0;
    }
    // 两像素一字节：成对处理，少一次 x/2、x%2
    const int pairs = fb_width >> 1;
    int p=0;
    // 小端 32 位并行交织八像素；输出/脏标记逐字节等价，减少前台差分等待。
    // Interlace eight pixels per little-endian word; output/dirty masks remain byte-exact.
    for(;p+4<=pairs;p+=4) {
        uint32_t target,previous,columns;
        memcpy(&target,to+p,4);memcpy(&previous,from+p,4);
        uint32_t changed=target^previous;dirty|=changed;
        memcpy(&columns,col_dirtyness+p,4);columns|=changed;memcpy(col_dirtyness+p,&columns,4);
        uint32_t low=(expand_nibbles(target&0xffffu)<<4)|expand_nibbles(previous&0xffffu);
        uint32_t high=(expand_nibbles(target>>16)<<4)|expand_nibbles(previous>>16);
        memcpy(interlaced+2*p,&low,4);memcpy(interlaced+2*p+4,&high,4);
    }
    for (; p < pairs; p++) {
        const uint8_t tb = to[p];
        const uint8_t fb = from[p];
        const uint8_t t0 = (uint8_t)(tb & 0x0f);
        const uint8_t t1 = (uint8_t)(tb >> 4);
        const uint8_t f0 = (uint8_t)(fb & 0x0f);
        const uint8_t f1 = (uint8_t)(fb >> 4);
        const uint8_t d0 = (uint8_t)(t0 ^ f0);
        const uint8_t d1 = (uint8_t)(t1 ^ f1);
        col_dirtyness[p] |= (uint8_t)(d0 | (d1 << 4));
        dirty |= (uint32_t)(d0 | d1);
        interlaced[p * 2] = (uint8_t)((t0 << 4) | f0);
        interlaced[p * 2 + 1] = (uint8_t)((t1 << 4) | f1);
    }
    if (fb_width & 1) {
        const int x = fb_width - 1;
        uint8_t t = to[x / 2];
        uint8_t f = from[x / 2];
        t = (uint8_t)((x % 2) ? (t >> 4) : (t & 0x0f));
        f = (uint8_t)((x % 2) ? (f >> 4) : (f & 0x0f));
        col_dirtyness[x / 2] |= (uint8_t)((t ^ f) << (4 * (x % 2)));
        dirty |= (uint32_t)(t ^ f);
        interlaced[x] = (uint8_t)((t << 4) | f);
    }
    return dirty;
}

__attribute__((optimize("O3"))) void IRAM_ATTR
epd_apply_line_mask_c(uint8_t* line, const uint8_t* mask, int mask_len) {
    if (line == NULL || mask == NULL || mask_len <= 0) {
        return;
    }
    // 非对齐调用用逐字节回退；生产队列和mask均16B对齐。
    // Unaligned callers fall back bytewise; production queues/masks align to16B.
    if(((uintptr_t)line|(uintptr_t)mask)&3u) {
        for(int i=0;i<mask_len;++i)line[i]&=mask[i];
        return;
    }
    uint32_t* l32 = (uint32_t*)line;
    const uint32_t* m32 = (const uint32_t*)mask;
    const int words = mask_len >> 2;
    int i = 0;
    for (; i + 4 <= words; i += 4) {
        l32[i] &= m32[i];
        l32[i + 1] &= m32[i + 1];
        l32[i + 2] &= m32[i + 2];
        l32[i + 3] &= m32[i + 3];
    }
    for (; i < words; i++) {
        l32[i] &= m32[i];
    }
    for (i = words << 2; i < mask_len; i++) {
        line[i] &= mask[i];
    }
}

__attribute__((optimize("O3"))) uint32_t IRAM_ATTR epd_interlace_4bpp_line_VE(
    const uint8_t* to,const uint8_t* from,uint8_t* output,uint8_t* columns,int pixels
) {
    return epd_interlace_4bpp_line_c(to,from,output,columns,pixels);
}

__attribute__((optimize("O3"))) void IRAM_ATTR epd_apply_line_mask_VE(uint8_t* line,const uint8_t* mask,int length) {
    epd_apply_line_mask_c(line,mask,length);
}

/**
 * @brief 1ppB / 1K VE 格式 LUT 查表（与 lut.c lookup_pixels_in_VE_LUT 一致）
 * @note S31 与 S3 字节内 4px 高低位对调：板级/LCD 沿 X 先吃高位 2bit，非 CPU 端序差异。
 *       VE LUT 每项占 4 字节，值在 LE 低字节；按字节取比整字 load 更省带宽。
 */
__attribute__((optimize("O3"))) void IRAM_ATTR
calc_epd_input_1ppB_1k_S3_VE_aligned(
    const uint32_t* ld, uint8_t* epd_input, const uint8_t* conversion_lut, uint32_t epd_width
) {
    if (ld == NULL || epd_input == NULL || conversion_lut == NULL || epd_width < 4) {
        return;
    }

    const uint8_t* lut = conversion_lut;
    const uint32_t n = epd_width >> 2; // 输出字节数 = 像素/4
    uint32_t j = 0;


#define LUT4(in)                                                                                   \
    ((uint8_t)(lut[((in) >> 24) << 2] << 6 | lut[(((in) >> 16) & 0xFFu) << 2] << 4                 \
               | lut[(((in) >> 8) & 0xFFu) << 2] << 2 | lut[((in) & 0xFFu) << 2]))

    for (; j + 4 <= n; j += 4) {
        const uint32_t in0 = ld[j];
        const uint32_t in1 = ld[j + 1];
        const uint32_t in2 = ld[j + 2];
        const uint32_t in3 = ld[j + 3];
        epd_input[j] = LUT4(in0);
        epd_input[j + 1] = LUT4(in1);
        epd_input[j + 2] = LUT4(in2);
        epd_input[j + 3] = LUT4(in3);
    }
    for (; j < n; j++) {
        epd_input[j] = LUT4(ld[j]);
    }
#undef LUT4
}
