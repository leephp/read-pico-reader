#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""构建时无损压缩网页，不携带时间戳/文件名。/ Build lossless web gzip without timestamps or filenames."""
import gzip
from pathlib import Path
import sys


def compress(source: Path, target: Path) -> None:
    original = source.read_bytes()
    data = gzip.compress(original, compresslevel=9, mtime=0)
    assert gzip.decompress(data) == original
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


if __name__ == '__main__':
    compress(Path(sys.argv[1]), Path(sys.argv[2]))
