/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dsi_dma.h"

#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0) && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 1, 0)
#include "esp_lcd_panel_interface.h"
#include "esp_log.h"
#include "esp_private/dw_gdma.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/dw_gdma_ll.h"

/* This board has one LCD. Only the LCD component calls these adapters;
 * camera DMA allocations keep using the unmodified ESP-IDF functions. */
static dw_gdma_channel_handle_t s_lcd_dma;
static esp_err_t (*s_panel_delete)(esp_lcd_panel_t *panel);

esp_err_t esp_vision_wt9932p4c61_lcd_new_dma_channel(const dw_gdma_channel_alloc_config_t *config,
                                                     dw_gdma_channel_handle_t *channel)
{
    if (s_lcd_dma != NULL) {
        /* A failed stop must not lose the still-owned DMA channel. */
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = dw_gdma_new_channel(config, channel);
    if (ret == ESP_OK) {
        s_lcd_dma = *channel;
    }
    return ret;
}

esp_err_t esp_vision_wt9932p4c61_lcd_del_dma_channel(dw_gdma_channel_handle_t channel)
{
    esp_err_t ret = dw_gdma_del_channel(channel);
    if ((ret == ESP_OK) && (s_lcd_dma == channel)) {
        s_lcd_dma = NULL;
    }
    return ret;
}

static esp_err_t wt9932p4c61_panel_delete(esp_lcd_panel_t *panel)
{
    if (s_lcd_dma != NULL) {
        int channel_id;
        esp_err_t ret = dw_gdma_channel_get_id(s_lcd_dma, &channel_id);
        if (ret != ESP_OK) {
            return ret;
        }
        dw_gdma_event_callbacks_t callbacks = {0};
        ret = dw_gdma_channel_register_event_callbacks(s_lcd_dma, &callbacks, NULL);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = dw_gdma_channel_enable_ctrl(s_lcd_dma, false);
        if (ret != ESP_OK) {
            return ret;
        }
        /* Keep DSI running until its DMA has drained. Disabling the bridge
         * first leaves the channel waiting for a peripheral handshake. */
        int64_t deadline = esp_timer_get_time() + 100000;
        while (DW_GDMA_LL_GET_HW(0)->chen0.val & (1U << channel_id)) {
            if (esp_timer_get_time() >= deadline) {
                ESP_LOGE("wt9932p4c61_lcd", "LCD DMA did not stop before panel deletion");
                return ESP_ERR_TIMEOUT;
            }
            vTaskDelay(1);
        }
    }
    esp_err_t ret = s_panel_delete(panel);
    if (ret == ESP_OK) {
        s_panel_delete = NULL;
    }
    return ret;
}

void esp_vision_wt9932p4c61_lcd_prepare_delete(esp_lcd_panel_handle_t panel)
{
    s_panel_delete = panel->del;
    panel->del = wt9932p4c61_panel_delete;
}
#else
void esp_vision_wt9932p4c61_lcd_prepare_delete(esp_lcd_panel_handle_t panel)
{
    (void)panel;
}
#endif
