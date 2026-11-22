#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 kiikoread
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""检查 SVG 线段坐标与固件成品掩模。/ Check SVG line coordinates and firmware masks."""
import importlib.util
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("icon_generator", ROOT / "tools/gen_ui_icons.py")
icons = importlib.util.module_from_spec(spec)
spec.loader.exec_module(icons)

# 原 SVG 的 x1/y1 等含数字属性必须保留，不能退化成左上角的点。
# Preserve numeric attribute names such as x1/y1 instead of drawing a corner dot.
attrs = icons._attributes('<line x1="12" x2="12" y1="7" y2="13" />')
assert attrs == {"x1": "12", "x2": "12", "y1": "7", "y2": "13"}, attrs
assert icons.element_subpaths("line", attrs) == [([(12.0, 7.0), (12.0, 13.0)], False)]
empty = icons.pack_4bpp(icons.render_icon("bookmark"))
saved = icons.pack_4bpp(icons.render_icon("animate-bookmark-plus"))

def alpha(mask, x, y):
    value = mask[(y * icons.MASK_SIZE + x) // 2]
    return (value >> (0 if x & 1 else 4)) & 15

for x, y in [(36, 30), (30, 30), (42, 30), (36, 24), (36, 36)]:
    assert alpha(empty, x, y) == 0, (x, y)
    assert alpha(saved, x, y) >= 14, (x, y)
assert alpha(saved, 0, 0) == 0
assert alpha(saved, 36, 9) >= 14  # 顶边保留。/ Retain the outer top edge.

# 检查实际编译头文件，防止只改 SVG 却忘记更新成品。
# Inspect the compiled header too, catching stale generated artwork.
header = icons.HEADER.read_text()
expected = b''.join(bytes(icons.pack_4bpp(icons.render_icon(name))) for name in icons.ICONS)
# 实际 C 解码器须与原始掩模逐字节一致；故障输入不得越界。
# The actual C decoder must reproduce every mask byte; malformed inputs must remain bounded.
code = r'''
#include <assert.h>
#include <stdio.h>
#include "main/assets/ui_icons.h"
int main(void) {
    uint8_t out[UI_ICON_MASK_BYTES];
    for (int i = 0; i < UI_ICON_COUNT; ++i) {
        assert(ui_icon_decode((ui_icon_t)i, out));
        assert(fwrite(out, 1, sizeof(out), stdout) == sizeof(out));
    }
    assert(!ui_icon_decode((ui_icon_t)-1, out));
    assert(!ui_icon_decode(UI_ICON_COUNT, out));
    const uint8_t repeat[] = {255, 0x5a}, literal[] = {1, 0x12, 0x34};
    assert(ui_icon_rle_decode(repeat, sizeof(repeat), out, 128));
    for (int i = 0; i < 128; ++i) assert(out[i] == 0x5a);
    assert(ui_icon_rle_decode(literal, sizeof(literal), out, 2));
    assert(out[0] == 0x12 && out[1] == 0x34);
    assert(!ui_icon_rle_decode(repeat, 1, out, 128));
    assert(!ui_icon_rle_decode(repeat, 2, out, 127));
    assert(!ui_icon_rle_decode(repeat, 2, out, 129));
    assert(!ui_icon_rle_decode(literal, 2, out, 2));
    assert(!ui_icon_rle_decode(NULL, 0, out, 0));
    assert(!ui_icon_rle_decode(literal, 3, NULL, 2));
    return 0;
}
'''
with tempfile.TemporaryDirectory() as folder:
    source = Path(folder) / 'icons.c'
    executable = Path(folder) / 'icons'
    source.write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I', str(ROOT), str(source), '-o', str(executable)], check=True)
    assert subprocess.check_output([str(executable)]) == expected
    original_header = icons.HEADER
    icons.HEADER = Path(folder) / 'generated.h'
    icons.emit_header({name: icons.pack_4bpp(icons.render_icon(name)) for name in icons.ICONS})
    assert icons.HEADER.read_text() == header, 'stale compressed header'
    icons.HEADER = original_header
print(f"PASS: SVG coordinates, {len(icons.ICONS)} byte-identical decoded masks, bounded C decoder, reproducible header")
