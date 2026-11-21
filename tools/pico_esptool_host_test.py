#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""USB reset loss: rediscover by identity, close stale handles, bound retries."""
from types import SimpleNamespace as NS
from unittest.mock import patch
import pico_esptool as target


def port(path, serial='pico', vid=0x303a):
    return NS(device=path, serial_number=serial, vid=vid, pid=0x1001)


clock = [0.0]
opened, modes, closed = [], [], []


def sleep(seconds):
    clock[0] += seconds


def factory(path):
    opened.append(path)
    def sync(mode, attempts):
        modes.append(mode)
        assert attempts == 1
        if path == 'old':
            raise OSError('Device not configured')
    return NS(connect=sync, _port=NS(close=lambda: closed.append(path)))


with patch.object(target.time, 'monotonic', lambda: clock[0]), \
        patch.object(target.time, 'sleep', sleep):
    scans = iter([[port('other', 'other')], [port('old')], [],
                  [port('wrong', vid=1), port('new')]])
    esp = target.connect('PICO', lambda: next(scans), factory, (OSError,))
    assert opened == ['old', 'new'] and modes == ['usb-reset', 'no-reset']
    assert closed == ['old']
    esp._port.close()
    before = len(opened)
    try:
        target.connect('pico', lambda: [port('a'), port('b')], factory, (OSError,))
        raise AssertionError('Ambiguous identity accepted')
    except RuntimeError:
        pass
    assert len(opened) == before
    try:
        target.connect('pico', lambda: [port('wrong', 'other')], factory, (OSError,), 2)
        raise AssertionError('Missing device accepted')
    except TimeoutError:
        pass
    assert len(opened) == before
    try:
        target.connect('pico', lambda: [port('old')], factory, (OSError,), 2)
        raise AssertionError('Failed connection accepted')
    except TimeoutError:
        pass
    assert modes[-4:] == ['usb-reset', 'no-reset', 'usb-reset', 'no-reset']
    assert len(closed) == len(opened)

# A flash error must escape, not restart or write to another device.
calls = []
def failed_command(args, esp):
    calls.append(args)
    raise OSError('write failed')
modules = {'esptool': NS(main=failed_command, FatalError=RuntimeError),
           'esptool.targets.esp32s3': NS(ESP32S3ROM=object),
           'serial.tools.list_ports': NS(comports=lambda: [])}
with patch.dict('sys.modules', modules), \
        patch('sys.argv', ['pico_esptool.py', '--serial', 'pico', '--', 'write-flash', '0x10000', 'app.bin']), \
        patch.object(target, 'connect', return_value=NS(_port=NS(close=lambda: closed.append('command')))) as connect:
    try:
        target.main()
        raise AssertionError('Write failure swallowed')
    except OSError:
        pass
    assert connect.call_count == 1 and len(calls) == 1 and closed[-1] == 'command'
print('PASS: USB reconnect/renumber, exact identity, timeout, handle cleanup, no flash retry')
