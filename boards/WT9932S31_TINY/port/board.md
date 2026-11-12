# WT9932S31-TINY

Build with ESP-IDF `release/v6.1` using `idf.py --board WT9932S31_TINY build`. This profile targets the WT9932S31-TINY board with a GC2145 DVP camera and 16 MiB Flash. ESP-IDF detects the PSRAM capacity from the memory device; the board connected during bring-up reported 32 MiB. The startup memory test and 2 MiB page-pattern smoke check sample memory and do not constitute an independent full-capacity test. It uses the repository's existing S31 dependency overlay. ESP-IDF 6.2 was used by the reference projects; this port uses 6.1 for compatibility with the other S31 boards.

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

The default `main.py` sends a QVGA camera preview over USB. The board has no onboard LCD. Use the host-reported serial port with the ESP-VISION Web IDE or VSCode extension. An open native USB CDC connection receives EV-MUX traffic; closing it returns traffic to USB-Serial/JTAG. MSC and MicroPython share the same FAT volumes; avoid writing to the same volume from the PC and the device at the same time. When using a Windows browser, the selected USB device must be attached to Windows rather than forwarded to WSL.

The GC2145 configuration follows the validated DVP reference: derive UYVY from the complete RGB565_BE UXGA register table and set output divider register 0xfa to 0x10. This reduces the rate to avoid incomplete DVP DMA frames observed at the original divider. The input PCLK is not inverted. PSRAM XIP is disabled to leave memory for the camera buffers and MicroPython. The camera reference reports an unresolved thin colored line near the right edge of the UXGA image; image-quality validation must distinguish this from successful capture and JPEG decoding.

The SD socket has no independent card-detect signal or controllable card-power switch. The board rejects a second `machine.SDCard(slot=0)` while storage owns the mounted volume, before the new object can initialize or deinitialize the active host. Do not assign an SPI driver to the occupied SD pins. Mount failure does not format the card. The board retains one SD IO LDO reference across retries and MicroPython soft resets; this fixed-voltage supply may be shared with the S31's PSRAM/MPLL circuitry. Bluetooth, ESP-NOW, and board-specific ADC interfaces are not enabled by this profile.

## Hardware checks

The board smoke test requires Python 3 and `pyserial` on the host. Replace `<port>` with the serial port assigned by the host:

```sh
python tools/test_wt9932s31_tiny.py --port <port> --require-sd
```

The test scans the PSRAM heap before and after each hardware check and after each camera shutdown. It checks board identity, 40 camera frames across RGB565/grayscale and QQVGA/QVGA/VGA with three reset/shutdown cycles, a 2 MiB PSRAM page-pattern allocation, 32 KiB Flash and SD readback, SD reads after 250/1000/3000 ms idle, Wi-Fi scanning, and button/LED GPIO access. It creates and removes only its own temporary files. It interrupts the running preview and leaves the REPL active; reset the device to resume `main.py`. LED appearance, button actuation, USB mass-storage access, and image quality require separate checks.

### Bring-up results

The 2026-09-09 bring-up used ESP-IDF release/v6.1 commit `9bf0c0eb952fe4c075daf20e065ceb27a42ba8bf` with Newlib. The installed SDK and vendored MicroPython sources were not patched. The firmware was flashed with hash verification and passed all eight board checks, including PSRAM heap traversal and SD access after rejecting a duplicate SDMMC driver. The duplicate-driver defect was reproduced before the guard: garbage collection of the failed object made `statvfs('/sdcard')` return EIO; the same sequence passed after the guard.

Three USB-Serial/JTAG reset/boot cycles passed with camera preview and both filesystems mounted. The control-transport suite passed discovery, diagnostics, script/file operations, and binary JPEG capture. An additional integration run completed 64 QVGA camera captures/previews and 64 INT8 TFLite sine inferences with periodic heap scans; maximum absolute sine error was 0.121105 against the test limit of 0.3. Model deinitialization and the final heap scan passed. The ESP32_P4X_EYE regression build passed with ESP-IDF 6.0.1.

Separate Windows native USB checks passed. CDC on the host-assigned native USB port completed EV-MUX hello and status RPCs and received 40 QVGA preview frames at 320x240 in 5.01 seconds. Windows enumerated Flash as LUN 0 with 9,371,648 bytes and FAT, and SD as LUN 1 with 63,864,569,856 bytes and FAT32. Drive letters and COM numbers depend on the host.

Both MSC volumes passed a 32 KiB file test using an exclusive `CreateNew` file, `WriteThrough`, and `Flush(true)`. Each file was closed and reopened for matching readback after 250/1000/3000 ms, and only the test's own files were deleted; both cleanup checks passed. Windows caching means these results do not establish that every reread accessed the physical medium. These USB checks did not include a complete physical power disconnection.

The official ESP-VISION Web IDE was also checked in Windows Chrome using native CDC. Creating, editing, saving, and running a uniquely named temporary Python file passed after checking that the active editor matched that file. The terminal reported 20 successful QVGA captures, a mounted SD volume, and a passing PSRAM heap scan; the browser received 20 preview updates and decoded them at 320x240 without a runtime error. The temporary file was deleted through the IDE. A subsequent device-side read confirmed that `main.py` retained its pre-test SHA-256, and the temporary path was absent. A soft reset through the IDE resumed the default preview; the isolated test browser was then closed.

A captured QVGA JPEG contained a complete scene and the thin right-edge line already reported by the camera reference. Physical LED/button observation and removal of this image artifact are not covered by the automated tests. An early PSRAM startup failure and a PSRAM heap-walk panic were observed during bring-up. Subsequent instrumented camera/storage/preview/inference runs passed, but no root cause for those early failures was established; selecting Newlib is a stdout compatibility fix, not a demonstrated fix for those failures. Cold-start reliability still requires a physical power-disconnection test.

## Sources

Pin assignments were checked against the supplied `SCH_WT9932S31-TINY_1V1(Kirto).pdf`. Its filename identifies 1V1 while the drawing title block says V1.0; the camera connector's pin numbering must not be confused with GPIO numbers. The camera configuration follows the Wireless-Tag `dvp_usb_device_uvc` reference project, and the SD host setup follows its validated `sd_card/sdmmc` reference. The GC2145 helper retains the reference's Apache-2.0 source notices. Sensor drivers remain Component Registry dependencies and are not copied into this board directory.
