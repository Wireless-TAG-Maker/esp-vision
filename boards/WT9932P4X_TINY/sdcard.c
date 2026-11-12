/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdcard.h"

#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#include "boardconfig.h"

static sd_pwr_ctrl_handle_t s_sdcard_ldo_handle;

esp_err_t esp_vision_board_sdcard_preinit_host(sdmmc_host_t *host, int slot)
{
    if (host == NULL || slot != ESP_VISION_SDCARD_SLOT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_sdcard_ldo_handle == NULL) {
        const sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = ESP_VISION_SDCARD_LDO_CHAN_ID,
        };
        esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &s_sdcard_ldo_handle);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    host->io_voltage = 3.3f;
    host->pwr_ctrl_handle = s_sdcard_ldo_handle;
    return ESP_OK;
}

void esp_vision_board_sdcard_deinit_host(sdmmc_host_t *host, int slot)
{
    if (host != NULL && slot == ESP_VISION_SDCARD_SLOT) {
        host->pwr_ctrl_handle = NULL;
    }
    // Keep the board-owned signal supply enabled across mount retries.
}
