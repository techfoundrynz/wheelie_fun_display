use slint::platform::software_renderer::{
    LineBufferProvider, MinimalSoftwareWindow, RenderingRotation, RepaintBufferType,
};
use slint::platform::{Platform, PointerEventButton, WindowAdapter, WindowEvent};
use slint::{ComponentHandle, PhysicalSize, Rgb8Pixel};
use std::cell::Cell;
use std::io::Write;
use std::rc::Rc;
use std::time::Duration;
slint::include_modules!();

struct Lines<'a> {
    pixels: &'a mut [Rgb8Pixel],
    stride: usize,
}
impl LineBufferProvider for Lines<'_> {
    type TargetPixel = Rgb8Pixel;
    fn process_line(
        &mut self,
        line: usize,
        range: std::ops::Range<usize>,
        render: impl FnOnce(&mut [Rgb8Pixel]),
    ) {
        render(&mut self.pixels[line * self.stride + range.start..line * self.stride + range.end]);
    }
}

struct TestPlatform {
    window: Rc<MinimalSoftwareWindow>,
    clock: Rc<Cell<Duration>>,
}
impl Platform for TestPlatform {
    fn create_window_adapter(&self) -> Result<Rc<dyn WindowAdapter>, slint::PlatformError> {
        Ok(self.window.clone())
    }
    fn duration_since_start(&self) -> Duration {
        self.clock.get()
    }
}

fn frame(
    window: &MinimalSoftwareWindow,
    clock: &Cell<Duration>,
    at: u64,
    pixels: &mut [Rgb8Pixel],
    width: usize,
) {
    clock.set(Duration::from_millis(at));
    slint::platform::update_timers_and_animations();
    window.request_redraw();
    window.draw_if_needed(|renderer| {
        renderer.render(pixels, width);
    });
}
fn capture(name: &str, pixels: &[Rgb8Pixel], width: usize, height: usize) {
    let directory = std::path::Path::new(".cache/transition-checks");
    std::fs::create_dir_all(directory).unwrap();
    let mut file = std::fs::File::create(directory.join(format!("{width}-{name}.ppm"))).unwrap();
    write!(file, "P6\n{width} {height}\n255\n").unwrap();
    let bytes: Vec<u8> = pixels.iter().flat_map(|p| [p.r, p.g, p.b]).collect();
    file.write_all(&bytes).unwrap();
}
fn click(window: &MinimalSoftwareWindow, x: f32, y: f32) {
    window.dispatch_event(WindowEvent::PointerPressed {
        position: slint::LogicalPosition::new(x, y),
        button: PointerEventButton::Left,
    });
    window.dispatch_event(WindowEvent::PointerReleased {
        position: slint::LogicalPosition::new(x, y),
        button: PointerEventButton::Left,
    });
}
// Resolve real layout geometry, then dispatch pointer input through the normal
// hit-testing path. Accessible actions alone would bypass disabled/modal checks.
fn control_center(ui: &Dashboard, label: &str) -> (f32, f32) {
    let controls: Vec<_> =
        i_slint_backend_testing::ElementHandle::find_by_accessible_label(ui, label)
            .filter(|control| {
                control.accessible_role() == Some(i_slint_backend_testing::AccessibleRole::Button)
            })
            .collect();
    assert_eq!(controls.len(), 1, "Expected one control labelled {label:?}");
    let position = controls[0].absolute_position();
    let size = controls[0].size();
    assert!(
        size.width > 0. && size.height > 0.,
        "Empty control: {label}"
    );
    (position.x + size.width / 2., position.y + size.height / 2.)
}

fn click_control(window: &MinimalSoftwareWindow, ui: &Dashboard, label: &str) {
    let (x, y) = control_center(ui, label);
    let size = window.window().size();
    assert!(
        x >= 0. && x < size.width as f32 && y >= 0. && y < size.height as f32,
        "Control {label:?} is outside the viewport at ({x}, {y})"
    );
    click(window, x, y);
}

