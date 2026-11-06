use slint::{ComponentHandle, ModelRc, Timer, TimerMode, VecModel};
use std::rc::Rc;
use std::time::{Duration, Instant};
slint::include_modules!();

fn main() -> Result<(), slint::PlatformError> {
    let ui = Dashboard::new()?;
    ui.set_device_label("Desktop simulator".into());
    let smoke = std::env::args().any(|arg| arg == "--smoke-test");
    let dimensions: Vec<_> = std::env::args()
        .skip(1)
        .filter(|arg| !arg.starts_with("--"))
        .collect();
    if dimensions.len() == 2 {
        ui.set_initial_width(
            dimensions[0]
                .parse::<f32>()
                .expect("width in pixels")
                .max(320.0),
        );
        ui.set_initial_height(
            dimensions[1]
                .parse::<f32>()
                .expect("height in pixels")
                .max(320.0),
        );
    }
    ui.window().set_size(slint::LogicalSize::new(
        ui.get_initial_width(),
        ui.get_initial_height(),
    ));
    ui.set_rotation(if ui.get_initial_width() > ui.get_initial_height() {
        90
    } else {
        0
    });
    ui.set_connection("Demo / simulated telemetry".into());
    ui.set_connected(true);
    ui.set_link_present(true);
    ui.set_fresh(true);
    ui.set_can_configure(true);
    ui.set_feedback("Demo controls never connect to a real bike.".into());
    let wired_demo = std::env::args().any(|arg| arg == "--wired");
    ui.set_can_available(wired_demo);
    ui.set_uart_available(wired_demo);
    let weak = ui.as_weak();
    ui.on_transport_requested(move |kind| {
        let ui = weak.unwrap();
        ui.set_transport_kind(kind);
        ui.set_connected(false);
        ui.set_link_present(false);
        ui.set_fresh(false);
        ui.set_controller_status("Demo / connect to discover the main controller".into());
    });
    let weak = ui.as_weak();
    ui.on_connect_wired(move || {
        let ui = weak.unwrap();
        ui.set_connected(true);
        ui.set_link_present(true);
        ui.set_fresh(true);
        ui.set_controller_status("Demo / main VESC CAN 7 / simulated firmware".into());
    });
    let weak = ui.as_weak();
    ui.on_discover_controller(move || {
        weak.unwrap()
            .set_controller_status("Demo / simulated controller discovery complete".into())
    });
    let weak = ui.as_weak();
    ui.on_ble_proxy_changed(move |enabled| {
        let ui = weak.unwrap();
        ui.set_ble_proxy_enabled(enabled);
        ui.set_ble_proxy_status(if enabled {
            "Simulator / no Bluetooth proxy traffic".into()
        } else {
            "Off".into()
        });
    });
    let peers = Rc::new(VecModel::from(vec![Peer {
        name: "WFB demo bike".into(),
        address: "DE:MO:00:00:00:01".into(),
        signal: "-54 dBm".into(),
    }]));
    ui.set_peers(ModelRc::from(peers));
    let weak = ui.as_weak();
    ui.on_forget_peer(move || {
        let ui = weak.unwrap();
        ui.set_connected(false);
        ui.set_link_present(false);
        ui.set_fresh(false);
        ui.set_connection("Demo / choose the sample bike".into());
    });
    let weak = ui.as_weak();
    ui.on_connect_peer(move |_| {
        let ui = weak.unwrap();
        ui.set_connected(true);
        ui.set_link_present(true);
        ui.set_fresh(true);
        ui.set_connection("Demo / simulated telemetry".into());
        ui.set_page(0);
    });
    let weak = ui.as_weak();
    ui.on_apply_mode(move |mode| {
        let ui = weak.unwrap();
        let names = ["Eco", "Cruise", "Sport"];
        ui.set_confirm_text(
            format!("Demo ride response: {}", names[mode.clamp(0, 2) as usize]).into(),
        );
        ui.set_confirming(true);
    });
    let weak = ui.as_weak();
    ui.on_apply_limit(move |mph| {
        let ui = weak.unwrap();
        ui.set_confirm_text(format!("Demo speed limit: {mph} mph").into());
        ui.set_confirming(true);
    });
    let weak = ui.as_weak();
    ui.on_confirm_change(move || {
        let ui = weak.unwrap();
        ui.set_confirming(false);
        ui.set_feedback("Demo change applied locally.".into());
    });
    let weak = ui.as_weak();
    ui.on_cancel_change(move || weak.unwrap().set_confirming(false));
    ui.set_firmware_version(env!("WFD_FIRMWARE_VERSION").into());
    let weak = ui.as_weak();
    ui.on_orientation_requested(move |degrees| {
        let ui = weak.unwrap();
        let (w, h) = (ui.get_initial_width(), ui.get_initial_height());
        ui.set_rotation(degrees);
        if (degrees == 90 || degrees == 270) != (w > h) {
            ui.set_initial_width(h);
            ui.set_initial_height(w);
            ui.window().set_size(slint::LogicalSize::new(h, w));
        }
    });
    ui.set_wifi_networks(ModelRc::from(Rc::new(VecModel::from(vec![WifiItem {
        ssid: "Workshop demo".into(),
        detail: "-45 dBm / locked".into(),
    }]))));
    let weak = ui.as_weak();
    ui.on_wifi_scan(move || {
        weak.unwrap()
            .set_wifi_status("Demo / choose a network".into())
    });
    let weak = ui.as_weak();
    ui.on_wifi_manual(move || {
        let ui = weak.unwrap();
        ui.set_editor_secret(false);
        ui.set_editor_heading("Network name (SSID)".into());
        ui.set_editor_value("".into());
        ui.set_editor_masked("".into());
        ui.set_editing(true);
    });
    let weak = ui.as_weak();
    ui.on_wifi_select(move |_| {
        let ui = weak.unwrap();
        ui.set_wifi_saved("Workshop demo".into());
        ui.set_editor_secret(true);
        ui.set_editor_heading("Wi-Fi password".into());
        ui.set_editor_value("".into());
        ui.set_editor_masked("".into());
        ui.set_editing(true);
    });
    let weak = ui.as_weak();
    ui.on_editor_type(move |text| {
        let ui = weak.unwrap();
        let mut value = ui.get_editor_value().to_string();
        let limit = if ui.get_editor_secret() { 64 } else { 32 };
        if value.len() + text.len() <= limit {
            value.push_str(&text);
        }
        ui.set_editor_masked("*".repeat(value.chars().count()).into());
        ui.set_editor_value(value.into());
    });
    let weak = ui.as_weak();
    ui.on_editor_backspace(move || {
        let ui = weak.unwrap();
        let mut value = ui.get_editor_value().to_string();
        value.pop();
        ui.set_editor_masked("*".repeat(value.chars().count()).into());
        ui.set_editor_value(value.into());
    });
    let weak = ui.as_weak();
    ui.on_editor_cancel(move || {
        let ui = weak.unwrap();
        ui.set_editing(false);
        ui.set_editor_value("".into());
        ui.set_editor_masked("".into());
    });
    let weak = ui.as_weak();
    ui.on_editor_done(move || {
        let ui = weak.unwrap();
        if !ui.get_editor_secret() {
            ui.set_wifi_saved(ui.get_editor_value());
            ui.set_editor_secret(true);
            ui.set_editor_heading("Wi-Fi password".into());
        } else {
            ui.set_wifi_connected(true);
            ui.set_editing(false);
            ui.set_wifi_status("Demo / Wi-Fi connected".into());
        }
        ui.set_editor_value("".into());
        ui.set_editor_masked("".into());
    });
    let weak = ui.as_weak();
    ui.on_wifi_forget(move || {
        let ui = weak.unwrap();
        ui.set_wifi_connected(false);
        ui.set_wifi_saved("".into());
    });
    let weak = ui.as_weak();
    ui.on_wifi_connect_saved(move || {
        let ui = weak.unwrap();
        ui.set_wifi_connected(true);
        ui.set_wifi_status("Demo / Wi-Fi connected".into());
    });
    let weak = ui.as_weak();
    ui.on_update_check(move || {
        let ui = weak.unwrap();
        ui.set_wifi_status("Demo / no real release downloaded".into());
    });
    ui.set_about_items(ModelRc::from(Rc::new(VecModel::from(vec![
        AboutItem {
            label: "Runtime".into(),
            value: "Desktop simulator / simulated telemetry".into(),
        },
        AboutItem {
            label: "Version".into(),
            value: env!("WFD_FIRMWARE_VERSION").into(),
        },
    ]))));
    if smoke {
        ui.invoke_apply_mode(0);
        assert!(ui.get_confirming());
        assert!(ui.get_confirm_text().contains("Eco"));
        ui.invoke_cancel_change();
        assert!(!ui.get_confirming());
        ui.invoke_apply_limit(15);
        ui.invoke_confirm_change();
        assert!(!ui.get_confirming());
        assert!(ui.get_feedback().contains("locally"));
        ui.invoke_forget_peer();
        assert!(!ui.get_connected());
        ui.invoke_connect_peer(0);
        assert!(ui.get_connected() && ui.get_page() == 0);
        ui.invoke_orientation_requested(90);
        assert!(ui.get_initial_width() > ui.get_initial_height());
        ui.invoke_orientation_requested(0);
        assert!(ui.get_initial_width() < ui.get_initial_height());
        ui.invoke_wifi_manual();
        assert!(ui.get_editing());
        ui.invoke_editor_type("Workshop".into());
        ui.invoke_editor_done();
        assert!(ui.get_editor_secret());
        ui.invoke_editor_type("password!".into());
        ui.invoke_editor_backspace();
        assert_eq!(ui.get_editor_value().as_str(), "password");
        ui.invoke_editor_done();
        assert!(ui.get_wifi_connected() && !ui.get_editing());
        assert!(ui.get_editor_value().is_empty());
        // Exercise the production Slint transition lifecycle, including a
        // programmatic redirect during a slide and reduced-motion snapshots.
        let tick = || slint::platform::update_timers_and_animations();
        ui.set_page(3);
        tick();
        assert!(ui.get_transitioning());
        assert_eq!(ui.get_displayed_page(), 3);
        assert_eq!(ui.get_outgoing_page(), 0);
        std::thread::sleep(Duration::from_millis(100));
        tick();
        assert!(ui.get_transitioning());
        ui.set_page(5);
        tick();
        assert_eq!(ui.get_displayed_page(), 3);
        std::thread::sleep(Duration::from_millis(180));
        tick();
        assert!(ui.get_transitioning());
        assert_eq!(ui.get_displayed_page(), 5);
        assert_eq!(ui.get_outgoing_page(), 3);
        std::thread::sleep(Duration::from_millis(280));
        tick();
        assert!(!ui.get_transitioning());
        assert_eq!(ui.get_outgoing_page(), -1);
        ui.set_page(1);
        tick();
        assert!(ui.get_transitioning());
        ui.set_animations_enabled(false);
        tick();
        assert!(!ui.get_transitioning());
        assert_eq!(ui.get_displayed_page(), 1);
        assert_eq!(ui.get_outgoing_page(), -1);
        ui.set_page(0);
        tick();
        assert_eq!(ui.get_displayed_page(), 0);
        println!("UI, rotation, keyboard, Wi-Fi, confirmation and page-transition checks passed");
        return Ok(());
    }
    let weak = ui.as_weak();
    let started = Instant::now();
    let timer = Timer::default();
    timer.start(TimerMode::Repeated, Duration::from_millis(100), move || {
        let ui = weak.unwrap();
        let moving = (started.elapsed().as_secs_f32() / 8.0).sin().max(0.0);
        let mps = moving * 9.0;
        let live = ui.get_connected();
        ui.set_can_configure(live && mps < 0.2);
        ui.set_speed(if live {
            format!("{:.1}", mps * if ui.get_metric() { 3.6 } else { 2.2369363 }).into()
        } else {
            "--".into()
        });
        ui.set_speed_progress(if live { mps / 15.6464 } else { 0.0 });
        ui.set_battery(if live { "82%".into() } else { "--".into() });
        ui.set_battery_level(if live { 82.0 } else { 0.0 });
        ui.set_voltage(if live { "75.6 V".into() } else { "--".into() });
        ui.set_power(if live {
            format!("{:.0} W", moving * 800.0).into()
        } else {
            "--".into()
        });
        ui.set_mosfet(if live {
            if ui.get_celsius() {
                "36 \u{b0}C".into()
            } else {
                "97 \u{b0}F".into()
            }
        } else {
            "--".into()
        });
        ui.set_motor(if live {
            if ui.get_celsius() {
                "42 \u{b0}C".into()
            } else {
                "108 \u{b0}F".into()
            }
        } else {
            "--".into()
        });
        ui.set_pitch(if live {
            format!("{:.1}°", moving * 15.0).into()
        } else {
            "--".into()
        });
    });
    ui.run()
}
