use slint::platform::software_renderer::{
    LineBufferProvider, MinimalSoftwareWindow, RenderingRotation, RepaintBufferType,
};
use slint::platform::{Platform, PointerEventButton, WindowAdapter, WindowEvent};
use slint::{ComponentHandle, Model, PhysicalSize, Rgb8Pixel};
use std::cell::Cell;
use std::io::Write;
use std::rc::Rc;
use std::time::Duration;
thread_local! { static SCROLL_TIME: Cell<u64> = const { Cell::new(0) }; }
slint::include_modules!();
#[path = "../stats_fixture.rs"]
mod stats_fixture;

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
    clock.set(Duration::from_millis(at + SCROLL_TIME.with(Cell::get)));
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
fn control(ui: &Dashboard, label: &str) -> i_slint_backend_testing::ElementHandle {
    let controls: Vec<_> =
        i_slint_backend_testing::ElementHandle::find_by_accessible_label(ui, label)
            .filter(|control| {
                control.accessible_role() == Some(i_slint_backend_testing::AccessibleRole::Button)
            })
            .collect();
    assert_eq!(controls.len(), 1, "Expected one control labelled {label:?}");
    controls.into_iter().next().unwrap()
}

fn element(ui: &Dashboard, id: &str) -> i_slint_backend_testing::ElementHandle {
    i_slint_backend_testing::ElementHandle::find_by_element_id(ui, id)
        .next()
        .expect(id)
}

fn element_pixels(
    item: &i_slint_backend_testing::ElementHandle,
    pixels: &[Rgb8Pixel],
    width: usize,
) -> Vec<Rgb8Pixel> {
    let p = item.absolute_position();
    let size = item.size();
    let left = p.x.round().max(0.) as usize;
    let right = ((p.x + size.width).round().max(0.) as usize).min(width);
    let top = p.y.round().max(0.) as usize;
    let bottom = ((p.y + size.height).round().max(0.) as usize).min(pixels.len() / width);
    (top..bottom)
        .flat_map(|y| pixels[y * width + left..y * width + right].iter().copied())
        .collect()
}

fn control_center(ui: &Dashboard, label: &str) -> (f32, f32) {
    let item = control(ui, label);
    let position = item.absolute_position();
    let size = item.size();
    assert!(
        size.width > 0. && size.height > 0.,
        "Empty control: {label}"
    );
    (position.x + size.width / 2., position.y + size.height / 2.)
}

