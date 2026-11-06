fn main() {
    let config = std::fs::read_to_string("../../platformio.ini").expect("read firmware version");
    let version = config
        .lines()
        .find_map(|line| line.trim().strip_prefix("custom_firmware_version ="))
        .expect("firmware version")
        .trim();
    println!("cargo:rustc-env=WFD_FIRMWARE_VERSION={version}");
    println!("cargo:rerun-if-changed=../../platformio.ini");
    // Use the device's bitmap font sizes/coverage, including the dynamic gauge.
    let firmware_build =
        std::fs::read_to_string("../../firmware/main/CMakeLists.txt").expect("font configuration");
    for key in ["SLINT_FONT_SIZES", "SLINT_FONT_PLAN"] {
        let prefix = format!("\"{key}=");
        if let Some(value) = firmware_build
            .lines()
            .find_map(|line| line.trim().strip_prefix(&prefix))
        {
            // Build scripts run this before the compiler starts worker threads.
            unsafe {
                std::env::set_var(key, value.trim_end_matches('"'));
            }
        }
    }
    println!("cargo:rerun-if-changed=../../firmware/main/CMakeLists.txt");
    slint_build::compile_with_config(
        "../../ui/dashboard.slint",
        slint_build::CompilerConfiguration::new()
            .embed_resources(slint_build::EmbedResourcesKind::EmbedForSoftwareRenderer),
    )
    .expect("compile shared device UI");
}
