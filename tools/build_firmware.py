#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Build one maintained source tree for Pico or Metalio 4.7; never flash or publish."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import shutil
import struct
import subprocess
import sys
import zipfile
from board_source_view import BOARDS, BOARD_ALIASES, canonical_board, materialize

ROOT = Path(__file__).resolve().parents[1]


def metadata(board):
    board = canonical_board(board)
    if board not in BOARDS:
        raise ValueError('Unknown board: ' + board)
    value = json.loads((ROOT / 'boards' / board / 'board.json').read_text(encoding='utf-8'))
    if value['id'] != board or value['idf_target'] != 'esp32s3':
        raise ValueError('Invalid board metadata')
    return value


def partition_entries(data):
    result = {}
    for offset in range(0, len(data) - 31, 32):
        magic, kind, subtype, address, size, label, flags = struct.unpack_from('<HBBII16sI', data, offset)
        if magic != 0x50aa:
            break
        result[label.split(b'\0')[0].decode()] = (kind, subtype, address, size)
    return result


def check_image(data, board):
    info = metadata(board)
    if len(data) < 288 or data[0] != 0xe9 or struct.unpack_from('<I', data, 32)[0] != 0xabcd5432:
        raise ValueError('Invalid ESP app image')
    field = lambda start: data[start:start + 32].split(b'\0')[0].decode('ascii')
    chip = struct.unpack_from('<H', data, 12)[0]
    if chip != info['image_chip_id'] or field(80) != info['project'] or field(48) != info['version']:
        raise ValueError('Image chip/project/version does not match selected board')
    return {'chip_id': chip, 'project': field(80), 'version': field(48), 'size': len(data),
            'sha256': hashlib.sha256(data).hexdigest()}


