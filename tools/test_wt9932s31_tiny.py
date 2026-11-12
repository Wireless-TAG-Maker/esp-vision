# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Board-specific MicroPython checks, run on the device through Device CLI."""

import binascii
import gc
import os
import time

import esp32
import machine
import network
import sensor
import vfs


def check_psram_heap():
    regions = esp32.idf_heap_info(1 << 10)
    assert regions, 'no PSRAM heap regions'
    for total, free, largest, minimum in regions:
        assert total > 0 and 0 <= minimum <= free <= total, 'invalid PSRAM heap sizes'
        assert 0 <= largest <= free, 'invalid PSRAM largest block'


def check_camera_restart():
    for _ in range(3):
        try:
            sensor.reset()
            assert sensor.get_id() == 0x2145, 'expected GC2145'
            sensor.set_pixformat(sensor.RGB565)
            sensor.set_framesize(sensor.QVGA)
            frame = sensor.snapshot()
            assert (frame.width(), frame.height(), frame.format()) == (320, 240, sensor.RGB565)
        finally:
            sensor.shutdown()
        assert not sensor.status()['ready'], 'camera did not shut down'
        gc.collect()
        check_psram_heap()


def check_psram_pages():
    probe = bytearray(2 * 1024 * 1024)
    for offset in range(0, len(probe), 4096):
        value = ((offset // 4096) * 2654435761) & 0xffffffff
        probe[offset:offset + 4] = value.to_bytes(4, 'little')
        probe[offset + 4092:offset + 4096] = (value ^ 0xffffffff).to_bytes(4, 'little')
    for offset in range(0, len(probe), 4096):
        value = ((offset // 4096) * 2654435761) & 0xffffffff
        assert probe[offset:offset + 4] == value.to_bytes(4, 'little'), 'PSRAM page start mismatch'
        assert probe[offset + 4092:offset + 4096] == (value ^ 0xffffffff).to_bytes(4, 'little'), 'PSRAM page end mismatch'


def check_sdcard():
    root = '/sdcard'
    assert any(entry[1] == root for entry in vfs.mount()), 'SD card must be mounted'
    try:
        duplicate = machine.SDCard(slot=0)
    except OSError as exc:
        assert exc.args == (-0x103, 'ESP_ERR_INVALID_STATE'), 'unexpected duplicate SD error: ' + str(exc)
    else:
        duplicate.deinit()
        raise AssertionError('duplicate SD host was accepted')
    gc.collect()
    stats = os.statvfs(root)
    assert stats[0] > 0 and stats[2] > 0, 'SD unavailable after duplicate host rejection'

    token = binascii.hexlify(os.urandom(8)).decode()
    path = root + '/.wt_s31_' + token + '.bin'
    payload = bytearray(32768)
    for offset in range(0, len(payload), 4):
        value = ((offset // 4) * 2654435761) & 0xffffffff
        payload[offset:offset + 4] = value.to_bytes(4, 'little')
    owned = False
    try:
        with open(path, 'xb') as stream:
            owned = True
            assert stream.write(payload) == len(payload), 'short SD write'
        os.sync()
        for delay in (250, 1000, 3000):
            time.sleep_ms(delay)
            with open(path, 'rb') as stream:
                assert stream.read(len(payload)) == payload, 'SD readback mismatch after %d ms' % delay
                assert stream.read(1) == b'', 'unexpected SD file length'
    finally:
        if owned:
            os.remove(path)
            os.sync()


def check_wifi():
    wlan = network.WLAN(network.STA_IF)
    was_active = wlan.active()
    try:
        wlan.active(True)
        wlan.scan()
    finally:
        wlan.active(was_active)


def check_gpio():
    button = machine.Pin(61, machine.Pin.IN, machine.Pin.PULL_UP)
    led = machine.Pin(19, machine.Pin.OUT, value=0)
    timings = (400, 850, 800, 450)
    try:
        for color in ((16, 0, 0), (0, 16, 0), (0, 0, 16)):
            machine.bitstream(led, 0, timings, bytes((color[1], color[0], color[2])))
            time.sleep_ms(200)
        assert button.value() in (0, 1)
    finally:
        machine.bitstream(led, 0, timings, b'\x00\x00\x00')
        led.value(0)
        time.sleep_us(80)


def main():
    board = os.uname().machine
    assert board == 'WT9932S31-TINY with ESP32S31', 'unexpected board: ' + board
    for check in (check_camera_restart, check_psram_pages, check_sdcard, check_wifi, check_gpio):
        gc.collect()
        check_psram_heap()
        check()
        gc.collect()
        check_psram_heap()
        print('PASS', check.__name__)
    print('WT9932S31-TINY board checks passed')


main()