fn click_control(
    window: &MinimalSoftwareWindow,
    ui: &Dashboard,
    clock: &Cell<Duration>,
    pixels: &mut [Rgb8Pixel],
    label: &str,
) {
    let modal = ui.get_pin_open()
        || ui.get_pairing_open()
        || ui.get_editing()
        || ui.get_confirming()
        || ui.get_ota_confirming();
    let navigation = ["Settings", "Ride", "Stats", "Tune"].contains(&label);
    let size = window.window().size();
    if !modal && !navigation {
        let settle = |pixels: &mut [Rgb8Pixel]| {
            let previous_offset = SCROLL_TIME.with(Cell::get);
            SCROLL_TIME.with(|offset| offset.set(previous_offset + 250));
            let base = (clock.get().as_millis() as u64).saturating_sub(previous_offset);
            frame(window, clock, base, pixels, size.width as usize);
        };
        settle(pixels);
        let mut reset_to_top = false;
        for attempt in 0..32 {
            let deck = element(ui, "Dashboard::pages");
            let top = deck.absolute_position().y;
            let bottom = top + deck.size().height;
            let visible = i_slint_backend_testing::ElementHandle::find_by_accessible_label(
                ui, label,
            )
            .find(|item| {
                item.accessible_role() == Some(i_slint_backend_testing::AccessibleRole::Button)
            });
            let center = visible
                .as_ref()
                .map(|item| item.absolute_position().y + item.size().height / 2.);
            if center.is_some_and(|y| y >= top + 2. && y <= bottom - 2.) {
                break;
            }
            assert!(attempt < 31, "Could not scroll {label:?} into view");
            let step = ((bottom - top) * 0.6).min(220.);
            let delta = match center {
                None if !reset_to_top => {
                    reset_to_top = true;
                    10000.
                }
                Some(y) if y < top => (top - y + 32.).min(step),
                _ => -step,
            };
            window.dispatch_event(WindowEvent::PointerScrolled {
                position: slint::LogicalPosition::new(size.width as f32 / 2., (top + bottom) / 2.),
                delta_x: 0.,
                delta_y: delta,
            });
            settle(pixels);
        }
    }
    let (x, y) = control_center(ui, label);
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
    if args.iter().any(|arg| arg == "--safe-area-fixtures") {
        ui.set_animations_enabled(false);
        frame(&window, &clock, 0, &mut pixels, width);
        let main = element(&ui, "SafeArea::content");
        assert_eq!(
            main.absolute_position(),
            slint::LogicalPosition::new(32., 16.)
        );
        assert_eq!(
            main.size(),
            slint::LogicalSize::new(width as f32 - 64., height as f32 - 32.)
        );
        for modal in 0..6 {
            ui.set_editing(modal == 1);
            ui.set_pin_open(modal == 2);
            ui.set_pairing_open(modal == 3);
            ui.set_confirming(modal == 4);
            ui.set_ota_confirming(modal == 5);
            ui.set_display_safe_inset_x(48.);
            ui.set_display_safe_inset_y(32.);
            frame(&window, &clock, modal * 100, &mut pixels, width);
            let contents: Vec<_> = i_slint_backend_testing::ElementHandle::find_by_element_id(
                &ui,
                "SafeArea::content",
            )
            .collect();
            assert_eq!(
                contents.len(),
                if modal == 0 { 1 } else { 2 },
                "Missing safe area for overlay {modal}"
            );
            for (x, y, at) in [(48., 32., 1), (48., 48., 2), (64., 48., 3)] {
                ui.set_display_safe_inset_x(x);
                ui.set_display_safe_inset_y(y);
                frame(&window, &clock, modal * 100 + at, &mut pixels, width);
                for content in &contents {
                    assert_eq!(
                        content.absolute_position(),
                        slint::LogicalPosition::new(x, y),
                        "Wrong inset on overlay {modal}"
                    );
                    assert_eq!(
                        content.size(),
                        slint::LogicalSize::new(width as f32 - 2. * x, height as f32 - 2. * y),
                        "Axes coupled on overlay {modal}"
                    );
                }
            }
        }
        println!(
            "{width}x{height}: independent safe-area axes and keyboard/PIN/confirmation propagation passed"
        );
        return;
    }
    if args.iter().any(|arg| arg == "--marquee-fixtures") {
        ui.set_connected(true);
        ui.set_link_present(true);
        ui.set_phone_connected(true);
        let status = "Discovering controller | waiting for encrypted Bluetooth pairing and verifying the installed vehicle package";
        ui.set_connection(status.into());
        frame(&window, &clock, 0, &mut pixels, width);
        frame(&window, &clock, 0, &mut pixels, width);
        let viewport =
            i_slint_backend_testing::ElementHandle::find_by_element_id(&ui, "Header::status-text")
                .next()
                .expect("Status viewport is missing");
        let label =
            i_slint_backend_testing::ElementHandle::find_by_element_id(&ui, "MarqueeText::label")
                .next()
                .expect("Status label is missing");
        let start_x = viewport.absolute_position().x;
        let travel = label.size().width - viewport.size().width;
        assert!(travel > 0., "Fixture must overflow even the widest header");
        let duration = (travel / 32. * 1000.).max(800.).ceil() as u64;
        let cog_control = control(&ui, "Settings");
        let status_pixels = |pixels: &[Rgb8Pixel]| element_pixels(&viewport, pixels, width);
        let cog_pixels = |pixels: &[Rgb8Pixel]| element_pixels(&cog_control, pixels, width);
        let start = status_pixels(&pixels);
        let cog = cog_pixels(&pixels);
        let rows = start
            .chunks(viewport.size().width.round() as usize)
            .filter(|row| {
                row.iter()
                    .any(|pixel| pixel.r > 32 || pixel.g > 32 || pixel.b > 32)
            })
            .count();
        assert!(
            rows <= 22,
            "Status wrapped into multiple text lines ({rows} rows)"
        );
        frame(&window, &clock, 1000, &mut pixels, width);
        assert_eq!(
            status_pixels(&pixels),
            start,
            "Status did not pause before scrolling"
        );
        frame(&window, &clock, 1200, &mut pixels, width);
        frame(&window, &clock, 1700, &mut pixels, width);
        assert_ne!(
            status_pixels(&pixels),
            start,
            "Overflowing status did not scroll"
        );
        assert_eq!(
            cog_pixels(&pixels),
            cog,
            "Marquee escaped its viewport over the cog"
        );
        capture("header-marquee-moving", &pixels, width, height);
        click_control(&window, &ui, &clock, &mut pixels, "Settings");
        assert_eq!(ui.get_page(), 3, "Marquee blocked the Settings cog");
        frame(&window, &clock, 1700, &mut pixels, width);
        let end = 1200 + duration;
        frame(&window, &clock, end, &mut pixels, width);
        assert!(
            (label.absolute_position().x - (start_x - travel)).abs() < 1.,
            "Marquee did not reveal the end"
        );
        let at_end = status_pixels(&pixels);
        frame(&window, &clock, end + 1000, &mut pixels, width);
        assert_eq!(
            status_pixels(&pixels),
            at_end,
            "Status did not pause at the end"
        );
        frame(&window, &clock, end + 1200, &mut pixels, width);
        frame(&window, &clock, end + 1700, &mut pixels, width);
        assert!(
            label.absolute_position().x > start_x - travel + 1.,
            "Marquee did not return"
        );
        let returned = end + 1200 + duration;
        frame(&window, &clock, returned, &mut pixels, width);
        assert!(
            (label.absolute_position().x - start_x).abs() < 1.,
            "Marquee did not return to the start"
        );
        ui.set_connection("Connected".into());
        frame(&window, &clock, returned + 10, &mut pixels, width);
        assert!(
            (label.absolute_position().x - start_x).abs() < 1.,
            "Changed status kept an old offset"
        );
        let short = status_pixels(&pixels);
        frame(&window, &clock, returned + 10000, &mut pixels, width);
        assert_eq!(
            status_pixels(&pixels),
            short,
            "Short status should stay still"
        );
        println!(
            "{width}x{height}: single-line marquee, both pauses, return, reset and cog touch passed"
        );
        return;
    }
    if args.iter().any(|arg| arg == "--ride-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_battery("82%".into());
        ui.set_battery_level(82.);
        ui.set_voltage("75.6 V".into());
        ui.set_power("-12000 W".into());
        ui.set_pitch("-12.5°".into());
        ui.set_mosfet("100 °C".into());
        ui.set_motor("100 °C".into());
        ui.set_ble_proxy_enabled(true); // Enabling the proxy alone must not show a phone connection.
        for (
            name,
            connected,
            fresh,
            pending,
            phone,
            physical,
            connection,
            fault,
            expected,
            alert,
            color,
        ) in [
            (
                "ride-live",
                true,
                true,
                false,
                false,
                true,
                "Connected",
                "",
                "Connected",
                false,
                (96, 211, 148),
            ),
            (
                "ride-stale",
                true,
                false,
                false,
                false,
                true,
                "Connected | telemetry stale",
                "Controller fault 6",
                "Connected | telemetry stale",
                false,
                (255, 195, 0),
            ),
            (
                "ride-fault",
                true,
                true,
                false,
                false,
                true,
                "Connected",
                "Controller fault 6",
                "Controller fault 6",
                true,
                (255, 68, 68),
            ),
            (
                "ride-hot",
                true,
                true,
                false,
                false,
                true,
                "Connected",
                "Motor hot",
                "Motor hot",
                true,
                (255, 68, 68),
            ),
            (
                "ride-disconnected",
                false,
                false,
                false,
                false,
                false,
                "Disconnected",
                "Controller fault 6",
                "Disconnected",
                false,
                (255, 68, 68),
            ),
            (
                "ride-reconnecting",
                false,
                false,
                true,
                false,
                false,
                "Reconnecting",
                "",
                "Reconnecting",
                false,
                (255, 195, 0),
            ),
            (
                "ride-connecting",
                false,
                false,
                true,
                false,
                false,
                "Connecting to bike...",
                "",
                "Connecting to bike...",
                false,
                (255, 195, 0),
            ),
            (
                "ride-probe",
                false,
                false,
                true,
                false,
                true,
                "Finding main VESC | CAN ping and package discovery",
                "",
                "Finding main VESC | CAN ping and package discovery",
                false,
                (255, 195, 0),
            ),
            (
                "ride-security",
                false,
                false,
                true,
                false,
                true,
                "Bike requires BLE authentication",
                "",
                "Bike requires BLE authentication",
                false,
                (255, 195, 0),
            ),
            (
                "ride-paused",
                false,
                false,
                true,
                false,
                false,
                "Vehicle link paused for update",
                "",
                "Vehicle link paused for update",
                false,
                (255, 195, 0),
            ),
            (
                "ride-error",
                false,
                false,
                false,
                false,
                true,
                "Vehicle link unavailable",
                "",
                "Vehicle link unavailable",
                false,
                (255, 68, 68),
            ),
            (
                "ride-phone-pairing",
                true,
                false,
                false,
                false,
                true,
                "Connected | waiting for telemetry",
                "",
                "Connected | waiting for telemetry",
                false,
                (255, 195, 0),
            ),
            (
                "ride-proxy",
                true,
                false,
                false,
                true,
                true,
                "Connected",
                "Controller hot",
                "Connected",
                false,
                (255, 195, 0),
            ),
            (
                "ride-bike-stale",
                true,
                true,
                false,
                true,
                true,
                "Connected | bike telemetry stale",
                "",
                "Connected | bike telemetry stale",
                false,
                (255, 195, 0),
            ),
            (
                "ride-proxy-fault",
                true,
                true,
                false,
                true,
                true,
                "Connected",
                "Controller fault 6",
                "Controller fault 6",
                true,
                (255, 68, 68),
            ),
            (
                "ride-proxy-discovery",
                false,
                false,
                true,
                true,
                true,
                "Finding main VESC",
                "",
                "Finding main VESC",
                false,
                (255, 195, 0),
            ),
        ] {
            ui.set_connected(connected);
            ui.set_data_stale(name == "ride-bike-stale");
            ui.set_bike_ble_rssi(-62);
            ui.set_bike_ble_signal(if connected && physical { 3 } else { -1 });
            ui.set_link_present(physical);
            ui.set_connection_pending(pending);
            ui.set_phone_connected(phone);
            ui.set_trip(if fresh { "12.34 km" } else { "--" }.into());
            ui.set_odometer(if fresh { "12345.6 km" } else { "--" }.into());
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
            let header_color = ui.get_header_color();
            assert_eq!(
                (
                    header_color.red(),
                    header_color.green(),
                    header_color.blue()
                ),
                color,
                "Wrong status color for {name}"
            );
            frame(&window, &clock, 0, &mut pixels, width);
            let dot = element(&ui, "Header::connection-indicator");
            let dot_position = dot.absolute_position();
            let dot_size = dot.size();
            let sample = pixels[(dot_position.y + dot_size.height / 2.).round() as usize * width
                + (dot_position.x + dot_size.width / 2.).round() as usize];
            assert_eq!(
                (sample.r, sample.g, sample.b),
                color,
                "Indicator does not match status color for {name}"
            );
            let status_pixels =
                element_pixels(&element(&ui, "Header::status-text"), &pixels, width);
            let text_color = if alert {
                (255, 68, 68)
            } else {
                (255, 255, 255)
            };
            let projected_text_color = ui.get_header_text_color();
            assert_eq!(
                (
                    projected_text_color.red(),
                    projected_text_color.green(),
                    projected_text_color.blue()
                ),
                text_color,
                "Only a current critical alert should change status text color for {name}"
            );
            assert!(
                status_pixels.iter().any(|p| (p.r, p.g, p.b) == text_color),
                "Status text must retain its normal color outside critical alerts for {name}"
            );
            let phone_icons: Vec<_> =
                i_slint_backend_testing::ElementHandle::find_by_accessible_label(
                    &ui,
                    "Phone connected to BLE proxy",
                )
                .collect();
            assert_eq!(
                phone_icons.len(),
                usize::from(phone),
                "Phone indicator must reflect an authorized connection for {name}"
            );
            if phone {
                let icon = &phone_icons[0];
                let pos = icon.absolute_position();
                let size = icon.size();
                let cog = control(&ui, "Settings");
                let cog_pos = cog.absolute_position();
                assert!(
                    (pos.y + size.height / 2. - cog_pos.y - cog.size().height / 2.).abs() < 1.,
                    "Phone icon and cog are not centered"
                );
                assert!(
                    (cog_pos.x - pos.x - size.width - 12.).abs() < 1.,
                    "Phone icon should sit immediately left of Settings"
                );
                let viewport = element(&ui, "Header::status-text");
                assert!(
                    viewport.absolute_position().x + viewport.size().width + 11. <= pos.x,
                    "Status text overlaps the phone indicator"
                );
            }
            let signal_icons: Vec<_> =
                i_slint_backend_testing::ElementHandle::find_by_accessible_label(
                    &ui,
                    "Bike BLE signal -62 dBm",
                )
                .collect();
            assert_eq!(
                signal_icons.len(),
                usize::from(connected && physical),
                "RSSI visibility for {name}"
            );
            if let Some(signal) = signal_icons.first() {
                let pos = signal.absolute_position();
                let size = signal.size();
                let next = if phone {
                    phone_icons[0].clone()
                } else {
                    control(&ui, "Settings")
                };
                let next_pos = next.absolute_position();
                assert!(
                    (next_pos.x - pos.x - size.width - 12.).abs() < 1.,
                    "RSSI must sit immediately left of phone/cog"
                );
                assert!(
                    (pos.y + size.height / 2. - next_pos.y - next.size().height / 2.).abs() < 1.,
                    "RSSI and phone/cog must be vertically centered"
                );
                let viewport = element(&ui, "Header::status-text");
                assert!(
                    viewport.absolute_position().x + viewport.size().width + 11. <= pos.x,
                    "Status text overlaps RSSI"
                );
                assert!(
                    pos.x >= 0. && pos.x + size.width <= width as f32,
                    "RSSI clipped horizontally"
                );
            }
            capture(name, &pixels, width, height);
            if width < height && name == "ride-live" {
                let area = element(&ui, "RidePage::dial-area");
                let distance = element(&ui, "RidePage::portrait-distance");
                let face = element(&ui, "Speedometer::dial-face");
                let dial = element(&ui, "RidePage::portrait-dial");
                let box_bounds = element(&ui, "RidePage::portrait-speedo-box");
                let origin = area.absolute_position();
                let bounds = area.size();
                assert!(
                    face.size().width > bounds.width * 0.7,
                    "Portrait arc did not reclaim the distance row"
                );
                assert!(
                    distance.absolute_position().y + distance.size().height
                        <= origin.y + bounds.height + 1.,
                    "Distance readings clipped by the stats"
                );
                let mut columns: Vec<_> =
                    i_slint_backend_testing::ElementHandle::find_by_element_id(
                        &ui,
                        "DistanceReading::column",
                    )
                    .collect();
                columns.sort_by(|a, b| a.absolute_position().x.total_cmp(&b.absolute_position().x));
                assert_eq!(columns.len(), 2);
                assert!((columns[0].absolute_position().x - origin.x).abs() < 1.);
                assert!(
                    (columns[1].absolute_position().x + columns[1].size().width
                        - origin.x
                        - bounds.width)
                        .abs()
                        < 1.,
                    "ODO is not anchored to the outside edge"
                );
                assert!(
                    (distance.absolute_position().y - box_bounds.absolute_position().y).abs() < 1.,
                    "Distance readings are not at the top of the speedometer box"
                );
                assert!(
                    (dial.absolute_position().y
                        - distance.absolute_position().y
                        - distance.size().height
                        - 4.)
                        .abs()
                        < 1.,
                    "Top readings do not retain their arc clearance"
                );
                let mut gauge_bottom = 0;
                for y in origin.y.round() as usize..(origin.y + bounds.height).round() as usize {
                    for pixel in &pixels[y * width + origin.x.round() as usize
                        ..y * width + (origin.x + bounds.width).round() as usize]
                    {
                        if pixel.r > 200 && pixel.g > 170 && pixel.b < 50 {
                            gauge_bottom = y;
                        }
                    }
                }
                let painted_bottom = gauge_bottom as f32 + 1.;
                let dial_bottom = dial.absolute_position().y + dial.size().height;
                assert!(
                    (dial_bottom - painted_bottom).abs() <= 2.,
                    "Speedometer retained an unpainted bottom margin: box ends {dial_bottom}, arc ends {painted_bottom}"
                );
                assert!(
                    (box_bounds.absolute_position().y + box_bounds.size().height - dial_bottom)
                        .abs()
                        < 1.,
                    "Speedometer box adds blank space below the painted dial"
                );
                assert!(
                    (dial_bottom - origin.y - bounds.height).abs() < 1.,
                    "Unused height leaked below the portrait arc instead of above the speedometer group"
                );
            }
            if width > height && name == "ride-live" {
                let mut gauge_bottom = None;
                let mut battery_top = None;
                let mut battery_bottom = None;
                let deck = element(&ui, "Dashboard::pages");
                let left = deck.absolute_position().x.round() as usize;
                let top = deck.absolute_position().y.round() as usize;
                let bottom = (deck.absolute_position().y + deck.size().height).round() as usize;
                for y in top..bottom {
                    for pixel in &pixels[y * width + left..y * width + width / 2 - 8] {
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
        ui.set_data_stale(false);
        ui.set_fault_text("".into());
        ui.set_connected(true);
        ui.set_fresh(true);
        ui.set_phone_connected(true);
        ui.set_connection("Connected".into());
        for level in 0..=4 {
            ui.set_bike_ble_signal(level);
            frame(&window, &clock, 0, &mut pixels, width);
            let bars = element(&ui, "Header::signal-bars");
            let pos = bars.absolute_position();
            for bar in 0..4 {
                let expected = if bar < level {
                    if level == 1 {
                        (255, 195, 0)
                    } else {
                        (187, 187, 187)
                    }
                } else {
                    (128, 128, 128)
                };
                let pixel = pixels[(pos.y + 20.).round() as usize * width
                    + (pos.x + 3. + bar as f32 * 6.).round() as usize];
                assert_eq!(
                    (pixel.r, pixel.g, pixel.b),
                    expected,
                    "Signal bars level {level}, bar {bar}"
                );
            }
        }
        ui.set_bike_ble_signal(-1);
        frame(&window, &clock, 0, &mut pixels, width);
        assert!(
            i_slint_backend_testing::ElementHandle::find_by_accessible_label(
                &ui,
                "Bike BLE signal -62 dBm"
            )
            .next()
            .is_none(),
            "CAN/UART or unknown RSSI must hide the BLE indicator"
        );
        println!(
            "{width}x{height}: Ride header priority, RSSI bars, stale data and status fixtures passed"
        );
        return;
    }
    frame(&window, &clock, 0, &mut pixels, width);
    let deck = element(&ui, "Dashboard::pages");
    let header_rows = deck.absolute_position().y.round() as usize;
    let footer_start = (deck.absolute_position().y + deck.size().height).round() as usize;
    let preset_y = header_rows as f32 + 88.;
    let apply_y = header_rows as f32 + 148.;
    if args.get(2).is_some_and(|arg| arg == "--stats-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_connected(true);
        ui.set_fresh(true);
        ui.set_speed("24.8".into());
        ui.set_power("640 W".into());
        ui.set_battery("82%".into());
        ui.set_voltage("50.4 V".into());
        ui.set_battery_current("12.7 A".into());
        ui.set_motor_current("22.8 A".into());
        ui.set_trip("12.34 km".into());
        ui.set_odometer("12345.6 km".into());
        ui.set_pitch("4.2°".into());
        ui.set_roll("0.8°".into());
        ui.set_mosfet("36°C".into());
        ui.set_motor("42°C".into());
        stats_fixture::project(&ui, true);
        click_control(&window, &ui, &clock, &mut pixels, "Stats");
        frame(&window, &clock, 0, &mut pixels, width);
        assert_eq!(ui.get_page(), 1);
        assert!(
            i_slint_backend_testing::ElementHandle::find_by_accessible_label(
                &ui,
                "Apply ride response"
            )
            .next()
            .is_none(),
            "Stats must not expose tuning actions"
        );
        assert_eq!(ui.get_vesc_stats().row_count(), 13);
        assert_eq!(ui.get_bms_stats().row_count(), 25);
        assert_eq!(ui.get_bms_cells().row_count(), 13);
        assert_eq!(ui.get_bms_temperatures().row_count(), 3);
        for label in [
            "Duty cycle",
            "Electrical RPM",
            "Consumed charge",
            "Consumed energy",
            "Controller ID",
            "PID position",
            "Remaining energy",
            "Fault code",
        ] {
            let model = ui.get_vesc_stats();
            assert!(
                (0..model.row_count()).any(|index| model
                    .row_data(index)
                    .is_some_and(|item| item.label == label)),
                "Missing controller field: {label}"
            );
        }
        for label in [
            "Pack voltage",
            "Charge voltage",
            "IC input current",
            "Lifetime charge in",
            "Pressure",
            "Status",
            "Cell spread",
        ] {
            let model = ui.get_bms_stats();
            assert!(
                (0..model.row_count()).any(|index| model
                    .row_data(index)
                    .is_some_and(|item| item.label == label)),
                "Missing BMS field: {label}"
            );
        }
        assert_eq!(
            ui.get_bms_cells().row_data(1).unwrap().label,
            "Cell 2 | balancing"
        );
        assert_eq!(ui.get_bms_cells().row_data(12).unwrap().label, "Cell 13");
        capture("stats-live", &pixels, width, height);
        let top = pixels.clone();
        let deck = element(&ui, "Dashboard::pages");
        let header_end = deck.absolute_position().y.floor() as usize * width;
        let header_pixels = top[..header_end].to_vec();
        for (at, delta) in [(10, -1464.), (20, 1464.)] {
            window.dispatch_event(WindowEvent::PointerScrolled {
                position: slint::LogicalPosition::new(width as f32 / 2., 250.),
                delta_x: 0.,
                delta_y: delta,
            });
            frame(&window, &clock, at, &mut pixels, width);
            assert!(
                pixels[..header_end] == header_pixels,
                "Large Stats wheel jump erased fixed header"
            );
        }
        let x = width as f32 / 2.;
        pointer(&window, "down", x, 260.);
        pointer(&window, "move", x, 110.);
        frame(&window, &clock, 25, &mut pixels, width);
        pointer(&window, "up", x, 110.);
        frame(&window, &clock, 50, &mut pixels, width);
        assert_eq!(ui.get_page(), 1, "Vertical stats scroll changed tabs");
        assert_ne!(top, pixels, "Stats did not scroll");
        capture("stats-scrolled", &pixels, width, height);
        assert!(
            pixels[..header_end] == header_pixels,
            "Scrolling Stats repainted over the fixed header"
        );
        let viewport_top = deck.absolute_position().y;
        let viewport_bottom = viewport_top + deck.size().height;
        let at = Cell::new(60_u64);
        let show = |label: &str, ui: &Dashboard, pixels: &mut [Rgb8Pixel]| {
            for attempt in 0..48 {
                let item =
                    i_slint_backend_testing::ElementHandle::find_by_accessible_label(ui, label)
                        .next();
                let geometry = item
                    .as_ref()
                    .map(|item| (item.absolute_position(), item.size()));
                if let Some((position, size)) = geometry {
                    if position.y >= viewport_top + 2.
                        && position.y + size.height <= viewport_bottom - 2.
                    {
                        let item = item.unwrap();
                        assert_eq!(
                            item.accessible_role(),
                            Some(i_slint_backend_testing::AccessibleRole::Text),
                            "Stats rows must remain read only"
                        );
                        let visible = element_pixels(&item, pixels, width);
                        assert!(
                            visible.iter().any(|p| p.r > 60 || p.g > 60 || p.b > 60),
                            "Stats row is visible but not painted: {label}"
                        );
                        return;
                    }
                }
                assert!(
                    attempt < 47,
                    "Could not scroll Stats row into view: {label}"
                );
                let step = ((viewport_bottom - viewport_top) * 0.6).min(240.);
                let delta = if geometry.is_some_and(|(position, _)| position.y < viewport_top) {
                    step
                } else {
                    -step
                };
                window.dispatch_event(WindowEvent::PointerScrolled {
                    position: slint::LogicalPosition::new(
                        width as f32 / 2.,
                        (viewport_top + viewport_bottom) / 2.,
                    ),
                    delta_x: 0.,
                    delta_y: delta,
                });
                at.set(at.get() + 50);
                frame(&window, &clock, at.get(), pixels, width);
                assert_eq!(
                    ui.get_page(),
                    1,
                    "Vertical Stats scroll switched primary pages"
                );
            }
        };
        for (label, image) in [
            ("Duty cycle", "stats-controller-details"),
            ("Pack voltage", "stats-bms"),
            ("Cell 13", "stats-bms-cells"),
            ("Sensor 3", "stats-bms-temperatures"),
        ] {
            show(label, &ui, &mut pixels);
            assert!(
                pixels[..header_end] == header_pixels,
                "Scrolling to {label} repainted over the fixed header"
            );
            capture(image, &pixels, width, height);
        }
        ui.set_celsius(false);
        stats_fixture::project(&ui, true);
        assert!(
            ui.get_bms_temperatures()
                .row_data(2)
                .unwrap()
                .value
                .ends_with("°F")
        );

        ui.set_connected(false);
        ui.set_fresh(false);
        ui.set_speed("--".into());
        ui.set_power("--".into());
        ui.set_voltage("--".into());
        ui.set_battery("--".into());
        ui.set_battery_current("--".into());
        ui.set_motor_current("--".into());
        stats_fixture::project(&ui, false);
        assert_eq!(ui.get_vesc_stats().row_count(), 13);
        assert!((0..13).all(|index| ui.get_vesc_stats().row_data(index).unwrap().value == "--"));
        assert_eq!(ui.get_bms_stats().row_count(), 0);
        assert_eq!(ui.get_bms_cells().row_count(), 0);
        assert_eq!(ui.get_bms_temperatures().row_count(), 0);
        assert_eq!(ui.get_bms_state(), "No recent VESC BMS data.");
        at.set(at.get() + 50);
        frame(&window, &clock, at.get(), &mut pixels, width);
        assert!(
            i_slint_backend_testing::ElementHandle::find_by_accessible_label(&ui, "Cell 13")
                .next()
                .is_none(),
            "Disconnected Stats retained old BMS cell rows"
        );
        window.dispatch_event(WindowEvent::PointerScrolled {
            position: slint::LogicalPosition::new(
                width as f32 / 2.,
                (viewport_top + viewport_bottom) / 2.,
            ),
            delta_x: 0.,
            delta_y: 10000.,
        });
        at.set(at.get() + 50);
        frame(&window, &clock, at.get(), &mut pixels, width);
        show("No recent VESC BMS data.", &ui, &mut pixels);
        capture("stats-disconnected", &pixels, width, height);
        println!(
            "{width}x{height}: Stats controller details, BMS data, 13 cell voltages, temperature sensors, vertical scrolling and disconnected clearing passed"
        );
        return;
    }
    if args.get(2).is_some_and(|arg| arg == "--tuning-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_page(2);
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
        ui.set_pin_pattern(true); // Upstream pairing must remain numeric even with a local pattern.
        ui.set_page(3);
        ui.set_pairing_open(true);
        ui.set_pairing_ready(true);
        let entered = Rc::new(std::cell::RefCell::new(String::new()));
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pairing_type(move |digit| {
            digits.borrow_mut().push(char::from(b'0' + digit as u8));
            weak.upgrade()
                .unwrap()
                .set_pairing_value(digits.borrow().clone().into());
        });
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pairing_clear(move || {
            digits.borrow_mut().clear();
            weak.unwrap().set_pairing_value("".into());
        });
        let submitted = Rc::new(Cell::new(0));
        let calls = submitted.clone();
        ui.on_pairing_submit(move || calls.set(calls.get() + 1));
        frame(&window, &clock, 0, &mut pixels, width);
        capture("pairing-pin", &pixels, width, height);
        assert!(
            i_slint_backend_testing::ElementHandle::find_by_accessible_label(&ui, "Use pattern")
                .next()
                .is_none()
        );
        assert!(
            i_slint_backend_testing::ElementHandle::find_by_accessible_label(&ui, "Use PIN")
                .next()
                .is_none()
        );
        click(&window, width as f32 - 2., 2.);
        click(&window, 2., height as f32 - 2.);
        assert_eq!(ui.get_page(), 3);
        for digit in 1..=3 {
            click_control(&window, &ui, &clock, &mut pixels, &digit.to_string());
        }
        click_control(&window, &ui, &clock, &mut pixels, "Clear PIN");
        assert!(entered.borrow().is_empty());
        assert_eq!(ui.get_pairing_value(), "");
        assert_eq!(submitted.get(), 0);
        click_control(&window, &ui, &clock, &mut pixels, "Submit");
        assert_eq!(submitted.get(), 0, "Empty BLE passkeys cannot submit");
        for digit in 1..=4 {
            click_control(&window, &ui, &clock, &mut pixels, &digit.to_string());
        }
        ui.set_pairing_ready(false);
        click_control(&window, &ui, &clock, &mut pixels, "Submit");
        assert_eq!(submitted.get(), 0);
        ui.set_pairing_ready(true);
        click_control(&window, &ui, &clock, &mut pixels, "Submit");
        assert_eq!(submitted.get(), 1, "Short numeric BLE passkeys must submit");
        click_control(&window, &ui, &clock, &mut pixels, "Clear PIN");
        for digit in 1..=6 {
            click_control(&window, &ui, &clock, &mut pixels, &digit.to_string());
            assert_eq!(submitted.get(), 1, "BLE pairing must wait for Submit");
            assert_eq!(ui.get_pairing_value().as_str(), &"123456"[..digit as usize]);
        }
        assert_eq!(&*entered.borrow(), "123456");
        click_control(&window, &ui, &clock, &mut pixels, "Submit");
        assert_eq!(submitted.get(), 2);
        click_control(&window, &ui, &clock, &mut pixels, "7");
        assert_eq!(submitted.get(), 2, "Full entry must not resubmit");
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
            click_control(&window, &ui, &clock, &mut pixels, "Vehicle connection");
            assert_eq!(ui.get_transport_kind(), kind);
            frame(&window, &clock, 20 + step * 20, &mut pixels, width);
            capture(&format!("transport-kind-{kind}"), &pixels, width, height);
        }
        ui.set_can_change_transport(false);
        click_control(&window, &ui, &clock, &mut pixels, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 0);
        ui.set_can_change_transport(true);
        ui.set_can_available(false);
        click_control(&window, &ui, &clock, &mut pixels, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 2); // Absent CAN is skipped.
        click_control(&window, &ui, &clock, &mut pixels, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 0);
        ui.set_can_available(true);
        ui.set_uart_available(false);
        click_control(&window, &ui, &clock, &mut pixels, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 1); // Absent UART is skipped.
        let proxy_calls = Rc::new(Cell::new(0));
        let called = proxy_calls.clone();
        ui.on_ble_proxy_changed(move |_| called.set(called.get() + 1));
        click_control(&window, &ui, &clock, &mut pixels, "BLE proxy");
        assert_eq!(proxy_calls.get(), 1);
        click_control(&window, &ui, &clock, &mut pixels, "Vehicle connection");
        assert_eq!(ui.get_transport_kind(), 0);
        click_control(&window, &ui, &clock, &mut pixels, "BLE proxy");
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
            for page in [0, 1, 2, 3, 6] {
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
                ui.set_page(2);
                fixture_time += 20;
                frame(&window, &clock, fixture_time, &mut pixels, width);
                click_control(&window, &ui, &clock, &mut pixels, "Apply ride response");
                assert_eq!(applied.get(), 1, "WFB tuning controls disappeared");
                ui.set_connected(false);
                fixture_time += 20;
                frame(&window, &clock, fixture_time, &mut pixels, width);
                click_control(&window, &ui, &clock, &mut pixels, "Apply ride response");
                assert_eq!(applied.get(), 1, "Disconnected WFB allowed a write");
                ui.set_connected(true);
            } else {
                ui.set_page(2);
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
    if args.get(2).is_some_and(|arg| arg == "--pattern-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_pin_open(true);
        ui.set_pin_pattern(true);
        ui.set_pin_heading("ENTER PATTERN".into());
        ui.set_pin_message("Connect six dots, then lift your finger.".into());
        ui.set_screen_locked(true);
        let entered = Rc::new(std::cell::RefCell::new(String::new()));
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pin_digit(move |digit| {
            digits.borrow_mut().push(char::from(b'0' + digit as u8));
            weak.unwrap().set_pin_value(digits.borrow().clone().into());
        });
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pin_clear(move || {
            digits.borrow_mut().clear();
            weak.unwrap().set_pin_value("".into());
        });
        let submissions = Rc::new(Cell::new(0));
        let called = submissions.clone();
        ui.on_pin_submit(move || called.set(called.get() + 1));
        frame(&window, &clock, 0, &mut pixels, width);
        capture("pattern-empty", &pixels, width, height);
        let grid = element(&ui, "PatternPad::grid");
        let origin = grid.absolute_position();
        let side = grid.size().width;
        let point = |node: i32| {
            (
                origin.x + ((node - 1) % 3) as f32 * side / 3. + side / 6.,
                origin.y + ((node - 1) / 3) as f32 * side / 3. + side / 6.,
            )
        };
        let (x, y) = point(1);
        pointer(&window, "down", x, y);
        for node in [3, 9, 8] {
            let (x, y) = point(node);
            pointer(&window, "move", x, y);
        }
        assert_eq!(
            ui.get_pin_value(),
            "123698",
            "Skipped dots must be inserted"
        );
        assert_eq!(submissions.get(), 0, "Pattern must wait for finger release");
        frame(&window, &clock, 5, &mut pixels, width);
        capture("pattern-entered", &pixels, width, height);
        let (x1, y1) = point(1);
        let (x2, _) = point(2);
        let connecting = pixels[y1.round() as usize * width + ((x1 + x2) / 2.).round() as usize];
        assert!(
            connecting.g > 180 && connecting.b > 180,
            "Pattern segment must join the selected dot centers"
        );
        let (x, y) = point(8);
        pointer(&window, "up", x, y);
        assert_eq!(submissions.get(), 1);
        let (x, y) = point(1);
        pointer(&window, "down", x, y);
        let (x, y) = point(3);
        pointer(&window, "move", x, y);
        pointer(&window, "up", x, y);
        assert_eq!(submissions.get(), 1, "Short patterns must not authenticate");
        assert_eq!(ui.get_pin_value(), "");
        ui.set_pin_ready(false);
        let (x, y) = point(1);
        pointer(&window, "down", x, y);
        let (x, y) = point(9);
        pointer(&window, "move", x, y);
        pointer(&window, "up", x, y);
        assert_eq!(ui.get_pin_value(), "");
        assert_eq!(submissions.get(), 1);
        ui.set_pin_ready(true);
        let (x, y) = point(1);
        pointer(&window, "down", x, y);
        for node in [9, 2, 3, 6] {
            let (x, y) = point(node);
            pointer(&window, "move", x, y);
        }
        assert_eq!(ui.get_pin_value(), "159236");
        frame(&window, &clock, 10, &mut pixels, width);
        capture("pattern-diagonal", &pixels, width, height);
        let (x1, y1) = point(1);
        let (x5, y5) = point(5);
        let line =
            pixels[((y1 + y5) / 2.).round() as usize * width + ((x1 + x5) / 2.).round() as usize];
        assert!(
            line.g > 180 && line.b > 180,
            "Diagonal segment must join the selected dots"
        );
        let (x, y) = point(6);
        pointer(&window, "up", x, y);
        assert_eq!(submissions.get(), 2);
        println!(
            "{width}x{height}: six-dot gesture, automatic skipped dots, release-only submission and disabled input passed"
        );
        return;
    }
    if args.get(2).is_some_and(|arg| arg == "--pin-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_page(3);
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
                .set_pin_value(digits.borrow().clone().into());
        });
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pin_clear(move || {
            digits.borrow_mut().clear();
            weak.unwrap().set_pin_value("".into());
        });
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pin_backspace(move || {
            digits.borrow_mut().pop();
            weak.unwrap().set_pin_value(digits.borrow().clone().into());
        });
        let submissions = Rc::new(Cell::new(0));
        let submitted = submissions.clone();
        let digits = entered.clone();
        let weak = ui.as_weak();
        ui.on_pin_submit(move || {
            submitted.set(submitted.get() + 1);
            let ui = weak.unwrap();
            if *digits.borrow() == "123456" {
                ui.set_pin_open(false);
                ui.set_screen_locked(false);
            } else {
                digits.borrow_mut().clear();
                ui.set_pin_value("".into());
                ui.set_pin_message("Incorrect PIN".into());
            }
        });
        frame(&window, &clock, 0, &mut pixels, width);
        capture("pin-locked", &pixels, width, height);
        // The full-screen PIN overlay must swallow taps in its corners too.
        click(&window, width as f32 - 2., 2.);
        click(&window, 2., height as f32 - 2.);
        assert_eq!(ui.get_page(), 3);
        // Exercise the actual keypad in both layouts, including its disabled state.
        ui.set_pin_ready(false);
        click_control(&window, &ui, &clock, &mut pixels, "1");
        assert!(entered.borrow().is_empty());
        ui.set_pin_ready(true);
        assert!(
            i_slint_backend_testing::ElementHandle::find_by_accessible_label(&ui, "OK")
                .next()
                .is_none()
        );
        let readout = element(&ui, "PinPad::entered-pin");
        let readout_position = readout.absolute_position();
        let readout_size = readout.size();
        let first_key = control(&ui, "1").absolute_position();
        if width < height {
            assert!(
                readout_position.y + readout_size.height <= first_key.y,
                "PIN readout overlaps keypad"
            );
        } else {
            assert!(
                readout_position.x + readout_size.width <= first_key.x,
                "PIN readout overlaps keypad"
            );
        }
        let key_size = control(&ui, "1").size();
        assert!(
            key_size.height >= 55.,
            "Keypad should use the available height: {key_size:?}"
        );
        click_control(&window, &ui, &clock, &mut pixels, "0");
        assert_eq!(ui.get_pin_value(), "0");
        click_control(&window, &ui, &clock, &mut pixels, "Backspace");
        assert_eq!(ui.get_pin_value(), "");
        for digit in [0, 9, 8] {
            click_control(&window, &ui, &clock, &mut pixels, &digit.to_string());
        }
        assert_eq!(ui.get_pin_value(), "098");
        click_control(&window, &ui, &clock, &mut pixels, "Clear PIN");
        assert_eq!(ui.get_pin_value(), "");
        assert!(entered.borrow().is_empty());
        assert_eq!(submissions.get(), 0, "Clear must not submit");
        for digit in [6, 5, 4, 3, 2, 1] {
            click_control(&window, &ui, &clock, &mut pixels, &digit.to_string());
        }
        assert_eq!(submissions.get(), 1);
        assert!(ui.get_pin_open() && ui.get_screen_locked());
        assert!(entered.borrow().is_empty());
        assert_eq!(ui.get_pin_value(), "");
        for digit in 1..=6 {
            click_control(&window, &ui, &clock, &mut pixels, &digit.to_string());
            assert_eq!(submissions.get(), if digit == 6 { 2 } else { 1 });
            assert_eq!(ui.get_pin_value().as_str(), &"123456"[..digit as usize]);
        }
        assert_eq!(&*entered.borrow(), "123456");
        assert!(!ui.get_pin_open() && !ui.get_screen_locked());
        ui.set_pin_open(true);
        frame(&window, &clock, 10, &mut pixels, width);
        ui.set_screen_asleep(true);
        frame(&window, &clock, 20, &mut pixels, width);
        assert!(
            pixels
                .iter()
                .all(|pixel| pixel.r == 0 && pixel.g == 0 && pixel.b == 0)
        );
        click(&window, width as f32 / 2., height as f32 - 56.);
        assert_eq!(ui.get_page(), 3);
        assert_eq!(submissions.get(), 2);
        println!(
            "{width}x{height}: visible PIN digits, automatic submit, wrong-code retry, backspace and sleep blocking passed"
        );
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
            click_control(&window, &ui, &clock, &mut pixels, "Orientation");
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
    // Long connection states stay clipped before the cog and preserve its hit area.
    ui.set_animations_enabled(false);
    let connection = ui.get_connection();
    let was_connected = ui.get_connected();
    ui.set_connected(false);
    ui.set_connection("Disconnected".into());
    ui.set_fresh(false);
    frame(&window, &clock, 0, &mut pixels, width);
    let status_view = element(&ui, "Header::status-text");
    let dot_x = status_view.absolute_position().x.round() as usize - 15;
    let dot_y =
        (status_view.absolute_position().y + status_view.size().height / 2.).round() as usize;
    let disconnected_dot = pixels[dot_y * width + dot_x];
    assert!(
        disconnected_dot.r > disconnected_dot.g.saturating_add(60),
        "Disconnected indicator is not at the header start"
    );
    let cog_control = control(&ui, "Settings");
    let cog_pixels = |pixels: &[Rgb8Pixel]| element_pixels(&cog_control, pixels, width);
    let cog = cog_pixels(&pixels);
    ui.set_connection(
        "Connected | receiving current telemetry through encrypted Bluetooth and a verified controller package".into(),
    );
    ui.set_connected(true);
    ui.set_fresh(true);
    frame(&window, &clock, 0, &mut pixels, width);
    let live_dot = pixels[dot_y * width + dot_x];
    assert!(
        (live_dot.r, live_dot.g, live_dot.b) == (96, 211, 148),
        "Live indicator is not at the header start"
    );
    assert_eq!(
        cog_pixels(&pixels),
        cog,
        "Connection text overlapped Settings cog"
    );
    capture("header-long-status", &pixels, width, height);
    click_control(&window, &ui, &clock, &mut pixels, "Settings");
    assert_eq!(ui.get_page(), 3, "Long status blocked the Settings control");
    ui.set_page(0);
    ui.set_connection(connection);
    ui.set_connected(was_connected);
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
            &start[footer_start * width..],
            &pixels[footer_start * width..],
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
                &before[footer_start * width..],
                &pixels[footer_start * width..],
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
    swipe(5., -100., 1); // Vertical movement stays on the scrollable Stats page.
    swipe(-100., 0., 2);
    swipe(0., -80., 2); // Tune still scrolls without applying a change.
    swipe(-100., 0., 2); // The rightmost Tune tab is bounded.
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
    ui.set_page(2);
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
