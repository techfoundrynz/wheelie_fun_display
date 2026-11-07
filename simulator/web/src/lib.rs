use slint::platform::software_renderer::{MinimalSoftwareWindow, RepaintBufferType};
use slint::platform::{Platform, PointerEventButton, WindowAdapter, WindowEvent};
use slint::{ComponentHandle, Model, ModelRc, PhysicalSize, Rgb8Pixel, VecModel};
use std::{
    cell::{Cell, RefCell},
    rc::Rc,
    time::Duration,
};
use wasm_bindgen::{JsCast, prelude::*};
slint::include_modules!();
#[path = "../../stats_fixture.rs"]
mod stats_fixture;
#[path = "../../tuning_fixture.rs"]
mod tuning_fixture;

struct BrowserPlatform {
    window: Rc<MinimalSoftwareWindow>,
    clock: Rc<Cell<Duration>>,
}
impl Platform for BrowserPlatform {
    fn create_window_adapter(&self) -> Result<Rc<dyn WindowAdapter>, slint::PlatformError> {
        Ok(self.window.clone())
    }
    fn duration_since_start(&self) -> Duration {
        self.clock.get()
    }
}
#[derive(Default)]
struct Demo {
    speed: f32,
    power_override: Option<f32>,
    battery: f32,
    temperature: f32,
    pitch: f32,
    connected: bool,
    documentation_fixture: bool,
    pin: String,
    digits: String,
    candidate: String,
    pin_mode: u8,
    entry_pattern: bool,
    last_touch: f64,
    editing_name: bool,
    proxy_name_preference: String,
    pairing_digits: String,
}

