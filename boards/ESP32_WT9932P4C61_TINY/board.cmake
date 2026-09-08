# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

set(ESP_VISION_ENABLE_BARCODE ON)
set(ESP_VISION_USE_BOARD_MANAGER ON)
set(ESP_VISION_BOARD_MANAGER_SETUP_SOURCES
    "${ESP_VISION_BOARD_DIR}/bmgr/setup_device.c"
    "${ESP_VISION_BOARD_DIR}/bmgr/dsi_dma.c"
)
set(ESP_VISION_BOARD_MANAGER_EXTRA_COMPONENTS kevincoooool__esp_lcd_st7102)

if(TARGET __idf_fatfs)
    include("${CMAKE_CURRENT_LIST_DIR}/fatfs.cmake")
endif()

# Pin the release with unambiguous Apache-2.0 headers. Adapt its IDF 5
# field name only for this component, without modifying registry sources.
if(IDF_VERSION_MAJOR GREATER_EQUAL 6 AND TARGET __idf_kevincoooool__esp_lcd_st7102)
    target_compile_definitions(__idf_kevincoooool__esp_lcd_st7102 PRIVATE color_space=rgb_ele_order)
endif()

# Track the LCD DMA channel so panel deletion can stop it before IDF 6.0
# disables the DSI clock. CSI may still own the shared DMA controller.
if(IDF_VERSION_MAJOR EQUAL 6 AND IDF_VERSION_MINOR EQUAL 0 AND TARGET __idf_esp_lcd)
    target_compile_definitions(__idf_esp_lcd PRIVATE
        dw_gdma_new_channel=esp_vision_wt9932p4c61_lcd_new_dma_channel
        dw_gdma_del_channel=esp_vision_wt9932p4c61_lcd_del_dma_channel
    )
endif()
