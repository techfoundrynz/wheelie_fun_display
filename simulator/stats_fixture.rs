//! Shared, illustrative controller and 13S BMS data matching firmware Stats rows.
use crate::{Dashboard, StatItem};
use slint::{Model, ModelRc, VecModel};
use std::rc::Rc;

fn row(label: &str, value: impl Into<slint::SharedString>) -> StatItem {
    StatItem {
        label: label.into(),
        value: value.into(),
    }
}
fn model(rows: Vec<StatItem>) -> ModelRc<StatItem> {
    ModelRc::from(Rc::new(VecModel::from(rows)))
}
fn update(model: &ModelRc<StatItem>, index: usize, value: impl Into<slint::SharedString>) {
    if let Some(mut item) = model.row_data(index) {
        let value = value.into();
        if item.value != value {
            item.value = value;
            model.set_row_data(index, item);
        }
    }
}
fn temperature(ui: &Dashboard, degrees: f32) -> String {
    let (value, unit) = if ui.get_celsius() {
        (degrees, "°C")
    } else {
        (degrees * 1.8 + 32., "°F")
    };
    format!("{value:.1} {unit}")
}

pub fn project(ui: &Dashboard, live: bool) {
    const CONTROLLER_LABELS: [&str; 13] = [
        "Duty cycle",
        "Electrical RPM",
        "Consumed charge",
        "Regenerated charge",
        "Consumed energy",
        "Regenerated energy",
        "Signed distance",
        "PID position",
        "Controller ID",
        "Controllers",
        "Remaining energy",
        "Controller uptime",
        "Fault code",
    ];
    if ui.get_vesc_stats().row_count() == 0 {
        ui.set_vesc_stats(model(
            CONTROLLER_LABELS
                .into_iter()
                .map(|label| row(label, "--"))
                .collect(),
        ));
    }
    if !live {
        for index in 0..CONTROLLER_LABELS.len() {
            update(&ui.get_vesc_stats(), index, "--");
        }
        if ui.get_bms_stats().row_count() > 0 {
            ui.set_bms_stats(ModelRc::default());
        }
        if ui.get_bms_cells().row_count() > 0 {
            ui.set_bms_cells(ModelRc::default());
        }
        if ui.get_bms_temperatures().row_count() > 0 {
            ui.set_bms_temperatures(ModelRc::default());
        }
        ui.set_bms_state("No recent VESC BMS data.".into());
        return;
    }
    if ui
        .get_vesc_stats()
        .row_data(2)
        .is_some_and(|item| item.value == "--")
    {
        for (index, value) in [
            "55.7%",
            "12690 eRPM",
            "1.240 Ah",
            "0.160 Ah",
            "62.5 Wh",
            "8.1 Wh",
            "12.20 km",
            "123.4°",
            "7",
            "1",
            "743.9 Wh",
            "2538 s",
            "0",
        ]
        .into_iter()
        .enumerate()
        {
            update(&ui.get_vesc_stats(), index, value);
        }
    }
    if ui.get_bms_stats().row_count() == 0 {
        ui.set_bms_stats(model(vec![
            row("Cells", "13"),
            row("Temperature sensors", "3"),
            row("Balancing cells", "2"),
            row("Pack voltage", "50.40 V"),
            row("Charge voltage", "54.60 V"),
            row("Input current", "12.70 A"),
            row("IC input current", "12.66 A"),
            row("Charge counter", "1.240 Ah"),
            row("Energy counter", "62.5 Wh"),
            row("State of charge", "82.0%"),
            row("State of health", "98.0%"),
            row("BMS CAN ID", "17"),
            row("IC temperature", "32.4 °C"),
            row("Humidity sensor", "28.6 °C"),
            row("Humidity", "42.0%"),
            row("Maximum cell temp", "31.2 °C"),
            row("Lifetime charge in", "338.42 Ah"),
            row("Lifetime energy in", "17056.3 Wh"),
            row("Lifetime charge out", "331.21 Ah"),
            row("Lifetime energy out", "16692.9 Wh"),
            row("Pressure", "101325 Pa"),
            row("Status", "Discharging"),
            row("Lowest cell", "3.872 V"),
            row("Highest cell", "3.884 V"),
            row("Cell spread", "12 mV"),
        ]));
        // Sum: exactly 50.400 V. Min/max: 3.872/3.884 V. Cells 2 and 8 balance.
        let millivolts = [
            3874, 3884, 3876, 3878, 3875, 3877, 3872, 3883, 3874, 3876, 3878, 3877, 3876,
        ];
        ui.set_bms_cells(model(
            millivolts
                .into_iter()
                .enumerate()
                .map(|(index, mv)| {
                    let balancing = if index == 1 || index == 7 {
                        " | balancing"
                    } else {
                        ""
                    };
                    row(
                        &format!("Cell {}{balancing}", index + 1),
                        format!("{:.3} V", mv as f32 / 1000.),
                    )
                })
                .collect(),
        ));
        ui.set_bms_temperatures(model(vec![
            row("Sensor 1", "31.2 °C"),
            row("Sensor 2", "30.8 °C"),
            row("Sensor 3", "31.0 °C"),
        ]));
    }
    // Mutate rows in place so live telemetry does not recreate scrolling lists.
    let speed = ui.get_speed().as_str().parse::<f32>().unwrap_or_default();
    let mps = speed / if ui.get_metric() { 3.6 } else { 2.236_936_3 };
    let charge = ui
        .get_battery()
        .trim_end_matches('%')
        .parse::<f32>()
        .unwrap_or_default()
        .clamp(0., 100.);
    let current = ui
        .get_battery_current()
        .trim_end_matches(" A")
        .parse::<f32>()
        .unwrap_or_default();
    update(
        &ui.get_vesc_stats(),
        0,
        format!("{:.1}%", (mps * 8.085).clamp(0., 95.)),
    );
    update(
        &ui.get_vesc_stats(),
        1,
        format!("{:.0} eRPM", mps / 0.798 * 60. * 3.5 * 7.),
    );
    update(
        &ui.get_vesc_stats(),
        6,
        if ui.get_metric() {
            "12.20 km"
        } else {
            "7.58 mi"
        },
    );
    update(
        &ui.get_vesc_stats(),
        10,
        format!("{:.1} Wh", charge * 0.18 * 50.4),
    );
    update(&ui.get_bms_stats(), 5, format!("{current:.2} A"));
    update(&ui.get_bms_stats(), 6, format!("{:.2} A", current * 0.997));
    update(&ui.get_bms_stats(), 9, format!("{charge:.1}%"));
    update(&ui.get_bms_stats(), 12, temperature(ui, 32.4));
    update(&ui.get_bms_stats(), 13, temperature(ui, 28.6));
    update(&ui.get_bms_stats(), 15, temperature(ui, 31.2));
    update(
        &ui.get_bms_stats(),
        21,
        if current > 0.05 {
            "Discharging"
        } else if current < -0.05 {
            "Charging"
        } else {
            "Idle"
        },
    );
    for (index, degrees) in [31.2, 30.8, 31.0].into_iter().enumerate() {
        update(&ui.get_bms_temperatures(), index, temperature(ui, degrees));
    }
    ui.set_bms_state("Recent controller reply | cached BMS values".into());
}