fn integration(ui: &Dashboard, id: i32) {
    ui.set_active_integration(id);
    presentation(ui, id);
}
fn presentation(ui: &Dashboard, id: i32) {
    ui.set_vehicle_brand(
        match id {
            2 => "wheel-e",
            0 => "VESC",
            _ => "Wheelie Fun Bike",
        }
        .into(),
    );
    ui.set_supports_tuning(id == 1);
    ui.set_supports_presets(id == 1);
    ui.set_supports_limit(id == 1);
    ui.set_supports_lock(id != 0);
}
fn project_proxy_name(ui: &Dashboard, state: &Rc<RefCell<Demo>>) {
    let id = if ui.get_connected() {
        ui.get_active_integration()
    } else {
        ui.get_integration_choice()
    };
    presentation(ui, id.max(0));
    let locked = id == 1;
    let mut demo = state.borrow_mut();
    ui.set_proxy_name_locked(locked);
    ui.set_proxy_name(
        if locked {
            "WFB"
        } else {
            &demo.proxy_name_preference
        }
        .into(),
    );
    if locked && demo.editing_name {
        demo.editing_name = false;
        ui.set_editing(false);
        editor_clear(ui);
    }
}
fn editor_clear(ui: &Dashboard) {
    ui.set_editor_value("".into());
    ui.set_editor_masked("".into());
}
fn pin_open(ui: &Dashboard, state: &mut Demo, mode: u8) {
    state.digits.clear();
    state.pin_mode = mode;
    ui.set_pin_open(true);
    ui.set_pin_value("".into());
    ui.set_pin_ready(true);
    let pattern = state.entry_pattern;
    ui.set_pin_pattern(pattern);
    ui.set_pin_heading(
        match (mode, pattern) {
            (1, true) => "CREATE PATTERN",
            (2, true) => "CONFIRM PATTERN",
            (_, true) => "ENTER PATTERN",
            (1, false) => "CREATE PIN",
            (2, false) => "CONFIRM PIN",
            _ => "ENTER PIN",
        }
        .into(),
    );
    ui.set_pin_message(
        if pattern {
            "Connect six dots, then lift your finger."
        } else if state.documentation_fixture {
            "Choose a six-digit PIN to secure your display."
        } else {
            "Simulator only: use a demo PIN, not a real code."
        }
        .into(),
    );
}
fn configure(ui: &Dashboard, state: &Rc<RefCell<Demo>>) {
    project_proxy_name(ui, state);
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_edit_proxy_name(move || {
        let ui = weak.unwrap();
        if ui.get_proxy_name_locked() {
            return;
        }
        demo.borrow_mut().editing_name = true;
        ui.set_editor_heading("BLE name | up to 29 bytes".into());
        ui.set_editor_secret(false);
        ui.set_editor_value(demo.borrow().proxy_name_preference.clone().into());
        ui.set_editor_masked("".into());
        ui.set_editing(true);
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pairing_type(move |digit| {
        let mut state = demo.borrow_mut();
        if state.pairing_digits.len() < 6 && (0..=9).contains(&digit) {
            state.pairing_digits.push(char::from(b'0' + digit as u8));
        }
        weak.unwrap()
            .set_pairing_value(state.pairing_digits.clone().into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pairing_clear(move || {
        demo.borrow_mut().pairing_digits.clear();
        weak.unwrap().set_pairing_value("".into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pairing_backspace(move || {
        let mut state = demo.borrow_mut();
        state.pairing_digits.pop();
        weak.unwrap()
            .set_pairing_value(state.pairing_digits.clone().into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pairing_submit(move || {
        let ui = weak.unwrap();
        let correct = demo.borrow().pairing_digits == "123456";
        if correct {
            ui.set_pairing_open(false);
            ui.set_feedback("Demo pairing complete | no radio traffic".into());
        } else {
            ui.set_pairing_failed(true);
        }
        demo.borrow_mut().pairing_digits.clear();
        ui.set_pairing_value("".into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pairing_cancel(move || {
        demo.borrow_mut().pairing_digits.clear();
        weak.unwrap().set_pairing_open(false);
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pairing_retry(move || {
        let ui = weak.unwrap();
        demo.borrow_mut().pairing_digits.clear();
        ui.set_pairing_value("".into());
        ui.set_pairing_failed(false);
    });
    ui.set_device_label("Browser simulator | 2.06-inch panel".into());
    ui.set_firmware_version(env!("WFD_FIRMWARE_VERSION").into());
    ui.set_connected(true);
    ui.set_link_present(true);
    ui.set_fresh(true);
    ui.set_connection("Connected".into());
    ui.set_can_available(true);
    ui.set_uart_available(true);
    ui.set_feedback("Demo only | no commands reach a real bike.".into());
    ui.set_peers(ModelRc::from(Rc::new(VecModel::from(vec![
        Peer {
            name: "WFB demo bike".into(),
            address: "DE:MO:00:00:00:01".into(),
            signal: "-54 dBm".into(),
        },
        Peer {
            name: "wheel-e demo bike".into(),
            address: "DE:MO:00:00:00:02".into(),
            signal: "-61 dBm".into(),
        },
        Peer {
            name: "VESC demo".into(),
            address: "DE:MO:00:00:00:03".into(),
            signal: "-67 dBm".into(),
        },
    ]))));
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_connect_peer(move |index| {
        let ui = weak.unwrap();
        let choice = ui.get_integration_choice();
        integration(
            &ui,
            if choice < 0 {
                [1, 2, 0][index.clamp(0, 2) as usize]
            } else {
                choice
            },
        );
        demo.borrow_mut().connected = true;
        ui.set_connected(true);
        ui.set_link_present(true);
        ui.set_fresh(true);
        ui.set_page(0);
        ui.set_controller_status("Demo | main VESC discovered".into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_forget_peer(move || {
        demo.borrow_mut().connected = false;
        let ui = weak.unwrap();
        ui.set_connected(false);
        ui.set_link_present(false);
        ui.set_fresh(false);
        ui.set_connection("Disconnected".into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_transport_requested(move |kind| {
        let ui = weak.unwrap();
        ui.set_transport_kind(kind);
        ui.set_connected(false);
        ui.set_link_present(false);
        demo.borrow_mut().connected = false;
        ui.set_controller_status("Demo | choose integration and connect".into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_connect_wired(move || {
        let ui = weak.unwrap();
        let id = ui.get_integration_choice();
        integration(&ui, if id < 0 { 1 } else { id });
        ui.set_connected(true);
        ui.set_link_present(true);
        demo.borrow_mut().connected = true;
        ui.set_controller_status("Demo | main VESC CAN 7 | firmware 6.6".into());
    });
    let weak = ui.as_weak();
    ui.on_discover_controller(move || {
        weak.unwrap()
            .set_controller_status("Demo | first matching package controller found".into())
    });
    let weak = ui.as_weak();
    ui.on_ble_proxy_changed(move |on| {
        weak.unwrap().set_ble_proxy_status(
            if on {
                "Demo | phone bridge enabled; no radio traffic"
            } else {
                "Off"
            }
            .into(),
        )
    });
    let weak = ui.as_weak();
    ui.on_apply_mode(move |mode| {
        let ui = weak.unwrap();
        ui.set_confirm_text(
            format!(
                "Demo ride response: {}",
                ["Eco", "Cruise", "Sport"][mode.clamp(0, 2) as usize]
            )
            .into(),
        );
        ui.set_confirming(true);
    });
    let weak = ui.as_weak();
    ui.on_apply_limit(move |limit| {
        let ui = weak.unwrap();
        ui.set_confirm_text(format!("Demo limit: {limit} mph").into());
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
    let weak = ui.as_weak();
    ui.on_orientation_requested(move |degrees| {
        let ui = weak.unwrap();
        let landscape = degrees == 90 || degrees == 270;
        let short = ui.get_initial_width().min(ui.get_initial_height());
        let long = ui.get_initial_width().max(ui.get_initial_height());
        ui.set_rotation(degrees);
        ui.set_initial_width(if landscape { long } else { short });
        ui.set_initial_height(if landscape { short } else { long });
    });
    ui.set_wifi_networks(ModelRc::from(Rc::new(VecModel::from(vec![WifiItem {
        ssid: "Workshop demo".into(),
        detail: "-45 dBm | secured".into(),
    }]))));
    let weak = ui.as_weak();
    ui.on_wifi_scan(move || {
        weak.unwrap()
            .set_wifi_status("Demo | choose a network".into())
    });
    let weak = ui.as_weak();
    ui.on_wifi_manual(move || {
        let ui = weak.unwrap();
        ui.set_editor_secret(false);
        ui.set_editor_heading("Network name (SSID)".into());
        editor_clear(&ui);
        ui.set_editing(true);
    });
    let weak = ui.as_weak();
    ui.on_wifi_select(move |_| {
        let ui = weak.unwrap();
        ui.set_wifi_saved("Workshop demo".into());
        ui.set_editor_secret(true);
        ui.set_editor_heading("Wi-Fi password | demo only".into());
        editor_clear(&ui);
        ui.set_editing(true);
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_editor_type(move |text| {
        let ui = weak.unwrap();
        let mut value = ui.get_editor_value().to_string();
        let limit = if demo.borrow().editing_name {
            29
        } else if ui.get_editor_secret() {
            64
        } else {
            32
        };
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
    let demo = state.clone();
    ui.on_editor_cancel(move || {
        let ui = weak.unwrap();
        ui.set_editing(false);
        demo.borrow_mut().editing_name = false;
        editor_clear(&ui);
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_editor_done(move || {
        let ui = weak.unwrap();
        if demo.borrow().editing_name {
            if ui.get_proxy_name_locked() {
                ui.set_editing(false);
                demo.borrow_mut().editing_name = false;
                editor_clear(&ui);
            } else if !ui.get_editor_value().trim().is_empty() {
                demo.borrow_mut().proxy_name_preference = ui.get_editor_value().to_string();
                ui.set_proxy_name(ui.get_editor_value());
                ui.set_editing(false);
                demo.borrow_mut().editing_name = false;
                editor_clear(&ui);
            }
            return;
        }
        if !ui.get_editor_secret() {
            ui.set_wifi_saved(ui.get_editor_value());
            ui.set_editor_secret(true);
            ui.set_editor_heading("Wi-Fi password | demo only".into());
        } else {
            ui.set_editing(false);
            ui.set_wifi_connected(true);
            ui.set_wifi_status("Demo | Wi-Fi connected".into());
        }
        editor_clear(&ui);
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
        ui.set_wifi_status("Demo | Wi-Fi connected".into());
    });
    ui.set_vesc_update_status("Ready for VESC Tool".into());
    let weak = ui.as_weak();
    ui.on_vesc_update_mode_requested(move |enabled| {
        let ui = weak.unwrap();
        ui.set_vesc_update_mode(enabled);
        ui.set_can_maintain(!enabled);
        ui.set_vesc_update_status(
            if enabled {
                "Waiting for VESC Tool (preview only)"
            } else {
                "Ready for VESC Tool"
            }
            .into(),
        );
    });
    let weak = ui.as_weak();
    ui.on_vesc_update_cancel(move || {
        let ui = weak.unwrap();
        ui.set_vesc_update_active(false);
        ui.set_vesc_update_status("Upload cancelled".into());
    });
    let weak = ui.as_weak();
    ui.on_update_check(move || {
        let ui = weak.unwrap();
        ui.set_release_version("Demo release".into());
        ui.set_update_available(true);
        ui.set_wifi_status("Demo only | no firmware is downloaded".into());
    });
    let weak = ui.as_weak();
    ui.on_update_install(move || {
        let ui = weak.unwrap();
        ui.set_update_progress(100);
        ui.set_reboot_ready(true);
        ui.set_wifi_status("Demo update verified locally | no real download".into());
    });
    let weak = ui.as_weak();
    ui.on_update_cancel(move || weak.unwrap().set_updating(false));
    let weak = ui.as_weak();
    ui.on_update_reboot(move || {
        let ui = weak.unwrap();
        ui.set_reboot_ready(false);
        ui.set_page(0);
    });
    ui.set_about_items(ModelRc::from(Rc::new(VecModel::from(vec![
        AboutItem {
            label: "Runtime".into(),
            value: "Shared Slint UI | software renderer | WebAssembly".into(),
        },
        AboutItem {
            label: "Hardware".into(),
            value: "All connections, telemetry and updates are simulated".into(),
        },
        AboutItem {
            label: "Persistence".into(),
            value: "This browser session only".into(),
        },
    ]))));
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_unlock_method_requested(move |method| {
        let ui = weak.unwrap();
        if !ui.get_pin_open() {
            return;
        }
        let mut state = demo.borrow_mut();
        state.entry_pattern = method == 1;
        ui.set_unlock_method(method);
        let mode = state.pin_mode;
        pin_open(&ui, &mut state, mode);
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pin_setup(move || {
        let ui = weak.unwrap();
        let mut state = demo.borrow_mut();
        let mode = if state.pin.is_empty() { 1 } else { 4 };
        pin_open(&ui, &mut state, mode);
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pin_remove(move || pin_open(&weak.unwrap(), &mut demo.borrow_mut(), 5));
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pin_digit(move |digit| {
        let mut state = demo.borrow_mut();
        if state.digits.len() < 6 && (0..=9).contains(&digit) {
            state.digits.push(char::from(b'0' + digit as u8));
        }
        weak.unwrap().set_pin_value(state.digits.clone().into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pin_clear(move || {
        demo.borrow_mut().digits.clear();
        weak.unwrap().set_pin_value("".into());
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pin_backspace(move || {
        let mut state = demo.borrow_mut();
        state.digits.pop();
        weak.unwrap().set_pin_value(state.digits.clone().into());
    });
    let weak = ui.as_weak();
    ui.on_pin_cancel(move || weak.unwrap().set_pin_open(false));
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_pin_submit(move || {
        let ui = weak.unwrap();
        let mut state = demo.borrow_mut();
        if state.digits.len() != 6 {
            return;
        }
        let entered = state.digits.clone();
        match state.pin_mode {
            1 => {
                state.candidate = entered;
                pin_open(&ui, &mut state, 2);
            }
            2 => {
                if entered == state.candidate {
                    ui.set_unlock_method(i32::from(state.entry_pattern));
                    ui.set_proxy_pairing_code(entered.clone().into());
                    state.pin = entered;
                    ui.set_pin_configured(true);
                    ui.set_pin_open(false);
                    ui.set_bike_lock_status(
                        if state.documentation_fixture {
                            "Screen lock saved."
                        } else {
                            "Demo screen lock saved for this session."
                        }
                        .into(),
                    );
                } else {
                    pin_open(&ui, &mut state, 1);
                    ui.set_pin_message("PINs did not match. Try again.".into());
                }
            }
            _ => {
                if entered != state.pin {
                    state.digits.clear();
                    ui.set_pin_value("".into());
                    ui.set_pin_message("Incorrect demo PIN".into());
                } else if state.pin_mode == 4 {
                    pin_open(&ui, &mut state, 1);
                } else {
                    if state.pin_mode == 5 {
                        state.pin.clear();
                        ui.set_unlock_method(i32::from(state.entry_pattern));
                        ui.set_proxy_pairing_code("".into());
                        ui.set_pin_configured(false);
                    } else {
                        ui.set_proxy_pairing_code(entered.into());
                    }
                    ui.set_screen_locked(false);
                    ui.set_pin_open(false);
                }
            }
        }
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_screen_lock(move || {
        let ui = weak.unwrap();
        ui.set_screen_locked(true);
        pin_open(&ui, &mut demo.borrow_mut(), 3);
    });
    let weak = ui.as_weak();
    let demo = state.clone();
    ui.on_bike_lock(move || {
        let ui = weak.unwrap();
        ui.set_screen_locked(true);
        pin_open(&ui, &mut demo.borrow_mut(), 3);
        ui.set_pin_message("Demo bike disabled | enter your demo PIN".into());
    });
    let weak = ui.as_weak();
    ui.on_lock_timeout_changed(move |seconds| weak.unwrap().set_lock_timeout(seconds));
}

#[wasm_bindgen]
pub struct WebSimulator {
    ui: Dashboard,
    window: Rc<MinimalSoftwareWindow>,
    clock: Rc<Cell<Duration>>,
    canvas: web_sys::HtmlCanvasElement,
    context: web_sys::CanvasRenderingContext2d,
    pixels: Vec<Rgb8Pixel>,
    rgba: Vec<u8>,
    state: Rc<RefCell<Demo>>,
    width: usize,
    height: usize,
    consume_touch: bool,
    pressed: bool,
    rotation: i32,
}

#[wasm_bindgen]
impl WebSimulator {
    #[wasm_bindgen(constructor)]
    pub fn new(canvas_id: &str) -> Result<WebSimulator, JsValue> {
        console_error_panic_hook::set_once();
        let canvas = web_sys::window()
            .and_then(|w| w.document())
            .and_then(|d| d.get_element_by_id(canvas_id))
            .ok_or_else(|| JsValue::from_str("Canvas missing"))?
            .dyn_into::<web_sys::HtmlCanvasElement>()?;
        let context = canvas
            .get_context("2d")?
            .ok_or_else(|| JsValue::from_str("Canvas2D unavailable"))?
            .dyn_into::<web_sys::CanvasRenderingContext2d>()?;
        let window = MinimalSoftwareWindow::new(RepaintBufferType::ReusedBuffer);
        let clock = Rc::new(Cell::new(Duration::ZERO));
        slint::platform::set_platform(Box::new(BrowserPlatform {
            window: window.clone(),
            clock: clock.clone(),
        }))
        .map_err(|e| JsValue::from_str(&e.to_string()))?;
        let ui = Dashboard::new().map_err(|e| JsValue::from_str(&e.to_string()))?;
        let state = Rc::new(RefCell::new(Demo {
            battery: 82.,
            temperature: 36.,
            connected: true,
            proxy_name_preference: "WB Disp".into(),
            ..Default::default()
        }));
        configure(&ui, &state);
        tuning_fixture::configure(&ui);
        window.set_size(PhysicalSize::new(410, 502));
        ui.show().map_err(|e| JsValue::from_str(&e.to_string()))?;
        Ok(Self {
            ui,
            window,
            clock,
            canvas,
            context,
            pixels: Vec::new(),
            rgba: Vec::new(),
            state,
            width: 0,
            height: 0,
            consume_touch: false,
            pressed: false,
            rotation: 0,
        })
    }
    pub fn frame(&mut self, milliseconds: f64) -> Result<(), JsValue> {
        project_proxy_name(&self.ui, &self.state);
        self.clock
            .set(Duration::from_secs_f64(milliseconds.max(0.) / 1000.));
        self.ui.set_pairing_permitted(!self.ui.get_screen_locked());
        slint::platform::update_timers_and_animations();
        let rotation = self.ui.get_rotation();
        if rotation != self.rotation {
            self.rotation = rotation;
            if self.pressed {
                self.window.dispatch_event(WindowEvent::PointerExited);
                self.window.dispatch_event(WindowEvent::PointerReleased {
                    position: slint::LogicalPosition::new(-1., -1.),
                    button: PointerEventButton::Left,
                });
                self.consume_touch = true;
            }
        }
        let width = self.ui.get_initial_width() as usize;
        let height = self.ui.get_initial_height() as usize;
        if width != self.width || height != self.height {
            self.width = width;
            self.height = height;
            self.canvas.set_width(width as u32);
            self.canvas.set_height(height as u32);
            self.window
                .set_size(PhysicalSize::new(width as u32, height as u32));
            self.pixels = vec![Rgb8Pixel::default(); width * height];
            self.rgba = vec![0; width * height * 4];
            self.window.request_redraw();
        }
        {
            let state = self.state.borrow();
            let live = state.connected;
            self.ui.set_data_stale(false);
            self.ui.set_bike_ble_rssi(-62);
            self.ui
                .set_bike_ble_signal(if live && self.ui.get_transport_kind() == 0 {
                    3
                } else {
                    -1
                });
            let metric = self.ui.get_metric();
            let speed = state.speed;
            if live {
                self.ui
                    .set_speed(format!("{:.1}", speed * if metric { 1. } else { 0.621371 }).into());
                self.ui.set_battery(format!("{:.0}%", state.battery).into());
                self.ui.set_voltage("50.4 V".into());
                let (divisor, unit) = if metric { (1., "km") } else { (1.609344, "mi") };
                self.ui
                    .set_trip(format!("{:.2} {unit}", 12.34 / divisor).into());
                self.ui
                    .set_odometer(format!("{:.1} {unit}", 12345.6 / divisor).into());
                let power = state.power_override.unwrap_or(
                    speed
                        * if state.documentation_fixture {
                            26.
                        } else {
                            42.
                        },
                );
                self.ui.set_power(format!("{power:.0} W").into());
                self.ui
                    .set_motor_current(format!("{:.1} A", power / 28.).into());
                self.ui
                    .set_battery_current(format!("{:.1} A", power / 50.4).into());
                self.ui.set_roll("0.8°".into());
                self.ui.set_mosfet(
                    format!(
                        "{:.0}°{}",
                        if self.ui.get_celsius() {
                            state.temperature
                        } else {
                            state.temperature * 1.8 + 32.
                        },
                        if self.ui.get_celsius() { "C" } else { "F" }
                    )
                    .into(),
                );
                self.ui.set_motor(
                    if self.ui.get_celsius() {
                        "42°C"
                    } else {
                        "108°F"
                    }
                    .into(),
                );
                self.ui.set_pitch(format!("{:.1}°", state.pitch).into());
            } else {
                self.ui.set_speed("--".into());
                self.ui.set_battery("--".into());
                self.ui.set_voltage("--".into());
                self.ui.set_trip("--".into());
                self.ui.set_odometer("--".into());
                self.ui.set_power("--".into());
                self.ui.set_motor_current("--".into());
                self.ui.set_battery_current("--".into());
                self.ui.set_roll("--".into());
                self.ui.set_pitch("--".into());
            }
            stats_fixture::project(&self.ui, live);
            self.ui.set_fresh(live);
            self.ui
                .set_can_configure(live && speed < 0.72 && !self.ui.get_screen_locked());
            self.ui.set_can_maintain(speed < 0.72);
            self.ui
                .set_speed_progress(if live { speed / 56.3263 } else { 0. });
            self.ui
                .set_battery_level(if live { state.battery } else { 0. });
            self.ui.set_hot(state.temperature >= 75.);
            if self.ui.get_screen_locked()
                && !self.pressed
                && milliseconds - state.last_touch >= self.ui.get_lock_timeout() as f64 * 1000.
            {
                self.ui.set_screen_asleep(true);
            }
        }
        let mut changed = false;
        self.window.draw_if_needed(|renderer| {
            renderer.render(&mut self.pixels, width);
            changed = true;
        });
        if changed {
            for (pixel, rgba) in self.pixels.iter().zip(self.rgba.chunks_exact_mut(4)) {
                rgba.copy_from_slice(&[pixel.r, pixel.g, pixel.b, 255]);
            }
            let image = web_sys::ImageData::new_with_u8_clamped_array_and_sh(
                wasm_bindgen::Clamped(&self.rgba),
                width as u32,
                height as u32,
            )?;
            self.context.put_image_data(&image, 0., 0.)?;
        }
        Ok(())
    }
    pub fn pointer(&mut self, kind: &str, x: f32, y: f32, milliseconds: f64) {
        self.state.borrow_mut().last_touch = milliseconds;
        if kind == "down" {
            self.pressed = true;
        }
        if kind == "up" || kind == "cancel" {
            self.pressed = false;
        }
        if self.ui.get_screen_asleep() {
            self.ui.set_screen_asleep(false);
            self.consume_touch = true;
            return;
        }
        if self.consume_touch {
            if kind == "up" || kind == "cancel" {
                self.consume_touch = false;
            }
            return;
        }
        if kind == "cancel" {
            self.window.dispatch_event(WindowEvent::PointerExited);
            self.window.dispatch_event(WindowEvent::PointerReleased {
                position: slint::LogicalPosition::new(-1., -1.),
                button: PointerEventButton::Left,
            });
            return;
        }
        let position = slint::LogicalPosition::new(x, y);
        self.window.dispatch_event(match kind {
            "down" => WindowEvent::PointerPressed {
                position,
                button: PointerEventButton::Left,
            },
            "up" => WindowEvent::PointerReleased {
                position,
                button: PointerEventButton::Left,
            },
            "cancel" => WindowEvent::PointerExited,
            _ => WindowEvent::PointerMoved { position },
        });
    }
    pub fn scroll(&self, x: f32, y: f32, delta: f32) {
        self.window.dispatch_event(WindowEvent::PointerScrolled {
            position: slint::LogicalPosition::new(x, y),
            delta_x: 0.,
            delta_y: delta,
        });
    }
    pub fn set_control(&self, name: &str, value: f32) {
        if name == "unlock-method" {
            self.ui.invoke_unlock_method_requested(value as i32);
            return;
        }
        if name == "phone-connected" {
            self.ui.set_phone_connected(value != 0.);
            return;
        }
        let mut state = self.state.borrow_mut();
        match name {
            "speed" => state.speed = value.clamp(0., 80.),
            "power" => state.power_override = Some(value.clamp(-100000., 100000.)),
            "battery" => state.battery = value.clamp(0., 100.),
            "temperature" => state.temperature = value.clamp(0., 120.),
            "pitch" => state.pitch = value.clamp(-90., 90.),
            _ => {}
        }
    }
    pub fn set_connected(&self, connected: bool) {
        self.state.borrow_mut().connected = connected;
        self.ui.set_connected(connected);
        self.ui.set_link_present(connected);
        self.ui.set_connection(
            if connected {
                "Connected"
            } else {
                "Disconnected"
            }
            .into(),
        );
    }
    pub fn set_integration(&self, id: i32) {
        integration(&self.ui, id.clamp(0, 2));
        project_proxy_name(&self.ui, &self.state);
    }
    pub fn set_display_model(&self, model: i32) {
        let large = model == 1;
        let (short, long) = if large { (480., 800.) } else { (410., 502.) };
        let landscape = matches!(self.ui.get_rotation(), 90 | 270);
        self.ui
            .set_initial_width(if landscape { long } else { short });
        self.ui
            .set_initial_height(if landscape { short } else { long });
        self.ui
            .set_display_safe_inset_x(if large { 24. } else { 32. });
        self.ui
            .set_display_safe_inset_y(if large { 24. } else { 16. });
        self.ui.set_device_label(
            if large {
                "GUITION P4 4.3-inch"
            } else {
                "Waveshare AMOLED 2.06"
            }
            .into(),
        );
    }
    pub fn set_rotation(&self, degrees: i32) {
        if matches!(degrees, 0 | 90 | 180 | 270) {
            self.ui.invoke_orientation_requested(degrees);
        }
    }
    pub fn navigate(&self, page: i32) {
        if !self.ui.get_pin_open() {
            self.ui.set_page(page.clamp(0, 6));
        }
    }
    /// Deterministic illustrative data for generated docs pictures only.
    /// Browser chrome continues to identify the preview as simulated hardware.
    pub fn use_documentation_fixture(&self, riding: bool) {
        let mut state = self.state.borrow_mut();
        state.documentation_fixture = true;
        state.connected = true;
        state.speed = if riding { 24.6 } else { 0. };
        state.battery = 82.;
        state.temperature = 36.;
        state.pitch = if riding { 4.2 } else { 0.8 };
        self.ui.set_connected(true);
        self.ui.set_link_present(true);
        self.ui.set_connection("Connected".into());
        self.ui.set_controller_status("VESC · CAN ID 7".into());
        self.ui.set_feedback(
            if riding {
                ""
            } else {
                "Changes are sent only when you tap Apply."
            }
            .into(),
        );
        self.ui.set_device_label(
            if self
                .ui
                .get_initial_width()
                .max(self.ui.get_initial_height())
                >= 800.
            {
                "GUITION P4 4.3-inch"
            } else {
                "Waveshare AMOLED 2.06"
            }
            .into(),
        );
        self.ui.set_ble_proxy_status("Phone bridge off".into());
        self.ui.set_wifi_status("Connected to Workshop".into());
        self.ui.set_wifi_connected(true);
        self.ui.set_wifi_saved("Workshop".into());
        self.ui
            .set_wifi_networks(ModelRc::from(Rc::new(VecModel::from(vec![WifiItem {
                ssid: "Workshop".into(),
                detail: "-45 dBm | secured".into(),
            }]))));
        self.ui
            .set_peers(ModelRc::from(Rc::new(VecModel::from(vec![Peer {
                name: if self.ui.get_active_integration() == 2 {
                    "wheel-e"
                } else {
                    "Wheelie Fun Bike"
                }
                .into(),
                address: "C2:34:56:78:9A:BC".into(),
                signal: "-54 dBm".into(),
            }]))));
        self.ui
            .set_about_items(ModelRc::from(Rc::new(VecModel::from(vec![
                AboutItem {
                    label: "Display profile".into(),
                    value: "410 × 502 | CO5300 | FT3168".into(),
                },
                AboutItem {
                    label: "Platform".into(),
                    value: "ESP32-S3 | ESP-IDF 6.1 | Slint".into(),
                },
            ]))));
    }
    pub fn show_pairing_demo(&self) {
        self.state.borrow_mut().pairing_digits.clear();
        self.ui.set_pairing_value("".into());
        self.ui.set_pairing_ready(true);
        self.ui.set_pairing_failed(false);
        self.ui.set_pairing_open(true);
    }
    pub fn set_tuning_section(&self, section: i32) {
        self.ui.invoke_tuning_section_requested(section);
    }
    pub fn adjust_tuning(&self, id: i32, delta: i32) {
        self.ui.invoke_tuning_adjust(id, delta);
    }
    pub fn edit_tuning(&self, id: i32) {
        if let Some(index) = (0..self.ui.get_tuning_fields().row_count()).find(|i| {
            self.ui
                .get_tuning_fields()
                .row_data(*i)
                .is_some_and(|row| row.id == id)
        }) {
            self.ui.invoke_tuning_edit(index as i32);
        }
    }
    pub fn invoke(&self, callback: &str) -> Result<(), JsValue> {
        match callback {
            "tuning-save" => self.ui.invoke_tuning_save(),
            "tuning-reset" => self.ui.invoke_tuning_reset(),
            "tuning-editor-done" => self.ui.invoke_tuning_edit(-1),
            "tuning-refresh" => self.ui.invoke_tuning_refresh(),
            "confirm-change" => self.ui.invoke_confirm_change(),
            "cancel-change" => self.ui.invoke_cancel_change(),
            "pin-setup" => self.ui.invoke_pin_setup(),
            "screen-lock" => self.ui.invoke_screen_lock(),
            "wifi-manual" => self.ui.invoke_wifi_manual(),
            "edit-proxy-name" => self.ui.invoke_edit_proxy_name(),
            "editor-done" => self.ui.invoke_editor_done(),
            "editor-backspace" => self.ui.invoke_editor_backspace(),
            _ => return Err(JsValue::from_str("Unknown demo callback")),
        }
        Ok(())
    }
    pub fn state(&self, name: &str) -> JsValue {
        match name {
            "tuning-dirty" => self.ui.get_tuning_dirty().into(),
            "tuning-ready" => self.ui.get_tuning_ready().into(),
            "tuning-editor-open" => self.ui.get_tuning_editor_open().into(),
            "tuning-status" => self.ui.get_tuning_status().as_str().into(),
            "confirming" => self.ui.get_confirming().into(),
            "documentation-fixture" => self.state.borrow().documentation_fixture.into(),
            "connected" => self.ui.get_connected().into(),
            "phone-connected" => self.ui.get_phone_connected().into(),
            "connection" => self.ui.get_connection().as_str().into(),
            "speed-progress" => self.ui.get_speed_progress().into(),
            "power" => self.ui.get_power().as_str().into(),
            "feedback" => self.ui.get_feedback().as_str().into(),
            "pin-message" => self.ui.get_pin_message().as_str().into(),
            "device-label" => self.ui.get_device_label().as_str().into(),
            "page" => self.ui.get_page().into(),
            "speed" => self.ui.get_speed().as_str().into(),
            "trip" => self.ui.get_trip().as_str().into(),
            "odometer" => self.ui.get_odometer().as_str().into(),
            "motor" => self.ui.get_motor().as_str().into(),
            "proxy-name" => self.ui.get_proxy_name().as_str().into(),
            "proxy-name-locked" => self.ui.get_proxy_name_locked().into(),
            "proxy-name-preference" => self.state.borrow().proxy_name_preference.as_str().into(),
            "pairing-open" => self.ui.get_pairing_open().into(),
            "pairing-failed" => self.ui.get_pairing_failed().into(),
            "rotation" => self.ui.get_rotation().into(),
            "safe-inset-x" => self.ui.get_display_safe_inset_x().into(),
            "safe-inset-y" => self.ui.get_display_safe_inset_y().into(),
            "display-model" => (self
                .ui
                .get_initial_width()
                .max(self.ui.get_initial_height())
                >= 800.)
                .into(),
            "landscape" => (self.ui.get_initial_width() > self.ui.get_initial_height()).into(),
            "metric" => self.ui.get_metric().into(),
            "celsius" => self.ui.get_celsius().into(),
            "editing" => self.ui.get_editing().into(),
            "wifi-connected" => self.ui.get_wifi_connected().into(),
            "pin-open" => self.ui.get_pin_open().into(),
            "pin-pattern" => self.ui.get_pin_pattern().into(),
            "unlock-method" => self.ui.get_unlock_method().into(),
            "proxy-pairing-code" => self.ui.get_proxy_pairing_code().as_str().into(),
            "pin-value" => self.ui.get_pin_value().as_str().into(),
            "pairing-value" => self.ui.get_pairing_value().as_str().into(),
            "editor-value" => self.ui.get_editor_value().as_str().into(),
            "pin-configured" => self.ui.get_pin_configured().into(),
            "screen-locked" => self.ui.get_screen_locked().into(),
            "transport-kind" => self.ui.get_transport_kind().into(),
            "brand" => self.ui.get_vehicle_brand().as_str().into(),
            _ => JsValue::NULL,
        }
    }
}
