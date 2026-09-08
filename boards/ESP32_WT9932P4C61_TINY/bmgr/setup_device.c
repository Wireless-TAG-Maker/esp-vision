/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dev_display_lcd.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_st7102.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/mipi_dsi_host_ll.h"

#include "boardconfig.h"
#include "dsi_dma.h"
#include "st7102_init.h"

/* ESP-IDF's DBI read waits indefinitely for a response. Bound the ST7102
 * constructor's ID read so an absent panel cannot block the Python VM.
 * DSI works independently of the camera/touch I2C bus on this module. */
static esp_err_t wt9932p4c61_read_panel_id(esp_lcd_panel_io_t *io, int command,
                                           void *param, size_t param_size)
{
    (void)io;
    if ((command != 0x04) || (param == NULL) || (param_size != 3)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    dsi_host_dev_t *host = MIPI_DSI_LL_GET_HOST(ESP_VISION_LCD_MIPI_DSI_BUS_ID);
    int64_t deadline = esp_timer_get_time() + 200000;
    mipi_dsi_host_ll_enable_video_mode(host, false);
    mipi_dsi_host_ll_enable_bta(host, true);
    mipi_dsi_host_ll_gen_set_rx_vcid(host, 0);

    const mipi_dsi_data_type_t packets[] = {
        MIPI_DSI_DT_SET_MAXIMUM_RETURN_PKT,
        MIPI_DSI_DT_DCS_READ_0,
    };
    const uint8_t data[] = {3, 0x04};
    for (size_t i = 0; i < sizeof(data); ++i) {
        while (mipi_dsi_host_ll_gen_is_cmd_fifo_full(host)) {
            if (esp_timer_get_time() >= deadline) {
                return ESP_ERR_TIMEOUT;
            }
            vTaskDelay(1);
        }
        mipi_dsi_host_ll_gen_set_packet_header(host, 0, packets[i], 0, data[i]);
    }

    while (mipi_dsi_host_ll_gen_is_read_cmd_busy(host) ||
            mipi_dsi_host_ll_gen_is_read_fifo_empty(host)) {
        if (esp_timer_get_time() >= deadline) {
            ESP_LOGW("wt9932p4c61_lcd", "DSI panel ID read timed out");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(1);
    }

    uint32_t value = mipi_dsi_host_ll_gen_read_payload_fifo(host);
    uint8_t *id = param;
    for (size_t i = 0; i < param_size; ++i) {
        id[i] = (value >> (8 * i)) & 0xff;
    }
    return ESP_OK;
}

esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle,
                                        dev_display_lcd_config_t *lcd_cfg,
                                        dev_display_lcd_handles_t *lcd_handles)
{
    if ((dsi_handle == NULL) || (lcd_cfg == NULL) || (lcd_handles == NULL) ||
            (lcd_handles->io_handle == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((lcd_cfg->sub_cfg.dsi.dbi_config.virtual_channel != 0) ||
            (lcd_cfg->sub_cfg.dsi.dbi_config.lcd_cmd_bits != 8)) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    esp_err_t ret = ESP_OK;

    /* This driver reads the panel ID in its constructor, before Board Manager
     * can call panel_reset(). Deassert reset before that first DSI read. */
    int reset_pin = lcd_cfg->sub_cfg.dsi.reset_gpio_num;
    if (reset_pin >= 0) {
        const gpio_config_t reset_gpio = {
            .pin_bit_mask = 1ULL << reset_pin,
                                 .mode = GPIO_MODE_OUTPUT,
        };
        ret = gpio_config(&reset_gpio);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = gpio_set_level(reset_pin, lcd_cfg->sub_cfg.dsi.reset_active_high);
        if (ret != ESP_OK) {
            gpio_reset_pin(reset_pin);
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = gpio_set_level(reset_pin, !lcd_cfg->sub_cfg.dsi.reset_active_high);
        if (ret != ESP_OK) {
            gpio_reset_pin(reset_pin);
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    const st7102_vendor_config_t vendor = {
        .init_cmds = s_st7102_vendor_specific_init,
        .init_cmds_size = sizeof(s_st7102_vendor_specific_init) / sizeof(s_st7102_vendor_specific_init[0]),
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &lcd_cfg->sub_cfg.dsi.dpi_config,
        },
        .flags.use_mipi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t panel = {
        .reset_gpio_num = lcd_cfg->sub_cfg.dsi.reset_gpio_num,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
        .data_endian = lcd_cfg->data_endian,
        .bits_per_pixel = lcd_cfg->bits_per_pixel,
        .flags.reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high,
        .vendor_config = (void *) &vendor,
    };
    esp_lcd_panel_io_t *io = lcd_handles->io_handle;
    esp_err_t (*read_param)(esp_lcd_panel_io_t *, int, void *, size_t) = io->rx_param;
    io->rx_param = wt9932p4c61_read_panel_id;
    ret = esp_lcd_new_panel_st7102(io, &panel, &lcd_handles->panel_handle);
    io->rx_param = read_param;
    if (ret == ESP_OK) {
        esp_vision_wt9932p4c61_lcd_prepare_delete(lcd_handles->panel_handle);
    }
    if ((ret != ESP_OK) && (reset_pin >= 0)) {
        gpio_reset_pin(reset_pin);
    }
    return ret;
}
