//! Cargo discovers these scenarios; each uses the real pointer/rendering fixture
//! process at every supported viewport, keeping Slint platform state isolated.
use std::process::Command;

const LAYOUTS: &[(u32, u32)] = &[(410, 502), (502, 410), (480, 800), (800, 480)];

fn check_layouts(fixture: Option<&str>, layouts: &[(u32, u32)]) {
    for &(width, height) in layouts {
        let mut command = Command::new(env!("CARGO_BIN_EXE_transition-checks"));
        command.current_dir(concat!(env!("CARGO_MANIFEST_DIR"), "/.."));
        command.args([width.to_string(), height.to_string()]);
        if let Some(fixture) = fixture {
            command.arg(fixture);
        }
        let output = command.output().expect("start isolated display fixture");
        assert!(
            output.status.success(),
            "{width}x{height} {fixture:?} failed:\n{}\n{}",
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr),
        );
    }
}

#[test]
fn navigation_gestures_and_rotation() {
    check_layouts(None, LAYOUTS);
}

#[test]
fn ride_status_priority() {
    check_layouts(Some("--ride-fixtures"), LAYOUTS);
}

#[test]
fn tuning_write_guards() {
    check_layouts(Some("--tuning-fixtures"), LAYOUTS);
}

#[test]
fn upstream_pairing() {
    check_layouts(Some("--pairing-fixtures"), LAYOUTS);
}

#[test]
fn bike_pin_and_sleep() {
    check_layouts(Some("--pin-fixtures"), LAYOUTS);
}

#[test]
fn transport_and_proxy_settings() {
    check_layouts(Some("--transport-fixtures"), LAYOUTS);
}

#[test]
fn integration_capabilities() {
    check_layouts(Some("--integration-fixtures"), LAYOUTS);
}

#[test]
fn renderer_rotation_pixels() {
    check_layouts(Some("--rotation-fixtures"), &[(410, 502), (480, 800)]);
}
