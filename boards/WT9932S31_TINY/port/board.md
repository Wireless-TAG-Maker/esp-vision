# WT9932S31-TINY

Build with ESP-IDF `release/v6.1` using `idf.py --board WT9932S31_TINY build`. This profile uses the S31 dependency overlay for a GC2145 DVP camera and 16 MiB Flash. ESP-IDF detects PSRAM capacity from the installed memory device.

| Function | Configuration |
| --- | --- |
| Camera | GC2145, SCCB address 0x3c, product ID 0x2145 |
| Camera I2C | SDA GPIO0, SCL GPIO1, 100 kHz |
| Camera DVP | D0..D7 GPIO46..53, PCLK GPIO54, XCLK GPIO55 at 20 MHz, VSYNC GPIO56, HREF GPIO57 |
| Camera control | RESET pulled to 2.8 V and PWDN pulled to GND; neither is MCU-controlled |
| Camera input | UYVY 1600x1200 at nominal 8 fps; two capture buffers |
| Camera output | RGB565 or grayscale, QQVGA/QVGA/VGA |
| SD storage | SDMMC slot 0, four-bit 20 MHz; fixed IOMUX CLK GPIO24, CMD GPIO25, D0..D3 GPIO20..23 |
| SD power | Card VDD and pullups at board 3.3 V; host IO uses fixed 1.8 V LDO1 |
| User button | GPIO61, active low |
| RGB LED | WS2812 on GPIO19 |
| USB | USB-Serial/JTAG for flashing and EV-MUX; native USB for CDC and Flash/SD MSC |

The profile selects `CONFIG_LIBC_NEWLIB=y` so ESP-VISION can attach its C stdout/stderr write hooks and route third-party `printf` output and TFLite diagnostics through EV-MUX.

The default `main.py` publishes a QVGA camera preview. The board has no onboard LCD. Use the USB-Serial/JTAG (`usj`) or native USB CDC (`usb`) port with an EV-MUX v4 client. USB-Serial/JTAG is the default route. When a client acquires the device, its EV-MUX session routes both streams through that client's connection. When using a Windows browser, attach the selected USB port to Windows so the browser can access it.

Native USB exposes Flash as MSC LUN 0 and SD as LUN 1. Each volume shares its FAT filesystem with MicroPython. Safely eject the relevant OS volume and verify its writer state before resuming Python or IDE writes. See the [USB guide](../../../docs/en/communication/usb.rst) and [storage guide](../../../docs/en/storage/index.rst).

The GC2145 configuration follows the validated DVP reference: derive UYVY from the complete RGB565_BE UXGA register table and set output divider register 0xfa to 0x10. This reduces the rate to avoid incomplete DVP DMA frames observed at the original divider. The input PCLK is not inverted. PSRAM XIP is disabled to leave memory for the camera buffers and MicroPython. The camera reference reports an unresolved thin colored line near the right edge of the UXGA image; image-quality validation must distinguish this from successful capture and JPEG decoding.

The SD socket has no independent card-detect signal or controllable card-power switch. The board rejects a second `machine.SDCard(slot=0)` while storage owns the mounted volume, before the new object can initialize or deinitialize the active host. Do not assign an SPI driver to the occupied SD pins. Mount failure does not format the card. The board retains one SD IO LDO reference across retries and MicroPython soft resets; this fixed-voltage supply may be shared with the S31's PSRAM/MPLL circuitry. Bluetooth, ESP-NOW, and board-specific ADC interfaces are not enabled by this profile.

## Hardware checks

With Node.js 22 or newer, install the Device Core dependencies from `host/device-core`. From the repository root, set `USJ_PORT` to the host's USB-Serial/JTAG serial path and run the shared hardware checks. Use the native CDC serial path and `--role usb` to verify that connection separately.

```sh
npm --prefix host/device-core run test:hardware -- --serial "$USJ_PORT" --role usj --expect-board WT9932S31_TINY --checks identity,memory,camera,pixformats,framesizes,orientation,preview --require-all
```

Add `--write` to verify Flash file transfer on a mounted, writable volume. For board-specific checks, insert a mountable FAT SD card, give the VM write access to both volumes, and use an EV-MUX v4-compatible [Device CLI](../../../host/device-cli/README.md):

```sh
esp-vision-device python --file tools/test_wt9932s31_tiny.py --port "$USJ_PORT" --role usj --timeout 120000
```

The board script checks three GC2145 startup/shutdown cycles, PSRAM heap traversal and a 2 MiB page-pattern allocation, SD duplicate-slot rejection and 32 KiB file readback after 250/1000/3000 ms idle, Wi-Fi scanning, and GPIO access for the button and LED. It uses an exclusive temporary SD file and removes it after the check. Watch for `WT9932S31-TINY board checks passed`. The checks interrupt the default preview. Reset the board to resume `main.py`. Observe LED colors and button presses on the board, and verify native USB mass storage separately.

### Historical bring-up

The 2026-09-09 EV-MUX v3 build used ESP-IDF `9bf0c0eb952fe4c075daf20e065ceb27a42ba8bf` and Newlib. The sample reported 32 MiB PSRAM and passed the board smoke checks, including duplicate SDMMC rejection. Three USB-Serial/JTAG boot cycles and 64 QVGA capture/TFLite inference iterations passed. Windows verified native CDC preview and 32 KiB file readback on both MSC volumes. The Web IDE ran a camera script and resumed the default preview after soft reset. Repeat these checks on EV-MUX v4 for current firmware evidence.

The historical camera sample showed a thin colored line at the right edge. Early PSRAM startup and heap-walk failures remain unexplained. Cold-start reliability and actual installed PSRAM capacity require separate verification.

## Sources

Pin assignments follow the WT9932S31-TINY schematic, whose filename identifies 1V1 and whose drawing title block says V1.0. The camera configuration follows the Wireless-Tag `dvp_usb_device_uvc` reference project, and the SD host setup follows `sd_card/sdmmc`. See the [license inventory](../../../docs/en/license/index.rst) for the derived GC2145 helper.
