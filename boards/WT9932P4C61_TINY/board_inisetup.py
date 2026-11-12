# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

MAIN_PY = """\
import display
import sensor
import time

print("ESP-VISION WT9932P4C61_TINY ready")

lcd = None
try:
    sensor.reset()
    sensor.set_pixformat(sensor.RGB565)
    sensor.set_framesize(sensor.QVGA)
    sensor.skip_frames(time=1000)

    try:
        lcd = display.Display()
    except OSError as exc:
        print("LCD unavailable:", exc)

    while True:
        img = sensor.snapshot()
        if lcd is not None:
            lcd.write(img)
        img.flush()
        time.sleep_ms(20)
finally:
    sensor.shutdown()
    if lcd is not None:
        lcd.deinit()
"""

README_TXT = """\
ESP-VISION WT9932P4C61_TINY

Edit main.py to run your Python vision script.
Use the ESP-VISION VSCode extension to run scripts and preview frames.
The default main.py previews the camera image on the board LCD.
"""
