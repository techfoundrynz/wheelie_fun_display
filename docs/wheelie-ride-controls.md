# Wheelie — ride customisation controls

Reference for every user-facing control that changes how the bike rides: where
it lives, what kind of control it is, and its exact label, range, step and
default.

## Use in Wheelie Fun Display

This supplied reference informs the WFB tuning interface. The display uses
Response, Button, Throttle and Horn groups, with discrete sliders for
rider adjustments, a button-action picker and toggles. The Advanced section,
including return response and PID gains, is hidden to keep the available
controls aligned with the app. The runtime bike schema can narrow every documented range;
readback values outside a slider's stops are preserved until explicitly edited.
The amber marker identifies the reference default, rather than the bike's saved
value. A separate readback label shows what is actually stored on the bike.

The display deliberately uses one consistent, explicit confirmation and readback
flow for all writes. Slider dragging never sends changes directly. Angle sliders
and their buttons both cover the full documented range, rather than using
different drag and button ranges. Linked
anti-loop angles remain visible and disabled. Coasting brake and the inactive
wheelie-mode choices are omitted because this reference describes no effective
transmitted change for them.

The app and stored defaults differ; defaults in this reference must never
replace a bike's reported configuration on connection. See the
[display user guide](user-guide.md#tune) for current interaction behaviour.

---

## 1. Where the controls live

Five editors, each a separate modal with its own save behaviour.

| Editor | Opened from | Control pattern | Command |
|---|---|---|---|
| **Ride Mode** | Dashboard mode button | 4 preset tiles, applied on tap | `BIKE_RIDE_CONFIGURE` (100) |
| **Custom Tuning** | "Custom" preset inside Ride Mode | 4 discrete sliders + preset shortcuts | `BIKE_RIDE_CONFIGURE` (100) |
| **Features** | Dashboard features card | 3 tabs: sliders, toggles, checkboxes, picker | `BIKE_FEATURE_CONFIGURE` (102) |
| **Speed Limit** | Dashboard speed-limit sign | Single slider with a "No Limit" stop | `BIKE_SPEED_LIMIT_CONFIGURE` (103) |
| **Wheelie Mode** | Dashboard wheelie button | 3 tiles (see §6) | — |

All five are translucent full-screen modals with a tap-to-dismiss scrim, a
close button, and hardware-back wired to the same close guard.

---

## 2. Ride Mode

**Title:** "Select a Ride Mode"

Four tiles. Selecting one applies immediately — no save step. The modal shows
a loading state while the write is in flight.

| id | Label | Icon | Colour | Description |
|---|---|---|---|---|
| `sport` | Sport | `S` | `#AC0E0E` | High performance mode with near-instant throttle response |
| `cruise` | Cruise | `C` | `#0E77AC` | Smooth, predictable power delivery suitable for most riders |
| `eco` | Eco | `E` | `#198E13` | Limited acceleration and speed for a more relaxed and efficient ride |
| `custom` | Custom | — | `#777777` | Opens the Custom Tuning editor |

Values behind each preset:

| id | Max current | Max brake current | Throttle ramp α | Brake ramp α |
|---|---|---|---|---|
| sport | 150 A | −90 A | 0.015 | 0.008 |
| cruise | 150 A | −90 A | 0.003 | 0.008 |
| eco | 90 A | −90 A | 0.002 | 0.005 |
| custom | 150 A | −90 A | 0.003 | 0.008 |

Sport and Cruise differ only in throttle ramp and have identical braking.
Eco uses the same brake-current limit but a different brake ramp. Custom is
seeded from Cruise.

### Mode detection on connect

The app reads the live configuration and matches it against each preset,
comparing all four values with a tolerance of 0.001. If nothing matches it
displays a fifth, non-selectable state:

> **Unknown** — "Configuration doesn't match any known mode"

This is how the dashboard shows the current mode without the firmware
reporting a mode ID.

---

## 3. Custom Tuning

**Title:** "Custom Tuning"

A preset shortcut row, then four discrete sliders in two titled groups.

### Preset shortcuts

Label "Preset" followed by three 48 px circular buttons (`S`, `C`, `E`).
Tapping one loads that preset's values into the sliders, after a confirmation:

| Situation | Title | Body | Buttons |
|---|---|---|---|
| Switching to a different preset with unsaved edits | **Overwrite Custom Values?** | Changing presets will overwrite your current custom values. Are you sure you want to continue? | Cancel · **Overwrite** |
| Re-tapping the preset already selected | **Reset to Preset Values?** | This will reset your custom values to the preset defaults. Are you sure you want to continue? | Cancel · **Reset** |

### Sliders

All four use the same control: a 10-position discrete track with tick marks, a
value pill in the header, and a small circular reset button beside the label
that restores that one slider to its preset value.

**Throttle** group:

| Label | End labels | Positions | Step | Effect |
|---|---|---|---|---|
| Strength | WEAK / STRONG | 0–9 | 1 | Max current `60 + v × 10` → **60–150 A** |
| Responsiveness | SLOW / QUICK | 1–10 | 1 | Throttle ramp α (see table below) |

**Brakes** group: same two controls, same labels and ranges, driving max brake
current and brake ramp α.

Responsiveness positions map to ramp α:

| Position | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| α | 0.0005 | 0.001 | 0.002 | 0.003 | 0.005 | 0.008 | 0.015 | 0.03 | 0.06 | **1.0** |

Position 10 is a dedicated "instant" setting rather than a continuation of the
curve, so there is a deliberate jump between positions 9 and 10.

Current resolution on the wire is 10 A per position, which is why the strength
slider has exactly ten stops across 60–150 A.

### Saving

A save bar at the bottom commits all four values. Success is confirmed in the
UI; failure shows an alert: "Failed to save custom tuning settings."

Closing with unsaved edits raises a three-option prompt:

> **Unsaved Changes** — You have unsaved changes. What would you like to do?
> Cancel · **Discard Changes** · Save

---

## 4. Features

The largest editor: a modal with three tabs. Each tab has its own
**"Reset to Defaults"** button affecting only that tab's fields. A single save
commits everything regardless of which tab is open.

Tabs: **Button** · **Throttle** · **Brakes**

### How the sliders work

Angle and value sliders here share one behaviour worth knowing: **the drag
range and the button range differ.** Dragging the track covers a narrow band
around the centre; the `−` and `+` buttons either side reach considerably
further and support press-and-hold repeat. A vertical line marks the default
position, the current value renders large above the track, and when a value has
been changed the previous value appears beneath it in amber.

### Button tab

| Control | Type | Label | Options / range | Default |
|---|---|---|---|---|
| Button function | Picker | "Select Button Function" | Wheelie · Turbo · Horn · Disabled | Wheelie |
| Wheelie angle | Angle slider | `Wheelie Angle:` | centre 30°; drag ±10°, buttons ±25° → **5–55°** | 30° |
| Maintain wheelie when held | Two checkboxes | `Maintain Wheelie When Held:` | `Throttle`, `Brake` | both off |

Button function values: Wheelie triggers a wheelie at any time; Turbo
accelerates as fast as possible while maintaining the current angle; Horn
triggers a horn sound through motor haptics; Disabled assigns no action.

### Throttle tab

| Control | Type | Label | Range | Default |
|---|---|---|---|---|
| Tilt enable | Toggle | `Throttle / Brake Tilt` | on / off | on |
| Throttle tilt | Angle slider | `Throttle Tilt:` | drag ±10, buttons **−20 … +20** | +5 |
| Brake tilt | Angle slider | `Brake Tilt:` | drag ±10, buttons **−20 … +20** | −5 |
| Anti-loop enable | Toggle | `Anti-Loop Throttle` | on / off | on |
| Anti-loop angle | Angle slider | `Anti-Loop Angle:` | centre 30°; drag ±10°, buttons ±25° → **5–55°** | 30° |
| Same angle as button | Checkbox, inside the anti-loop control | `Same Angle as Button` | on / off | on |
| Wheel lift resistance | Value slider | `Wheel Lift Resistance:` | **0 – 3.0**, step **0.3**, shown as **0–10** | 1.5 (displays as 5) |

Both tilt sliders show a signed value (`+5`, `−5`) and a default-position
marker. With tilt enabled, the bike's angle in Wheelie mode can be modulated
with throttle and regen brake while the button is held, within the configured
tilt angles.

"Same Angle as Button" makes anti-loop reference the wheelie button's
configured angle instead of its own; the anti-loop slider stays visible but is
driven by that value.

Wheel lift resistance is stored 0–3.0 and displayed on a 0–10 scale
(×3.3333), with tick marks every 0.3.

### Brakes tab

| Control | Type | Label | Range | Default |
|---|---|---|---|---|
| Coasting brake strength | Value slider | `Coasting Brake Strength:` | **0–10**, step 1, displayed as **0–100 %** | 0 |

The remainder of the tab is a dimmed **"Coming Soon"** panel. Coasting brake
strength is stored with the local configuration; it is not part of the
transmitted feature payload in this version.

### Horn settings

A `Horn Settings` section, relevant when the button function is set to Horn.
The horn is produced through motor haptics rather than a speaker.

| Control | Label | Range | Step | Default |
|---|---|---|---|---|
| Strength | `Horn Strength:` | **1–10** | 1 | 4 |
| Frequency | `Horn Frequency:` | **0–27** (note index) | 1 | 15 |

Frequency displays musical note names, not numbers, with tick marks every
2 positions. The 28-entry table spans G#3 to B5:

```
0  G#3 208   7  D#4 311   14 A#4 466   21 F5  698
1  A3  220   8  E4  330   15 B4  494   22 F#5 740
2  A#3 233   9  F4  349   16 C5  523   23 G5  784
3  B3  247   10 F#4 370   17 C#5 554   24 G#5 831
4  C4  262   11 G4  392   18 D5  587   25 A5  880
5  C#4 277   12 G#4 415   19 D#5 622   26 A#5 932
6  D4  294   13 A4  440   20 E5  659   27 B5  988
```

Default index 15 is B4 = 494 Hz. The firmware accepts 200–1000 Hz, which
brackets the table exactly.

### Saving

Success shows a **"Features Saved"** confirmation for 2 seconds. Failure shows
an alert: "Failed to save feature settings."

Closing with unsaved edits:

> **Unsaved Changes** — You have unsaved changes. Are you sure you want to
> discard them?
> Cancel · **Discard Changes**

---

## 5. Speed Limit

A single slider with a stop past the top end.

| Property | Value |
|---|---|
| Minimum | 5 mph |
| Maximum | 35 mph |
| Step | 1 mph |
| Default | 20 mph |
| Extra position | **No Limit** |

The slider has 32 positions: the first 31 cover 5–35 mph in 1 mph increments,
and the last is **"No Limit"**, which clears the limit on the bike. Labels
read `<n> mph` or `No Limit`.

The dashboard displays the active value in a highway-style `SPEED LIMIT` sign,
which switches to a no-limit graphic when the limit is cleared.

Saving is debounced by 300 ms. Success shows a **"Speed Limit Saved"**
confirmation for 2 seconds. Failure shows **"Save Failed" — "Failed to save
speed limit settings."** with an `Ok` button. Closing with unsaved edits raises
the same two-option discard prompt as the Features editor.

---

## 6. Wheelie Mode

**Title:** "Select a Wheelie Mode"

| id | Label | Icon | Colour | Availability | Description |
|---|---|---|---|---|---|
| `agro` | Agro | `3` | `#AC0E0E` | Disabled | Stiff, locked in response; sticks to set wheelie angle at all costs |
| `standard` | Standard | `2` | `#0E77AC` | Available | Secure feeling wheelie with some give for a smooth, fun ride |
| `float` | Float | `1` | `#198E13` | Disabled | Soft, loose wheelie for a more floaty feel |

Two of the three options are disabled in this version, and selecting a wheelie
mode does not currently change any tuning on the bike.

The underlying wheelie PID parameters — angle P, rate P, Mahony P, brake angle
P, brake rate P and lift-resistance rate P — are part of the bike's stored
configuration but have no control surface in the app.

---

## 7. Stored configuration parameters

The parameters below live in the bike's persisted configuration. Where a
control exists, it is named; the rest are configuration-only in this version.

### Wheelie PID

| Parameter | Range | Default | Control |
|---|---|---|---|
| Angle P | 0 – 40 | 30 | — |
| Rate P | 0 – 3 | 1 | — |
| Mahony P | 0 – 3 | 2 | — |
| Brake angle P | 0.2 – 2 | 1.5 | — |
| Brake rate P | 0 – 2 | 1.5 | — |
| Lift resistance rate P | 0 – 3 | 1.5 | Wheel Lift Resistance (Throttle tab) |

Rate P responds to the nose's angular velocity independently of the setpoint
and acts as the damping term against Angle P. Recommended working range is
0.1–0.6; higher values up to 1.2 are usable but bring side effects. Angle P and
Rate P are best scaled together to preserve ride feel.

### Motor limits and ramps

| Parameter | Range | Default | Control |
|---|---|---|---|
| Max current | 0 – 150 A | 150 | Custom Tuning → Throttle Strength |
| Max brake current | −150 – 0 A | −90 | Custom Tuning → Brakes Strength |
| Throttle ramp α | 0.0001 – 1 | 0.003 | Custom Tuning → Throttle Responsiveness |
| Brake ramp α | 0.0001 – 1 | 0.008 | Custom Tuning → Brakes Responsiveness |
| Return ramp α | 0.0001 – 1 | 0.008 | — |
| Balance pitch | 5 – 55 | 30 | — |

### Button, tilt and anti-loop

| Parameter | Range | Default | Control |
|---|---|---|---|
| Button function | Wheelie / Turbo / Horn / Disabled | Horn | Button tab picker |
| Throttle/brake tilt enabled | on / off | on | Throttle tab toggle |
| Throttle tilt angle limit | −20 – 20 | 5 | `Throttle Tilt:` |
| Brake tilt angle limit | −20 – 20 | −5 | `Brake Tilt:` |
| Persist wheelie on throttle | on / off | off | `Maintain Wheelie When Held:` → Throttle |
| Persist wheelie on brake | on / off | off | `Maintain Wheelie When Held:` → Brake |
| Anti-loop enabled | on / off | on | `Anti-Loop Throttle` |
| Use button angle for anti-loop | on / off | on | `Same Angle as Button` |
| Anti-loop angle | 5 – 55 | 30 | `Anti-Loop Angle:` |

### Horn and limit

| Parameter | Range | Default | Control |
|---|---|---|---|
| Horn strength | 0 – 100 | 4 | `Horn Strength:` (1–10) |
| Horn frequency | 200 – 1000 Hz | 494 | `Horn Frequency:` (note index 0–27) |
| Speed limit | 0 – 35 mph | 19 | Speed Limit modal (5–35 or No Limit) |

Note that the app's own default speed limit (20 mph) and default button
function (Wheelie) differ from the stored configuration defaults listed here
(19 mph, Horn); the stored values take effect on a bike that has never been
configured from the app.

---

## 8. Interaction summary

**Three save models:**

1. **Immediate** — Ride Mode presets apply on tap.
2. **Explicit** — Features and Custom Tuning stage edits behind a save bar.
3. **Debounced** — Speed Limit commits 300 ms after the last change.

**Reset at three levels:**

- Per-slider — small circular button beside the label, restores that control's
  preset or default value.
- Per-tab — "Reset to Defaults" in the Features editor.
- Per-editor — re-tapping the active preset in Custom Tuning.

**Confirmation** appears only where edits would be lost or overwritten: the two
preset prompts in Custom Tuning, a two-option discard in Features and Speed
Limit, and a three-option save/discard/cancel when closing Custom Tuning.

**Feedback** is a 2-second success confirmation on save, and a blocking alert
on failure.

**Edit affordances** across the sliders: discrete tick marks, a default-position
line, amber previous-value hints on changed rows, and ± buttons that reach
beyond the drag range.
