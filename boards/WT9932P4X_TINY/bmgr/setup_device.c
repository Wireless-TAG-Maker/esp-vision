/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>

#include "dev_display_lcd.h"
#include "driver/gpio.h"
#include "esp_lcd_st7102.h"
#include "esp_lcd_touch_st7123.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "st7102_init.h"

esp_err_t lcd_dsi_panel_factory_entry_t(esp_lcd_dsi_bus_handle_t dsi_handle,
                                        dev_display_lcd_config_t *lcd_cfg,
                                        dev_display_lcd_handles_t *lcd_handles)
{
    if ((dsi_handle == NULL) || (lcd_cfg == NULL) || (lcd_handles == NULL) ||
            (lcd_handles->io_handle == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    int reset_pin = lcd_cfg->sub_cfg.dsi.reset_gpio_num;
    bool reset_active_high = lcd_cfg->sub_cfg.dsi.reset_active_high;
    esp_err_t ret;
    if (reset_pin >= 0) {
        /* The ST7102 constructor reads the ID before Board Manager resets it. */
        const uint64_t reset_pin_mask = UINT64_C(1) << reset_pin;
        const gpio_config_t reset_gpio = {
            .pin_bit_mask = reset_pin_mask,
            .mode = GPIO_MODE_OUTPUT,
        };
        ret = gpio_config(&reset_gpio);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = gpio_set_level(reset_pin, reset_active_high);
        if (ret != ESP_OK) {
            gpio_reset_pin(reset_pin);
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        ret = gpio_set_level(reset_pin, !reset_active_high);
        if (ret != ESP_OK) {
            gpio_reset_pin(reset_pin);
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(120));
    }

    st7102_vendor_config_t vendor = {
        .init_cmds = s_st7102_vendor_specific_init,
        .init_cmds_size = sizeof(s_st7102_vendor_specific_init) / sizeof(s_st7102_vendor_specific_init[0]),
        .mipi_config = {
            .dsi_bus = dsi_handle,
            .dpi_config = &lcd_cfg->sub_cfg.dsi.dpi_config,
        },
        .flags.use_mipi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t panel = {
        .reset_gpio_num = reset_pin,
        .rgb_ele_order = lcd_cfg->rgb_ele_order,
        .data_endian = lcd_cfg->data_endian,
        .bits_per_pixel = lcd_cfg->bits_per_pixel,
        .flags.reset_active_high = reset_active_high,
        .vendor_config = &vendor,
    };
    ret = esp_lcd_new_panel_st7102(lcd_handles->io_handle, &panel, &lcd_handles->panel_handle);
    if ((ret != ESP_OK) && (reset_pin >= 0)) {
        gpio_reset_pin(reset_pin);
    }
    return ret;
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_touch_config_t *touch_config,
                                    esp_lcd_touch_handle_t *touch_handle)
{
    return esp_lcd_touch_new_i2c_st7123(io, touch_config, touch_handle);
}
