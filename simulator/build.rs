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
    println!("cargo:rerun-if-env-changed=WFD_PAGE_SNAPSHOTS");
    println!("cargo:rerun-if-changed=../ui");
    let mut ui = std::path::PathBuf::from("../ui/dashboard.slint");
    let mut ui_config = slint_build::CompilerConfiguration::new().with_debug_info(true);
    if std::env::var("WFD_PAGE_SNAPSHOTS").as_deref() == Ok("1") {
        let staged =
            std::path::PathBuf::from(std::env::var_os("OUT_DIR").expect("Cargo output directory"))
                .join("ui");
        std::fs::create_dir_all(&staged).expect("create UI capability staging directory");
        std::fs::copy(&ui, staged.join("dashboard.slint")).expect("stage shared dashboard");
        std::fs::write(
            staged.join("hardware.slint"),
            "export global HardwareCapabilities { out property <bool> page-snapshots: true; }\n",
        )
        .expect("stage UI capability");
        ui = staged.join("dashboard.slint");
        ui_config = ui_config.with_include_paths(vec![
            std::path::PathBuf::from("../ui")
                .canonicalize()
                .expect("shared UI directory"),
        ]);
    }
    // Native fixtures locate real hit targets through Slint element metadata.
    slint_build::compile_with_config(ui, ui_config).expect("compile shared dashboard UI");
}
