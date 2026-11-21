#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：从唯一源码生成隔离的设备构建/测试视图，不修改维护源码。/ English: Generate isolated device build/test views from the one maintained source tree."""
from pathlib import Path
import json

BOARDS = ('pico',)
BOARD_ALIASES = {}

def canonical_board(board):
    return BOARD_ALIASES.get(board, board)



def select_source(text, board):
    board = canonical_board(board)
    if board not in BOARDS:
        raise ValueError('Unknown board: ' + board)
    return text


def materialize(root, destination, board, tests=False):
    board = canonical_board(board)
    root, destination = Path(root).resolve(), Path(destination).resolve()
    if board not in BOARDS or root == destination or root.is_relative_to(destination):
        raise ValueError('Use a separate destination for Pico')
    if destination.is_relative_to(root) and destination.relative_to(root).parts[0] in (
            'main', 'components', 'boards', 'cmake', 'tools', 'docs'):
        raise ValueError('Do not generate build views inside maintained source directories')
    destination.mkdir(parents=True, exist_ok=True)
    marker = destination / '.kiiko-source-view.json'
    previous = json.loads(marker.read_text(encoding='utf-8')) if marker.exists() else {}
    if previous and (previous['source'] != str(root) or previous['board'] != board):
        raise ValueError('Use separate source views for different boards/source trees')
    generated = set()

    def write(relative, data):
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        # 内容变化时使用当前mtime，避免copy2保留旧时间造成Ninja漏编。
        # Changed contents need a fresh mtime so Ninja cannot miss an older restored source.
        if not target.is_file() or target.read_bytes() != data:
            target.write_bytes(data)
        generated.add(str(relative))

    ignored = {'.git', 'build', '__pycache__', 'managed_components', 'build-s31-native', '.venv'}
    for top in root.iterdir():
        if top.name in ignored or (top.name == 'flash' and not tests) or top.name.startswith('.'):
            continue
        if top.is_dir() and destination.is_relative_to(top):
            continue
        if top.name == 'tools' and not tests:
            continue
        paths = top.rglob('*') if top.is_dir() else [top]
        for path in paths:
            relative = path.relative_to(root)
            if not path.is_file() or any(part in ignored for part in relative.parts):
                continue
            # 本机计划/调试资料不入设备视图：既不是源码，也会与并行写入产生竞争。
            # Keep local plans/debug material out of generated views: not source, and parallel
            # writers would race the copy.
            if relative.parts[:2] == ('docs', 'local'):
                continue
            write(relative, path.read_bytes())
    write(Path('dependencies.lock'), (root / 'boards/pico/dependencies.lock').read_bytes())
    for relative in set(previous.get('files', [])) - generated:
        stale = destination / relative
        if not stale.resolve().is_relative_to(destination):
            raise ValueError('Invalid generated-source inventory')
        stale.unlink(missing_ok=True)
    marker.write_text(json.dumps({'source': str(root), 'board': board,
                                  'files': sorted(generated)}, indent=2) + '\n', encoding='utf-8')
    return destination


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=(*BOARDS, *BOARD_ALIASES), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tests', action='store_true')
    args = parser.parse_args()
    materialize(Path(__file__).resolve().parents[1], args.output, args.board, args.tests)
