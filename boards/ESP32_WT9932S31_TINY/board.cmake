# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

if(NOT ESP_VISION_IDF_OVERLAY STREQUAL "release6.1")
    message(FATAL_ERROR "ESP32_WT9932S31_TINY requires ESP-IDF release/v6.1")
endif()

set(ESP_VISION_USE_BOARD_MANAGER ON)
set(ESP_VISION_BOARD_MANAGER_SETUP_SOURCES
    "${ESP_VISION_BOARD_DIR}/gc2145_uxga.c"
)
