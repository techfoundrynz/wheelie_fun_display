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
thread_local! { static PIXEL_WORK: Cell<u64> = const { Cell::new(0) }; }
slint::include_modules!();
#[path = "../stats_fixture.rs"]
mod stats_fixture;
#[path = "../tuning_fixture.rs"]
mod tuning_fixture;

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
        PIXEL_WORK.set(PIXEL_WORK.get() + range.len() as u64);
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
                matches!(
                    control.accessible_role(),
                    Some(
                        i_slint_backend_testing::AccessibleRole::Button
                            | i_slint_backend_testing::AccessibleRole::Slider
                    )
                )
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
        || ui.get_tuning_editor_open()
        || ui.get_ota_confirming()
        || ui.get_update_modal_open();
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
                matches!(
                    item.accessible_role(),
                    Some(
                        i_slint_backend_testing::AccessibleRole::Button
                            | i_slint_backend_testing::AccessibleRole::Slider
                    )
                )
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
    tuning_fixture::configure(&ui);
    ui.set_fresh(true);
    ui.set_speed("24.8".into());
    ui.set_speed_progress(0.44);
    ui.set_initial_width(width as f32);
    ui.set_initial_height(height as f32);
    window.set_size(PhysicalSize::new(width as u32, height as u32));
    ui.show().unwrap();
    let mut pixels = vec![Rgb8Pixel::default(); width * height];
    if args.iter().any(|arg| arg == "--gesture-snapshot-fixtures") {
        if !ui.get_native_page_snapshots() {
            return;
        }
        ui.set_animations_enabled(false);
        ui.set_page(1);
        frame(&window, &clock, 0, &mut pixels, width);
        ui.set_animations_enabled(true);
        frame(&window, &clock, 10, &mut pixels, width);
        let x = width as f32 * 0.5;
        let y = ui.get_native_page_y() + ui.get_native_page_height() * 0.5;
        pointer(&window, "down", x, y);
        frame(&window, &clock, 20, &mut pixels, width);
        pointer(&window, "move", x - width as f32 * 0.25, y);
        frame(&window, &clock, 60, &mut pixels, width);
        assert!(ui.get_native_slide_active());
        assert_eq!(ui.get_native_slide_incoming(), 2);
        assert_eq!(ui.get_native_slide_outgoing(), 1);
        assert!((ui.get_native_slide_progress() - 0.25).abs() < 0.01);
        ui.set_native_capture_page(2);
        frame(&window, &clock, 60, &mut pixels, width);
        // Reversing across the origin selects a different neighbor but must
        // retain the same outgoing page image throughout the contact.
        pointer(&window, "move", x + width as f32 * 0.25, y);
        frame(&window, &clock, 100, &mut pixels, width);
        assert_eq!(ui.get_native_slide_incoming(), 0);
        assert_eq!(ui.get_native_slide_outgoing(), 1);
        assert_eq!(ui.get_native_slide_direction(), -1);
        ui.set_native_capture_page(0);
        frame(&window, &clock, 100, &mut pixels, width);
        pointer(&window, "up", x + width as f32 * 0.25, y);
        window.dispatch_event(WindowEvent::PointerExited);
        frame(&window, &clock, 200, &mut pixels, width);
        assert!(ui.get_native_slide_active());
        assert!(ui.get_native_slide_progress() > 0.25);
        frame(&window, &clock, 400, &mut pixels, width);
        assert!(!ui.get_native_slide_active());
        assert_eq!(ui.get_displayed_page(), 0);
        // A reversal that remains on the same side cancels the gesture. The
        // settled page is the outgoing snapshot, rather than the neighbor.
        ui.set_native_capture_page(-1);
        pointer(&window, "down", x, y);
        frame(&window, &clock, 500, &mut pixels, width);
        pointer(&window, "move", x - width as f32 * 0.25, y);
        frame(&window, &clock, 530, &mut pixels, width);
        assert_eq!(ui.get_native_slide_incoming(), 1);
        assert_eq!(ui.get_native_slide_outgoing(), 0);
        ui.set_native_capture_page(1);
        frame(&window, &clock, 530, &mut pixels, width);
        pointer(&window, "move", x - width as f32 * 0.1, y);
        frame(&window, &clock, 560, &mut pixels, width);
        pointer(&window, "up", x - width as f32 * 0.1, y);
        window.dispatch_event(WindowEvent::PointerExited);
        frame(&window, &clock, 700, &mut pixels, width);
        assert!(!ui.get_native_slide_active());
        assert_eq!(ui.get_displayed_page(), 0);
        ui.set_native_capture_page(0);
        frame(&window, &clock, 700, &mut pixels, width);
        let incremental = pixels.clone();
        window.request_redraw();
        window.draw_if_needed(|renderer| {
            renderer.set_repaint_buffer_type(RepaintBufferType::NewBuffer);
            renderer.render(&mut pixels, width);
        });
        assert!(
            incremental == pixels,
            "cancelled swipe retained stale pixels"
        );
        return;
    }
    if args.iter().any(|arg| arg == "--momentum-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_connected(true);
        stats_fixture::project(&ui, true);
        ui.set_page(1);
        frame(&window, &clock, 0, &mut pixels, width);
        let content = element(&ui, "StatsPage::stats-content");
        let initial_top = content.absolute_position().y;
        let deck = element(&ui, "Dashboard::pages");
        let x = deck.absolute_position().x + deck.size().width * 0.5;
        let y = deck.absolute_position().y + deck.size().height * 0.75;
        let mut fling = |at: u64| {
            frame(&window, &clock, at, &mut pixels, width);
            pointer(&window, "down", x, y);
            frame(&window, &clock, at + 10, &mut pixels, width);
            for (dt, dy) in [(30, 25.), (50, 65.), (70, 105.)] {
                pointer(&window, "move", x, y - dy);
                frame(&window, &clock, at + dt, &mut pixels, width);
            }
            pointer(&window, "up", x, y - 105.);
            window.dispatch_event(WindowEvent::PointerExited);
            frame(&window, &clock, at + 80, &mut pixels, width);
        };
        fling(0);
        let released = content.absolute_position().y;
        frame(&window, &clock, 200, &mut pixels, width);
        let coasted = content.absolute_position().y;
        assert!(
            coasted < released - 5.,
            "Stats lost release momentum: {released} -> {coasted}"
        );
        let before_update = pixels.clone();
        ui.set_speed("32.4".into());
        ui.set_power("875 W".into());
        ui.set_battery_current("18.5 A".into());
        ui.set_motor_current("29.7 A".into());
        ui.set_mosfet("47 C".into());
        ui.set_motor("52 C".into());
        for model in [ui.get_vesc_stats(), ui.get_bms_stats()] {
            for index in 0..model.row_count() {
                let mut item = model.row_data(index).unwrap();
                item.value = "Updated".into();
                model.set_row_data(index, item);
            }
        }
        frame(&window, &clock, 200, &mut pixels, width);
        assert!(before_update != pixels, "Stats froze during momentum");
        assert!(
            (content.absolute_position().y - coasted).abs() < 0.5,
            "Live values reset the scroll position"
        );
        frame(&window, &clock, 250, &mut pixels, width);
        assert!(
            content.absolute_position().y < coasted - 1.,
            "Live values stopped the coast"
        );
        frame(&window, &clock, 1800, &mut pixels, width);
        let stopped = content.absolute_position().y;
        frame(&window, &clock, 2100, &mut pixels, width);
        assert!(
            (content.absolute_position().y - stopped).abs() < 0.5,
            "Momentum did not settle"
        );
        // An identical subsequent touch location must still stop the coast.
        pointer(&window, "down", x, y);
        frame(&window, &clock, 2120, &mut pixels, width);
        pointer(&window, "move", x, y - 20.);
        frame(&window, &clock, 2140, &mut pixels, width);
        pointer(&window, "move", x, y - 60.);
        frame(&window, &clock, 2160, &mut pixels, width);
        pointer(&window, "up", x, y - 60.);
        window.dispatch_event(WindowEvent::PointerExited);
        frame(&window, &clock, 2180, &mut pixels, width);
        pointer(&window, "down", x, y);
        frame(&window, &clock, 2200, &mut pixels, width);
        let touched = content.absolute_position().y;
        frame(&window, &clock, 2350, &mut pixels, width);
        assert!(
            (content.absolute_position().y - touched).abs() < 0.5,
            "A new contact failed to stop momentum"
        );
        pointer(&window, "up", x, y);
        window.dispatch_event(WindowEvent::PointerExited);
        frame(&window, &clock, 2400, &mut pixels, width);
        // Cancellation never turns the last movement into a fling.
        pointer(&window, "down", x, y);
        frame(&window, &clock, 2410, &mut pixels, width);
        pointer(&window, "move", x, y - 40.);
        frame(&window, &clock, 2430, &mut pixels, width);
        window.dispatch_event(WindowEvent::PointerExited);
        frame(&window, &clock, 2450, &mut pixels, width);
        let cancelled = content.absolute_position().y;
        frame(&window, &clock, 2700, &mut pixels, width);
        assert!(
            (content.absolute_position().y - cancelled).abs() < 0.5,
            "Cancelled drag started momentum"
        );
        pointer(&window, "down", x, y);
        frame(&window, &clock, 2720, &mut pixels, width);
        pointer(&window, "move", x, y + 40.);
        frame(&window, &clock, 2740, &mut pixels, width);
        pointer(&window, "move", x, y + 80.);
        frame(&window, &clock, 2760, &mut pixels, width);
        frame(&window, &clock, 3000, &mut pixels, width);
        pointer(&window, "up", x, y + 80.);
        window.dispatch_event(WindowEvent::PointerExited);
        frame(&window, &clock, 3020, &mut pixels, width);
        let paused = content.absolute_position().y;
        frame(&window, &clock, 3300, &mut pixels, width);
        assert!(
            (content.absolute_position().y - paused).abs() < 0.5,
            "Paused release reused stale drag velocity"
        );
        // Profiling must position using contacts inside the viewport; an
        // off-window jump may cancel before applying its requested offset.
        let reset_start = deck.absolute_position().y + deck.size().height * 0.2;
        let reset_end = deck.absolute_position().y + deck.size().height * 0.8;
        for index in 0..12 {
            let at = 3400 + index * 60;
            pointer(&window, "down", x, reset_end);
            frame(&window, &clock, at, &mut pixels, width);
            pointer(&window, "move", x, reset_start);
            frame(&window, &clock, at + 20, &mut pixels, width);
            window.dispatch_event(WindowEvent::PointerExited);
            pointer(&window, "up", -1., -1.);
            frame(&window, &clock, at + 40, &mut pixels, width);
        }
        assert!(
            content.absolute_position().y < initial_top - 20.,
            "In-bounds setup did not scroll away from the top"
        );
        for index in 0..64 {
            let at = 4200 + index * 60;
            pointer(&window, "down", x, reset_start);
            frame(&window, &clock, at, &mut pixels, width);
            pointer(&window, "move", x, reset_end);
            frame(&window, &clock, at + 20, &mut pixels, width);
            window.dispatch_event(WindowEvent::PointerExited);
            pointer(&window, "up", -1., -1.);
            frame(&window, &clock, at + 40, &mut pixels, width);
        }
        assert!(
            (content.absolute_position().y - initial_top).abs() < 0.5,
            "In-bounds drags failed to restore the top scroll position"
        );
        frame(&window, &clock, 8400, &mut pixels, width);
        assert!(
            (content.absolute_position().y - initial_top).abs() < 0.5,
            "Momentum escaped the top scroll bound"
        );
        assert_eq!(ui.get_page(), 1, "Vertical flick changed tabs");
        return;
    }
    if args.iter().any(|arg| arg == "--page-preparation-fixtures") {
        if !ui.get_native_page_snapshots() {
            return;
        }
        ui.set_animations_enabled(false);
        frame(&window, &clock, 0, &mut pixels, width);
        let original = pixels.clone();
        for page in 0..7 {
            ui.set_native_capture_page(page);
            frame(&window, &clock, 10 + page as u64, &mut pixels, width);
            assert_eq!(ui.get_page(), 0, "preparation must not navigate");
        }
        ui.set_native_capture_page(-1);
        frame(&window, &clock, 30, &mut pixels, width);
        assert!(pixels == original, "preparation changed the visible page");
        // Hidden retained pages must not intercept pointer input.
        click_control(&window, &ui, &clock, &mut pixels, "Stats");
        frame(&window, &clock, 400, &mut pixels, width);
        assert_eq!(ui.get_page(), 1);
        return;
    }
    if args
        .iter()
        .any(|arg| arg == "--double-buffer-snapshot-fixtures")
    {
        if !ui.get_native_page_snapshots() {
            return;
        }
        let mut buffers = [pixels.clone(), pixels.clone()];
        ui.set_animations_enabled(false);
        frame(&window, &clock, 0, &mut buffers[0], width);
        buffers[1] = buffers[0].clone();
        frame(&window, &clock, 16, &mut buffers[1], width);
        let x = ui.get_native_page_x().round() as usize;
        let y = ui.get_native_page_y().round() as usize;
        let w = ui.get_native_page_width().round() as usize;
        let h = ui.get_native_page_height().round() as usize;
        let copy_viewport = |source: &[Rgb8Pixel], destination: &mut [Rgb8Pixel]| {
            for row in y..y + h {
                let range = row * width + x..row * width + x + w;
                destination[range.clone()].copy_from_slice(&source[range]);
            }
        };
        ui.set_animations_enabled(true);
        ui.set_page(1);
        ui.set_native_capture_page(1);
        // Initial capture clears the entire viewport, including static gutters.
        for row in y..y + h {
            buffers[0][row * width + x..row * width + x + w].fill(Rgb8Pixel::default());
        }
        frame(&window, &clock, 32, &mut buffers[0], width);
        let incoming = buffers[0].clone();
        // The native backend repairs unchanged chrome from the latest front.
        buffers[1].copy_from_slice(&incoming);
        frame(&window, &clock, 48, &mut buffers[1], width);
        // Both scanout buffers now contain translated pixels unknown to Slint.
        for buffer in &mut buffers {
            for row in y..y + h {
                buffer[row * width + x..row * width + x + w].fill(Rgb8Pixel {
                    r: 255,
                    g: 0,
                    b: 255,
                });
            }
        }
        ui.set_native_capture_page(-1);
        copy_viewport(&incoming, &mut buffers[0]);
        frame(&window, &clock, 400, &mut buffers[0], width);
        // The second retired buffer needs restoration too, before a later scroll.
        copy_viewport(&incoming, &mut buffers[1]);
        frame(&window, &clock, 416, &mut buffers[1], width);
        window.request_redraw();
        window.draw_if_needed(|renderer| {
            renderer.set_repaint_buffer_type(RepaintBufferType::NewBuffer);
            renderer.render(&mut pixels, width);
        });
        for (index, buffer) in buffers.iter().enumerate() {
            let differences = buffer.iter().zip(&pixels).filter(|(a, b)| a != b).count();
            assert_eq!(
                differences, 0,
                "scanout buffer {index} retained stale slide pixels"
            );
        }
        return;
    }
    if args.iter().any(|arg| arg == "--fullwidth-slide-fixtures") {
        ui.set_animations_enabled(false);
        frame(&window, &clock, 0, &mut pixels, width);
        let viewport = element(&ui, "PageDeck::viewport");
        assert_eq!(viewport.absolute_position().x, 0.);
        assert_eq!(viewport.size().width, width as f32);
        let left = element(&ui, "SafeArea::content")
            .absolute_position()
            .x
            .round() as usize;
        let top = viewport.absolute_position().y.round() as usize;
        let bottom = top + viewport.size().height.round() as usize;
        ui.set_animations_enabled(true);
        frame(&window, &clock, 10, &mut pixels, width);
        ui.set_page(1);
        frame(&window, &clock, 20, &mut pixels, width);
        frame(&window, &clock, 120, &mut pixels, width);
        assert!(
            (top..bottom).any(|y| pixels[y * width..y * width + left]
                .iter()
                .any(|p| p.r != 0 || p.g != 0 || p.b != 0)),
            "moving page content must reach the side margin instead of clipping at the safe area"
        );
        capture("fullwidth-page-slide", &pixels, width, height);
        return;
    }
    if args.iter().any(|arg| arg == "--live-snapshot-fixtures") {
        if !ui.get_native_page_snapshots() {
            return;
        }
        ui.set_connected(true);
        stats_fixture::project(&ui, false);
        ui.set_animations_enabled(false);
        frame(&window, &clock, 0, &mut pixels, width);
        ui.set_animations_enabled(true);
        frame(&window, &clock, 10, &mut pixels, width);
        ui.set_page(1);
        frame(&window, &clock, 20, &mut pixels, width);
        ui.set_native_capture_page(1);
        frame(&window, &clock, 20, &mut pixels, width);
        let before = pixels.clone();
        stats_fixture::project(&ui, true);
        ui.set_speed("32.4".into());
        ui.set_battery_current("18.5 A".into());
        frame(&window, &clock, 100, &mut pixels, width);
        assert!(
            before != pixels,
            "incoming snapshot must retain live telemetry"
        );
        let incoming = pixels.clone();
        let x = ui.get_native_page_x().round() as usize;
        let y = ui.get_native_page_y().round() as usize;
        let w = ui.get_native_page_width().round() as usize;
        let h = ui.get_native_page_height().round() as usize;
        // A composed framebuffer no longer matches Slint's stationary scene.
        // Restore its snapshot before normal incremental rendering resumes.
        for row in y..y + h {
            pixels[row * width + x..row * width + x + w].fill(Rgb8Pixel {
                r: 255,
                g: 0,
                b: 255,
            });
        }
        for row in y..y + h {
            pixels[row * width + x..row * width + x + w]
                .copy_from_slice(&incoming[row * width + x..row * width + x + w]);
        }
        ui.set_native_capture_page(-1);
        frame(&window, &clock, 400, &mut pixels, width);
        let incremental = pixels.clone();
        capture("live-handoff-incremental", &pixels, width, height);
        window.request_redraw();
        window.draw_if_needed(|renderer| {
            renderer.set_repaint_buffer_type(RepaintBufferType::NewBuffer);
            renderer.render(&mut pixels, width);
        });
        capture("live-handoff-oracle", &pixels, width, height);
        let differences: Vec<_> = incremental
            .iter()
            .zip(&pixels)
            .enumerate()
            .filter(|(_, (a, b))| a != b)
            .map(|(i, _)| (i % width, i / width))
            .collect();
        assert!(
            differences.is_empty(),
            "snapshot handoff differs at {} pixels, first {:?}",
            differences.len(),
            differences.first()
        );
        return;
    }
    if args.iter().any(|arg| arg == "--snapshot-fixtures") {
        if !ui.get_native_page_snapshots() {
            assert_eq!(ui.get_native_page_width(), 0.);
            assert_eq!(ui.get_native_page_height(), 0.);
            return;
        }
        ui.set_animations_enabled(false);
        frame(&window, &clock, 0, &mut pixels, width);
        let x = ui.get_native_page_x().round() as usize;
        let y = ui.get_native_page_y().round() as usize;
        let w = ui.get_native_page_width().round() as usize;
        let h = ui.get_native_page_height().round() as usize;
        assert!(w > 0 && h > 0 && x + w <= width && y + h <= height);
        let content = |pixels: &[Rgb8Pixel]| -> Vec<Rgb8Pixel> {
            (y..y + h)
                .flat_map(|row| pixels[row * width + x..row * width + x + w].iter().copied())
                .collect()
        };
        let ride = content(&pixels);
        ui.set_animations_enabled(true);
        frame(&window, &clock, 10, &mut pixels, width);
        ui.set_page(1);
        frame(&window, &clock, 20, &mut pixels, width);
        frame(&window, &clock, 120, &mut pixels, width);
        assert!(ui.get_native_slide_active());
        ui.set_native_capture_page(0);
        frame(&window, &clock, 120, &mut pixels, width);
        assert_eq!(
            content(&pixels),
            ride,
            "outgoing snapshot must be the stationary page"
        );
        ui.set_native_capture_page(1);
        frame(&window, &clock, 120, &mut pixels, width);
        let stats = content(&pixels);
        ui.set_native_capture_page(-2);
        frame(&window, &clock, 120, &mut pixels, width);
        let hidden = content(&pixels);
        assert!(
            hidden.iter().all(|p| *p == hidden[0]),
            "chrome capture must hide both pages"
        );
        // A pairing prompt can arrive asynchronously during the slide.
        ui.set_pairing_open(true);
        assert!(!ui.get_native_slide_active());
        ui.set_native_capture_page(-1);
        frame(&window, &clock, 120, &mut pixels, width);
        let pairing = pixels.clone();
        window.request_redraw();
        window.draw_if_needed(|renderer| {
            renderer.set_repaint_buffer_type(RepaintBufferType::NewBuffer);
            renderer.render(&mut pixels, width);
            renderer.set_repaint_buffer_type(RepaintBufferType::ReusedBuffer);
        });
        assert_eq!(
            pairing, pixels,
            "pairing during a slide must match a full repaint"
        );
        ui.set_pairing_open(false);
        ui.set_native_capture_page(-2);
        frame(&window, &clock, 120, &mut pixels, width);
        // Simulate DMA changing pixels outside the renderer's damage tracking.
        for row in y..y + h {
            pixels[row * width + x..row * width + x + w].fill(Rgb8Pixel {
                r: 255,
                g: 0,
                b: 255,
            });
        }
        ui.set_native_capture_page(-1);
        frame(&window, &clock, 400, &mut pixels, width);
        assert_eq!(
            content(&pixels),
            stats,
            "leaving capture mode must repaint the entire page"
        );
        capture("native-page-snapshot-restored", &pixels, width, height);
        ui.set_native_capture_page(-2);
        frame(&window, &clock, 500, &mut pixels, width);
        for row in y..y + h {
            pixels[row * width + x..row * width + x + w].fill(Rgb8Pixel {
                r: 255,
                g: 0,
                b: 255,
            });
        }
        ui.set_screen_locked(true);
        ui.set_pin_open(true);
        ui.set_native_capture_page(-1);
        frame(&window, &clock, 510, &mut pixels, width);
        let incremental = pixels.clone();
        window.request_redraw();
        window.draw_if_needed(|renderer| {
            renderer.set_repaint_buffer_type(RepaintBufferType::NewBuffer);
            renderer.render(&mut pixels, width);
        });
        assert_eq!(
            incremental, pixels,
            "locking during capture must match a full repaint, without stale page pixels"
        );
        return;
    }
    if args.iter().any(|arg| arg == "--keyboard-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_page(4);
        ui.set_wifi_networks(
            std::rc::Rc::new(slint::VecModel::from(vec![WifiItem {
                ssid: "Test network".into(),
                detail: "Secured".into(),
            }]))
            .into(),
        );
        let resets = Rc::new(Cell::new(0));
        let reset_count = resets.clone();
        let manual_count = resets.clone();
        ui.on_wifi_manual(move || manual_count.set(manual_count.get() + 1));
        let weak = ui.as_weak();
        ui.on_wifi_select(move |_| {
            reset_count.set(reset_count.get() + 1);
            weak.unwrap().set_editor_value("".into());
        });
        let weak = ui.as_weak();
        ui.on_editor_type(move |text| {
            let ui = weak.unwrap();
            let value = format!("{}{text}", ui.get_editor_value());
            ui.set_editor_masked("*".repeat(value.len()).into());
            ui.set_editor_value(value.into());
        });
        ui.set_editor_secret(true);
        ui.set_editor_heading("Wi-Fi password".into());
        frame(&window, &clock, 0, &mut pixels, width);
        click_control(&window, &ui, &clock, &mut pixels, "Test network / Secured");
        assert_eq!(resets.get(), 1);
        resets.set(0);
        ui.set_editing(true);
        frame(&window, &clock, 0, &mut pixels, width);
        let keys: Vec<_> =
            i_slint_backend_testing::ElementHandle::find_by_element_id(&ui, "Key::touch").collect();
        assert_eq!(keys.len(), 47);
        let tap_key = |index: usize| {
            let position = keys[index].absolute_position();
            let size = keys[index].size();
            click(
                &window,
                position.x + size.width / 2.,
                position.y + size.height / 2.,
            );
        };
        tap_key(45); // Show
        for index in 10..20 {
            if index == 15 {
                capture("keyboard-revealed", &pixels, width, height);
                tap_key(45); // Hide; changing visibility must preserve the draft.
            }
            tap_key(index);
            window.dispatch_event(WindowEvent::PointerExited);
            frame(
                &window,
                &clock,
                (index as u64 - 9) * 2000,
                &mut pixels,
                width,
            );
        }
        assert_eq!(ui.get_editor_value().as_str(), "qwertyuiop");
        capture("keyboard-hidden", &pixels, width, height);
        // Include key gaps, the value field and safe margins. None may reach
        // the network list underneath and reopen an empty password editor.
        let network = control(&ui, "Test network / Secured");
        let network_top = network.absolute_position().y as usize;
        let network_bottom = network_top + network.size().height as usize;
        for y in (network_top..network_bottom)
            .step_by(2)
            .chain((2..height).step_by(5))
        {
            for x in (2..width).step_by(5) {
                ui.set_editor_value("test".into());
                click(&window, x as f32, y as f32);
                window.dispatch_event(WindowEvent::PointerExited);
                assert_eq!(resets.get(), 0, "Keyboard leaked a tap at {x},{y}");
                assert_eq!(ui.get_page(), 4, "Keyboard leaked navigation at {x},{y}");
            }
        }
        frame(&window, &clock, 21000, &mut pixels, width);
        capture("keyboard", &pixels, width, height);
        println!("{width}x{height}: keyboard typing and modal touch isolation passed");
        return;
    }
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
    if args.iter().any(|arg| arg == "--battery-fixtures") {
        ui.set_animations_enabled(false);
        ui.set_connected(true);
        ui.set_battery("407%".into());
        ui.set_battery_level(407.);
        ui.set_voltage("81.7 V".into());
        frame(&window, &clock, 0, &mut pixels, width);
        capture("battery-reported-ride", &pixels, width, height);
        ui.set_page(1);
        frame(&window, &clock, 100, &mut pixels, width);
        capture("battery-reported-stats", &pixels, width, height);
        println!("{width}x{height}: reported battery percentage rendered on Ride and Stats");
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
                let area = element(&ui, "PortraitRide::dial-area");
                let distance = element(&ui, "PortraitRide::portrait-distance");
                let face = element(&ui, "Speedometer::dial-face");
                let dial = element(&ui, "PortraitRide::portrait-dial");
                let box_bounds = element(&ui, "PortraitRide::portrait-speedo-box");
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
            if width >= 700 && width > height && name == "ride-live" {
                let face = element(&ui, "Speedometer::dial-face");
                let dial = element(&ui, "WideRide::dial-area");
                let battery = element(&ui, "WideRide::battery");
                let telemetry = element(&ui, "WideRide::telemetry");
                let distance = element(&ui, "WideRide::distance");
                let deck = element(&ui, "Dashboard::pages");
                assert!(
                    face.size().width >= 280.,
                    "Large screen retained a small dial"
                );
                assert!(
                    distance.absolute_position().x
                        >= dial.absolute_position().x + dial.size().width + 27.,
                    "Wide distance readings overlap the speed dial"
                );
                assert!(
                    (battery.absolute_position().x - deck.absolute_position().x).abs() < 1.
                        && (battery.size().width - deck.size().width).abs() < 1.,
                    "Wide battery must span the content width"
                );
                assert!(
                    distance.absolute_position().y + distance.size().height
                        <= telemetry.absolute_position().y + telemetry.size().height - 180.,
                    "Wide distance readings must stay above the stat cards"
                );
                assert!(
                    dial.absolute_position().y
                        >= battery.absolute_position().y + battery.size().height + 15.,
                    "Wide battery overlaps the dial and telemetry area"
                );
                assert!(
                    telemetry.absolute_position().y + telemetry.size().height
                        <= deck.absolute_position().y + deck.size().height + 1.,
                    "Wide telemetry cards escape the page viewport"
                );
            }
            if width > height && width < 700 && name == "ride-live" {
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
        if width >= 700 {
            let captions: Vec<_> = ["SPEED", "POWER", "BATTERY"]
                .iter()
                .map(|label| {
                    i_slint_backend_testing::ElementHandle::find_by_accessible_label(&ui, label)
                        .next()
                        .expect("Wide overview caption missing")
                })
                .collect();
            for pair in captions.windows(2) {
                let first = pair[0].absolute_position();
                let second = pair[1].absolute_position();
                assert!(
                    (first.y - second.y).abs() < 1.,
                    "Wide Stats is not three columns"
                );
                assert!(second.x > first.x + 150., "Wide Stats columns overlap");
            }
        }
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
    if args
        .get(2)
        .is_some_and(|arg| arg == "--wfb-tuning-fixtures")
    {
        ui.set_animations_enabled(false);
        ui.set_connected(true);
        ui.set_fresh(true);
        ui.set_connection("Connected".into());
        ui.set_can_configure(true);
        ui.set_page(2);
        frame(&window, &clock, 0, &mut pixels, width);
        click_control(&window, &ui, &clock, &mut pixels, "Button");
        click_control(&window, &ui, &clock, &mut pixels, "Increase Wheelie angle");
        let angle = || {
            (0..ui.get_tuning_fields().row_count())
                .filter_map(|i| ui.get_tuning_fields().row_data(i))
                .find(|item| item.label == "Wheelie angle")
                .expect("Wheelie angle field missing")
        };
        assert_eq!(angle().value, "30.5°");
        assert_eq!(angle().saved, "30.0°", "Draft overwrote readback");
        assert!(ui.get_tuning_dirty());
        click_control(&window, &ui, &clock, &mut pixels, "Throttle");
        click_control(
            &window,
            &ui,
            &clock,
            &mut pixels,
            "Disable Anti-loop throttle",
        );
        assert!(
            (0..ui.get_tuning_fields().row_count())
                .filter_map(|i| ui.get_tuning_fields().row_data(i))
                .any(|item| item.label == "Same angle as button" && item.read_only)
        );
        click_control(
            &window,
            &ui,
            &clock,
            &mut pixels,
            "Enable Anti-loop throttle",
        );
        click_control(
            &window,
            &ui,
            &clock,
            &mut pixels,
            "Disable Same angle as button",
        );
        assert!(
            (0..ui.get_tuning_fields().row_count())
                .filter_map(|i| ui.get_tuning_fields().row_data(i))
                .any(|item| item.label == "Anti-loop angle")
        );
        capture("wfb-wheelie-draft", &pixels, width, height);
        ui.set_can_configure(false);
        click_control(&window, &ui, &clock, &mut pixels, "Save tuning changes");
        assert!(
            !ui.get_confirming(),
            "Moving bike opened a write confirmation"
        );
        ui.set_can_configure(true);
        frame(&window, &clock, SCROLL_TIME.get() + 5, &mut pixels, width);
        click_control(&window, &ui, &clock, &mut pixels, "Save tuning changes");
        frame(&window, &clock, SCROLL_TIME.get() + 10, &mut pixels, width);
        assert!(ui.get_confirming());
        assert!(ui.get_confirm_text().contains("Wheelie angle"));
        let panel = element(&ui, "ModalFrame::panel");
        let position = panel.absolute_position();
        let fill = pixels[(position.y as usize + 20) * width + position.x as usize + 6];
        assert_eq!(
            fill,
            Rgb8Pixel {
                r: 29,
                g: 29,
                b: 29
            },
            "Confirmation panel must have an opaque theme surface fill"
        );
        let settings = control_center(&ui, "Settings");
        click(&window, settings.0, settings.1);
        frame(&window, &clock, SCROLL_TIME.get() + 15, &mut pixels, width);
        assert_eq!(
            ui.get_page(),
            2,
            "Confirmation allowed a background Settings tap"
        );
        assert!(ui.get_confirming());
        capture("wfb-save-confirmation", &pixels, width, height);
        click_control(&window, &ui, &clock, &mut pixels, "Cancel");
        assert!(!ui.get_confirming());
        assert!(ui.get_tuning_dirty());
        click_control(&window, &ui, &clock, &mut pixels, "Save tuning changes");
        click_control(&window, &ui, &clock, &mut pixels, "Apply now");
        assert!(!ui.get_tuning_dirty());
        click_control(&window, &ui, &clock, &mut pixels, "Button");
        assert_eq!(angle().saved, "30.5°");
        click_control(&window, &ui, &clock, &mut pixels, "Adjust Wheelie angle");
        frame(&window, &clock, SCROLL_TIME.get() + 20, &mut pixels, width);
        assert!(ui.get_tuning_editor_open());
        let panel = element(&ui, "ModalFrame::panel");
        let panel_position = panel.absolute_position();
        let panel_size = panel.size();
        assert!(
            panel_position.x >= 24.
                && panel_position.y >= 24.
                && panel_position.x + panel_size.width <= width as f32 - 24.
                && panel_position.y + panel_size.height <= height as f32 - 24.,
            "Tuning panel exceeds its safe margins"
        );
        capture("wfb-tuning-editor", &pixels, width, height);
        let slider = control(&ui, "Wheelie angle");
        let p = slider.absolute_position();
        let size = slider.size();
        assert!(
            size.width >= 180. && size.height >= 48.,
            "Tuning editor compressed the slider's touch target"
        );
        for (offset, value) in [5.0, 30.0, 55.0].into_iter().enumerate() {
            ui.invoke_tuning_set(5, value);
            frame(
                &window,
                &clock,
                SCROLL_TIME.get() + 21 + offset as u64,
                &mut pixels,
                width,
            );
            let rail = element(&ui, "TuningSlider::rail");
            let fill = element(&ui, "TuningSlider::fill");
            let thumb = element(&ui, "TuningSlider::thumb");
            let expected = (value - 5.0) / 50.0 * rail.size().width;
            assert!(
                (fill.absolute_position().x - rail.absolute_position().x).abs() < 0.1,
                "Slider fill must begin at the start of the rail"
            );
            assert!(
                (fill.size().width - expected).abs() < 0.1,
                "Slider fill does not represent its displayed value"
            );
            assert!(
                (thumb.absolute_position().x + thumb.size().width / 2.0
                    - rail.absolute_position().x
                    - expected)
                    .abs()
                    < 0.1,
                "Slider thumb and fill disagree"
            );
        }
        ui.invoke_tuning_set(5, 30.5);
        frame(&window, &clock, SCROLL_TIME.get() + 24, &mut pixels, width);
        let x = p.x + size.width * 0.35;
        let y = p.y + size.height / 2.;
        pointer(&window, "down", x, y);
        pointer(&window, "move", p.x + size.width * 0.7, y);
        frame(&window, &clock, SCROLL_TIME.get() + 25, &mut pixels, width);
        pointer(&window, "up", p.x + size.width * 0.7, y);
        frame(&window, &clock, SCROLL_TIME.get() + 50, &mut pixels, width);
        assert_eq!(ui.get_page(), 2, "Slider drag navigated away from Tune");
        assert!(angle().position > 35., "Slider did not follow the finger");
        assert_eq!(angle().saved, "30.5°", "Slider overwrote saved readback");
        let cog = control_center(&ui, "Settings");
        click(&window, cog.0, cog.1);
        assert_eq!(ui.get_page(), 2, "Slider editor allowed a background tap");
        click_control(&window, &ui, &clock, &mut pixels, "Done");
        assert!(!ui.get_tuning_editor_open());
        ui.set_ota_confirming(true);
        frame(&window, &clock, SCROLL_TIME.get() + 80, &mut pixels, width);
        let update_panel = element(&ui, "ModalFrame::panel");
        let p = update_panel.absolute_position();
        let size = update_panel.size();
        assert!(
            p.x >= 24.
                && p.y >= 24.
                && p.x + size.width <= width as f32 - 24.
                && p.y + size.height <= height as f32 - 24.,
            "Update confirmation exceeds safe margins"
        );
        capture("update-confirmation-panel", &pixels, width, height);
        click_control(&window, &ui, &clock, &mut pixels, "Cancel");
        assert!(!ui.get_ota_confirming());
        println!(
            "{width}x{height}: WFB draft/readback, conditional controls, guards, opaque modal and save fixtures passed"
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
        click_control(&window, &ui, &clock, &mut pixels, "Eco");
        click_control(&window, &ui, &clock, &mut pixels, "Apply ride response");
        assert_eq!(ui.get_draft_mode(), 1, "Disconnected preset accepted a tap");
        assert_eq!(applied.get(), 0, "Disconnected tuning accepted a write");
        ui.set_connected(true);
        ui.set_can_configure(false);
        frame(&window, &clock, 20, &mut pixels, width);
        click_control(&window, &ui, &clock, &mut pixels, "Eco");
        assert_eq!(
            ui.get_draft_mode(),
            0,
            "Connected preset could not be selected"
        );
        click_control(&window, &ui, &clock, &mut pixels, "Apply ride response");
        assert_eq!(applied.get(), 0, "Telemetry/stationary guard was bypassed");
        ui.set_can_configure(true);
        frame(&window, &clock, 40, &mut pixels, width);
        capture("tuning-connected", &pixels, width, height);
        click_control(&window, &ui, &clock, &mut pixels, "Apply ride response");
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
    if args.get(2).is_some_and(|arg| arg == "--update-fixtures") {
        let mut update_time = 0;
        ui.set_animations_enabled(false);
        ui.set_page(5);
        ui.set_can_maintain(false);
        let weak = ui.as_weak();
        ui.on_vesc_update_mode_requested(move |enabled| {
            weak.unwrap().set_vesc_update_mode(enabled)
        });
        let weak = ui.as_weak();
        ui.on_vesc_update_cancel(move || weak.unwrap().set_vesc_update_active(false));
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        click_control(&window, &ui, &clock, &mut pixels, "Start BLE update mode");
        assert!(!ui.get_vesc_update_mode());
        ui.set_can_maintain(true);
        click_control(&window, &ui, &clock, &mut pixels, "Start BLE update mode");
        assert!(ui.get_vesc_update_mode());
        ui.set_can_maintain(false);
        let wifi_cancelled = Rc::new(Cell::new(0));
        let called = wifi_cancelled.clone();
        let weak = ui.as_weak();
        ui.on_update_cancel(move || {
            called.set(called.get() + 1);
            let ui = weak.unwrap();
            ui.set_wifi_status("Update cancelled; current firmware unchanged".into());
            ui.set_updating(false);
        });
        let restarted = Rc::new(Cell::new(0));
        let called = restarted.clone();
        let weak = ui.as_weak();
        ui.on_update_reboot(move || {
            called.set(called.get() + 1);
            weak.unwrap().set_reboot_ready(false);
        });
        // Native CAN and direct BLE share the same source-independent upload state.
        for source in ["BLE", "CAN"] {
            ui.set_page(0);
            ui.set_vesc_update_transport(source.into());
            ui.set_vesc_update_status("Receiving firmware from VESC Tool".into());
            ui.set_vesc_update_cancellable(true);
            ui.set_vesc_update_active(true);
            ui.set_vesc_update_progress(42);
            frame(
                &window,
                &clock,
                {
                    update_time += 20;
                    update_time
                },
                &mut pixels,
                width,
            );
            assert!(ui.get_update_modal_open());
            capture(&format!("update-{source}"), &pixels, width, height);
            let track = element(&ui, "UpdateProgress::track");
            let fill = element(&ui, "UpdateProgress::fill");
            assert!((fill.absolute_position().x - track.absolute_position().x).abs() < 0.1);
            assert!((fill.size().width - track.size().width * 0.42).abs() < 0.1);
            let cancel = control(&ui, "Cancel firmware update");
            let panel = element(&ui, "ModalFrame::panel");
            assert!(cancel.absolute_position().y >= panel.absolute_position().y);
            assert!(
                cancel.absolute_position().y + cancel.size().height
                    <= panel.absolute_position().y + panel.size().height + 0.1
            );

            click_control(&window, &ui, &clock, &mut pixels, "Settings");
            assert_eq!(ui.get_page(), 0); // Modal blocks header/navigation hit testing.
            ui.set_vesc_update_status("Verifying received firmware...".into());
            ui.set_vesc_update_cancellable(false);
            frame(
                &window,
                &clock,
                {
                    update_time += 20;
                    update_time
                },
                &mut pixels,
                width,
            );
            click_control(&window, &ui, &clock, &mut pixels, "Cancel firmware update");
            assert!(ui.get_vesc_update_active());
            ui.set_vesc_update_cancellable(true);
            frame(
                &window,
                &clock,
                {
                    update_time += 20;
                    update_time
                },
                &mut pixels,
                width,
            );
            click_control(&window, &ui, &clock, &mut pixels, "Cancel firmware update");
            assert!(!ui.get_vesc_update_active());
            click_control(&window, &ui, &clock, &mut pixels, "Close update status");
            assert!(!ui.get_update_modal_open());
        }
        ui.set_page(2);
        ui.set_wifi_status("Downloading and verifying firmware...".into());
        ui.set_update_progress(67);
        ui.set_update_cancellable(true);
        ui.set_updating(true);
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        capture("update-wifi", &pixels, width, height);
        click_control(&window, &ui, &clock, &mut pixels, "Ride");
        assert_eq!(ui.get_page(), 2);
        ui.set_update_cancellable(false);
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        click_control(&window, &ui, &clock, &mut pixels, "Cancel firmware update");
        assert_eq!(wifi_cancelled.get(), 0);
        ui.set_update_cancellable(true);
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        click_control(&window, &ui, &clock, &mut pixels, "Cancel firmware update");
        assert_eq!(wifi_cancelled.get(), 1);
        assert!(ui.get_update_modal_open()); // Outcome persists until acknowledged.
        click_control(&window, &ui, &clock, &mut pixels, "Close update status");
        assert!(!ui.get_update_modal_open());
        ui.set_updating(true);
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        ui.set_updating(false);
        ui.set_reboot_ready(true);
        ui.set_wifi_status("Verified update ready. Restart to install.".into());
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        capture("update-ready", &pixels, width, height);
        click_control(&window, &ui, &clock, &mut pixels, "Restart into update");
        assert_eq!(restarted.get(), 0);
        ui.set_can_maintain(true);
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        click_control(&window, &ui, &clock, &mut pixels, "Restart into update");
        assert_eq!(restarted.get(), 1);
        assert!(!ui.get_update_modal_open());
        ui.set_page(5);
        frame(
            &window,
            &clock,
            {
                update_time += 20;
                update_time
            },
            &mut pixels,
            width,
        );
        click_control(&window, &ui, &clock, &mut pixels, "Exit BLE update mode");
        assert!(!ui.get_vesc_update_mode());
        println!(
            "{width}x{height}: Wi-Fi, BLE and native CAN progress modals, cancellation, verification, blocking and restart passed"
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
            ui.set_supports_tuning(id == 1);
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
    if args
        .get(2)
        .is_some_and(|arg| arg == "--rotation-repaint-fixtures")
    {
        ui.set_animations_enabled(false);
        ui.set_page(3);
        // Keep the same native panel memory across rotations, including old
        // button pixels. Expand the first frame's damage to the entire native
        // panel, then restore small dirty regions for incremental updates.
        for degrees in [0, 90, 180, 270, 0, 270, 180, 90] {
            let landscape = degrees == 90 || degrees == 270;
            let (logical_width, logical_height) = if landscape {
                (height, width)
            } else {
                (width, height)
            };
            let rotation = match degrees {
                90 => RenderingRotation::Rotate90,
                180 => RenderingRotation::Rotate180,
                270 => RenderingRotation::Rotate270,
                _ => RenderingRotation::NoRotation,
            };
            ui.set_rotation(degrees);
            ui.set_initial_width(logical_width as f32);
            ui.set_initial_height(logical_height as f32);
            window.set_size(PhysicalSize::new(
                logical_width as u32,
                logical_height as u32,
            ));
            slint::platform::update_timers_and_animations();
            PIXEL_WORK.with(|work| work.set(0));
            window.request_redraw();
            window.draw_if_needed(|renderer| {
                renderer.set_rendering_rotation(rotation);
                renderer.set_repaint_buffer_type(RepaintBufferType::ReusedBuffer);
                renderer.set_dirty_region_alignment(
                    slint::platform::software_renderer::DirtyRegionAlignment::new(
                        width as u16,
                        height as u16,
                    ),
                );
                renderer.render_by_line(Lines {
                    pixels: &mut pixels,
                    stride: width,
                });
                renderer.set_dirty_region_alignment(
                    slint::platform::software_renderer::DirtyRegionAlignment::new(2, 2),
                );
            });
            assert_eq!(
                PIXEL_WORK.with(Cell::get),
                (width * height) as u64,
                "Rotation must replace every native panel pixel"
            );
            for update in 0..2 {
                if update == 1 {
                    ui.set_connection(format!("Orientation {degrees}").into());
                    window.request_redraw();
                    window.draw_if_needed(|renderer| {
                        renderer.render_by_line(Lines {
                            pixels: &mut pixels,
                            stride: width,
                        });
                    });
                }
                let mut oracle = vec![
                    Rgb8Pixel {
                        r: 255,
                        g: 0,
                        b: 255
                    };
                    pixels.len()
                ];
                window.request_redraw();
                window.draw_if_needed(|renderer| {
                    renderer.set_repaint_buffer_type(RepaintBufferType::NewBuffer);
                    renderer.render_by_line(Lines {
                        pixels: &mut oracle,
                        stride: width,
                    });
                    renderer.set_repaint_buffer_type(RepaintBufferType::ReusedBuffer);
                });
                assert_eq!(
                    pixels, oracle,
                    "Rotation {degrees}, update {update} left stale pixels"
                );
            }
        }
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
        let start_y = apply_y; // Start within page content; drags must never submit a change.
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
    swipe(-60., 100., 1); // Scroll back from the bound while preserving axis arbitration.
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
    click_control(&window, &ui, &clock, &mut pixels, "Apply ride response");
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