fn pointer(window: &MinimalSoftwareWindow, kind: &str, x: f32, y: f32) {
    let position = slint::LogicalPosition::new(x, y);
    window.dispatch_event(match kind {
        "down" => WindowEvent::PointerPressed {
            position,
            button: PointerEventButton::Left,
        },
        "up" => WindowEvent::PointerReleased {
            position,
            button: PointerEventButton::Left,
        },
        _ => WindowEvent::PointerMoved { position },
    });
}
fn main() {
    let args: Vec<_> = std::env::args().skip(1).collect();
    let width = args.first().map_or(410, |v| v.parse::<usize>().unwrap());
    let height = args.get(1).map_or(502, |v| v.parse::<usize>().unwrap());
    let window = MinimalSoftwareWindow::new(RepaintBufferType::ReusedBuffer);
    let clock = Rc::new(Cell::new(Duration::ZERO));
    slint::platform::set_platform(Box::new(TestPlatform {
        window: window.clone(),
        clock: clock.clone(),
    }))
    .unwrap();
    let ui = Dashboard::new().unwrap();
    ui.set_fresh(true);
    ui.set_speed("24.8".into());
    ui.set_speed_progress(0.44);
    ui.set_initial_width(width as f32);
    ui.set_initial_height(height as f32);
    window.set_size(PhysicalSize::new(width as u32, height as u32));
    ui.show().unwrap();
    let mut pixels = vec![Rgb8Pixel::default(); width * height];
    if args.iter().any(|arg| arg == "--ride-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_battery("82%".into());
        ui.set_battery_level(82.);
        ui.set_voltage("75.6 V".into());
        ui.set_power("-12000 W".into());
        ui.set_pitch("-12.5°".into());
        ui.set_mosfet("100 °C".into());
        ui.set_motor("100 °C".into());
        for (name, connected, fresh, connection, fault, expected, alert) in [
            (
                "ride-live",
                true,
                true,
                "Connected / BLE",
                "",
                "Connected / BLE",
                false,
            ),
            (
                "ride-stale",
                true,
                false,
                "Connected / telemetry stale",
                "Controller fault 6",
                "Connected / telemetry stale",
                false,
            ),
            (
                "ride-fault",
                true,
                true,
                "Connected / BLE",
                "Controller fault 6",
                "Controller fault 6",
                true,
            ),
            (
                "ride-hot",
                true,
                true,
                "Connected / BLE",
                "Motor hot",
                "Motor hot",
                true,
            ),
            (
                "ride-disconnected",
                false,
                false,
                "Disconnected",
                "Controller fault 6",
                "Disconnected",
                false,
            ),
            (
                "ride-probe",
                false,
                false,
                "Finding main VESC / CAN ping and package discovery",
                "",
                "Finding main VESC / CAN ping and package discovery",
                false,
            ),
            (
                "ride-proxy",
                true,
                false,
                "Phone app active",
                "Controller hot",
                "Phone app active",
                false,
            ),
            (
                "ride-proxy-fault",
                true,
                true,
                "Phone app active",
                "Controller fault 6",
                "Controller fault 6",
                true,
            ),
        ] {
            ui.set_connected(connected);
            ui.set_fresh(fresh);
            ui.set_connection(connection.into());
            ui.set_fault_text(fault.into());
            assert_eq!(
                ui.get_header_status().as_str(),
                expected,
                "Wrong status priority for {name}"
            );
            assert_eq!(
                ui.get_header_alert(),
                alert,
                "Wrong status severity for {name}"
            );
            frame(&window, &clock, 0, &mut pixels, width);
            capture(name, &pixels, width, height);
            if width > height && name == "ride-live" {
                let mut gauge_bottom = None;
                let mut battery_top = None;
                let mut battery_bottom = None;
                for y in 100..height - 82 {
                    for pixel in &pixels[y * width + 32..y * width + width / 2 - 8] {
                        if pixel.r > 200 && pixel.g > 170 && pixel.b < 50 {
                            gauge_bottom = Some(y);
                        }
                        if pixel.r < 130 && pixel.g > 180 && pixel.b > 180 {
                            battery_top.get_or_insert(y);
                            battery_bottom = Some(y);
                        }
                    }
                }
                let gap = battery_top.expect("Battery fill is missing")
                    - gauge_bottom.expect("Speed arc is missing")
                    - 1;
                assert!(
                    (18..=23).contains(&gap),
                    "{width}x{height}: painted speed arc/battery gap is {gap}px, expected 20px plus rasterization rounding"
                );
                let navigation =
                    i_slint_backend_testing::ElementHandle::find_by_accessible_label(&ui, "Ride")
                        .find(|control| {
                            control.accessible_role()
                                == Some(i_slint_backend_testing::AccessibleRole::Button)
                        })
                        .expect("Ride tab is missing");
                let navigation_gap = navigation.absolute_position().y.round() as usize
                    - battery_bottom.expect("Battery fill is missing")
                    - 1;
                assert_eq!(
                    navigation_gap, 16,
                    "Landscape content/navigation gap changed"
                );
            }
        }
        println!(
            "{width}x{height}: Ride header priority, stale fault clearing and status fixtures passed"
        );
        return;
    }
    // Safe inset 32 + one 48px status/cog row + page spacing.
    let header_rows = if height < 560 { 88 } else { 92 };
    let preset_y = header_rows as f32 + 88.;
    let apply_y = header_rows as f32 + 148.;
    if args.get(2).is_some_and(|arg| arg == "--tuning-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_page(1);
        ui.set_draft_mode(1);
        // Even an inconsistent backend guard must not enable a disconnected write.
        ui.set_can_configure(true);
        let applied = Rc::new(Cell::new(0));
        let writes = applied.clone();
        ui.on_apply_mode(move |_| writes.set(writes.get() + 1));
        frame(&window, &clock, 0, &mut pixels, width);
        capture("tuning-disconnected", &pixels, width, height);
        click(&window, 80., preset_y);
        click(&window, width as f32 / 2., apply_y);
        assert_eq!(ui.get_draft_mode(), 1, "Disconnected preset accepted a tap");
        assert_eq!(applied.get(), 0, "Disconnected tuning accepted a write");
        ui.set_connected(true);
        ui.set_can_configure(false);
        frame(&window, &clock, 20, &mut pixels, width);
        click(&window, 80., preset_y);
        assert_eq!(
            ui.get_draft_mode(),
            0,
            "Connected preset could not be selected"
        );
        click(&window, width as f32 / 2., apply_y);
        assert_eq!(applied.get(), 0, "Telemetry/stationary guard was bypassed");
        ui.set_can_configure(true);
        frame(&window, &clock, 40, &mut pixels, width);
        capture("tuning-connected", &pixels, width, height);
        click(&window, width as f32 / 2., apply_y);
        assert_eq!(applied.get(), 1, "Ready tuning write stopped working");
        println!("{width}x{height}: disconnected tuning, preset selection and write guards passed");
        return;
    }
    if args.get(2).is_some_and(|arg| arg == "--pairing-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_page(2);
        ui.set_pairing_open(true);
        ui.set_pairing_ready(true);
        let entered = Rc::new(std::cell::RefCell::new(String::new()));
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pairing_type(move |digit| {
            digits.borrow_mut().push(char::from(b'0' + digit as u8));
            weak.upgrade()
                .unwrap()
                .set_pairing_mask("*".repeat(digits.borrow().len()).into());
        });
        let submitted = Rc::new(Cell::new(0));
        let calls = submitted.clone();
        ui.on_pairing_submit(move || calls.set(calls.get() + 1));
        frame(&window, &clock, 0, &mut pixels, width);
        capture("pairing-pin", &pixels, width, height);
        click(&window, width as f32 - 54., 56.);
        click(&window, 40., height as f32 - 56.);
        assert_eq!(ui.get_page(), 2);
        for digit in 1..=6 {
            click_control(&window, &ui, &digit.to_string());
        }
        assert_eq!(&*entered.borrow(), "123456");
        click_control(&window, &ui, "OK");
        assert_eq!(submitted.get(), 1);
        ui.set_pairing_failed(true);
        frame(&window, &clock, 20, &mut pixels, width);
        capture("pairing-retry", &pixels, width, height);
        println!(
            "{width}x{height}: independent pairing PIN entry/submit, retry layout and modal blocking passed"
        );
        return;
    }
    if args.get(2).is_some_and(|arg| arg == "--transport-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_page(3);
        ui.set_can_available(true);
        ui.set_uart_available(true);
        let weak = ui.as_weak();
        ui.on_transport_requested(move |kind| weak.upgrade().unwrap().set_transport_kind(kind));
        frame(&window, &clock, 0, &mut pixels, width);
        window.dispatch_event(WindowEvent::PointerScrolled {
            position: slint::LogicalPosition::new(width as f32 / 2., 200.),
            delta_x: 0.,
            delta_y: if width < height { -250. } else { -350. },
        });
        frame(&window, &clock, 20, &mut pixels, width);
        capture("transport-settings", &pixels, width, height);
        for (step, kind) in [(1, 1), (2, 2), (3, 0)] {
            click_control(&window, &ui, "Vehicle connection");
            assert_eq!(ui.get_transport_kind(), kind);
            frame(&window, &clock, 20 + step * 20, &mut pixels, width);
            capture(&format!("transport-kind-{kind}"), &pixels, width, height);
        }
        ui.set_can_change_transport(false);
        click_control(&window, &ui, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 0);
        ui.set_can_change_transport(true);
        ui.set_can_available(false);
        click_control(&window, &ui, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 2); // Absent CAN is skipped.
        click_control(&window, &ui, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 0);
        ui.set_can_available(true);
        ui.set_uart_available(false);
        click_control(&window, &ui, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 1); // Absent UART is skipped.
        let proxy_calls = Rc::new(Cell::new(0));
        let called = proxy_calls.clone();
        ui.on_ble_proxy_changed(move |_| called.set(called.get() + 1));
        click_control(&window, &ui, "BLE proxy");
        assert_eq!(proxy_calls.get(), 1);
        click_control(&window, &ui, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 0);
        click_control(&window, &ui, "BLE proxy");
        assert_eq!(proxy_calls.get(), 2);
        println!(
            "{width}x{height}: transport cycling, pin availability, disabled changes and proxy availability on wired transports passed"
        );
        return;
    }
    if args
        .get(2)
        .is_some_and(|arg| arg == "--integration-fixtures")
    {
        ui.set_animations_enabled(false);
        ui.set_connected(true);
        ui.set_link_present(true);
        ui.set_pin_configured(true);
        ui.set_can_configure(true);
        let applied = Rc::new(Cell::new(0));
        let invoked = applied.clone();
        ui.on_apply_mode(move |_| invoked.set(invoked.get() + 1));
        let mut fixture_time = 0;
        for (id, name) in [(1, "Wheelie Fun Bike"), (2, "wheel-e"), (0, "VESC")] {
            applied.set(0);
            ui.set_active_integration(id);
            ui.set_vehicle_brand(name.into());
            ui.set_supports_presets(id == 1);
            ui.set_supports_limit(id == 1);
            ui.set_supports_lock(id != 0);
            for page in [0, 1, 2, 6] {
                ui.set_page(page);
                frame(
                    &window,
                    &clock,
                    {
                        fixture_time += 20;
                        fixture_time
                    },
                    &mut pixels,
                    width,
                );
                capture(
                    &format!("integration-{id}-page-{page}"),
                    &pixels,
                    width,
                    height,
                );
            }
            if id == 1 {
                ui.set_page(1);
                fixture_time += 20;
                frame(&window, &clock, fixture_time, &mut pixels, width);
                click_control(&window, &ui, "Apply ride response");
                assert_eq!(applied.get(), 1, "WFB tuning controls disappeared");
                ui.set_connected(false);
                fixture_time += 20;
                frame(&window, &clock, fixture_time, &mut pixels, width);
                click_control(&window, &ui, "Apply ride response");
                assert_eq!(applied.get(), 1, "Disconnected WFB allowed a write");
                ui.set_connected(true);
            } else {
                ui.set_page(1);
                fixture_time += 20;
                frame(&window, &clock, fixture_time, &mut pixels, width);
                click(&window, width as f32 / 2., apply_y);
                assert_eq!(
                    applied.get(),
                    0,
                    "Unsupported preset controls must not be interactive"
                );
            }
        }
        println!("{width}x{height}: integration branding and feature fixtures passed");
        return;
    }
    if args.get(2).is_some_and(|arg| arg == "--pin-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_page(2);
        ui.set_pin_open(true);
        ui.set_screen_locked(true);
        ui.set_pin_heading("ENTER PIN".into());
        ui.set_pin_message("Bike lock state unconfirmed. Reconnect and retry.".into());
        ui.set_lock_retry_available(true);
        let entered = Rc::new(std::cell::RefCell::new(String::new()));
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pin_digit(move |digit| {
            digits.borrow_mut().push(char::from(b'0' + digit as u8));
            weak.upgrade()
                .unwrap()
                .set_pin_mask("*".repeat(digits.borrow().len()).into());
        });
        let submissions = Rc::new(Cell::new(0));
        let submitted = submissions.clone();
        ui.on_pin_submit(move || submitted.set(submitted.get() + 1));
        frame(&window, &clock, 0, &mut pixels, width);
        capture("pin-locked", &pixels, width, height);
        // The full-screen PIN overlay must swallow taps in its corners too.
        click(&window, width as f32 - 54., 56.);
        click(&window, 40., height as f32 - 56.);
        assert_eq!(ui.get_page(), 2);
        // Exercise the actual keypad in both layouts, including its disabled state.
        ui.set_pin_ready(false);
        click_control(&window, &ui, "1");
        assert!(entered.borrow().is_empty());
        ui.set_pin_ready(true);
        for digit in 1..=6 {
            click_control(&window, &ui, &digit.to_string());
        }
        assert_eq!(&*entered.borrow(), "123456");
        click_control(&window, &ui, "OK");
        assert_eq!(submissions.get(), 1);
        ui.set_screen_asleep(true);
        frame(&window, &clock, 20, &mut pixels, width);
        assert!(
            pixels
                .iter()
                .all(|pixel| pixel.r == 0 && pixel.g == 0 && pixel.b == 0)
        );
        click(&window, width as f32 / 2., height as f32 - 56.);
        assert_eq!(ui.get_page(), 2);
        assert_eq!(submissions.get(), 1);
        println!("{width}x{height}: PIN overlay blocks navigation; sleeping screen is black");
        return;
    }
    if args.get(2).is_some_and(|arg| arg == "--rotation-fixtures") {
        ui.set_animations_enabled(false);
        let weak = ui.as_weak();
        let rotating_window = window.clone();
        ui.on_orientation_requested(move |degrees| {
            let ui = weak.unwrap();
            let landscape = degrees == 90 || degrees == 270;
            let (w, h) = if landscape {
                (height, width)
            } else {
                (width, height)
            };
            ui.set_rotation(degrees);
            ui.set_initial_width(w as f32);
            ui.set_initial_height(h as f32);
            rotating_window.set_size(PhysicalSize::new(w as u32, h as u32));
        });
        ui.set_page(3);
        for degrees in [90, 180, 270, 0] {
            let w = ui.get_initial_width() as usize;
            frame(&window, &clock, 20, &mut pixels, w);
            click(&window, 200., header_rows as f32 + 94.);
            assert_eq!(
                ui.get_rotation(),
                degrees,
                "Settings rotation must cycle all four angles"
            );
        }
        ui.set_page(0);
        for degrees in [0, 90, 180, 270] {
            let landscape = degrees == 90 || degrees == 270;
            let (logical_width, logical_height) = if landscape {
                (height, width)
            } else {
                (width, height)
            };
            ui.set_rotation(degrees);
            ui.set_initial_width(logical_width as f32);
            ui.set_initial_height(logical_height as f32);
            window.set_size(PhysicalSize::new(
                logical_width as u32,
                logical_height as u32,
            ));
            slint::platform::update_timers_and_animations();
            for rotated in [false, true] {
                let rotation = if rotated {
                    match degrees {
                        90 => RenderingRotation::Rotate90,
                        180 => RenderingRotation::Rotate180,
                        270 => RenderingRotation::Rotate270,
                        _ => RenderingRotation::NoRotation,
                    }
                } else {
                    RenderingRotation::NoRotation
                };
                let stride = if rotated { width } else { logical_width };
                window.request_redraw();
                window.draw_if_needed(|renderer| {
                    renderer.set_repaint_buffer_type(RepaintBufferType::NewBuffer);
                    renderer.set_rendering_rotation(rotation);
                    renderer.render_by_line(Lines {
                        pixels: &mut pixels,
                        stride,
                    });
                });
                let orientation = degrees;
                let kind = if rotated { "panel" } else { "logical" };
                capture(
                    &format!("touch-{orientation}-{kind}"),
                    &pixels,
                    stride,
                    width * height / stride,
                );
            }
        }
        println!("Actual render_by_line rotation fixtures generated");
        return;
    }
    // Long connection states must elide before the cog and preserve its hit area.
    ui.set_animations_enabled(false);
    let connection = ui.get_connection();
    ui.set_connection("Disconnected".into());
    ui.set_fresh(false);
    frame(&window, &clock, 0, &mut pixels, width);
    let disconnected_dot = pixels[56 * width + 37];
    assert!(
        disconnected_dot.r > disconnected_dot.g.saturating_add(60),
        "Disconnected indicator is not at the header start"
    );
    let cog_pixels = |pixels: &[Rgb8Pixel]| -> Vec<Rgb8Pixel> {
        (32..80)
            .flat_map(|y| {
                pixels[y * width + width - 80..y * width + width - 32]
                    .iter()
                    .copied()
            })
            .collect()
    };
    let cog = cog_pixels(&pixels);
    ui.set_connection(
        "Discovering controller / waiting for encrypted pairing and package verification".into(),
    );
    ui.set_fresh(true);
    frame(&window, &clock, 0, &mut pixels, width);
    let live_dot = pixels[56 * width + 37];
    assert!(
        live_dot.g > live_dot.r.saturating_add(60) && live_dot.b > live_dot.r.saturating_add(60),
        "Live indicator is not at the header start"
    );
    assert_eq!(
        cog_pixels(&pixels),
        cog,
        "Connection text overlapped Settings cog"
    );
    capture("header-long-status", &pixels, width, height);
    click(&window, width as f32 - 56., 56.);
    assert_eq!(ui.get_page(), 3, "Long status blocked the Settings control");
    ui.set_page(0);
    ui.set_connection(connection);
    frame(&window, &clock, 0, &mut pixels, width);
    ui.set_animations_enabled(true);
    frame(&window, &clock, 0, &mut pixels, width);
    capture("ride", &pixels, width, height);
    let mut time = 10;
    for page in [3, 4, 5, 6, 2, 1, 0] {
        let previous = ui.get_page();
        ui.set_page(page);
        frame(&window, &clock, time, &mut pixels, width);
        assert!(ui.get_transitioning());
        assert_eq!(ui.get_displayed_page(), page);
        assert_eq!(ui.get_outgoing_page(), previous);
        let start = pixels.clone();
        frame(&window, &clock, time + 100, &mut pixels, width);
        assert!(ui.get_transitioning());
        assert_ne!(start, pixels, "Page content did not animate");
        assert_eq!(
            &start[..header_rows * width],
            &pixels[..header_rows * width],
            "Header moved"
        );
        assert_eq!(
            &start[(height - 82) * width..],
            &pixels[(height - 82) * width..],
            "Footer moved"
        );
        capture(&format!("to-{page}-mid"), &pixels, width, height);
        click(&window, 75., (height - 57) as f32);
        assert_eq!(
            ui.get_page(),
            page,
            "A moving screen accepted navigation input"
        );
        frame(&window, &clock, time + 280, &mut pixels, width);
        assert!(!ui.get_transitioning());
        assert_eq!(ui.get_outgoing_page(), -1);
        capture(&format!("to-{page}-end"), &pixels, width, height);
        time += 300;
    }
    // Primary-page swipes use real input events through the native recognizer.
    let applied = Rc::new(Cell::new(0));
    let writes = applied.clone();
    ui.on_apply_mode(move |_| writes.set(writes.get() + 1));
    ui.set_connected(true);
    ui.set_can_configure(true);
    // Deliberately overflow every viewport so the vertical gesture check exercises
    // scrolling rather than assuming a production page must always overflow.
    ui.set_feedback("Scrollable feedback fixture\n".repeat(40).into());
    ui.set_peers(slint::ModelRc::new(slint::VecModel::from(
        (0..8)
            .map(|i| Peer {
                name: format!("Test bike {i}").into(),
                address: format!("00:00:00:00:00:{i:02}").into(),
                signal: "-50 dBm".into(),
            })
            .collect::<Vec<_>>(),
    )));
    let bike_actions = Rc::new(Cell::new(0));
    let actions = bike_actions.clone();
    ui.on_forget_peer(move || actions.set(actions.get() + 1));
    let actions = bike_actions.clone();
    ui.on_connect_peer(move |_| actions.set(actions.get() + 1));
    let mut swipe = |dx: f32, dy: f32, expected: i32| {
        let start_page = ui.get_page();
        let start_x = width as f32 / 2.;
        let start_y = apply_y; // Start on Tune Apply to verify drags never submit it.
        pointer(&window, "down", start_x, start_y);
        frame(&window, &clock, time, &mut pixels, width);
        let before = pixels.clone();
        pointer(&window, "move", start_x + dx / 2., start_y + dy / 2.);
        frame(&window, &clock, time + 25, &mut pixels, width);
        pointer(&window, "move", start_x + dx, start_y + dy);
        frame(&window, &clock, time + 60, &mut pixels, width);
        assert_eq!(
            ui.get_page(),
            start_page,
            "Swipe committed before finger release"
        );
        if dy == 0. && expected != start_page {
            assert_ne!(before, pixels, "Page did not follow the finger");
            assert_eq!(
                &before[..header_rows * width],
                &pixels[..header_rows * width],
                "Swipe moved the header"
            );
            assert_eq!(
                &before[(height - 82) * width..],
                &pixels[(height - 82) * width..],
                "Swipe moved the footer"
            );
            capture(
                &format!("swipe-{start_page}-to-{expected}"),
                &pixels,
                width,
                height,
            );
        }
        if dy.abs() > dx.abs() {
            assert!(before != pixels, "Vertical scrolling stopped working");
        }
        pointer(&window, "up", start_x + dx, start_y + dy);
        frame(&window, &clock, time + 80, &mut pixels, width);
        frame(&window, &clock, time + 400, &mut pixels, width);
        assert_eq!(ui.get_page(), expected);
        assert_eq!(ui.get_displayed_page(), expected);
        assert!(!ui.get_transitioning());
        println!(
            "swipe {start_page} delta({dx},{dy}) -> page{} shown{} outgoing{}",
            ui.get_page(),
            ui.get_displayed_page(),
            ui.get_outgoing_page()
        );
        assert_eq!(ui.get_outgoing_page(), -1);
        assert_eq!(applied.get(), 0, "Swipe activated a tuning write");
        assert_eq!(bike_actions.get(), 0, "Swipe selected or forgot a bike");
        time += 450;
    };
    swipe(100., 0., 0); // Right edge on Ride is bounded.
    swipe(-100., 0., 1);
    swipe(-25., 0., 1); // Short horizontal gestures snap back, without a click.
    swipe(5., -100., 1); // Vertical movement stays on the vertically scrollable Tune page.
    swipe(-100., 0., 2);
    swipe(0., -80., 2); // Bike list still scrolls, without selecting/forgetting a peer.
    swipe(-100., 0., 2); // Left edge on Bike is bounded.
    swipe(100., 0., 1);
    swipe(100., 0., 0);
    drop(swipe);
    // A deliberate reversal cancels a drag even if net displacement exceeds40px.
    let x = width as f32 / 2.;
    pointer(&window, "down", x, 200.);
    pointer(&window, "move", x - 140., 200.);
    frame(&window, &clock, time, &mut pixels, width);
    pointer(&window, "move", x - 70., 200.);
    frame(&window, &clock, time + 50, &mut pixels, width);
    pointer(&window, "up", x - 70., 200.);
    frame(&window, &clock, time + 400, &mut pixels, width);
    assert_eq!(ui.get_page(), 0);
    time += 450;
    // A normal button tap still works after a cancelled swipe.
    ui.set_page(1);
    frame(&window, &clock, time, &mut pixels, width);
    frame(&window, &clock, time + 280, &mut pixels, width);
    click(&window, width as f32 / 2., apply_y);
    frame(&window, &clock, time + 300, &mut pixels, width);
    assert_eq!(applied.get(), 1);
    ui.set_page(0);
    frame(&window, &clock, time + 350, &mut pixels, width);
    frame(&window, &clock, time + 650, &mut pixels, width);
    time += 700;
    // The same touch must work after the animation blocker has disappeared.
    click(&window, (width / 2) as f32, (height - 57) as f32);
    frame(&window, &clock, time, &mut pixels, width);
    assert_eq!(ui.get_page(), 1);
    assert!(ui.get_transitioning());
    frame(&window, &clock, time + 280, &mut pixels, width);
    pointer(&window, "down", width as f32 / 2., 200.);
    pointer(&window, "move", width as f32 / 2. - 80., 200.);
    frame(&window, &clock, time + 310, &mut pixels, width);
    assert_eq!(ui.get_outgoing_page(), 2);
    // Rotation cancels a finger-held drag; its later release must not navigate.
    ui.set_initial_width(height as f32);
    ui.set_initial_height(width as f32);
    window.set_size(PhysicalSize::new(height as u32, width as u32));
    pixels.resize(width * height, Rgb8Pixel::default());
    frame(&window, &clock, time + 350, &mut pixels, height);
    assert!(!ui.get_transitioning());
    assert_eq!(ui.get_outgoing_page(), -1);
    pointer(&window, "up", width as f32 / 2. - 80., 200.);
    frame(&window, &clock, time + 650, &mut pixels, height);
    assert_eq!(ui.get_page(), 1);
    // A half-turn must cancel gestures even though the logical dimensions do not change.
    pointer(&window, "down", height as f32 / 2., 200.);
    pointer(&window, "move", height as f32 / 2. - 80., 200.);
    frame(&window, &clock, time + 700, &mut pixels, height);
    assert_eq!(ui.get_outgoing_page(), 2);
    ui.set_rotation(180);
    frame(&window, &clock, time + 720, &mut pixels, height);
    assert_eq!(ui.get_outgoing_page(), -1);
    assert!(!ui.get_transitioning());
    pointer(&window, "up", height as f32 / 2. - 80., 200.);
    frame(&window, &clock, time + 1000, &mut pixels, height);
    assert_eq!(ui.get_page(), 1);
    println!(
        "{width}x{height}: transitions, bounded swipes, cancelled writes, scrolling, taps and rotation passed"
    );
}
