fn main() {
    let config =
        std::fs::read_to_string("../platformio.ini").expect("read PlatformIO configuration");
    let version = config
        .lines()
        .find_map(|line| line.trim().strip_prefix("custom_firmware_version ="))
        .expect("PlatformIO firmware version")
        .trim();
    println!("cargo:rustc-env=WFD_FIRMWARE_VERSION={version}");
    println!("cargo:rerun-if-changed=../platformio.ini");
    // Native fixtures locate real hit targets through Slint element metadata.
    slint_build::compile_with_config(
        "../ui/dashboard.slint",
        slint_build::CompilerConfiguration::new().with_debug_info(true),
    )
    .expect("compile shared dashboard UI");
}
