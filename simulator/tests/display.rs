//! Cargo discovers these scenarios; each uses the real pointer/rendering fixture
//! process at every supported viewport, keeping Slint platform state isolated.
use std::process::Command;

const LAYOUTS: &[(u32, u32)] = &[(410, 502), (502, 410), (480, 800), (800, 480)];

#[test]
fn keyboard_preserves_password_and_blocks_background_taps() {
    check_layouts(Some("--keyboard-fixtures"), LAYOUTS);
}

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
fn rotation_replaces_old_panel_pixels() {
    check_layouts(Some("--rotation-repaint-fixtures"), LAYOUTS);
}

#[test]
fn native_page_snapshot_capture() {
    check_layouts(Some("--snapshot-fixtures"), LAYOUTS);
}

#[test]
fn native_page_snapshot_live_handoff() {
    check_layouts(Some("--live-snapshot-fixtures"), LAYOUTS);
}

#[test]
fn native_page_snapshot_restores_both_buffers() {
    check_layouts(Some("--double-buffer-snapshot-fixtures"), LAYOUTS);
}

#[test]
fn native_page_preparation_preserves_visible_page() {
    check_layouts(Some("--page-preparation-fixtures"), LAYOUTS);
}

#[test]
fn stats_scroll_momentum() {
    check_layouts(Some("--momentum-fixtures"), LAYOUTS);
}

#[test]
fn native_gesture_snapshot_reversal_and_cancel() {
    check_layouts(Some("--gesture-snapshot-fixtures"), LAYOUTS);
}

#[test]
fn page_slides_include_side_margins() {
    check_layouts(Some("--fullwidth-slide-fixtures"), LAYOUTS);
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

#[test]
fn status_marquee() {
    check_layouts(Some("--marquee-fixtures"), LAYOUTS);
}

#[test]
fn safe_area_axes() {
    check_layouts(Some("--safe-area-fixtures"), LAYOUTS);
}

#[test]
fn pattern_unlock() {
    check_layouts(Some("--pattern-fixtures"), LAYOUTS);
}

#[test]
fn detailed_stats() {
    check_layouts(Some("--stats-fixtures"), LAYOUTS);
}

#[test]
fn wfb_configuration_drafts() {
    check_layouts(Some("--wfb-tuning-fixtures"), LAYOUTS);
}

#[test]
fn vesc_update_controls() {
    check_layouts(Some("--update-fixtures"), LAYOUTS);
}
