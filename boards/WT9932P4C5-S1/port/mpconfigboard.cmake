set(IDF_TARGET esp32p4)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/WT9932P4C5-S1/sdkconfig.wt9932p4c5_s1
    boards/WT9932P4C5-S1/sdkconfig.board
)

# Keep the first bring-up independent of optional MicroPython submodules.
set(MICROPY_PY_BTREE OFF)
