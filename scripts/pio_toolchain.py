"""Apply a board's explicit compiler prefix before PlatformIO clones build environments.

The pinned espressif32 8.0.0 builder only recognizes C3/C6 as RISC-V.
Keep P4 selection in our board profile without modifying installed packages.
"""

Import("env")

prefix = env.BoardConfig().get("build.toolchain", "")
if prefix:
    build_program = env.BuildProgram

    def build_with_board_toolchain(build_env, *args, **kwargs):
        # The platform initializes ELF2BINFLAGS after `pre:` scripts. Add these
        # here, before ESP-IDF clones its bootloader environment.
        if build_env.BoardConfig().get("build.mcu") == "esp32p4":
            build_env.Append(ELF2BINFLAGS=["$WFD_IMAGE_REVISION_FLAGS"])
        build_env.Replace(
            AR=prefix + "-elf-gcc-ar",
            AS=prefix + "-elf-as",
            CC=prefix + "-elf-gcc",
            CXX=prefix + "-elf-g++",
            RANLIB=prefix + "-elf-gcc-ranlib",
            SIZETOOL=prefix + "-elf-size",
        )
        platform = build_env.PioPlatform()
        get_package_dir = platform.get_package_dir

        def board_package_dir(name):
            # IDF's linker preprocessor has the same incomplete MCU table.
            if name == "toolchain-xtensa-esp-elf":
                name = "toolchain-" + prefix
            return get_package_dir(name)

        platform.get_package_dir = board_package_dir
        try:
            return build_program(*args, **kwargs)
        finally:
            # Another environment in the same invocation may target Xtensa.
            platform.get_package_dir = get_package_dir

    env.AddMethod(build_with_board_toolchain, "BuildProgram")
