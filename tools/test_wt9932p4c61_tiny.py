#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Exercise the WT9932P4C61-TINY board through its EV-MUX REPL.

Requires pyserial and a flashed board. Test files use create-only writes and
are removed after the run. Wi-Fi testing scans without connecting to a network
or printing SSIDs. An absent SD card is reported as skipped.
"""

import argparse
import base64
import json
import time

from test_control_transport import Transport, read_rpc, read_until_frame


SOURCE = '''\
import gc, os, sys, time, json
import sensor, image, display, network, machine, neopixel

def check(name, fn):
    print('WT_BEGIN ' + name)
    try:
        detail = fn()
        print('WT_RESULT ' + json.dumps({'test': name, 'status': 'pass', 'detail': detail}))
        return True
    except Exception as exc:
        print('WT_RESULT ' + json.dumps({'test': name, 'status': 'fail', 'detail': str(exc)}))
        return False

def identity():
    assert 'WT9932P4C61-TINY' in os.uname().machine
    # MicroPython grows its heap on demand; a large live allocation exercises
    # PSRAM without assuming the initial GC heap size.
    probe = bytearray(2 * 1024 * 1024)
    for offset in range(0, len(probe), 4096):
        probe[offset] = (offset // 4096) & 255
    for offset in range(0, len(probe), 4096):
        assert probe[offset] == ((offset // 4096) & 255)
    return {'board': os.uname().machine, 'psram_probe_bytes': len(probe)}

def camera():
    sensor.reset()
    assert sensor.get_id() == 0x2336
    tested = []
    for fmt in (sensor.RGB565, sensor.GRAYSCALE):
        sensor.set_pixformat(fmt)
        for size, w, h in ((sensor.QQVGA,160,120),(sensor.QVGA,320,240),(sensor.VGA,640,480)):
            sensor.set_framesize(size)
            for _ in range(5):
                img = sensor.snapshot()
                assert (img.width(), img.height(), img.format()) == (w, h, fmt)
            tested.append([w, h, fmt])
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    return {'sensor': sensor.get_id(), 'formats': tested, 'frames': 30}

def lcd():
    frame = image.Image(480, 640, image.RGB565)
    for index, color in enumerate(((255,0,0),(0,255,0),(0,0,255))):
        frame.draw_rectangle(index * 160, 0, 160, 640, color=color, fill=True)
    for _ in range(3):
        panel = display.Display()
        try:
            assert (panel.width(), panel.height()) == (480, 640)
            for level in (20, 60, 100):
                panel.backlight(level)
                assert panel.backlight() == level
                panel.write(frame, fit=False)
                time.sleep_ms(100)
            panel.clear()
        finally:
            panel.deinit()
        if camera_ready:
            sample = sensor.snapshot()
            assert (sample.width(), sample.height()) == (320, 240)
    return {'width': 480, 'height': 640, 'init_cycles': 3, 'camera_coexistence': camera_ready}

def storage(root):
    path = root + '/.wt9932p4c61_probe.bin'
    try:
        os.stat(path)
    except OSError as exc:
        if exc.args[0] != 2:
            raise
    else:
        raise RuntimeError('probe path exists; refusing to overwrite')
    block = bytes(range(256)) * 16
    created = False
    try:
        with open(path, 'xb') as f:
            created = True
            for _ in range(8):
                assert f.write(block) == len(block)
        os.sync()
        idle_delays_ms = (250, 1000, 3000) if root == '/sdcard' else (250,)
        for delay_ms in idle_delays_ms:
            time.sleep_ms(delay_ms)
            with open(path, 'rb') as f:
                for _ in range(8):
                    assert f.read(len(block)) == block
                assert f.read(1) == b''
    finally:
        if created:
            os.remove(path)
            os.sync()
    return {'bytes': len(block) * 8, 'block_size': os.statvfs(root or '/')[0],
            'idle_delays_ms': idle_delays_ms}

def wifi():
    wlan = network.WLAN(network.STA_IF)
    was_active = wlan.active()
    try:
        wlan.active(True)
        aps = wlan.scan()
        assert len(aps) > 0
        return {'access_points': len(aps)}
    finally:
        wlan.active(was_active)

def gpio():
    button = machine.Pin(35, machine.Pin.IN, machine.Pin.PULL_UP)
    led = neopixel.NeoPixel(machine.Pin(51, machine.Pin.OUT), 1)
    try:
        for color in ((16,0,0),(0,16,0),(0,0,16)):
            led[0] = color
            led.write()
            time.sleep_ms(200)
    finally:
        led[0] = (0,0,0)
        led.write()
    return {'button_level': button.value(), 'rgb_writes': 3}

check('identity_psram', identity)
camera_ready = check('camera', camera)
check('display', lcd)
check('flash_rw', lambda: storage(''))
if 'sdcard' in os.listdir('/'):
    check('sdcard_rw', lambda: storage('/sdcard'))
else:
    print('WT_RESULT ' + json.dumps({'test': 'sdcard_rw', 'status': 'skip', 'detail': 'SD card not mounted'}))
check('wifi_scan', wifi)
check('gpio', gpio)
sensor.shutdown()
gc.collect()
print('WT_DONE')
'''


def run(port, require_sd=False):
    transport = Transport(port, 115200)
    path = '/wt9932p4c61_smoke.py'
    created = False
    results = []
    try:
        transport.send_rpc('hello', {})
        assert read_rpc(transport, 'hello')['evMuxVersion'] == 3
        transport.serial.write(transport.frame('repl.signal', 'data', None, b'\x03'))
        time.sleep(0.3)
        source = SOURCE.encode()
        for offset in range(0, len(source), 900):
            transport.send_rpc('script.write', {
                'path': path, 'encoding': 'utf-8', 'mode': 'create',
                'offset': offset, 'totalBytes': len(source),
                'contentBase64': base64.b64encode(source[offset:offset + 900]).decode(),
            })
            reply = read_rpc(transport, 'script.write')
            created = bool(reply.get('complete'))
        transport.send_rpc('script.run', {'path': path})
        read_rpc(transport, 'script.run')
        output = ''
        deadline = time.monotonic() + 120
        while 'WT_DONE' not in output:
            frame = read_until_frame(transport, lambda f: f.metadata.get('channel') == 'repl.stdout',
                                     timeout=max(0.1, deadline - time.monotonic()))
            output += frame.payload.decode(errors='replace')
            while '\n' in output:
                line, output = output.split('\n', 1)
                if line.startswith('WT_BEGIN '):
                    print(line.strip(), flush=True)
                if results and 'ESP-VISION ESP32_WT9932P4C61_TINY ready' in line:
                    raise RuntimeError('board restarted during hardware tests')
                if line.startswith('WT_RESULT '):
                    result = json.loads(line[len('WT_RESULT '):])
                    results.append(result)
                    print(json.dumps(result), flush=True)
                if line.strip() == 'WT_DONE':
                    output = 'WT_DONE'
                    break
        expected = {'identity_psram', 'camera', 'display', 'flash_rw', 'sdcard_rw', 'wifi_scan', 'gpio'}
        assert {r['test'] for r in results} == expected, 'missing board test results'
        assert all(r['status'] != 'fail' for r in results), 'board test failed'
        if require_sd:
            assert any(r['test'] == 'sdcard_rw' and r['status'] == 'pass' for r in results), 'SD card test did not pass'
    finally:
        try:
            if created:
                transport.serial.write(transport.frame('repl.signal', 'data', None, b'\x03'))
                time.sleep(0.3)
                transport.send_repl('import os; os.remove(' + repr(path) + '); print("WT_CLEANED")\r\n')
                cleaned = ''
                deadline = time.monotonic() + 5
                while '\nWT_CLEANED\r\n' not in cleaned:
                    frame = read_until_frame(transport, lambda f: f.metadata.get('channel') == 'repl.stdout',
                                             timeout=max(0.1, deadline - time.monotonic()))
                    cleaned += frame.payload.decode(errors='replace')
        finally:
            transport.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='P4 USB-Serial/JTAG or USB-OTG CDC port')
    parser.add_argument('--require-sd', action='store_true', help='fail if the SD card is not mounted')
    args = parser.parse_args()
    run(args.port, args.require_sd)
