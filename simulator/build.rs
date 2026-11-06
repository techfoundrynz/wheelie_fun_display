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
    slint_build::compile("../ui/dashboard.slint").expect("compile shared dashboard UI");
}
