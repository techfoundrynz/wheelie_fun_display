idf_build_get_property(board_driver WFD_BOARD_DRIVER)
set(page_snapshots true)
if(board_driver STREQUAL "waveshare_qspi")
    set(board_source "board.cpp")
    set(renderer_source "slint_renderer_s3.cpp")
    set(board_requires espressif__esp_lcd_sh8601 espressif__esp_lcd_touch_ft5x06)
elseif(board_driver STREQUAL "guition_p4")
    set(board_source "board_guition_p4.cpp")
    set(renderer_source "slint_renderer_p4.cpp")
    set(board_requires espressif__esp_lcd_touch_gt911 espressif__esp_hosted espressif__esp_wifi_remote esp_driver_ledc esp_driver_ppa)
else()
    message(FATAL_ERROR "Select a supported custom_board_driver in the PlatformIO hardware profile")
endif()

idf_component_register(SRCS "main.cpp" "tuning.cpp" ${board_source} "ble.cpp" "ble_proxy.cpp" "slint_backend.cpp" "${renderer_source}" "network.cpp" "security.cpp" "ble_security.cpp" "transport.cpp" "wired_transport.cpp" "can_driver.cpp"
    INCLUDE_DIRS "." "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/.pio/generated/${WFD_HARDWARE_ID}"
    REQUIRES bike slint_runtime bt nvs_flash esp_timer esp_lcd esp_driver_spi esp_driver_i2c esp_driver_twai esp_driver_uart
    esp_wifi esp_netif esp_event esp_http_client app_update mbedtls espressif__cjson
    esp_hw_support esp_psram spi_flash
    ${board_requires})
target_sources(${COMPONENT_LIB} PRIVATE ota_image.cpp vesc_update.cpp)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
set(profile_enabled 0)
set(profile_seconds 10)
set(profile_start 0)
if("$ENV{WFD_RENDER_PROFILE}" STREQUAL "1")
    set(profile_enabled 1)
    if(NOT "$ENV{WFD_RENDER_PROFILE_SECONDS}" STREQUAL "")
        set(profile_seconds "$ENV{WFD_RENDER_PROFILE_SECONDS}")
    endif()
    if(NOT profile_seconds MATCHES "^[1-9][0-9]*$" OR profile_seconds GREATER 600)
        message(FATAL_ERROR "WFD_RENDER_PROFILE_SECONDS must be between 1 and 600")
    endif()
    if(NOT "$ENV{WFD_RENDER_PROFILE_START_STAGE}" STREQUAL "")
        set(profile_start "$ENV{WFD_RENDER_PROFILE_START_STAGE}")
        if(NOT profile_start MATCHES "^(0|[1-9][0-9]*)$" OR profile_start GREATER 15)
            message(FATAL_ERROR "WFD_RENDER_PROFILE_START_STAGE must be between 0 and 15")
        endif()
    endif()
endif()
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/render_profile_config.hpp" CONTENT
    "#pragma once\n#ifndef WFD_RENDER_PROFILE\n#define WFD_RENDER_PROFILE @profile_enabled@\n#endif\n#ifndef WFD_RENDER_PROFILE_SECONDS\n#define WFD_RENDER_PROFILE_SECONDS @profile_seconds@\n#endif\n#define WFD_RENDER_PROFILE_START @profile_start@\n" @ONLY)
