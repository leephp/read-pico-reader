#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""运行设置写入故障测试。/ Run settings persistence fault injection."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as folder:
    executable = str(Path(folder) / 'settings')
    gc = '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-ffunction-sections', '-fdata-sections', gc,
                    '-Itools/settings_backup_stubs', '-Icomponents/read_pico_transfer/include',
                    'tools/settings_persistence_host_test.c', '-o', executable], cwd=root, check=True)
    subprocess.run([executable], check=True)
