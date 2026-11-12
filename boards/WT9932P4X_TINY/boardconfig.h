/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ESP_VISION_BOARD_CONFIG_H
#define ESP_VISION_BOARD_CONFIG_H

#define ESP_VISION_BOARD_ARCH                       "ESP32P4"
#define ESP_VISION_BOARD_TYPE                       "WT9932P4X_TINY"
#define ESP_VISION_PORT_ESP32                       (1)

#define ESP_VISION_IMLIB_PROFILER_ENABLE            (0)
#define ESP_VISION_IMLIB_GPU_ENABLE                 (0)
#define ESP_VISION_IMLIB_JPEG_CODEC_ENABLE          (0)

#define ESP_VISION_CACHE_LINE_SIZE                  (32)
#define ESP_VISION_ALLOC_ALIGNMENT                  (ESP_VISION_CACHE_LINE_SIZE)
#define ESP_VISION_DMA_ALIGNMENT                    (ESP_VISION_CACHE_LINE_SIZE)

#define ESP_VISION_JPEG_QUALITY_LOW                 (60)
#define ESP_VISION_JPEG_QUALITY_HIGH                (60)
#define ESP_VISION_JPEG_QUALITY_THRESHOLD           (320 * 240 * 2)

#define ESP_VISION_CAMERA_SENSOR_ID                 (0x2336)
#define ESP_VISION_CAMERA_RAW_INPUT_WIDTH           (1024)
#define ESP_VISION_CAMERA_RAW_INPUT_HEIGHT          (600)
#define ESP_VISION_CAMERA_ACTIVE_INPUT_WIDTH        (640)
#define ESP_VISION_CAMERA_ACTIVE_INPUT_HEIGHT       (480)
#define ESP_VISION_CAMERA_PPA_OUTPUT_QQVGA_WIDTH    (160)
#define ESP_VISION_CAMERA_PPA_OUTPUT_QQVGA_HEIGHT   (120)
#define ESP_VISION_CAMERA_PPA_OUTPUT_QVGA_WIDTH     (320)
#define ESP_VISION_CAMERA_PPA_OUTPUT_QVGA_HEIGHT    (240)
#define ESP_VISION_CAMERA_PPA_OUTPUT_VGA_WIDTH      (640)
#define ESP_VISION_CAMERA_PPA_OUTPUT_VGA_HEIGHT     (480)
#define ESP_VISION_CAMERA_BUFFER_COUNT              (3)

#define ESP_VISION_LCD_WIDTH                        (480)
#define ESP_VISION_LCD_HEIGHT                       (640)

#define ESP_VISION_SDCARD_MOUNT_PATH                "/sdcard"
#define ESP_VISION_SDCARD_SLOT                      (0)
#define ESP_VISION_SDCARD_BUS_WIDTH                 (4)
#define ESP_VISION_SDCARD_LDO_CHAN_ID               (4)
#define ESP_VISION_SDCARD_CLK_PIN                   (43)
#define ESP_VISION_SDCARD_CMD_PIN                   (44)
#define ESP_VISION_SDCARD_D0_PIN                    (39)
#define ESP_VISION_SDCARD_D1_PIN                    (40)
#define ESP_VISION_SDCARD_D2_PIN                    (41)
#define ESP_VISION_SDCARD_D3_PIN                    (42)

#define ESP_VISION_TRANSPORT_DEFAULT_SINK          "usj"
#define ESP_VISION_TRANSPORT_FALLBACK_SINK         "usj"
#define ESP_VISION_ENABLE_BACKGROUND_NETWORK       (0)
#define ESP_VISION_WIFI_DEFAULT_SSID                ""
#define ESP_VISION_WIFI_DEFAULT_PASSWORD            ""
#define ESP_VISION_ENABLE_TCP_TRANSPORT            (0)
#define ESP_VISION_TCP_PORT                        (3232)

#endif /* ESP_VISION_BOARD_CONFIG_H */