find_package(Git QUIET)
set(wfd_source_revision "")
if(GIT_FOUND)
    function(wfd_watch_git_path name)
        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${name}"
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE wfd_git_path
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE wfd_git_path_result)
        if(wfd_git_path_result EQUAL 0)
            get_filename_component(wfd_git_path "${wfd_git_path}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
            if(EXISTS "${wfd_git_path}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${wfd_git_path}")
            endif()
        endif()
    endfunction()
    wfd_watch_git_path(HEAD)
    wfd_watch_git_path(index)
    wfd_watch_git_path(packed-refs)
    execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref --quiet HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE wfd_git_ref
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE wfd_git_ref_result)
    if(wfd_git_ref_result EQUAL 0)
        wfd_watch_git_path("${wfd_git_ref}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" OUTPUT_VARIABLE wfd_source_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE wfd_revision_result)
    if(NOT wfd_revision_result EQUAL 0)
        set(wfd_source_revision "")
    else()
        execute_process(COMMAND "${GIT_EXECUTABLE}" diff-index --quiet HEAD --
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" RESULT_VARIABLE wfd_dirty_result)
        if(wfd_dirty_result EQUAL 1)
            string(APPEND wfd_source_revision "-dirty")
        endif()
    endif()
endif()
set_property(SOURCE wired_transport.cpp vesc_update.cpp APPEND PROPERTY COMPILE_DEFINITIONS
    "WFD_SOURCE_REVISION=\"${wfd_source_revision}\"")
set(ui "${CMAKE_SOURCE_DIR}/ui/dashboard.slint")
file(GLOB_RECURSE ui_inputs CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/ui/*.slint" "${CMAKE_SOURCE_DIR}/ui/fonts/*" "${CMAKE_SOURCE_DIR}/ui/assets/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ui_inputs})
set(gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${gen}")
if(page_snapshots)
    file(MAKE_DIRECTORY "${gen}/ui")
    file(COPY_FILE "${ui}" "${gen}/ui/dashboard.slint" ONLY_IF_DIFFERENT)
    file(CONFIGURE OUTPUT "${gen}/ui/hardware.slint" CONTENT
        "export global HardwareCapabilities { out property <bool> page-snapshots: true; }\n")
    set(ui "${gen}/ui/dashboard.slint")
endif()
if(CMAKE_HOST_WIN32)
    set(slint_compiler "${CMAKE_BINARY_DIR}/slint-prebuilt/slint-compiler.exe")
else()
    set(slint_compiler "${CMAKE_BINARY_DIR}/slint-prebuilt/slint-compiler")
endif()
# The runtime component stages a matching compiler and headers during configuration.
set(header_preparer "${CMAKE_SOURCE_DIR}/scripts/prepare_slint_headers.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${header_preparer}")
set(slint_compat_headers "${CMAKE_CURRENT_BINARY_DIR}/slint-compat")
execute_process(COMMAND "${PYTHON}" "${header_preparer}"
    --source "${CMAKE_BINARY_DIR}/slint-prebuilt/current/include"
    --output "${slint_compat_headers}" RESULT_VARIABLE header_result)
if(NOT header_result EQUAL 0)
    message(FATAL_ERROR "Slint header compatibility preparation failed")
endif()
target_include_directories(${COMPONENT_LIB} BEFORE PRIVATE "${slint_compat_headers}")
# Font glyph ordering can vary between compiler runs, so unchanged UI inputs must
# reuse the complete generated pair instead of recompiling it on every configure.
set(font_preparer "${CMAKE_SOURCE_DIR}/scripts/prepare_slint_fonts.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${font_preparer}")
set(slint_font_sizes "14,16,18,20,24,28,36,48,60,72,84,120")
set(slint_font_plan "Roboto=14/16/18/20/24/28/36/48:0123456789.- /60:0123456789.- /72:0123456789.- /84:0123456789.- /120:0123456789.- ;Sonsie One=20:WHEELIE")
file(SHA256 "${slint_compiler}" slint_compiler_hash)
file(SHA256 "${font_preparer}" font_preparer_hash)
set(slint_fingerprint "wfd-slint-generation-v2\n${slint_compiler_hash}\n${font_preparer_hash}\n${slint_font_sizes}\n${slint_font_plan}\npage-snapshots=${page_snapshots}\n--embed-resources=embed-for-software-renderer\n-o=dashboard.h\n--cpp-file=dashboard-raw.cpp\n")
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
        "${slint_compiler}" "${ui}" -I "${CMAKE_SOURCE_DIR}/ui" --embed-resources embed-for-software-renderer
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
if(CONFIG_IDF_TARGET_ESP32P4)
    # Avoid relaxation over millions of generated debug labels.
    # Other objects retain normal linker optimizations.
    set_property(SOURCE "${gen}/dashboard.cpp" APPEND PROPERTY COMPILE_OPTIONS -mno-relax)
endif()
target_include_directories(${COMPONENT_LIB} PRIVATE "${gen}")

