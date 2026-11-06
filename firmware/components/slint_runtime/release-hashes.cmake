# GitHub release asset SHA256 digests for the pinned mcu-v1.19.5 package.
# Updating the Slint pin requires updating these values too.
if(NOT SLINT_PREBUILT_REPO STREQUAL "techfoundrynz/slint" OR
   NOT SLINT_PREBUILT_TAG STREQUAL "mcu-v1.19.5" OR
   NOT SLINT_PREBUILT_VERSION STREQUAL "1.19.0")
    message(FATAL_ERROR "Update release-hashes.cmake when changing the Slint pin")
endif()
if(rust_target STREQUAL "xtensa-esp32s3-none-elf")
    set(archive_sha256 "100287c96bca87a289388ff0f1e66b152655ba46f18c8658c691214b2b4ba0b6")
elseif(rust_target STREQUAL "riscv32imafc-esp-espidf")
    set(archive_sha256 "22847024c7023c8321c3459af6546bb1eacddce1e3daec3fca0fa2effee1f814")
else()
    message(FATAL_ERROR "No pinned release archive for ${rust_target}")
endif()
if(CMAKE_HOST_WIN32)
    set(compiler_sha256 "9f696c1d6e7d7cfbf6b235f7816159a6c7c752cb552be924f24aa8534d46ba92")
elseif(CMAKE_HOST_APPLE)
    set(compiler_sha256 "b423d24b4b60a2b573a54705f801bf6da0a14f6b70f727c7282ca089f62c9003")
else()
    set(compiler_sha256 "8b9b926c64cdfb5af7b9155ced6cf96ebeeeb22759c514e923f5dd97cb49eae8")
endif()
