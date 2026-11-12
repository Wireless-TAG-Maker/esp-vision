# WT9932P4X-TINY

Build with ESP-IDF v5.5.6 from the ESP-VISION repository root.

```sh
idf.py --board WT9932P4X_TINY build
```

This profile selects ESP32-P4 silicon v3.0 through v3.99, a 400 MHz CPU, 16 MiB QIO flash and 200 MHz PSRAM. For flashing and serial logs, connect FUSB and pass the host serial port to `idf.py --board WT9932P4X_TINY -p PORT flash monitor`. HUSB is the USB-OTG interface for CDC, mass storage and UVC. Wi-Fi and Bluetooth are disabled for this board.

| Peripheral | Configuration |
| --- | --- |
| Camera | SC2336 MIPI-CSI, RAW8 1024×600 at 30 fps, SCCB SDA GPIO7 and SCL GPIO8, GPIO0 active-high camera power with 100 ms startup delay |
| Display | ST7102 480×640 MIPI-DSI, two lanes at 1000 Mbps, 24 MHz DPI clock, reset GPIO27 and backlight GPIO26 |
| Touch | ST7123 at I2C address 0x55, sharing GPIO7 and GPIO8 with the camera, exposed through `lcd_touch` |
| MIPI supply | LDO3 at 2.5 V, shared by CSI and DSI |
| SD | Four-bit SDMMC slot 0, CLK GPIO43, CMD GPIO44, D0–D3 GPIO39–42, LDO4 at 3.3 V |
| User controls | Active-low button on GPIO35 and a WS2812 GRB LED on GPIO51, accessible through `machine.Pin` and `neopixel` |

The default Python script captures RGB565 QVGA frames and sends USB previews. It also draws to the ST7102 LCD when available. The storage manager mounts a supported SD card at `/sdcard` and owns its SDMMC slot. Use the mounted filesystem for SD access.

Pin assignments and panel timings follow the [WT_BSP P4X-TINY board profile](https://github.com/Wireless-TAG-Maker/WT_BSP/tree/c4962c2a8d69117f27f8a8db9b031fced45b63a9/components/wt_bsp/boards/WT9932P4X-TINY).
