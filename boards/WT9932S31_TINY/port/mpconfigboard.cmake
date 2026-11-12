set(IDF_TARGET esp32s31)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/WT9932S31_TINY/sdkconfig.wt9932s31_tiny
    boards/WT9932S31_TINY/sdkconfig.defaults.board
    boards/WT9932S31_TINY/sdkconfig.board
)

set(MICROPY_PY_BTREE OFF)
