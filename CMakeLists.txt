cmake_minimum_required(VERSION 3.20)
set(EXTRA_COMPONENT_DIRS firmware/components firmware/main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(NOT PROJECT_VER)
    message(FATAL_ERROR "PROJECT_VER must come from platformio.ini")
endif()
if(NOT WFD_HARDWARE_ID)
    message(FATAL_ERROR "WFD_HARDWARE_ID must come from the PlatformIO hardware profile")
endif()
if(NOT WFD_DEPENDENCIES_LOCK)
    message(FATAL_ERROR "Dependency lock path must come from the PlatformIO environment")
endif()
idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_SOURCE_DIR}/${WFD_DEPENDENCIES_LOCK}")
idf_build_set_property(WFD_BOARD_DRIVER "${WFD_BOARD_DRIVER}")
# Hosted/Wi-Fi Remote Kconfig selects its compatibility headers by SDK major.minor.
# PlatformIO does not run idf.py's environment setup, so supply it from the SDK.
if(WFD_BOARD_DRIVER STREQUAL "guition_p4")
    include("$ENV{IDF_PATH}/tools/cmake/version.cmake")
    set(ENV{ESP_IDF_VERSION} "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")
endif()
project("wfd_${WFD_HARDWARE_ID}")

if(WFD_BOARD_DRIVER STREQUAL "guition_p4")
    # PlatformIO rebases bare forced-include filenames against its builder path.
    # Keep Hosted's master configuration header absolute before exporting flags.
    idf_component_get_property(hosted_dir espressif__esp_hosted COMPONENT_DIR)
    get_target_property(hosted_options eh_host_config INTERFACE_COMPILE_OPTIONS)
    set(hosted_config_header "${hosted_dir}/host/eh_host_config/include/eh_host_port_master_config.h")
    if(NOT EXISTS "${hosted_config_header}")
        message(FATAL_ERROR "ESP-Hosted master configuration header is missing")
    endif()
    list(TRANSFORM hosted_options REPLACE
        "^SHELL:-include eh_host_port_master_config.h$"
        "SHELL:-include \"${hosted_config_header}\"")
    set_property(TARGET eh_host_config PROPERTY INTERFACE_COMPILE_OPTIONS "${hosted_options}")
endif()
file(WRITE "${CMAKE_SOURCE_DIR}/.pio/generated/managed-components.target" "${WFD_HARDWARE_ID}")
