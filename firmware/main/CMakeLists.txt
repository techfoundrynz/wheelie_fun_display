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
option(WFD_BLE_BENCHMARK "Run bounded, read-only BLE telemetry throughput diagnostics" OFF)
if(WFD_BLE_BENCHMARK)
    set_property(SOURCE main.cpp ble_proxy.cpp APPEND PROPERTY COMPILE_DEFINITIONS WFD_BLE_BENCHMARK=1)
endif()

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
# Font glyph ordering can vary between compiler runs, so unchanged UI inputs must
# reuse the complete generated pair instead of recompiling it on every configure.
set(font_preparer "${CMAKE_SOURCE_DIR}/scripts/prepare_slint_fonts.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${font_preparer}")
set(slint_font_sizes "14,16,18,20,24,28,36,48,60,72,84,120")
set(slint_font_plan "Roboto=14/16/18/20/24/28/36/48:0123456789.- /60:0123456789.- /72:0123456789.- /84:0123456789.- /120:0123456789.- ;Sonsie One=20:WHEELIE")
file(SHA256 "${slint_compiler}" slint_compiler_hash)
file(SHA256 "${font_preparer}" font_preparer_hash)
set(slint_fingerprint "wfd-slint-generation-v2\n${slint_compiler_hash}\n${font_preparer_hash}\n${slint_font_sizes}\n${slint_font_plan}\n--embed-resources=embed-for-software-renderer\n-o=dashboard.h\n--cpp-file=dashboard-raw.cpp\n")
list(SORT ui_inputs)
foreach(ui_input IN LISTS ui_inputs)
    file(RELATIVE_PATH ui_relative "${CMAKE_SOURCE_DIR}" "${ui_input}")
    file(SHA256 "${ui_input}" ui_hash)
    string(APPEND slint_fingerprint "${ui_relative}:${ui_hash}\n")
endforeach()
string(SHA256 slint_fingerprint "${slint_fingerprint}")
set(slint_stamp "${gen}/.slint-inputs")
set(slint_previous "")
if(EXISTS "${slint_stamp}")
    file(READ "${slint_stamp}" slint_previous)
endif()
set(slint_expected "${slint_fingerprint}\n")
foreach(slint_output dashboard.h dashboard-raw.cpp dashboard.cpp)
    if(EXISTS "${gen}/${slint_output}")
        file(SHA256 "${gen}/${slint_output}" slint_output_hash)
        string(APPEND slint_expected "${slint_output}:${slint_output_hash}\n")
    endif()
endforeach()
if(NOT slint_previous STREQUAL slint_expected OR
   NOT EXISTS "${gen}/dashboard.h" OR NOT EXISTS "${gen}/dashboard-raw.cpp" OR
   NOT EXISTS "${gen}/dashboard.cpp")
    set(slint_staging "${gen}/staging")
    file(MAKE_DIRECTORY "${slint_staging}")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        "SLINT_FONT_SIZES=${slint_font_sizes}"
        "SLINT_FONT_PLAN=${slint_font_plan}"
        "${slint_compiler}" "${ui}" --embed-resources embed-for-software-renderer
        -o dashboard.h --cpp-file dashboard-raw.cpp
        WORKING_DIRECTORY "${slint_staging}" RESULT_VARIABLE slint_result)
    if(NOT slint_result EQUAL 0)
        message(FATAL_ERROR "Slint UI compilation failed")
    endif()
    # Constant-initialize immutable font metadata so it remains in flash-mapped memory.
    execute_process(COMMAND "${PYTHON}" "${font_preparer}" "${slint_staging}/dashboard-raw.cpp"
        --output "${slint_staging}/dashboard.cpp" RESULT_VARIABLE font_result)
    if(NOT font_result EQUAL 0)
        message(FATAL_ERROR "Slint font resource preparation failed")
    endif()
    # SCons repeats font preparation from this raw snapshot before compilation.
    # Publish all three outputs from the same compiler run to keep that idempotent.
    foreach(slint_output dashboard.h dashboard-raw.cpp dashboard.cpp)
        file(COPY_FILE "${slint_staging}/${slint_output}" "${gen}/${slint_output}" ONLY_IF_DIFFERENT)
    endforeach()
    set(slint_completed "${slint_fingerprint}\n")
    foreach(slint_output dashboard.h dashboard-raw.cpp dashboard.cpp)
        file(SHA256 "${gen}/${slint_output}" slint_output_hash)
        string(APPEND slint_completed "${slint_output}:${slint_output_hash}\n")
    endforeach()
    # Hash the published trio so missing, corrupted or overwritten output heals.
    # A failed compiler, preparer or output copy must never mark stale output current.
    file(WRITE "${slint_stamp}" "${slint_completed}")
endif()

target_sources(${COMPONENT_LIB} PRIVATE "${gen}/dashboard.cpp")
target_include_directories(${COMPONENT_LIB} PRIVATE "${gen}")

