#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Run esptool after reconnecting the same Pico across native USB resets.

Requires esptool 5.6 and pyserial from the Pico ESP-IDF environment.
Only connection is retried. Once the command starts, failures are propagated.
"""
import argparse
import time


def connect(serial_number, ports, factory, retry_errors, timeout=30):
    deadline = time.monotonic() + timeout
    attempts = 0
    last_error = None
    while time.monotonic() < deadline:
        matches = [p for p in ports() if (p.vid, p.pid) == (0x303a, 0x1001)
                   and (p.serial_number or '').lower() == serial_number.lower()]
        if len(matches) > 1:
            raise RuntimeError('Multiple devices have the selected USB serial number')
        if matches:
            esp = None
            try:
                esp = factory(matches[0].device)
                # After a dropped reset, first synchronize without resetting again.
                mode = 'no-reset' if attempts % 2 else 'usb-reset'
                attempts += 1
                esp.connect(mode, 1)
                return esp
            except retry_errors as error:
                last_error = error
                if esp is not None:
                    esp._port.close()
                print(f'USB connection pending: {error}', flush=True)
        time.sleep(0.5)
    raise TimeoutError(f'Pico {serial_number} did not reconnect: {last_error}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial', required=True, help='Exact native USB serial number')
    parser.add_argument('command', nargs=argparse.REMAINDER,
                        help='esptool command and arguments, following --')
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('An esptool command is required')
    import esptool
    from esptool.targets.esp32s3 import ESP32S3ROM
    from serial.tools.list_ports import comports
    # macOS may report USB removal as termios.error instead of OSError.
    errors = (OSError, esptool.FatalError)
    try:
        import termios
        errors += (termios.error,)
    except ImportError:
        pass
    esp = connect(args.serial, comports, lambda port: ESP32S3ROM(port, 115200), errors)
    try:
        esptool.main(['--chip', 'esp32s3', '--baud', '460800',
                      '--before', 'no-reset', '--after', 'hard-reset', *command], esp=esp)
    finally:
        esp._port.close()


if __name__ == '__main__':
    main()
