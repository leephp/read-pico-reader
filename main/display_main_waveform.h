/* SPDX-License-Identifier: Apache-2.0
 * 中文：主页面灰阶保持未变像素；书架退出加强同向擦白，主页水波纹端点单向推动，中灰保持原序列。
 * English: Main-page grays hold unchanged pixels; shelf exit strengthens directional whitening and main water uses directional endpoints while retaining original midgray sequences.
 */
#pragma once
#include "epdiy.h"
const EpdWaveform* display_main_gray_waveform(void);
/// 离开书架单向变白并补四相，避免GC16反向压黑；其他迁移用原DU，等值保持。/ Shelf exit whitens directionally with four extra phases, avoiding GC16 reverse darkening; other transitions use original DU and equal values hold.
const EpdWaveform* display_main_shelf_exit_waveform(void);
/// 水波纹与普通切页共用：端点单向驱动，中灰变色保留完整序列，等灰保持；调用者决定是否错相。
/// Shared by water and ordinary transitions: directional endpoints, complete changed midgray sequences and held equal grays; callers choose staggering.
const EpdWaveform* display_main_water_waveform(void);
/// 快刷首帧保留原DU并保持所有等值像素，亚克力首帧不驱动。/ Fast first pass retains original DU while holding every equal pair, including skipped acrylic.
const EpdWaveform* display_main_bw_waveform(void);
/// Pico列表滑动专用12相：保留FOLLOW8黑推，变白补四相，所有等值保持；仅列表变化像素输出。
/// Pico list-only12 phases retain FOLLOW8 black drives, add four changed-white drives and hold all equals; changed list pixels only.
extern const EpdWaveform PICO_SETTINGS_MOTION_WAVEFORM;
const EpdWaveform* display_settings_motion_waveform(void);

/// 普通模式灰边交错，端点/相数/推动数量不变；OOM回退原柔和灰阶。
/// Ordinary gray-edge interleaving keeps endpoints/phase counts/pulse counts; OOM falls back to original soft grays.
const EpdWaveform* display_main_normal_waveform(void);

/// 板级根页清残影：保留原动作并补变白及释放，分配失败回退原表；各板使用各自相位预算。
/// Board root cleanup retains original actions and adds white/release phases within each board's own budget, with safe allocation fallback.
const EpdWaveform* display_main_clean_waveform(const EpdWaveform* source, enum EpdDrawMode mode);

/// 板级根页按来源/目标选择六或八相变白；缓存区分力度且不叠加，分配失败保留原表。
/// Board roots select six/eight changed-white phases; strength-keyed bounded caches never stack; OOM retains the source.
const EpdWaveform* display_main_clean_waveform_with_phases(
    const EpdWaveform* source, enum EpdDrawMode mode, unsigned white_phases
);

#define PICO_MAIN_FAST_WHITE_PHASES 10u
/// PICO快刷切页为所有目标白补十相，保留原迁移及定稿。/ Add ten white drives to all white targets on Pico fast switches, retaining native actions and settling.
const EpdWaveform* display_main_fast_white_waveform(const EpdWaveform* source, enum EpdDrawMode mode);

/// 书架封面/材质保留校准灰阶，白色留白只向白推动；只供普通切页，手动全刷保留原表。
/// Shelf art keeps calibrated grays while white space drives only white; ordinary transitions only, preserving vendor manual full refresh.
const EpdWaveform* display_main_shelf_waveform(enum EpdDrawMode mode);

/// 设置滚动用各板原生灰阶加强擦白：S31为48+8+2，Pico为37+8；保留字缘和头像灰阶。
/// Board-owned list cleanup retains glyph/avatar grays: S31 uses48+8+2, Pico uses37+8.
const EpdWaveform* display_settings_scroll_waveform(void);
/// 系统底色恢复原完整柔和灰阶，白底同向擦除；底栏由调用方裁剪保护，不改原始波形。
/// Restore complete original soft system grays with directional white erasure; callers crop navigation and vendor tables remain intact.
const EpdWaveform* display_main_ui_waveform(void);
