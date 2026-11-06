idf_build_get_property(board_driver WFD_BOARD_DRIVER)
if(board_driver STREQUAL "waveshare_qspi")
    set(board_source "board.cpp")
    set(renderer_source "slint_renderer_scanline.cpp")
    set(board_requires espressif__esp_lcd_sh8601 espressif__esp_lcd_touch_ft5x06)
elseif(board_driver STREQUAL "guition_p4")
    set(board_source "board_guition_p4.cpp")
    set(renderer_source "slint_renderer_p4.cpp")
    set(board_requires espressif__esp_lcd_touch_gt911 espressif__esp_hosted espressif__esp_wifi_remote esp_driver_ledc esp_driver_ppa)
else()
    message(FATAL_ERROR "Select a supported custom_board_driver in the PlatformIO hardware profile")
endif()

idf_component_register(SRCS "main.cpp" ${board_source} "ble.cpp" "ble_proxy.cpp" "slint_backend.cpp" "${renderer_source}" "network.cpp" "security.cpp" "ble_security.cpp" "transport.cpp" "wired_transport.cpp" "can_driver.cpp"
    INCLUDE_DIRS "." "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/.pio/generated/${WFD_HARDWARE_ID}"
    REQUIRES bike slint_runtime bt nvs_flash esp_timer esp_lcd esp_driver_spi esp_driver_i2c esp_driver_twai esp_driver_uart
    esp_wifi esp_netif esp_event esp_http_client app_update mbedtls espressif__cjson
    esp_hw_support esp_psram spi_flash
    ${board_requires})
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
set(ui "${CMAKE_SOURCE_DIR}/ui/dashboard.slint")
file(GLOB_RECURSE ui_inputs CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/ui/*.slint" "${CMAKE_SOURCE_DIR}/ui/fonts/*" "${CMAKE_SOURCE_DIR}/ui/assets/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ui_inputs})
set(gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${gen}")
if(CMAKE_HOST_WIN32)
    set(slint_compiler "${CMAKE_BINARY_DIR}/slint-prebuilt/slint-compiler.exe")
else()
    set(slint_compiler "${CMAKE_BINARY_DIR}/slint-prebuilt/slint-compiler")
endif()
# The runtime component stages a matching compiler and headers during configuration.
execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "SLINT_FONT_SIZES=14,16,18,20,24,28,36,48,60,72,84"
    "SLINT_FONT_PLAN=Roboto=14/16/18/20/24/28/36/48:0123456789.- /60:0123456789.- /72:0123456789.- /84:0123456789.- ;Sonsie One=20:WHEELIE"
    "${slint_compiler}" "${ui}" --embed-resources embed-for-software-renderer
    -o dashboard.h --cpp-file dashboard-raw.cpp
    WORKING_DIRECTORY "${gen}" RESULT_VARIABLE slint_result)
if(NOT slint_result EQUAL 0)
    message(FATAL_ERROR "Slint UI compilation failed")
endif()
# Constant-initialize immutable font metadata so it remains in flash-mapped memory.
set(font_preparer "${CMAKE_SOURCE_DIR}/scripts/prepare_slint_fonts.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${font_preparer}")
execute_process(COMMAND "${PYTHON}" "${font_preparer}" "${gen}/dashboard-raw.cpp" --output "${gen}/dashboard.cpp"
    RESULT_VARIABLE font_result)
if(NOT font_result EQUAL 0)
    message(FATAL_ERROR "Slint font resource preparation failed")
endif()

target_sources(${COMPONENT_LIB} PRIVATE "${gen}/dashboard.cpp")
target_include_directories(${COMPONENT_LIB} PRIVATE "${gen}")

