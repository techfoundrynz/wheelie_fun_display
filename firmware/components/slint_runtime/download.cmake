# Download into a temporary file, verify it, then publish it to the cache.
function(slint_download url destination expected_sha256)
    foreach(attempt RANGE 1 4)
        file(DOWNLOAD "${url}" "${destination}.tmp"
             STATUS download_status LOG download_log TLS_VERIFY ON
             TIMEOUT 120 INACTIVITY_TIMEOUT 30 SHOW_PROGRESS)
        list(GET download_status 0 download_code)
        if(download_code EQUAL 0)
            file(SHA256 "${destination}.tmp" actual_sha256)
            if(NOT actual_sha256 STREQUAL expected_sha256)
                file(REMOVE "${destination}.tmp")
                message(FATAL_ERROR "Slint download checksum mismatch: ${url}")
            endif()
            file(RENAME "${destination}.tmp" "${destination}")
            return()
        endif()

        file(REMOVE "${destination}.tmp")
        list(GET download_status 1 download_error)
        # Include the final HTTP status rather than only libcurl's generic error.
        string(REGEX MATCHALL "HTTP/[0-9.]+ [0-9]+" responses "${download_log}")
        set(http_status "")
        if(responses)
            list(GET responses -1 http_status)
        endif()
        if(attempt EQUAL 4 OR
           (http_status MATCHES " 4[0-9][0-9]$" AND
            NOT http_status MATCHES " (408|429)$"))
            message(FATAL_ERROR
                "Failed to download ${url} after ${attempt} attempt(s): ${http_status} ${download_error}")
        endif()
        math(EXPR delay "1 << ${attempt}")
        message(STATUS "Slint download attempt ${attempt} failed: ${http_status} ${download_error}; retrying in ${delay}s")
        execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep "${delay}")
    endforeach()
endfunction()
