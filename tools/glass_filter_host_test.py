#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""逐字节核对实际玻璃横向滤波。/ Byte-compare the actual horizontal glass filter."""
from pathlib import Path
import subprocess
import tempfile
import re
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define GAUSS_RADIUS 3
#define GAUSS_TAPS 7
'''
code += function('panel_row_h', root / 'main/ui/ui_glass.c')
production = (root / 'main/ui/ui_glass.c').read_text()
for name in ('s_gauss', 's_reader_gauss'):
    code += re.search(r'static const uint8_t ' + name + r'\[GAUSS_TAPS\]\s*=\s*\{[^}]+\};', production).group(0) + '\n'
code += r'''
static void reference(const uint8_t *source, int w, int row, uint8_t *out, const uint8_t *kernel) {
    for (int x = 0; x < w; ++x) {
        int sum = 0;
        for (int k = 0; k < 7; ++k) {
            int at = x + k - 3;
            if (at < 0) at = 0;
            if (at >= w) at = w - 1;
            sum += kernel[k] * source[row * w + at];
        }
        out[x] = sum >> 8;
    }
}
int main(void) {
    const uint8_t *kernels[2] = {s_gauss, s_reader_gauss};
    for (int k = 0; k < 2; ++k) for (int i = 0; i < 3; ++i)
        assert(kernels[k][i] == kernels[k][6-i]);
    uint8_t source[684*3], actual[684], expected[684];
    uint32_t random = 71;
    for (int pattern = 0; pattern < 4; ++pattern) {
        for (unsigned i = 0; i < sizeof(source); ++i) {
            random = random * 1664525u + 1013904223u;
            source[i] = pattern == 0 ? 0 : pattern == 1 ? 15 : pattern == 2 ? i % 16 : random >> 28;
        }
        for (int w = 1; w <= 684; ++w) for (int row = 0; row < 3; ++row) for (int k = 0; k < 2; ++k) {
            reference(source, w, row, expected, kernels[k]);
            panel_row_h(source, w, row, actual, kernels[k]);
            assert(!memcmp(actual, expected, w));
        }
    }
    puts("PASS: horizontal glass filter byte-identical, widths 1..684, two kernels, four patterns, three rows");
}
'''
with tempfile.TemporaryDirectory() as folder:
    source = Path(folder) / 'filter.c'
    executable = Path(folder) / 'filter'
    source.write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(source), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
