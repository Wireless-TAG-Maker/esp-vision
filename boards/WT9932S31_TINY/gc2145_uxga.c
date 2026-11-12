/*
 * SPDX-FileCopyrightText: 2026 Wireless-Tag
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "esp_log.h"
#include "esp_video_ioctl.h"
#include "gc2145.h"
#include "gc2145_uxga.h"

#define ESP_VISION_GC2145_FPS               8
#define ESP_VISION_GC2145_PAGE_SELECT_REG   0xfe
#define ESP_VISION_GC2145_OUTPUT_FORMAT_REG 0x84
#define ESP_VISION_GC2145_OUTPUT_RGB565     0x06
#define ESP_VISION_GC2145_OUTPUT_UYVY       0x00
#define ESP_VISION_GC2145_CLOCK_DIVIDER_REG 0xfa
#define ESP_VISION_GC2145_CLOCK_DIVIDER_2   0x10

static const char *TAG = "gc2145_uxga";

/* gc2145_set_format() stores this pointer instead of copying the descriptor. */
static esp_cam_sensor_format_t s_uxga_format;

static esp_err_t esp_vision_gc2145_sensor_control(int fd, uint32_t command,
                                                  void *data, size_t size)
{
    struct v4l2_ext_control control = {
        .id = command,
        .p_u8 = data,
        .size = size,
    };
    struct v4l2_ext_controls controls = {
        .ctrl_class = V4L2_CTRL_CLASS_ESP_CAM_IOCTL,
        .count = 1,
        .controls = &control,
    };
    int request = command == ESP_CAM_SENSOR_IOC_S_REG ? VIDIOC_S_EXT_CTRLS : VIDIOC_G_EXT_CTRLS;

    if (ioctl(fd, request, &controls) != 0) {
        ESP_LOGE(TAG, "Sensor control 0x%x failed (errno=%d)", (unsigned int)command, errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static bool esp_vision_gc2145_is_uxga(const esp_cam_sensor_format_t *format, int fps)
{
    return format->port == ESP_CAM_SENSOR_DVP &&
           format->width == 1600 && format->height == 1200 &&
           format->xclk == 20000000 && format->fps == fps;
}

esp_err_t esp_vision_gc2145_configure_uxga(int capture_fd)
{
    esp_cam_sensor_id_t sensor_id = {0};
    esp_cam_sensor_format_t base_format = {0};

    if (capture_fd < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_uxga_format.regs != NULL) {
        ESP_LOGE(TAG, "UXGA format has already been submitted");
        return ESP_ERR_INVALID_STATE;
    }
    if (esp_vision_gc2145_sensor_control(capture_fd, ESP_CAM_SENSOR_IOC_G_CHIP_ID,
                                         &sensor_id, sizeof(sensor_id)) != ESP_OK) {
        return ESP_FAIL;
    }
    if (sensor_id.pid != GC2145_PID) {
        ESP_LOGE(TAG, "Refusing GC2145 format change for PID=0x%x", (unsigned int)sensor_id.pid);
        return ESP_ERR_INVALID_STATE;
    }
    if (ioctl(capture_fd, VIDIOC_G_SENSOR_FMT, &base_format) != 0) {
        ESP_LOGE(TAG, "Could not read base sensor format (errno=%d)", errno);
        return ESP_FAIL;
    }
    if (!esp_vision_gc2145_is_uxga(&base_format, 13) ||
            base_format.format != ESP_CAM_SENSOR_PIXFORMAT_RGB565_BE ||
            base_format.regs == NULL || base_format.regs_size <= 0) {
        ESP_LOGE(TAG, "Expected GC2145 DVP RGB565_BE 1600x1200@13fps, 20 MHz base format");
        return ESP_ERR_INVALID_STATE;
    }

    gc2145_reginfo_t *registers = calloc(base_format.regs_size, sizeof(*registers));
    if (registers == NULL) {
        ESP_LOGE(TAG, "Could not allocate UXGA register table");
        return ESP_ERR_NO_MEM;
    }
    memcpy(registers, base_format.regs, (size_t)base_format.regs_size * sizeof(*registers));

    /*
     * esp_cam_sensor 2.3.0 gc2145_regs.h and gc2145_settings.h define page 0
     * 0x84=0x06 as RGB565, and 0x84=0x00 as Cb Y Cr Y (UYVY). Preserve the
     * upstream UXGA crop, AEC/AWB and ISP table. The final global 0xfa write
     * also selects the two-way output divider: without it this board receives
     * only about 2.31 MB of a 3.84 MB frame. With 0xfa=0x10, DMA receives full
     * UXGA frames at about 8.1 fps; the sensor descriptor uses 8 fps.
     */
    uint8_t page = 0xff;
    unsigned int output_format_count = 0;
    int divider_index = -1;
    for (int i = 0; i < base_format.regs_size; i++) {
        if (registers[i].reg == ESP_VISION_GC2145_CLOCK_DIVIDER_REG) {
            divider_index = i;
        }
        if (registers[i].reg == ESP_VISION_GC2145_PAGE_SELECT_REG) {
            page = registers[i].val & 0x07;
        } else if (page == 0 && registers[i].reg == ESP_VISION_GC2145_OUTPUT_FORMAT_REG) {
            if (registers[i].val != ESP_VISION_GC2145_OUTPUT_RGB565) {
                ESP_LOGE(TAG, "Unexpected output format in the GC2145 base register table");
                free(registers);
                return ESP_ERR_INVALID_STATE;
            }
            registers[i].val = ESP_VISION_GC2145_OUTPUT_UYVY;
            output_format_count++;
        }
    }
    if (output_format_count != 1) {
        ESP_LOGE(TAG, "Expected one GC2145 output-format register, found %u", output_format_count);
        free(registers);
        return ESP_ERR_INVALID_STATE;
    }
    if (divider_index < 0 || registers[divider_index].val != 0x00) {
        ESP_LOGE(TAG, "Unexpected GC2145 base output-clock divider");
        free(registers);
        return ESP_ERR_INVALID_STATE;
    }
    registers[divider_index].val = ESP_VISION_GC2145_CLOCK_DIVIDER_2;

    s_uxga_format = base_format;
    s_uxga_format.name = "DVP_8bit_20Minput_YUV422_UYVY_1600x1200_8fps";
    s_uxga_format.format = ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY;
    s_uxga_format.fps = ESP_VISION_GC2145_FPS;
    s_uxga_format.regs = registers;

    /*
     * This ioctl writes the sensor AND updates esp_video's DVP/V4L2 metadata.
     * Do not free registers after submission, even on error: the driver may
     * already retain s_uxga_format when esp_video reports a later failure.
     */
    if (ioctl(capture_fd, VIDIOC_S_SENSOR_FMT, &s_uxga_format) != 0) {
        ESP_LOGE(TAG, "Could not configure GC2145 UXGA UYVY (errno=%d)", errno);
        return ESP_FAIL;
    }

    esp_cam_sensor_format_t configured = {0};
    if (ioctl(capture_fd, VIDIOC_G_SENSOR_FMT, &configured) != 0) {
        ESP_LOGE(TAG, "Could not verify GC2145 UXGA format (errno=%d)", errno);
        return ESP_FAIL;
    }
    if (!esp_vision_gc2145_is_uxga(&configured, ESP_VISION_GC2145_FPS) ||
            configured.format != ESP_CAM_SENSOR_PIXFORMAT_YUV422_UYVY) {
        ESP_LOGE(TAG, "GC2145 sensor format does not match UXGA UYVY");
        return ESP_ERR_INVALID_STATE;
    }

    esp_cam_sensor_reg_val_t reg_value = {
        .regaddr = ESP_VISION_GC2145_PAGE_SELECT_REG,
        .value = 0x00,
    };
    if (esp_vision_gc2145_sensor_control(capture_fd, ESP_CAM_SENSOR_IOC_S_REG,
                                         &reg_value, sizeof(reg_value)) != ESP_OK) {
        return ESP_FAIL;
    }
    const gc2145_reginfo_t expected[] = {
        {ESP_VISION_GC2145_OUTPUT_FORMAT_REG, ESP_VISION_GC2145_OUTPUT_UYVY},
        {ESP_VISION_GC2145_CLOCK_DIVIDER_REG, ESP_VISION_GC2145_CLOCK_DIVIDER_2},
        {0x90, 0x01}, {0x95, 0x04}, {0x96, 0xb0}, {0x97, 0x06}, {0x98, 0x40},
        {0x99, 0x11},
    };
    for (unsigned int i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        reg_value.regaddr = expected[i].reg;
        if (esp_vision_gc2145_sensor_control(capture_fd, ESP_CAM_SENSOR_IOC_G_REG,
                                             &reg_value, sizeof(reg_value)) != ESP_OK) {
            return ESP_FAIL;
        }
        if (reg_value.value != expected[i].val) {
            ESP_LOGE(TAG, "GC2145 mode register mismatch: 0x%02x=0x%02x, expected 0x%02x",
                     expected[i].reg, (unsigned int)reg_value.value, expected[i].val);
            return ESP_ERR_INVALID_STATE;
        }
    }

    ESP_LOGI(TAG, "GC2145 UXGA configured: UYVY 1600x1200@8fps, XCLK=20000000 Hz (0x84=0x00, 0xfa=0x10)");
    return ESP_OK;
}

void esp_vision_gc2145_release_uxga(void)
{
    /* Only called after esp_video has destroyed the sensor that owns this pointer. */
    free((void *)s_uxga_format.regs);
    memset(&s_uxga_format, 0, sizeof(s_uxga_format));
}
