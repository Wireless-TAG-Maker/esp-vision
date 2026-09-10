/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdcard.h"
#include "storage.h"

#include "esp_log.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "sdmmc_cmd.h"

#include "boardconfig.h"

static const char *TAG = "wt_s31_sdcard";
static sd_pwr_ctrl_handle_t s_sdcard_ldo_handle;

void esp_vision_board_sdcard_init0(void)
{
    // The card VDD and external pullups are connected to the board's 3.3 V rail.
}

bool esp_vision_board_sdcard_is_present(void)
{
    // The socket has no separate card-detect signal; initialization probes it.
    return true;
}

esp_err_t esp_vision_board_sdcard_preinit_host(sdmmc_host_t *host, int slot)
{
    if ((host == NULL) || (slot != ESP_VISION_SDCARD_SLOT)) {
        return ESP_OK;
    }

    // A second SDCard owner could deinitialize the slot used by storage.
    if (esp_vision_storage_sdcard_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    host->pwr_ctrl_handle = NULL;
    if (s_sdcard_ldo_handle == NULL) {
        const sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = ESP_VISION_SDCARD_LDO_CHAN_ID,
        };
        sd_pwr_ctrl_handle_t ldo_handle = NULL;
        esp_err_t ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &ldo_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to acquire SD IO LDO: %s", esp_err_to_name(ret));
            return ret;
        }
        s_sdcard_ldo_handle = ldo_handle;
    }

    host->slot = ESP_VISION_SDCARD_SLOT;
    host->max_freq_khz = SDMMC_FREQ_DEFAULT;
    // S31's SD IO LDO is fixed at 1.8 V and may be shared with PSRAM/MPLL.
    host->io_voltage = 1.8f;
    host->unaligned_multi_block_rw_max_chunk_size = 8;
    host->pwr_ctrl_handle = s_sdcard_ldo_handle;
    return ESP_OK;
}

void esp_vision_board_sdcard_deinit_host(sdmmc_host_t *host, int slot)
{
    if ((host == NULL) || (slot != ESP_VISION_SDCARD_SLOT)) {
        return;
    }

    // Retain one board-lifetime LDO reference across mount retries and soft resets.
    // The host/slot lifetime is managed by the caller and the SDMMC driver.
    host->pwr_ctrl_handle = NULL;
}
