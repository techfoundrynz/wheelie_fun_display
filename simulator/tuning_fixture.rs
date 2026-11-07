//! Deterministic virtual WFB profile used by both simulators.
use super::{Dashboard, TuneField};
use slint::{ComponentHandle, Model, ModelRc, VecModel};
use std::{cell::RefCell, rc::Rc};

struct Field {
    label: &'static str,
    value: f64,
    suffix: &'static str,
    decimals: usize,
}
struct Profile {
    rows: Rc<VecModel<TuneField>>,
    fields: Vec<Field>,
    saved: Vec<f64>,
    limit: i32,
    saved_limit: i32,
    pending: bool,
}
fn data() -> Profile {
    let fields = vec![
        Field {
            label: "Throttle strength",
            value: 150.,
            suffix: " A",
            decimals: 0,
        },
        Field {
            label: "Brake strength",
            value: -90.,
            suffix: " A",
            decimals: 0,
        },
        Field {
            label: "Throttle responsiveness",
            value: 0.003,
            suffix: "",
            decimals: 4,
        },
        Field {
            label: "Brake responsiveness",
            value: 0.008,
            suffix: "",
            decimals: 4,
        },
        Field {
            label: "Return response",
            value: 0.008,
            suffix: "",
            decimals: 4,
        },
        Field {
            label: "Wheelie angle",
            value: 30.,
            suffix: "°",
            decimals: 1,
        },
        Field {
            label: "Button action",
            value: 0.,
            suffix: "",
            decimals: 0,
        },
        Field {
            label: "Anti-loop throttle",
            value: 1.,
            suffix: "",
            decimals: 0,
        },
        Field {
            label: "Same angle as button",
            value: 1.,
            suffix: "",
            decimals: 0,
        },
        Field {
            label: "Anti-loop angle",
            value: 30.,
            suffix: "°",
            decimals: 1,
        },
        Field {
            label: "Wheel lift resistance",
            value: 1.5,
            suffix: "",
            decimals: 2,
        },
        Field {
            label: "Throttle / brake tilt",
            value: 1.,
            suffix: "",
            decimals: 0,
        },
        Field {
            label: "Throttle tilt",
            value: 5.,
            suffix: "°",
            decimals: 1,
        },
        Field {
            label: "Brake tilt",
            value: -5.,
            suffix: "°",
            decimals: 1,
        },
        Field {
            label: "Maintain on throttle",
            value: 0.,
            suffix: "",
            decimals: 0,
        },
        Field {
            label: "Maintain on brake",
            value: 0.,
            suffix: "",
            decimals: 0,
        },
        Field {
            label: "Horn strength",
            value: 4.,
            suffix: "",
            decimals: 0,
        },
        Field {
            label: "Horn frequency",
            value: 494.,
            suffix: " Hz",
            decimals: 0,
        },
        Field {
            label: "Angle P",
            value: 30.,
            suffix: "",
            decimals: 1,
        },
        Field {
            label: "Rate P",
            value: 1.,
            suffix: "",
            decimals: 2,
        },
        Field {
            label: "Pitch filter gain",
            value: 2.,
            suffix: "",
            decimals: 2,
        },
        Field {
            label: "Brake angle P",
            value: 1.5,
            suffix: "",
            decimals: 1,
        },
        Field {
            label: "Brake rate P",
            value: 1.5,
            suffix: "",
            decimals: 1,
        },
    ];
    let saved = fields.iter().map(|f| f.value).collect();
    Profile {
        rows: Rc::new(VecModel::default()),
        fields,
        saved,
        limit: 19,
        saved_limit: 19,
        pending: false,
    }
}
fn value(field: &Field, n: f64) -> String {
    match field.label {
        "Button action" => ["Wheelie", "Turbo", "Horn", "Disabled"][n as usize].into(),
        "Anti-loop throttle"
        | "Same angle as button"
        | "Throttle / brake tilt"
        | "Maintain on throttle"
        | "Maintain on brake" => if n != 0. { "Enabled" } else { "Disabled" }.into(),
        _ => format!("{:.*}{}", field.decimals, n, field.suffix),
    }
}
fn preset(values: &[f64]) -> i32 {
    if (values[1] + 90.).abs() > 0.1 {
        return 3;
    }
    if (values[0] - 90.).abs() < 0.1
        && (values[2] - 0.002).abs() < 0.00005
        && (values[3] - 0.005).abs() < 0.00005
    {
        return 0;
    }
    if (values[0] - 150.).abs() < 0.1 && (values[3] - 0.008).abs() < 0.00005 {
        if (values[2] - 0.003).abs() < 0.00005 {
            return 1;
        }
        if (values[2] - 0.015).abs() < 0.00005 {
            return 2;
        }
    }
    3
}
fn show(profile: &Profile, index: usize) -> bool {
    let v = |i: usize| profile.fields[i].value;
    match index {
        16 | 17 => v(6) == 2.,
        5 => v(6) == 0. || (v(7) != 0. && v(8) != 0.),
        14 | 15 => v(6) == 0.,
        _ => true,
    }
}
const RAMPS: [f64; 10] = [
    0.0005, 0.001, 0.002, 0.003, 0.005, 0.008, 0.015, 0.03, 0.06, 1.,
];
const NOTES: [f64; 28] = [
    208., 220., 233., 247., 262., 277., 294., 311., 330., 349., 370., 392., 415., 440., 466., 494.,
    523., 554., 587., 622., 659., 698., 740., 784., 831., 880., 932., 988.,
];
const NOTE_NAMES: [&str; 28] = [
    "G#3", "A3", "A#3", "B3", "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4",
    "A#4", "B4", "C5", "C#5", "D5", "D#5", "E5", "F5", "F#5", "G5", "G#5", "A5", "A#5", "B5",
];
fn section(id: usize) -> i32 {
    match id {
        0..=3 => 0,
        5 | 6 | 14 | 15 => 1,
        7..=13 => 2,
        16 | 17 => 3,
        _ => 4,
    }
}
fn limits(id: usize) -> (f64, f64, f64, f64, i32, i32) {
    match id {
        0 => (0., 9., 1., 9., 1, 3),
        1 => (0., 9., 1., 3., 1, 3),
        2 => (1., 10., 1., 4., 1, 3),
        3 => (1., 10., 1., 6., 1, 3),
        4 => (0.0001, 1., 0.0001, 0.008, 1, 0),
        5 | 9 => (5., 55., 0.5, 30., 10, 3),
        6 => (0., 3., 1., 2., 1, 2),
        7 | 8 | 11 => (0., 1., 1., 1., 1, 1),
        14 | 15 => (0., 1., 1., 0., 1, 1),
        10 => (0., 10., 1., 5., 1, 3),
        12 => (-20., 20., 0.5, 5., 10, 3),
        13 => (-20., 20., 0.5, -5., 10, 3),
        16 => (1., 10., 1., 4., 1, 3),
        17 => (0., 27., 1., 15., 2, 3),
        18 => (0., 40., 0.5, 30., 1, 0),
        19 => (0., 3., 0.05, 1., 1, 0),
        20 => (0., 3., 0.1, 2., 1, 0),
        21 => (0.2, 2., 0.1, 1.5, 1, 0),
        _ => (0., 2., 0.1, 1.5, 1, 0),
    }
}
fn position(id: usize, raw: f64) -> f64 {
    match id {
        0 => (raw - 60.) / 10.,
        1 => (-raw - 60.) / 10.,
        10 => raw / 0.3,
        2 | 3 | 17 => {
            let (values, base): (&[f64], f64) = if id == 17 { (&NOTES, 0.) } else { (&RAMPS, 1.) };
            if raw < values[0] {
                return base - 1.;
            }
            for (i, n) in values.iter().enumerate() {
                if (raw - n).abs() < 1e-8 {
                    return i as f64 + base;
                }
                if raw < *n {
                    return base + i as f64 - 1. + (raw - values[i - 1]) / (n - values[i - 1]);
                }
            }
            base + values.len() as f64
        }
        _ => raw,
    }
}
fn wire(id: usize, position: f64) -> f64 {
    match id {
        0 => 60. + position * 10.,
        1 => -60. - position * 10.,
        10 => position * 0.3,
        2 | 3 => RAMPS[(position.round() as usize).saturating_sub(1).min(9)],
        17 => NOTES[(position.round() as usize).min(27)],
        _ => position,
    }
}
fn disabled(profile: &Profile, id: usize) -> bool {
    let v = |i: usize| profile.fields[i].value;
    match id {
        9 => v(7) == 0. || v(8) != 0.,
        8 => v(7) == 0.,
        12 | 13 => v(11) == 0.,
        _ => false,
    }
}
fn display(profile: &Profile, id: usize, raw: f64) -> String {
    let field = &profile.fields[id];
    match id {
        2 | 3 => {
            let p = position(id, raw);
            if (p - p.round()).abs() < 1e-7 && (1.0..=10.0).contains(&p) {
                if p == 10. {
                    format!("Instant ({raw:.4})")
                } else {
                    format!("{p:.0} / 10 ({raw:.4})")
                }
            } else {
                format!("{raw:.4}")
            }
        }
        10 => format!("{:.1} / 10", raw / 0.3),
        12 | 13 => format!("{raw:+.1}°"),
        17 => NOTES
            .iter()
            .position(|n| (*n - raw).abs() < 1e-7)
            .map(|i| format!("{} ({raw:.0} Hz)", NOTE_NAMES[i]))
            .unwrap_or(format!("{raw:.0} Hz")),
        _ => value(field, raw),
    }
}
fn row(profile: &Profile, id: usize) -> TuneField {
    let f = &profile.fields[id];
    let (minimum, maximum, step, default, tick_every, kind) = limits(id);
    let raw = if id == 9 && profile.fields[8].value != 0. {
        profile.fields[5].value
    } else {
        f.value
    };
    let actual = position(id, raw);
    let read_only = disabled(profile, id);
    let options = if id == 6 {
        vec![
            "Wheelie".into(),
            "Turbo".into(),
            "Horn".into(),
            "Disabled".into(),
        ]
    } else {
        vec![]
    };
    TuneField {
        id: id as i32,
        label: f.label.into(),
        value: display(profile, id, raw).into(),
        saved: display(profile, id, profile.saved[id]).into(),
        can_decrease: !read_only && actual > minimum,
        can_increase: !read_only && actual < maximum,
        changed: (f.value - profile.saved[id]).abs() > 1e-8,
        kind,
        minimum: minimum as f32,
        maximum: maximum as f32,
        position: actual.clamp(minimum, maximum) as f32,
        step: step as f32,
        default_position: default as f32,
        has_default: true,
        read_only,
        off_grid: actual < minimum
            || actual > maximum
            || (((actual - minimum) / step).round() - (actual - minimum) / step).abs() > 1e-6,
        tick_every,
        options: ModelRc::from(Rc::new(VecModel::from(options))),
        read_only_reason: if id == 9 && profile.fields[8].value != 0. {
            "Linked to wheelie angle"
        } else if id == 8 || id == 9 {
            "Enable anti-loop to edit"
        } else {
            "Enable throttle / brake tilt to edit"
        }
        .into(),
    }
}
fn project(ui: &Dashboard, profile: &Profile, drafts: bool) {
    ui.set_tuning_ready(true);
    ui.set_tuning_busy(false);
    ui.set_tuning_dirty(
        profile.limit != profile.saved_limit
            || profile
                .fields
                .iter()
                .zip(&profile.saved)
                .any(|(f, s)| (f.value - s).abs() > 1e-8),
    );
    ui.set_tuning_saved_response(
        ["Eco", "Cruise", "Sport", "Custom"][preset(&profile.saved) as usize].into(),
    );
    ui.set_tuning_unlimited(true);
    ui.set_tuning_saved_limit(if profile.saved_limit == 0 {
        "No limit".into()
    } else {
        format!("{} mph", profile.saved_limit).into()
    });
    if drafts {
        ui.set_draft_mode(preset(
            &profile.fields.iter().map(|f| f.value).collect::<Vec<_>>(),
        ));
        ui.set_draft_limit(profile.limit);
    }
    let rows = profile
        .fields
        .iter()
        .enumerate()
        .filter(|(i, _)| section(*i) == ui.get_tuning_section() && show(profile, *i))
        .map(|(i, _)| row(profile, i))
        .collect::<Vec<_>>();
    for (i, row) in rows.into_iter().enumerate() {
        if i < profile.rows.row_count() {
            profile.rows.set_row_data(i, row);
        } else {
            profile.rows.push(row);
        }
    }
    let count = profile
        .fields
        .iter()
        .enumerate()
        .filter(|(i, _)| section(*i) == ui.get_tuning_section() && show(profile, *i))
        .count();
    while profile.rows.row_count() > count {
        profile.rows.remove(count);
    }
    ui.set_tuning_fields(ModelRc::from(profile.rows.clone()));
}
fn prepare(ui: &Dashboard, profile: &mut Profile) {
    if !ui.get_connected() || !ui.get_can_configure() || !ui.get_tuning_dirty() {
        return;
    }
    let mut summary = String::from("Save these settings to the bike:\n");
    for (i, f) in profile.fields.iter().enumerate() {
        if (f.value - profile.saved[i]).abs() > 1e-8 {
            summary.push_str(&format!(
                "{}: {} to {}\n",
                f.label,
                value(f, profile.saved[i]),
                value(f, f.value)
            ));
        }
    }
    if profile.limit != profile.saved_limit {
        if profile.limit == 0 {
            summary.push_str("Speed limit: No limit\n");
        } else {
            summary.push_str(&format!("Speed limit: {} mph\n", profile.limit));
        }
    }
    ui.set_confirm_text(summary.into());
    profile.pending = true;
    ui.set_confirming(true);
}
pub fn configure(ui: &Dashboard) {
    let profile = Rc::new(RefCell::new(data()));
    ui.set_tuning_status("Bike profile read. Changes remain drafts until saved.".into());
    project(ui, &profile.borrow(), true);
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_tuning_section_requested(move |section| {
        let ui = weak.unwrap();
        ui.set_tuning_section(section.clamp(0, 4));
        project(&ui, &state.borrow(), false);
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_tuning_adjust(move |id, delta| {
        let ui = weak.unwrap();
        if !ui.get_connected() || !ui.get_tuning_ready() || (delta != -1 && delta != 1) {
            return;
        }
        let mut p = state.borrow_mut();
        let id = id as usize;
        if id < p.fields.len() && !disabled(&p, id) {
            let (min, max, step, _, _, _) = limits(id);
            let current = position(id, p.fields[id].value);
            let next = if delta > 0 {
                ((current - min) / step + 1e-7).floor() + 1.
            } else {
                ((current - min) / step - 1e-7).ceil() - 1.
            };
            p.fields[id].value = wire(id, (min + next * step).clamp(min, max));
        }
        project(&ui, &p, true);
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_tuning_set(move |id, value| {
        let ui = weak.unwrap();
        let mut p = state.borrow_mut();
        let id = id as usize;
        if !ui.get_connected() || !ui.get_tuning_ready() || id >= p.fields.len() || disabled(&p, id)
        {
            return;
        }
        let (min, max, step, _, _, _) = limits(id);
        let value = f64::from(value);
        if value < min
            || value > max
            || (((value - min) / step).round() - (value - min) / step).abs() > 1e-5
        {
            return;
        }
        p.fields[id].value = wire(id, value);
        project(&ui, &p, true);
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_tuning_reset(move || {
        let ui = weak.unwrap();
        let mut p = state.borrow_mut();
        for i in 0..p.fields.len() {
            p.fields[i].value = p.saved[i];
        }
        p.limit = p.saved_limit;
        project(&ui, &p, true);
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_tuning_refresh(move || {
        let ui = weak.unwrap();
        let mut p = state.borrow_mut();
        for i in 0..p.fields.len() {
            p.fields[i].value = p.saved[i];
        }
        p.limit = p.saved_limit;
        ui.set_tuning_status("Bike profile read. Changes remain drafts until saved.".into());
        project(&ui, &p, true);
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_tuning_save(move || {
        let ui = weak.unwrap();
        prepare(&ui, &mut state.borrow_mut());
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_apply_mode(move |mode| {
        let ui = weak.unwrap();
        if !(0..=2).contains(&mode) {
            return;
        }
        let mut p = state.borrow_mut();
        p.fields[0].value = if mode == 0 { 90. } else { 150. };
        p.fields[1].value = -90.;
        p.fields[2].value = if mode == 0 {
            0.002
        } else if mode == 2 {
            0.015
        } else {
            0.003
        };
        p.fields[3].value = if mode == 0 { 0.005 } else { 0.008 };
        project(&ui, &p, true);
        prepare(&ui, &mut p);
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_apply_limit(move |limit| {
        let ui = weak.unwrap();
        if limit != 0 && !(5..=35).contains(&limit) {
            return;
        }
        let mut p = state.borrow_mut();
        p.limit = limit;
        project(&ui, &p, true);
        prepare(&ui, &mut p);
    });
    let weak = ui.as_weak();
    let state = profile.clone();
    ui.on_cancel_change(move || {
        state.borrow_mut().pending = false;
        weak.unwrap().set_confirming(false);
    });
    let weak = ui.as_weak();
    ui.on_confirm_change(move || {
        let ui = weak.unwrap();
        let mut p = profile.borrow_mut();
        if p.pending && ui.get_connected() && ui.get_can_configure() {
            p.saved = p.fields.iter().map(|f| f.value).collect();
            p.saved_limit = p.limit;
            ui.set_tuning_status("Saved profile verified by readback.".into());
            ui.set_feedback("Saved profile verified by readback.".into());
        }
        p.pending = false;
        ui.set_confirming(false);
        project(&ui, &p, true);
    });
}
