set(IDF_TARGET esp32p4)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/sdkconfig.p4_wifi_common
    boards/WT9932P4C61_TINY/sdkconfig.wt9932p4c61_tiny
    boards/WT9932P4C61_TINY/sdkconfig.defaults.board
    boards/WT9932P4C61_TINY/sdkconfig.board
)

# Keep the first bring-up independent of optional MicroPython submodules.
set(MICROPY_PY_BTREE OFF)
