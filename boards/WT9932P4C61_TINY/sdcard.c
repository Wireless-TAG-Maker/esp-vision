/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdcard.h"

#include "esp_idf_version.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

#include "boardconfig.h"

static sd_pwr_ctrl_handle_t s_sdcard_ldo_handle;

void esp_vision_board_sdcard_init0(void)
{
}

bool esp_vision_board_sdcard_is_present(void)
{
    /* No card-detect signal; mounting probes the socket. */
    return true;
}

esp_err_t esp_vision_board_sdcard_preinit_host(sdmmc_host_t *host, int slot)
{
    if ((host == NULL) || (slot != ESP_VISION_SDCARD_SLOT)) {
        return ESP_ERR_INVALID_ARG;
    }

    host->slot = ESP_VISION_SDCARD_SLOT;
    host->max_freq_khz = SDMMC_FREQ_DEFAULT;
    host->pwr_ctrl_handle = NULL;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    /* Release a failed SD mount's slot so it can be retried. IDF 6 keeps
     * the controller alive while the C61 still owns slot 1. */
    host->flags |= SDMMC_HOST_FLAG_DEINIT_ARG;
    host->deinit_p = sdmmc_host_deinit_slot;
#endif

    if (s_sdcard_ldo_handle == NULL) {
        const sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = ESP_VISION_SDCARD_LDO_CHAN_ID,
        };
        esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &s_sdcard_ldo_handle);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    host->pwr_ctrl_handle = s_sdcard_ldo_handle;
    return ESP_OK;
}

void esp_vision_board_sdcard_deinit_host(sdmmc_host_t *host, int slot)
{
    (void)slot;

    if (host != NULL) {
        host->pwr_ctrl_handle = NULL;
    }
    /* Keep the board-owned signal supply enabled across failed mounts and
     * MicroPython soft resets, as on the other P4 boards. */
}
