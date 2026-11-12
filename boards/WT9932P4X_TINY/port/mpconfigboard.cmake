set(IDF_TARGET esp32p4)

set(SDKCONFIG_DEFAULTS
    boards/sdkconfig.base
    boards/WT9932P4X_TINY/sdkconfig.wt9932p4x_tiny
    boards/WT9932P4X_TINY/sdkconfig.defaults.board
    boards/WT9932P4X_TINY/sdkconfig.board
)

set(MICROPY_PY_BTREE OFF)
