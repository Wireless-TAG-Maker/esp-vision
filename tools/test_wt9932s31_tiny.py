#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise WT9932S31-TINY hardware through EV-MUX (requires pyserial).

This interrupts the current script/preview and leaves the board at the REPL.
It does not change main.py or use a display. Temporary files use random names
and create-only writes; only this run's files are removed. Wi-Fi is scanned
without connecting or printing SSIDs. --require-sd makes an unmounted SD fail.
The SD check verifies duplicate-driver rejection before creating its test file.
The PSRAM check probes a 2 MiB allocation with distinct 4 KiB page patterns;
it does not measure the installed PSRAM capacity. PSRAM heap traversal is checked
before/after each test and after each camera shutdown. RGB writes and the current
button level are exercised, but LED appearance/button presses need observation.
"""

import argparse
import base64
import json
import sys
import time
import uuid

from test_control_transport import Transport, read_rpc, read_until_frame


EXPECTED = {'identity', 'camera', 'psram', 'flash_rw', 'sdcard_rw', 'wifi_scan', 'gpio', 'cleanup'}

SOURCE = '''\
import gc, os, time, json, vfs
import sensor, network, machine, esp32

TOKEN = '__TOKEN__'
PREFIX = 'WT_S31_' + TOKEN + ' '
owned = []
globals()['_wt_owned_' + TOKEN] = owned
failures = 0

def heap_scan(stage):
    print(PREFIX + 'STEP heap ' + stage)
    regions = esp32.idf_heap_info(1 << 10)
    assert regions, 'no PSRAM heap regions'
    for region in regions:
        assert len(region) == 4, 'invalid PSRAM heap region'
        total, free, largest, minimum = region
        assert total > 0 and 0 <= minimum <= free <= total, 'invalid PSRAM heap sizes'
        assert 0 <= largest <= free, 'invalid PSRAM largest block'

def check(name, fn):
    global failures
    print(PREFIX + 'BEGIN ' + name)
    errors = []
    detail = None
    try:
        heap_scan(name + ' before')
    except Exception as exc:
        errors.append('heap before: ' + str(exc))
    if not errors or name == 'cleanup':
        try:
            detail = fn()
        except Exception as exc:
            errors.append(str(exc))
    try:
        heap_scan(name + ' after')
    except Exception as exc:
        errors.append('heap after: ' + str(exc))
    if errors:
        failures += 1
        result = {'test': name, 'status': 'fail', 'detail': '; '.join(errors)}
    else:
        result = {'test': name, 'status': 'pass', 'detail': detail}
    print(PREFIX + 'RESULT ' + json.dumps(result))
    return result['status'] == 'pass'

def identity():
    board = os.uname().machine
    assert board == 'WT9932S31-TINY with ESP32S31', 'unexpected board: ' + board
    return {'board': board}

def psram():
    gc.collect()
    probe = bytearray(2 * 1024 * 1024)
    for offset in range(0, len(probe), 4096):
        value = ((offset // 4096) * 2654435761) & 0xffffffff
        probe[offset:offset + 4] = value.to_bytes(4, 'little')
        probe[offset + 4092:offset + 4096] = (value ^ 0xffffffff).to_bytes(4, 'little')
    for offset in range(0, len(probe), 4096):
        value = ((offset // 4096) * 2654435761) & 0xffffffff
        assert probe[offset:offset + 4] == value.to_bytes(4, 'little')
        assert probe[offset + 4092:offset + 4096] == (value ^ 0xffffffff).to_bytes(4, 'little')
    return {'probe_bytes': len(probe), 'page_bytes': 4096, 'pages': 512}

def capture(fmt, size, width, height):
    print(PREFIX + 'STEP camera format=%d size=%dx%d' % (fmt, width, height))
    sensor.set_pixformat(fmt)
    sensor.set_framesize(size)
    for _ in range(5):
        img = sensor.snapshot()
        assert (img.width(), img.height(), img.format()) == (width, height, fmt)

def camera():
    tested = []
    frames = 0
    for cycle in range(3):
        try:
            print(PREFIX + 'STEP camera reset cycle=%d' % cycle)
            sensor.reset()
            assert sensor.get_id() == 0x2145, 'expected GC2145'
            assert sensor.status()['ready'], 'camera did not initialize'
            if cycle == 0:
                for fmt in (sensor.RGB565, sensor.GRAYSCALE):
                    for size, w, h in ((sensor.QQVGA, 160, 120),
                                       (sensor.QVGA, 320, 240),
                                       (sensor.VGA, 640, 480)):
                        capture(fmt, size, w, h)
                        tested.append([w, h, fmt])
                        frames += 5
            else:
                capture(sensor.RGB565, sensor.QVGA, 320, 240)
                frames += 5
        finally:
            print(PREFIX + 'STEP camera shutdown cycle=%d' % cycle)
            sensor.shutdown()
            assert not sensor.status()['ready'], 'camera did not shut down'
            gc.collect()
            heap_scan('camera shutdown cycle=%d' % cycle)
    return {'sensor': 0x2145, 'formats': tested, 'frames': frames, 'reset_shutdown_cycles': 3}

def remove_owned(path):
    os.remove(path)
    owned.remove(path)

def storage(root):
    if root == '/sdcard':
        print(PREFIX + 'STEP sdcard duplicate host rejection')
        try:
            machine.SDCard(slot=0)
        except OSError as exc:
            assert exc.args == (-0x103, 'ESP_ERR_INVALID_STATE'), 'unexpected duplicate SD error: ' + str(exc)
        else:
            raise AssertionError('duplicate SD host was accepted')
        gc.collect()
        stats = os.statvfs(root)
        assert stats[0] > 0 and stats[2] > 0, 'SD unavailable after duplicate host rejection'
    path = root + '/.wt_s31_' + TOKEN + '.bin'
    payload = bytearray(32768)
    for offset in range(0, len(payload), 4):
        value = ((offset // 4) * 2654435761) & 0xffffffff
        payload[offset:offset + 4] = value.to_bytes(4, 'little')
    try:
        with open(path, 'xb') as stream:
            owned.append(path)
            assert stream.write(payload) == len(payload), 'short write'
        os.sync()
        delays = (250, 1000, 3000) if root == '/sdcard' else (250,)
        for delay in delays:
            time.sleep_ms(delay)
            with open(path, 'rb') as stream:
                assert stream.read(len(payload)) == payload, 'readback mismatch after %d ms' % delay
                assert stream.read(1) == b'', 'unexpected file length'
    finally:
        if path in owned:
            remove_owned(path)
            os.sync()
    detail = {'bytes': len(payload), 'idle_delays_ms': delays}
    if root == '/sdcard':
        detail['duplicate_host_rejected'] = True
    return detail

def wifi():
    wlan = network.WLAN(network.STA_IF)
    was_active = wlan.active()
    try:
        wlan.active(True)
        aps = wlan.scan()
        return {'access_points': len(aps)}
    finally:
        wlan.active(was_active)

def gpio():
    button = machine.Pin(61, machine.Pin.IN, machine.Pin.PULL_UP)
    led = machine.Pin(19, machine.Pin.OUT, value=0)
    timings = (400, 850, 800, 450)
    try:
        for color in ((16, 0, 0), (0, 16, 0), (0, 0, 16)):
            machine.bitstream(led, 0, timings, bytes((color[1], color[0], color[2])))
            time.sleep_ms(200)
        level = button.value()
        assert level in (0, 1)
    finally:
        machine.bitstream(led, 0, timings, b'\\x00\\x00\\x00')
        led.value(0)
        time.sleep_us(80)
    return {'button_pin': 61, 'button_level': level, 'rgb_pin': 19, 'rgb_writes': 3}

def cleanup():
    errors = []
    for path in owned[:]:
        try:
            remove_owned(path)
        except Exception as exc:
            errors.append(str(exc))
    os.sync()
    gc.collect()
    if errors:
        raise RuntimeError('; '.join(errors))
    return {'remaining_files': len(owned)}

try:
    # Check camera allocation before a large MicroPython probe grows its split heap.
    # Do not drive board-specific pins if identification fails.
    if check('identity', identity):
        gc.collect()
        check('camera', camera)
        check('psram', psram)
        gc.collect()
        check('flash_rw', lambda: storage(''))
        if any(entry[1] == '/sdcard' for entry in vfs.mount()):
            check('sdcard_rw', lambda: storage('/sdcard'))
        else:
            print(PREFIX + 'RESULT ' + json.dumps({'test': 'sdcard_rw', 'status': 'skip',
                                                  'detail': 'SD card not mounted'}))
        check('wifi_scan', wifi)
        check('gpio', gpio)
finally:
    check('cleanup', cleanup)
print(PREFIX + 'DONE ' + json.dumps({'failures': failures}))
'''


def interrupt(transport):
    transport.serial.write(transport.frame('repl.signal', 'data', None, b'\x03'))
    time.sleep(0.3)


def synchronize(transport):
    # Opening an active preview port can start inside a JPEG/EV-MUX frame.
    # Stop its producer and discard old bytes before parsing any RPC response.
    interrupt(transport)
    deadline = time.monotonic() + 3
    last_data = time.monotonic()
    while time.monotonic() < deadline:
        if transport.serial.read(8192):
            last_data = time.monotonic()
        elif time.monotonic() - last_data >= 0.3:
            break
    transport.serial.reset_input_buffer()
    transport.frame_buffer.clear()
    transport.pending_frames.clear()


def stdout_lines(transport, timeout, fail_on_stderr=False):
    deadline = time.monotonic() + timeout
    pending = ''
    while time.monotonic() < deadline:
        frame = read_until_frame(transport, lambda item: True,
                                 timeout=max(0.01, deadline - time.monotonic()))
        channel = frame.metadata.get('channel')
        if channel == 'repl.stderr' and fail_on_stderr and frame.payload.strip():
            raise RuntimeError('device reported an uncaught exception on repl.stderr')
        if channel != 'repl.stdout':
            continue
        pending += frame.payload.decode(errors='replace')
        if len(pending) > 16384:
            raise RuntimeError('device output line exceeds 16 KiB')
        while '\n' in pending:
            line, pending = pending.split('\n', 1)
            yield line.rstrip('\r')
    raise TimeoutError('device did not finish before the deadline')


def repl_exec(transport, source, marker):
    command = 'exec(' + repr(source + '\nprint(' + repr(marker) + ')') + ')\r\n'
    # The device REPL input ring holds 511 bytes; script bodies use script.write.
    if len(command.encode()) > 480:
        raise ValueError('REPL helper exceeds the input buffer limit')
    transport.send_repl(command)
    # Ctrl-C can leave a KeyboardInterrupt frame queued before this helper runs.
    # The exact marker is printed only after every helper operation succeeds.
    for line in stdout_lines(transport, 8):
        if line == marker:
            return


def collect_results(transport, prefix, require_sd):
    results = {}
    done = None
    for line in stdout_lines(transport, 120, fail_on_stderr=True):
        if line.startswith(prefix + 'BEGIN ') or line.startswith(prefix + 'STEP '):
            print(line, flush=True)
        elif line.startswith(prefix + 'RESULT '):
            result = json.loads(line[len(prefix + 'RESULT '):])
            name = result.get('test')
            if name not in EXPECTED or name in results:
                raise RuntimeError('unexpected or duplicate hardware test result')
            if result.get('status') not in ('pass', 'fail', 'skip'):
                raise RuntimeError('invalid hardware test status')
            results[name] = result
            print(json.dumps(result), flush=True)
        elif line.startswith(prefix + 'DONE '):
            done = json.loads(line[len(prefix + 'DONE '):])
            break
    if done is None or set(results) != EXPECTED:
        raise RuntimeError('hardware tests ended without all expected results')
    allowed = lambda name, result: result['status'] == 'pass' or (
        name == 'sdcard_rw' and result['status'] == 'skip' and not require_sd)
    if done.get('failures') != 0 or not all(allowed(name, result) for name, result in results.items()):
        raise RuntimeError('hardware tests failed; see individual results above')


def run(port, require_sd=False):
    token = uuid.uuid4().hex[:16]
    prefix = 'WT_S31_' + token + ' '
    path = '/.wt_s31_' + token + '.py'
    staging = path + '.tmp'
    reserved = False
    created = False
    transport = Transport(port, 115200)
    try:
        synchronize(transport)
        transport.send_rpc('hello', {})
        if read_rpc(transport, 'hello').get('evMuxVersion') != 3:
            raise RuntimeError('EV-MUX v3 firmware required')
        ready = prefix + 'READY'
        transport.send_repl('print(' + repr(ready) + ')\r\n')
        for line in stdout_lines(transport, 8):
            if line == ready:
                break
        # Reserve the RPC staging file with O_EXCL before script.write opens it.
        reserve = (
            'import os\n'
            'try: os.stat(' + repr(path) + ')\n'
            'except OSError as e:\n'
            ' if e.args[0] != 2: raise\n'
            'else: raise RuntimeError("temporary path already exists")\n'
            'with open(' + repr(staging) + ', "xb"): pass'
        )
        repl_exec(transport, reserve, prefix + 'RESERVED')
        reserved = True
        source = SOURCE.replace('__TOKEN__', token).encode()
        for offset in range(0, len(source), 900):
            chunk = source[offset:offset + 900]
            transport.send_rpc('script.write', {
                'path': path, 'encoding': 'utf-8', 'mode': 'create',
                'offset': offset, 'totalBytes': len(source),
                'contentBase64': base64.b64encode(chunk).decode(),
            })
            reply = read_rpc(transport, 'script.write')
            created = reply.get('complete') is True
            expected_complete = offset + len(chunk) == len(source)
            if reply.get('complete') is not expected_complete:
                raise RuntimeError('unexpected script upload completion state')
        transport.send_rpc('script.run', {'path': path})
        read_rpc(transport, 'script.run')
        collect_results(transport, prefix, require_sd)
    finally:
        try:
            if reserved:
                interrupt(transport)
                paths = ([path] if created else []) + [staging]
                cleanup = (
                    'import os\n'
                    'for p in ' + repr(paths) + ' + globals().get(' + repr('_wt_owned_' + token) + ', []):\n'
                    ' try: os.remove(p)\n'
                    ' except OSError as e:\n'
                    '  if e.args[0] != 2: raise\n'
                    'os.sync()'
                )
                repl_exec(transport, cleanup, prefix + 'CLEANED')
        finally:
            transport.close()
    print('WT_S31 DONE PASS', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='S31 USB-Serial/JTAG or USB-OTG CDC port')
    parser.add_argument('--require-sd', action='store_true', help='fail unless the SD read/write test passes')
    args = parser.parse_args()
    try:
        run(args.port, args.require_sd)
    except (Exception, KeyboardInterrupt) as exc:
        print('WT_S31 DONE FAIL: ' + str(exc), file=sys.stderr, flush=True)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
