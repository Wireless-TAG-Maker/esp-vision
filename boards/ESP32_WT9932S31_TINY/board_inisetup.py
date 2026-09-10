# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

MAIN_PY = """\
import sensor
import time

print("ESP-VISION ESP32_WT9932S31_TINY ready")

try:
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.skip_frames(time=1000)
    while True:
        img = sensor.snapshot()
        img.flush()
        time.sleep_ms(20)
finally:
    sensor.shutdown()
"""

README_TXT = """\
ESP-VISION ESP32_WT9932S31_TINY

Edit main.py to run your Python vision script.
The default main.py sends a GC2145 camera preview over USB.
Use the ESP-VISION Web IDE or VSCode extension to view preview frames.
The board has no onboard LCD.
"""