def export(build, output, board):
    info = metadata(board)
    app = build / (info['project'] + '.bin')
    identity = check_image(app.read_bytes(), board)
    entries = partition_entries((build / 'partition_table/partition-table.bin').read_bytes())
    csv = (ROOT / 'boards' / board / info['partitions']).read_text(encoding='utf-8')
    expected = {}
    for line in csv.splitlines():
        fields = [value.strip() for value in line.split(',')]
        if not fields or not fields[0] or fields[0].startswith('#'):
            continue
        expected[fields[0]] = (int(fields[3], 0), int(fields[4], 0))
    if set(entries) != set(expected) or any(tuple(entries[name][2:]) != shape for name, shape in expected.items()):
        raise ValueError('Built partition table does not match selected board')
    if identity['size'] > entries['ota_0'][3] or identity['size'] > entries['ota_1'][3]:
        raise ValueError('Application does not fit both OTA slots')
    output.mkdir(parents=True, exist_ok=True)
    # 从本次SDK构建结果读取地址，不得手写地址。
    # Read this build's SDK flash plan: never guess flash offsets.
    plan = json.loads((build / 'flasher_args.json').read_text(encoding='utf-8'))
    loader = plan['extra_esptool_args']
    if loader['chip'] != info['idf_target']:
        raise ValueError('Flash plan uses the wrong chip')
    expected_offsets = {'bootloader': info['bootloader_offset'], 'partition-table': 0x8000,
                        'app': entries['ota_0'][2], 'otadata': entries['otadata'][2]}
    parts = []
    for key, name in [('bootloader', 'bootloader.bin'), ('partition-table', 'partitions.bin'),
                      ('app', 'firmware.bin'), ('otadata', 'ota_data_initial.bin')]:
        part = plan[key]
        offset = int(part['offset'], 0)
        source = (build / part['file']).resolve()
        if (offset != expected_offsets[key] or not source.is_relative_to(build.resolve()) or
                plan['flash_files'].get(part['offset']) != part['file']):
            raise ValueError('Flash plan does not match the board partition layout')
        if key == 'bootloader' and (source.read_bytes()[0] != 0xe9 or
                struct.unpack_from('<H', source.read_bytes(), 12)[0] != info['image_chip_id']):
            raise ValueError('Bootloader chip does not match the selected board')
        parts.append((name, source, offset))
    cache = build / 'CMakeCache.txt'
    experimental_main_clean = cache.exists() and 'PICO_MAIN_CLEAN_EXPERIMENT:BOOL=ON' in cache.read_text(encoding='utf-8')
    manifest = {'name': info['name'], 'version': identity['version'], 'board': board,
              'experimental_main_clean': bool(experimental_main_clean),
                'project': identity['project'], 'new_install_prompt_erase': False,
                'builds': [{'chipFamily': 'ESP32-S3', 'parts': []}]}
    checksums, args = [], [shlex.join(plan['write_flash_args'])]
    flash_pairs = []
    for name, source, offset in parts:
        shutil.copy2(source, output / name)
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        manifest['builds'][0]['parts'].append({'path': name, 'offset': offset,
                                             'size': source.stat().st_size, 'sha256': digest})
        checksums.append(f'{digest}  {name}\n')
        args.append(f'{offset:#x} {name}')
        flash_pairs.extend([f'{offset:#x}', name])
    shutil.copy2(app, output / f"kiikoread-{board}-{identity['version']}.bin")
    (output / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n',
                                          encoding='utf-8')
    (output / 'flash_args').write_text('\n'.join(args) + '\n', encoding='utf-8')
    command = ['python', '-m', 'esptool', '--chip', loader['chip'], '--port', 'PORT',
               '--before', loader['before'], '--after', loader['after']]
    if not loader['stub']:
        command.append('--no-stub')
    # 中文：esptool v5 把 flash 选项挪到 write-flash 子命令，且不再解析 @flash_args，
    # 所以给出的命令必须把选项和地址写全；flash_args 仍保留给认它的旧工具。
    # English: esptool v5 moved the flash options onto the write-flash subcommand and no longer
    # expands @flash_args, so the ready-to-run command spells out options and addresses; the
    # flash_args file stays for older tools that understand it.
    command.append('write-flash')
    command.extend(plan['write_flash_args'])
    command.extend(flash_pairs)
    (output / 'flash-command.txt').write_text(shlex.join(command) + '\n', encoding='utf-8')
    (output / 'SHA256SUMS.txt').write_text(''.join(checksums), encoding='utf-8')
    (output / '使用说明.txt').write_text(
        f"本地测试固件 / {info['name']} / {info['idf_target']} / {identity['version']}\n"
        "只用于对应设备，不可跨板型刷写。应用Bin仅用于已有正确分区的升级，不可写到地址0。\n"
        "完整初装命令见flash-command.txt（esptool v5 可用），旧工具可用flash_args。\n"
        "保留数据时不要全片擦除。当前工具只编译，不会连接或刷写设备。\n",
        encoding='utf-8')
    result = {'board': board, 'target': info['idf_target'], **identity,
              'partitions': entries, 'bootloader_offset': expected_offsets['bootloader'],
              'esptool_stub': loader['stub'], 'free_app_bytes': entries['ota_0'][3] - identity['size'],
              'experimental_main_clean': bool(experimental_main_clean),
              'built': True, 'device_flashed': False, 'published': False}
    (output / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    with zipfile.ZipFile(output.parent / f"kiikoread-{board}-{identity['version']}-USB.zip", 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(output.iterdir()):
            if path.is_file():
                archive.write(path, path.name)
    return result


def build_one(board, args):
    info = metadata(board)
    sdk_argument = getattr(args, 'idf_' + board)
    sdk = Path(sdk_argument or os.environ.get('KIIKO_IDF_' + board.upper(), '') or os.environ.get('IDF_PATH', '')).resolve()
    if not (sdk / 'tools/idf.py').is_file() or not (sdk / 'components/soc' / info['idf_target']).is_dir():
        raise ValueError(f"{board} requires an ESP-IDF SDK supporting {info['idf_target']}: {sdk}")
    work = args.output.resolve() / board
    source, build = work / 'source', work / 'build'
    # 虚拟环境解释器常为符号链接；保留入口路径，不能解析成系统Python。
    # Venv executables are often symlinks; retain that entry path instead of resolving to system Python.
    python = args.python.absolute()
    command = [str(python), str(sdk / 'tools/idf.py'), '-B', str(build),
               '-DIDF_TARGET=' + info['idf_target'], '-DKIIKO_BOARD=' + board,
               '-DSDKCONFIG=' + str(work / 'sdkconfig'),
               '-DSDKCONFIG_DEFAULTS=' + str(source / 'boards' / board / info['sdkconfig']),
               '-DCCACHE_ENABLE=OFF',
               '-DPICO_MAIN_CLEAN_EXPERIMENT=' + ('ON' if args.pico_main_clean else 'OFF'),
               'reconfigure', 'build']
    if args.dry_run:
        print(json.dumps({'board': board, 'sdk': str(sdk), 'command': command}, ensure_ascii=False))
        return None
    # Retain the published HTTPS trust behavior on Pico, including real certificate dates.
    from prepare_tls_bundle import prepare
    prepare(sdk)
    materialize(ROOT, source, board)
    path = work / 'sdkconfig'
    if path.exists():
        key = 'CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_CROSS_SIGNED_VERIFY'
        lines = [line for line in path.read_text().splitlines()
                 if not line.startswith(key + '=') and line != '# ' + key + ' is not set']
        path.write_text('\n'.join(lines + [key + '=y']) + '\n')
    cache = getattr(args, 'managed_' + board)
    if cache:
        shutil.copytree(cache, source / 'managed_components', dirs_exist_ok=True)
    env = dict(os.environ)
    env.update(IDF_PATH=str(sdk), IDF_TARGET=info['idf_target'], KIIKO_BOARD=board,
               KIIKO_PROJECT_VER=info['version'], IDF_PYTHON_ENV_PATH=str(python.parent.parent),
               IDF_PYTHON_CHECK_CONSTRAINTS='no', IDF_SKIP_CHECK_SUBMODULES='1',
               IDF_CCACHE_ENABLE='0', IDF_VER='v6.1', ESP_IDF_VERSION='6.1.0')
    for key in ('PICO_PROJECT_VER', 'KIIKO_LOCAL_LATENCY'):
        env.pop(key, None)
    extra = [str(python.parent)]
    if args.tools:
        tools = args.tools.resolve()
        env['IDF_TOOLS_PATH'] = str(tools)
        architecture = 'xtensa-esp-elf'
        extra.extend(str(path) for path in (tools / 'tools' / architecture).glob('**/bin') if path.is_dir())
        extra.extend(str(path) for path in (tools / 'tools/esp32ulp-elf').glob('**/bin') if path.is_dir())
    env['PATH'] = os.pathsep.join(extra + [env.get('PATH', '')])
    with (work / 'build.log').open('w', encoding='utf-8', errors='replace') as log:
        result = subprocess.run(command, cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((work / 'build.log').read_text(errors='replace')[-8000:])
        raise RuntimeError(f'{board} build failed; see {work / "build.log"}')
    result = export(build, args.output.resolve() / 'packages' / board, board)
    print(json.dumps(result, ensure_ascii=False))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--board', choices=BOARDS, default='pico')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/boards')
    parser.add_argument('--python', type=Path, default=Path(sys.executable))
    parser.add_argument('--tools', type=Path)
    for board in BOARDS:
        parser.add_argument('--idf-' + board, dest='idf_' + board)
        parser.add_argument('--managed-' + board, dest='managed_' + board, type=Path)
    parser.add_argument('--dry-run', action='store_true')
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--pico-main-clean', action='store_true', help='Local Pico root-switch erasure using its own LUTs within40 phases')
    args = parser.parse_args()
    args.board = canonical_board(args.board)
    boards = (args.board,)
    if args.list:
        print(json.dumps([metadata(board) for board in boards], ensure_ascii=False, indent=2))
        return
    for board in boards:
        build_one(board, args)


if __name__ == '__main__':
    main()
