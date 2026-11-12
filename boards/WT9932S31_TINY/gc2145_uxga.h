/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

/*
 * Configure GC2145 UYVY from the driver's 1600x1200 RGB565_BE/20 MHz/13 fps
 * default. Call once after opening DVP, before allocating buffers or streaming.
 * The caller must already supply a 20 MHz XCLK. The format and register copy
 * remain allocated for the device lifetime, including failures after submission.
 * Release them only after successful camera-device deinitialization.
 */
esp_err_t esp_vision_gc2145_configure_uxga(int capture_fd);

void esp_vision_gc2145_release_uxga(void);
