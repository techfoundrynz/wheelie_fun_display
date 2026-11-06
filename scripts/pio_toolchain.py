"""Apply a board's explicit compiler prefix before PlatformIO clones build environments.

The pinned espressif32 8.0.0 builder only recognizes C3/C6 as RISC-V.
Keep P4 selection in our board profile without modifying installed packages.
"""

Import("env")

prefix = env.BoardConfig().get("build.toolchain")
if prefix:
    build_program = env.BuildProgram

    def build_with_board_toolchain(build_env, *args, **kwargs):
        build_env.Replace(
            AR=prefix + "-elf-gcc-ar",
            AS=prefix + "-elf-as",
            CC=prefix + "-elf-gcc",
            CXX=prefix + "-elf-g++",
            RANLIB=prefix + "-elf-gcc-ranlib",
            SIZETOOL=prefix + "-elf-size",
        )
        return build_program(*args, **kwargs)

    env.AddMethod(build_with_board_toolchain, "BuildProgram")
