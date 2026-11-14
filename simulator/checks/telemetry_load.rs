//! Synthetic native UI load probe; timings are desktop work, never device FPS.
//! Usage: telemetry-load-check <0=Ride|1=Stats> <all|overview|rows> [width height]
use slint::platform::software_renderer::{
    LineBufferProvider, MinimalSoftwareWindow, RepaintBufferType,
};
use slint::platform::{Platform, WindowAdapter};
use slint::{ComponentHandle, Model, ModelRc, PhysicalSize, Rgb8Pixel, VecModel};
use std::{
    cell::Cell,
    rc::Rc,
    time::{Duration, Instant},
};
slint::include_modules!();
#[path = "../stats_fixture.rs"]
mod stats_fixture;
struct Backend {
    window: Rc<MinimalSoftwareWindow>,
    clock: Rc<Cell<Duration>>,
}
impl Platform for Backend {
    fn create_window_adapter(&self) -> Result<Rc<dyn WindowAdapter>, slint::PlatformError> {
        Ok(self.window.clone())
    }
    fn duration_since_start(&self) -> Duration {
        self.clock.get()
    }
}
struct Lines<'a> {
    pixels: &'a mut [Rgb8Pixel],
    width: usize,
    count: &'a mut u64,
    first: &'a mut Option<Duration>,
    started: Instant,
}
impl LineBufferProvider for Lines<'_> {
    type TargetPixel = Rgb8Pixel;
    fn process_line(
        &mut self,
        y: usize,
        x: std::ops::Range<usize>,
        draw: impl FnOnce(&mut [Rgb8Pixel]),
    ) {
        if self.first.is_none() {
            *self.first = Some(self.started.elapsed());
        }
        *self.count += x.len() as u64;
        draw(&mut self.pixels[y * self.width + x.start..y * self.width + x.end]);
    }
}
fn change(model: ModelRc<StatItem>, indexes: &[usize], phase: f32) {
    for &index in indexes {
        if let Some(mut item) = model.row_data(index) {
            let value: slint::SharedString =
                format!("{:.3} V", 3.7 + 0.1 * (phase + index as f32).sin()).into();
            if item.value != value {
                item.value = value;
                model.set_row_data(index, item);
            }
        }
    }
}
fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 3 || args.iter().any(|arg| arg == "--help") {
        println!("Usage: telemetry-load-check <0=Ride|1=Stats> <all|overview|rows> [width height]");
        return;
    }
    let page: i32 = args.get(1).unwrap().parse().unwrap();
    let mode = args.get(2).unwrap();
    assert!(matches!(page, 0 | 1), "Page must be 0 (Ride) or 1 (Stats)");
    assert!(
        matches!(mode.as_str(), "all" | "overview" | "rows"),
        "Unknown update mode"
    );
    let width = args.get(3).map_or(800, |v| v.parse().unwrap());
    let height = args.get(4).map_or(480, |v| v.parse().unwrap());
    let window = MinimalSoftwareWindow::new(RepaintBufferType::ReusedBuffer);
    let clock = Rc::new(Cell::new(Duration::ZERO));
    slint::platform::set_platform(Box::new(Backend {
        window: window.clone(),
        clock: clock.clone(),
    }))
    .unwrap();
    let ui = Dashboard::new().unwrap();
    ui.set_initial_width(width as f32);
    ui.set_initial_height(height as f32);
    ui.set_display_safe_inset_x(0.);
    ui.set_display_safe_inset_y(0.);
    window.set_size(PhysicalSize::new(width as u32, height as u32));
    ui.set_animations_enabled(false);
    ui.set_connected(true);
    ui.set_fresh(true);
    ui.set_connection("TEST | simulated 60 Hz".into());
    stats_fixture::project(&ui, true);
    ui.set_bms_cells(ModelRc::from(Rc::new(VecModel::from(
        (0..20)
            .map(|i| StatItem {
                label: format!("Cell {}", i + 1).into(),
                value: "3.700 V".into(),
            })
            .collect::<Vec<_>>(),
    ))));
    ui.set_bms_temperatures(ModelRc::from(Rc::new(VecModel::from(
        (0..4)
            .map(|i| StatItem {
                label: format!("Sensor {}", i + 1).into(),
                value: "30.0 C".into(),
            })
            .collect::<Vec<_>>(),
    ))));
    ui.set_page(page);
    ui.show().unwrap();
    let mut pixels = vec![Rgb8Pixel::default(); width * height];
    let (mut update, mut scene, mut render, mut dirty) =
        (Duration::ZERO, Duration::ZERO, Duration::ZERO, 0u64);
    let frames = 180;
    for frame in 0..frames + 30 {
        clock.set(Duration::from_micros(frame * 1_000_000 / 60));
        slint::platform::update_timers_and_animations();
        let phase = frame as f32 / 60. * 1.7;
        let start = Instant::now();
        if mode == "all" || mode == "overview" {
            ui.set_speed(format!("{:.1}", 28. + 25. * phase.sin()).into());
            ui.set_speed_progress(0.5 + 0.4 * phase.sin());
            ui.set_power(format!("{:.0} W", 2000. * phase.sin()).into());
            ui.set_battery(format!("{:.0}%", 65. + 30. * phase.cos()).into());
            ui.set_battery_level(65. + 30. * phase.cos());
            ui.set_voltage(format!("{:.1} V", 74. + 6. * phase.cos()).into());
            ui.set_motor_current(format!("{:.1} A", 70. + 100. * phase.sin()).into());
            ui.set_battery_current(format!("{:.1} A", 30. + 50. * phase.sin()).into());
            ui.set_pitch(format!("{:.1}\u{b0}", 20. * phase.sin()).into());
            ui.set_roll(format!("{:.1}\u{b0}", 15. * phase.cos()).into());
            ui.set_mosfet(format!("{:.0} \u{b0}C", 50. + 15. * phase.sin()).into());
            ui.set_motor(format!("{:.0} \u{b0}C", 55. + 15. * phase.cos()).into());
            ui.set_trip(format!("{:.2} km", 12.34 + frame as f32 / 7500.).into());
            ui.set_odometer("1234.5 km".into());
        }
        if mode == "all" || mode == "rows" {
            change(
                ui.get_vesc_stats(),
                &[0, 1, 2, 3, 4, 5, 6, 7, 10, 11],
                phase,
            );
            change(
                ui.get_bms_stats(),
                &[3, 5, 6, 7, 8, 9, 12, 14, 15, 22, 23, 24],
                phase,
            );
            change(ui.get_bms_cells(), &(0..20).collect::<Vec<_>>(), phase);
            change(
                ui.get_bms_temperatures(),
                &(0..4).collect::<Vec<_>>(),
                phase,
            );
        }
        let projected = start.elapsed();
        window.request_redraw();
        let started = Instant::now();
        let mut count = 0;
        let mut first = None;
        window.draw_if_needed(|renderer| {
            renderer.render_by_line(Lines {
                pixels: &mut pixels,
                width,
                count: &mut count,
                first: &mut first,
                started,
            });
        });
        let elapsed = started.elapsed();
        if frame >= 30 {
            update += projected;
            scene += first.unwrap_or(elapsed);
            render += elapsed;
            dirty += count;
        }
    }
    println!(
        "page={page} mode={mode} size={width}x{height} update_us={:.1} scene_us={:.1} render_us={:.1} dirty_pixels={}",
        update.as_secs_f64() * 1e6 / frames as f64,
        scene.as_secs_f64() * 1e6 / frames as f64,
        render.as_secs_f64() * 1e6 / frames as f64,
        dirty / frames
    );
}
