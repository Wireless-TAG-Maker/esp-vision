# WT9932P4C61-TINY

Build this Wireless-Tag board with `idf.py --board ESP32_WT9932P4C61_TINY build`. The firmware runs on the ESP32-P4 (revision 3.1 or later), using 16 MiB Flash and external PSRAM at 200 MHz. ESP-IDF 6.0 is the bring-up toolchain.

Connect **FUSB** to flash or monitor the P4 USB-Serial/JTAG port. **HUSB** exposes the ESP-VISION USB-OTG CDC and Flash/SD MSC interfaces. Use the port reported by the host with `idf.py --board ESP32_WT9932P4C61_TINY -p PORT flash monitor`.

For the [ESP-VISION Web IDE](https://ide.vision.espressif.com/), connect HUSB and select its `ESP-VISION CDC` serial port in a browser with Web Serial support. When running the browser on Windows, HUSB must be attached to Windows rather than forwarded to WSL through USB/IP; FUSB can remain in WSL for flashing. Connecting the IDE stops the running Python script. Wait until the intended file is active in the editor before saving or running it.

| Function | Configuration |
| --- | --- |
| Camera | SC2336 MIPI-CSI, RAW8 1024x600 at 30 fps; GPIO0 high enables camera power, with a 100 ms settling delay |
| Shared camera/display I2C | SDA GPIO7, SCL GPIO8, port 0 |
| Display | ST7102, 480x640, two DSI lanes at 1000 Mbps, 24 MHz pixel clock; reset GPIO27, backlight GPIO26 |
| MIPI PHY supply | P4 LDO3, 2.5 V, shared by CSI and DSI |
| SD storage | SDMMC slot 0, four-bit bus at 20 MHz: CLK GPIO43, CMD GPIO44, D0..D3 GPIO39..42; LDO4 enabled at 3.3 V, no card-detect signal |
| ESP32-C61 | Hosted SDIO slot 1; CLK GPIO18, CMD GPIO19, D0..D3 GPIO14..17, reset GPIO13 |
| User button | GPIO35, active low |
| RGB LED | One WS2812 on GPIO51, GRB order |

The default script shows a QVGA camera preview on the LCD and sends USB preview frames. The LCD resets before reading its ID, and its DSI ID read has a 200 ms deadline; an absent LCD raises an error without blocking the REPL, and the default script continues USB camera preview. Display initialization is independent of camera/touch I2C detection: the tested panel works over DSI even when I2C address 0x28 does not respond. The image API also supports QQVGA and VGA outputs using the existing PPA path. Touch, audio, Bluetooth, and ESP-NOW are outside this board profile's enabled API surface. The button and RGB LED can be used through `machine.Pin` and `neopixel`.

The storage manager uses SDMMC slot 0 and reuses the controller initialized for the C61's SDIO slot 1. The board enables LDO4 before card initialization, following the validated `RBIS_P4_UVC_RECORDER` configuration; unlike that recorder, this profile also has Hosted SDIO active. A failed SD mount releases only slot 0, while the board retains the LDO handle across retries and MicroPython soft resets. The automatically mounted SD volume owns slot 0 and its pins; do not create another SPI or SDCard instance on them. `CONFIG_VFS_MAX_COUNT=16` leaves room for the storage, console, socket, CSI, and ISP registrations. On IDF 6.0, a board-local LCD adaptation disables the refresh callback and drains its DMA channel before the driver shuts down DSI, allowing repeated display initialization while CSI keeps the shared DMA controller active.

After running firmware that initializes this card in SPI mode, disconnect all board power before booting this SDMMC profile. A card stays in SPI mode until its next power cycle; resetting only the P4 does not switch it back. The recorder's older C2-board schematic shows card VDD on fixed 3.3 V and CMD/D0..D3 pull-ups on LDO4, so toggling LDO4 must not be treated as a card power cycle.

FAT and exFAT volumes are supported without automatically formatting an unrecognized card. ESP-IDF 6.0 hardcodes exFAT off, so `fatfs.cmake` prepares a small copy of the SDK's FatFs core under the board build directory and enables exFAT in its generated configuration. FatFs and its consumers share the generated headers; the installed SDK and MicroPython's separate ooFatFs configuration stay unchanged. Large-volume support does not extend the current 32-bit POSIX file-offset limits; files larger than 2 GiB are outside this board's validation.

`network.WLAN` requires compatible ESP-Hosted slave firmware on the onboard ESP32-C61. Flashing this P4 image does not provision the C61. Follow the [WT_BSP C61 slave instructions](https://github.com/Wireless-TAG-Maker/WT_BSP/tree/9d1050d0957aaa050e635ff1f22a686881e54444/examples/wt_factory/wt9932p4c61-tiny) when the slave needs provisioning. Firmware flashing replaces the P4 application and partition table; back up existing data before changing partition layouts.

Full ESP-Hosted transport teardown/reinitialization remains unsupported in this bring-up profile: the pinned Hosted 2.12.8 teardown releases the entire SDMMC controller, including an active SD slot 0. Normal `WLAN.active()` and `scan()` calls leave the Hosted transport initialized and do not take this path. Full transport restart needs a separate lifecycle adaptation and regression test before it can safely coexist with a mounted SD volume.

## Hardware checks

Run `python tools/test_wt9932p4c61_tiny.py --port PORT` with pyserial installed. The script checks the board identity and PSRAM heap, 30 camera frames across RGB565/grayscale and QQVGA/QVGA/VGA, three display initialization cycles while the camera remains active, 32 KiB Flash/SD file round trips, SD rereads after 250/1000/3000 ms idle, Wi-Fi scanning, and button/RGB GPIO access. It removes its probe files and reports a missing SD mount as skipped. Add `--require-sd` when testing with an installed card to make a missing mount fail the run. Display API success does not verify visible colors or touch; GPIO reads/writes do not confirm a physical button press or LED color.

Run `python tools/test_control_transport.py --port PORT --output capture.jpg` to exercise EV-MUX RPC, script upload/execution, and JPEG capture. Both tests interrupt the running Python script; reset the board afterward to resume the default preview.

### Bring-up results (2026-09-07)

ESP-IDF 6.0.1 builds passed for this board and `ESP32_P4X_EYE`. The P4 revision 3.2 board accepted the firmware with flash hash verification. EV-MUX script execution, a 2 MiB PSRAM allocation with page-stride pattern checks, a 32 KiB Flash file round trip, C61 Wi-Fi scanning, and button/RGB GPIO access passed. The LCD returned DSI ID `80 A0 FB`, and the operator confirmed correct red, green, and blue bars at 480x640.

After correcting the wiring, I2C responded at 0x28, 0x30, and 0x55, and the SC2336 passed 30 captures across RGB565/grayscale and QQVGA/QVGA/VGA. The hardware smoke test passed all six non-SD checks. Three LCD initialization cycles with an active camera passed, followed by a separate ten-cycle check with 40 successful camera captures. EV-MUX RPC, filesystem contracts, script upload/execution, and a 320x240 JPEG capture also passed. The September 7 firmware delivered 164 valid 320x240 USB preview frames over 20 seconds while running the default LCD preview, with increasing timestamps and no restart. Test files were removed, and `main.py` matched the board's default script.

The earlier SPI configuration without LDO4 mounted the card but failed reads after idle, including in a read-only factory-project comparison. A SPI/LDO4 comparison reached filesystem detection but returned `FR_NO_FILESYSTEM`; this did not by itself establish filesystem damage.

### SD verification (2026-09-08)

After applying the recorder's native SDMMC/LDO4 configuration and fully removing board power, the read-only probe passed initialization and sector comparisons after 250/1000/3000 ms idle at four-bit 20 MHz. The installed card reported 124735488 sectors of 512 bytes (approximately 64 GB), with an exFAT partition. The original FatFs configuration could not mount it; enabling exFAT in the board's build copy resolved the mount failure. The full hardware smoke test then passed all seven checks, including 32 KiB Flash and SD file round trips, all three SD idle intervals, 30 camera frames, three LCD initialization cycles with an active camera, Wi-Fi scanning, PSRAM, and GPIO access. No SD formatting was performed.

A separate SD/Wi-Fi coexistence check passed twelve 32 KiB file comparisons across Wi-Fi disabled, after the first scan, and after disabling/re-enabling Wi-Fi and scanning again. Each round checked immediate reads and 250/1000/3000 ms idle reads. The test used an exclusive temporary file with position-dependent data, removed it afterward, and restored the original Wi-Fi active state without connecting to a network.

EV-MUX RPC, filesystem contracts, script upload/execution, and JPEG capture passed again with exFAT enabled. The final reset resumed the default camera/LCD preview and delivered 147 valid 320x240 USB preview frames over 20 seconds without a restart. Before that reset, the SD volume was mounted, `main.py` matched the board default, and no test scripts remained.

After restarting the final firmware, the operator confirmed HUSB enumeration on Windows: a CDC COM port and two MSC drive letters appeared. Subsequent tests accessed the Windows host through WSL's PowerShell interoperability. HUSB CDC passed EV-MUX `hello` and `device.control` status requests and received 69 valid 320x240 JPEG preview frames in five seconds, with increasing timestamps. Closing the CDC test left the default preview running.

Windows device identities, capacities, and LUNs confirmed the two MSC volumes as onboard Flash (LUN 0, FAT) and SD (LUN 1, exFAT); drive-letter assignments are host-specific. Each volume passed an exclusive 32 KiB temporary-file write with `WriteThrough` and `Flush(true)`, followed by complete comparisons after requested 250/1000/3000 ms file-operation intervals. Both test files were deleted. These checks establish host file read/write consistency; reopened reads may use Windows caches, and they do not establish power-loss durability. The absent-panel timeout still needs a physical disconnect test.

### Web IDE verification (2026-09-08)

The official Web IDE connected to HUSB from Windows Chrome and loaded the device file list. Saving and running a camera script passed: the terminal reported ten successful 320x240 captures and a mounted SD volume, and the browser decoded the preview at 320x240. The automated new-file workflow was not counted as passing: it edited the previous buffer before the new file became active and temporarily replaced the default `main.py`. Cleanup restored and compared `main.py` against `board_inisetup.MAIN_PY` and removed the temporary file. The isolated test browser was closed and HUSB forwarding to WSL was restored. The default camera/LCD script resumed and delivered 154 valid 320x240 USB preview frames in 20 seconds, with increasing timestamps and no LCD initialization error.

### Hand Detect runtime check (2026-09-08)

The `espdet_pico.py` example was run with the repository's `espdet_pico_224_224_hand.espdl` model, RGB565 QVGA input, `score=0.5`, and `nms=0.7`. The runtime copy added LCD output and timing/result assertions. The 497296-byte model was uploaded to SD and its SHA-256 matched `4d5b80a5d4a6ee10d5a3a62ddf47f06dba754d1a54bd95004d31262094a0d3c1`. Model loading took 149 ms. A 130-second observation received 1003 QVGA USB preview frames; the last periodic report covered 990 inference calls with a mean `detect()` duration of 61.28 ms and maximum of 62 ms, approximately 7.8 fps including camera, drawing, LCD, preview, and the example's delay. No detection-result assertion or runtime error was observed. The decoded preview showed the overhead lighting without a hand, and all detection lists were empty, so positive hand recognition remains unverified.

The test left `/sdcard/hand_detect.py` and `/sdcard/espdet_pico_224_224_hand.espdl` on the device for reuse. `main.py` was checked against its pre-test SHA-256 and was unchanged. Temporary command scripts were removed, and stopping the example released its model, camera, and LCD resources. The hand example was then started again for live LCD/USB viewing; resetting the board still runs the original default preview.

## Source and licensing

Pin definitions and the 480x640 panel command table follow [WT_BSP](https://github.com/Wireless-TAG-Maker/WT_BSP/tree/9d1050d0957aaa050e635ff1f22a686881e54444), specifically `components/wt_bsp/boards/WT9932P4C61-TINY/board.c` and `components/wt_bsp/features/dsi/wt_bsp_dsi.c`. The command table in `../bmgr/st7102_init.h` retains its source notice; WT_BSP's `LICENSE` and component manifest declare Apache-2.0, and the source file contains no separate license override.

The registry dependency `kevincoooool/esp_lcd_st7102` is pinned to **1.0.0**. Its C/H files carry Apache-2.0 SPDX identifiers, and its archive includes the Apache-2.0 license. Later releases add conflicting use restrictions to their source headers, so do not relax this version pin without reviewing every source file. The board's CMake adds the IDF 6 `color_space` to `rgb_ele_order` compatibility alias only to this driver's compilation.
