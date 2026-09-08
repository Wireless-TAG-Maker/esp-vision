# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Keep the installed SDK untouched. Copy the core sources because their quoted
# ff.h includes otherwise bypass an overridden public include directory.
get_target_property(wt_fatfs_configured __idf_fatfs ESP_VISION_WT_EXFAT)
if(wt_fatfs_configured)
    return()
endif()

idf_component_get_property(wt_fatfs_component_dir fatfs COMPONENT_DIR)
set(wt_fatfs_source_dir "${wt_fatfs_component_dir}/src")
set(wt_fatfs_build_dir "${CMAKE_BINARY_DIR}/esp_vision_fatfs")
file(MAKE_DIRECTORY "${wt_fatfs_build_dir}")
foreach(name ff.c ffunicode.c ff.h diskio.h)
    configure_file("${wt_fatfs_source_dir}/${name}" "${wt_fatfs_build_dir}/${name}" COPYONLY)
endforeach()
configure_file("${CMAKE_CURRENT_LIST_DIR}/fatfs_config.h.in"
               "${wt_fatfs_build_dir}/ffconf.h" @ONLY)

get_target_property(wt_fatfs_sources __idf_fatfs SOURCES)
set(wt_fatfs_replaced 0)
foreach(name ff.c ffunicode.c)
    set(wt_fatfs_updated_sources)
    foreach(source IN LISTS wt_fatfs_sources)
        get_filename_component(absolute_source "${source}" ABSOLUTE BASE_DIR "${wt_fatfs_component_dir}")
        if(absolute_source STREQUAL "${wt_fatfs_source_dir}/${name}")
            list(APPEND wt_fatfs_updated_sources "${wt_fatfs_build_dir}/${name}")
            math(EXPR wt_fatfs_replaced "${wt_fatfs_replaced} + 1")
        else()
            list(APPEND wt_fatfs_updated_sources "${source}")
        endif()
    endforeach()
    set(wt_fatfs_sources ${wt_fatfs_updated_sources})
endforeach()
if(NOT wt_fatfs_replaced EQUAL 2)
    message(FATAL_ERROR "WT9932P4C61-TINY: ESP-IDF FatFs source layout changed")
endif()
set_property(TARGET __idf_fatfs PROPERTY SOURCES ${wt_fatfs_sources})

# Replace both private and public header paths: every FIL/FATFS/FSIZE_t user,
# including the MicroPython qstr pass, must see the same exFAT configuration.
foreach(property INCLUDE_DIRECTORIES INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(wt_fatfs_includes __idf_fatfs ${property})
    list(FIND wt_fatfs_includes "${wt_fatfs_source_dir}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "WT9932P4C61-TINY: ESP-IDF FatFs include layout changed")
    endif()
    list(REMOVE_AT wt_fatfs_includes ${index})
    list(INSERT wt_fatfs_includes ${index} "${wt_fatfs_build_dir}")
    set_property(TARGET __idf_fatfs PROPERTY ${property} ${wt_fatfs_includes})
endforeach()
set_property(TARGET __idf_fatfs PROPERTY ESP_VISION_WT_EXFAT TRUE)
