#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""检查实际网页响应与 gzip 字节。/ Check the real page handler and gzip response bytes."""
import gzip
from pathlib import Path
import re
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'components/read_pico_transfer'))
from gzip_asset import compress
source = ROOT / 'components/read_pico_transfer/upload.html'
text = (ROOT / 'components/read_pico_transfer/read_pico_transfer.c').read_text()
handler = re.search(r'static esp_err_t index_handler\([^}]+\}', text).group()
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    asset = folder / 'upload.html.gz'
    compress(source, asset)
    first = asset.read_bytes()
    compress(source, asset)
    assert first == asset.read_bytes(), 'non-reproducible gzip'
    assert gzip.decompress(first) == source.read_bytes()
    assert len(first) < source.stat().st_size
    # 使用真实 handler 捕获类型、编码和完整长度，防止错误减去结尾字节。
    # Capture the real handler's type, encoding and full length, catching accidental byte truncation.
    code = r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
typedef struct {int type, encoding;} httpd_req_t;
static const unsigned char data[] = {PAYLOAD};
static const char *upload_start = (const char *)data;
static const char *upload_end = (const char *)data + sizeof(data);
static int httpd_resp_set_type(httpd_req_t *req, const char *value) {
    assert(!strcmp(value, "text/html; charset=utf-8")); req->type = 1; return 0;
}
static int httpd_resp_set_hdr(httpd_req_t *req, const char *name, const char *value) {
    assert(!strcmp(name, "Content-Encoding") && !strcmp(value, "gzip")); req->encoding = 1; return 0;
}
static int httpd_resp_send(httpd_req_t *req, const char *body, ptrdiff_t count) {
    assert(req->type && req->encoding && count == sizeof(data));
    assert(fwrite(body, 1, count, stdout) == (size_t)count);
    return 17;
}
HANDLER
int main(void) { httpd_req_t req = {0}; assert(index_handler(&req) == 17); }
'''.replace('PAYLOAD', ','.join(str(v) for v in first)).replace('HANDLER', handler)
    (folder / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(folder / 'test.c'), '-o', str(folder / 'test')], check=True)
    assert gzip.decompress(subprocess.check_output([str(folder / 'test')])) == source.read_bytes()
    print(f'PASS: actual HTTP handler, gzip round-trip and reproducible asset: {source.stat().st_size} -> {len(first)} bytes')
