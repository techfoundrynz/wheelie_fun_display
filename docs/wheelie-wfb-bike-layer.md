# WFB bike layer — app ID 102, sub-commands 100–103, and `bike_config`

Companion to `wheelie-vesc-comms-api.md`. Everything here is the vendor-specific
layer that sits on top of `COMM_CUSTOM_APP_DATA`.

---

## 1. Frame shape

Every bike command is one VESC packet:

```
0x02  len  36  102  <sub>  <payload…>  crc16  0x03
       │    │   │     │
       │    │   │     └── FloatPacketTypes sub-command
       │    │   └────────  AppPacketTypes.WFB = 102
       │    └────────────  COMM_CUSTOM_APP_DATA = 36
       └─────────────────  length byte
```

`getVescAppData` reads bytes 0 and 1 of the payload and **returns `null` unless
byte 0 is 102**. Stock Float package boards advertise 101, so this app is a
closed loop with its own firmware package — it won't read a Float or Refloat
board even though it understands the config format.

Two send helpers exist:

- `writeData(...bytes)` — fire and forget, used by the three configure commands.
- `getData(...bytes)` — writes and awaits a matching response, used for polling.

Both prepend `[COMM_FORWARD_CAN, canId]` automatically when
`_primaryCANDeviceId` is set.

---

## 2. Sub-command 100 — `BIKE_RIDE_CONFIGURE`

Sets the four motor-response parameters as a **3-byte packed payload**. Full
frame is 6 bytes of payload:

```
36  102  100  b0  b1  b2
```

Built by `createRideModeCommand(maxCurrent, maxCurrentBrake, rampAlphaPos, rampAlphaNeg)`:

| Byte | Content |
|---|---|
| `b0` low nibble | `clamp(round(maxCurrent / 10), 0, 15)` |
| `b0` high nibble | `clamp(round(abs(maxCurrentBrake) / 10), 0, 15)` |
| `b1` | `rampAlphaToByte(rampAlphaPos)` |
| `b2` | `rampAlphaToByte(rampAlphaNeg)` |

So currents are transmitted in **10 A units, 4 bits each**, brake current as an
absolute value in the high nibble. Range on the wire is 0–150 A in 10 A steps —
which is exactly why `max_current` in the config table tops out at 150.

### `rampAlphaToByte(a)` — piecewise non-linear quantisation

A single byte covering four orders of magnitude, with finer resolution at the
low (smoother) end:

| Input range | Byte value | Resolution |
|---|---|---|
| `a <= 0.0001` | `0` | — |
| `0.0001 < a <= 0.001` | `round((a − 0.0001) / 0.0001)` → 0–9 | 0.0001 |
| `0.001 < a <= 0.01` | `round(10 + (a − 0.001) / 0.0005)` → 10–28 | 0.0005 |
| `0.01 < a <= 0.1` | `round(28 + (a − 0.01) / 0.001)` → 28–118 | 0.001 |
| `0.1 < a <= 0.5` | `round(118 + (a − 0.1) / 0.005)` → 118–198 | 0.005 |
| `0.5 < a < 1` | `round(198 + (a − 0.5) / 0.01)` → 198–248 | 0.01 |
| `a >= 1` | `248` | — |

The bands join cleanly (0.001→10, 0.01→28, 0.1→118, 0.5→198, 1.0→248), so this
is a deliberate companding curve, not accumulated drift. Byte values 249–255
are unreachable.

### Ride mode presets

`RIDE_MODE_PARAMS`:

| id | maxCurrent | maxCurrentBrake | rampAlphaPos | rampAlphaNeg |
|---|---|---|---|---|
| `sport` | 150 | −90 | 0.015 | 0.008 |
| `cruise` | 150 | −90 | 0.003 | 0.008 |
| `eco` | 90 | −90 | 0.002 | 0.005 |
| `custom` | 150 | −90 | 0.003 | 0.008 |

`sport` and `cruise` differ only in throttle ramp; braking is identical across
all four presets. `custom` is seeded from `cruise`.

UI metadata: Sport `#AC0E0E` "High performance mode with near-instant throttle
response", Cruise `#0E77AC` "Smooth, predictable power delivery suitable for
most riders", Eco `#198E13` "Limited acceleration and speed for a more relaxed
and efficient ride", Custom `#777777`, plus an `unknown` pseudo-mode
("Configuration doesn't match any known mode").

### Custom mode: slider → parameter mapping

When the user picks Custom, the four values come from sliders:

```
maxCurrent      =  60 + throttleCurrent * 10
maxCurrentBrake = −(60 + brakeCurrent   * 10)
rampAlphaPos    =  throttleRamp === 10 ? 1 : RAMP_VALUES[throttleRamp − 1]
rampAlphaNeg    =  brakeRamp    === 10 ? 1 : RAMP_VALUES[brakeRamp    − 1]
```

`RAMP_VALUES = [0.0005, 0.001, 0.002, 0.003, 0.005, 0.008, 0.015, 0.03, 0.06, 0.1]`

Slider position 10 short-circuits to `rampAlpha = 1` (instant response,
byte 248) rather than reading `RAMP_VALUES[9] = 0.1`. **Index 9 of the table is
therefore dead** — position 9 gives 0.06 and position 10 jumps straight to 1.0.
Whether that's intentional ("10 = instant") or an off-by-one is unclear from
the bytecode; the effect is a large discontinuity at the top of the slider.

### Reverse mapping — `determineInitialRideMode`

On connect, the app reads the live config and tries to match it back to a
preset using `isCloseEnough(a, b, epsilon = 0.001)` on each of the four values.
No match → the `unknown` pseudo-mode. A match on `custom`'s values →
`'custom'`. This is how the UI shows which mode a board is currently in without
the firmware reporting a mode ID.

Logging is verbose here: `"Sending ride mode command for <name>:"` with the
resolved values and `commandBytes`, then `"Successfully sent ride mode
configuration for <name>"`. `"Cannot select unknown mode"` throws if the
`unknown` pseudo-mode is somehow selected.

---

## 3. Sub-command 101 — `BIKE_WHEELIE_CONFIGURE`

**Defined but not transmitted by app version 1.0.0.** A supported payload
encoding for this sub-command is not established here.

The associated UI exists but is largely disabled. `WHEELIE_MODES`:

| id | name | icon | colour | disabled |
|---|---|---|---|---|
| `agro` | Agro | `3` | `#AC0E0E` | **true** |
| `standard` | Standard | `2` | `#0E77AC` | false |
| `float` | Float | `1` | `#198E13` | **true** |

Descriptions: Agro — "Stiff, locked in response; sticks to set wheelie angle at
all costs"; Standard — "Secure feeling wheelie with some give for a smooth, fun
ride"; Float — "Soft, loose wheelie for a more floaty feel".

The mode picker includes selection and cancellation state, but two options
are disabled and no transmit path is documented. These controls do not establish
a supported sub-command 101 payload. Separate `WHEELIE_CONFIG_SIGNATURES` and
`isWheelieConfig` identifiers suggest another config profile may have been
planned; that remains unconfirmed.

Wheelie *tuning* in this build goes through the ordinary
`COMM_SET_CUSTOM_CONFIG` path instead (the `WHEELIE_KP*` / `MAHONY_KP` params in
§5), not through sub-command 101.

---

## 4. Sub-command 102 — `BIKE_FEATURE_CONFIGURE`

The big one: **13 payload bytes**, all scalar, no length prefix. Full frame
payload is 16 bytes.

```
36  102  102  b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 b10 b11 b12
```

| # | Field | Encoding | Wire range |
|---|---|---|---|
| 0 | `selectedFunction` | `{wheelie:0, turbo:1, horn:2, disabled:3}`, unknown → 0, then `min(x, 3)` | 0–3 |
| 1 | `wheelieAngle` | `clamp(round(deg × 2), 10, 110)` | 10–110 (5–55°) |
| 2 | `throttleBrakeTilt` | `bool ? 1 : 0` | 0/1 |
| 3 | `throttleTiltAngle` | `clamp(round((deg + 20) × 2), 0, 80)` | 0–80 (−20…+20°) |
| 4 | `brakeTiltAngle` | `clamp(round((deg + 20) × 2), 0, 80)` | 0–80 (−20…+20°) |
| 5 | `persistWheelieThrottle` | `bool ? 1 : 0` | 0/1 |
| 6 | `persistWheelieBrake` | `bool ? 1 : 0` | 0/1 |
| 7 | `antiLoopEnabled` | `bool ? 1 : 0` | 0/1 |
| 8 | `sameAngleAsButton` | `bool ? 1 : 0` | 0/1 |
| 9 | `antiLoopAngle` | `clamp(round(deg × 2), 10, 110)` | 10–110 (5–55°) |
| 10 | `wheelLiftResistance` | `clamp(round(x × 10), 0, 30)` | 0–30 (0–3.0) |
| 11 | `hornStrength` | `hornStrengthUiToWireByte(ui)`, clamped 0–250 | 10–100 |
| 12 | `hornNoteIndex` | `clamp(round(i), 0, 27)` | 0–27 |

Two encoding families worth noting: **angles are ×2 with a +20 bias** where
they can go negative (tilt angles), and **plain ×2** where they can't (wheelie
and anti-loop angles, both 5–55°). The +20/×2 bias is what lets a signed
−20…+20° range fit an unsigned byte.

`hornStrengthUiToWireByte(ui) = max(10, min(100, round(ui × 10)))` — UI 1–10
maps to wire 10–100. The inverse used when reading firmware state is
`hornStrengthFirmwareToUi(fw) = max(1, min(10, round(fw)))`, which does **not**
divide by 10. The two paths disagree by a factor of 10; the `horn_strength`
config field (§5) is a 0–100 double with default 4, and the UI default is also
4, so the round-trip only stays consistent because the config path and the
sub-command path are used in different places. Flagging as a likely bug.

### Horn note table

`HORN_NOTE_LABELS` / `HORN_NOTE_HZ_TABLE`, 28 entries, G#3 through B5:

```
G#3 208   A3 220   A#3 233   B3 247   C4 262   C#4 277   D4 294
D#4 311   E4 330   F4 349   F#4 370   G4 392   G#4 415   A4 440
A#4 466   B4 494   C5 523   C#5 554   D5 587   D#5 622   E5 659
F5 698    F#5 740  G5 784    G#5 831   A5 880   A#5 932   B5 988
```

`DEFAULT_HORN_NOTE_INDEX_B4 = 15` → 494 Hz, matching the `horn_frequency`
config default exactly. Helpers: `hornNoteLabel(i)` and
`nearestHornNoteIndexFromHz(hz)` for converting a firmware-reported frequency
back to a table index. The horn is produced by motor haptics, not a speaker —
per the `button_function` description, "Trigger a horn sound through motor
haptics."

### Not transmitted

`coastingBrakeStrength` is held in component state and written back to local
config after a successful save, but **is not one of the 13 bytes**. Either
it's carried by a different path or the feature is incomplete. Its default,
`DEFAULT_COASTING_BRAKE_STRENGTH`, is 0.

### Defaults

| Constant | Value |
|---|---|
| `DEFAULT_WHEELIE_ANGLE` | 30 |
| `DEFAULT_SELECTED_FUNCTION` | `'wheelie'` |
| `DEFAULT_THROTTLE_BRAKE_TILT` | `false` |
| `DEFAULT_THROTTLE_TILT_ANGLE` | 5 |
| `DEFAULT_BRAKE_TILT_ANGLE` | −5 |
| `DEFAULT_PERSIST_WHEELIE_THROTTLE` | `false` |
| `DEFAULT_PERSIST_WHEELIE_BRAKE` | `false` |
| `DEFAULT_ANTI_LOOP_ENABLED` | `true` |
| `DEFAULT_ANTI_LOOP_ANGLE` | 30 |
| `DEFAULT_SAME_ANGLE_AS_BUTTON` | `true` |
| `DEFAULT_WHEEL_LIFT_RESISTANCE` | 1.5 |
| `WHEEL_LIFT_RESISTANCE_STEP` | 0.3 |
| `WHEEL_LIFT_RESISTANCE_MAX` | 3 |
| `WHEEL_LIFT_DISPLAY_SCALE` | 3.3333333333333335 |
| `DEFAULT_COASTING_BRAKE_STRENGTH` | 0 |
| `DEFAULT_HORN_STRENGTH_UI` | 4 |
| `OVERHEAT_THRESHOLD` | 75 |

`wheelLiftResistanceToDisplay(x) = round(x × 3.3333…)` turns the 0–3.0
internal value into a 0–10 display scale; `snapWheelLiftResistance` quantises
user input to the 0.3 step.

---

## 5. Sub-command 103 — `BIKE_SPEED_LIMIT_CONFIGURE`

Single-byte payload:

```
36  102  103  <mph>
```

`speedLimitMphToWireByte(mph)`:

```js
n = round(mph)
return (n <= 0 || n > SPEED_LIMIT_MAX) ? 0 : n
```

So the value is transmitted as **whole mph, unscaled**, and anything out of
range collapses to `0`. `SPEED_LIMIT_MIN = 5`, `SPEED_LIMIT_MAX = 35`,
`DEFAULT_SPEED_LIMIT = 20`. Note the guard is `n <= 0`, not `n < SPEED_LIMIT_MIN`
— values 1–4 pass through to the wire even though the UI won't produce them.

`0` evidently means "no limit" rather than "limit of zero": the sibling helper
`speedLimitIconKey` uses the identical guard and returns the string `'none'`
for the same inputs, which is what the UI renders.

The send path is a save button with an optimistic-UI wrapper: on failure it
logs `"Failed to execute BIKE_SPEED_LIMIT_CONFIGURE command:"` and shows a
"Save Failed" / "Failed to save speed limit settings." alert; there's a
300 ms debounce branch and an "Unsaved Changes" discard confirmation on close.

The config table's `speed_limit` field has default **19**, while
`DEFAULT_SPEED_LIMIT` is **20** — the firmware default and the app's UI default
disagree by 1.

---

## 6. `bike_config` — the full custom-config profile

Read with `COMM_GET_CUSTOM_CONFIG` (93), written with `COMM_SET_CUSTOM_CONFIG`
(95). This is a separate channel from the 100–103 sub-commands: the sub-commands
are live/volatile pushes, the custom config is the persisted profile.

### Write payload — `getFloatConfigPayload(version, config)`

```
u8   COMM_SET_CUSTOM_CONFIG (95)
u8   0                       // config slot index
u32  floatSignatures[version]
...  floatConfigEncoders[version](config)
```

Buffer is pre-allocated at 250 bytes. An unknown version throws
`"Unsupported Package Version: <v>"`.

### Read parse — `getFloatConf(buffer)`

```
u8   (discarded)
u32  signature   →  getWfbVersionForSignature()  →  parser table
```

Only **version 1** is registered. Unrecognised signature → `null` → the user
sees "Package Version Unsupported or failed to get config". The signature is
computed at runtime from the parameter table. Its numeric value is not
specified here and must be verified before writing configuration.

### Wire types

`vTx` follows VESC Tool's `VESC_TX_*` enum (*inferred* from the observed values
and the accompanying scales):

| vTx | Type | Used by |
|---|---|---|
| 1 | uint8 | `speed_limit` |
| 3 | uint16 | `horn_frequency` |
| 4 | int16 | `max_current`, `max_current_brake` |
| 7 | double16 with `vTxDoubleScale` | all the tuning doubles |

`type` is the editor type, not the wire type: `0` label, `1` double, `2` int,
`3` string, `4` enum, `5` bool.

### The 27 parameters

Grouped by function rather than declaration order.

**Identity / control**

| Name | cDefine | Wire | Default |
|---|---|---|---|
| `config_name` | — | string | `bike_config` |
| `hw_name` | — | label | "WFB Cfg" — *"This is the VESC Logging and Communication Module."* |
| `package_disable` | `PACKAGE_DISABLE` | bool | 1 |

**Wheelie PID** — the tune the disabled sub-command 101 would have driven

| Name | cDefine | Scale | Range | Default |
|---|---|---|---|---|
| `kp` (Angle P) | `WHEELIE_KP` | 10 | 0–40 | 30 |
| `kp2` (Rate P) | `WHEELIE_KP2` | 100 | 0–3 | 1 |
| `mahony_kp` | `WHEELIE_MAHONY_KP` | 100 | 0–3 | 2 |
| `kp_brake` | `WHEELIE_KP_BRAKE` | 10 | 0.2–2 | 1.5 |
| `kp2_brake` | `WHEELIE_KP2_BRAKE` | 10 | 0–2 | 1.5 |
| `lift_resistance_kp2` | `BIKE_LIFT_RESISTANCE_KP2` | 100 | 0–3 | 1.5 |

The `lift_resistance_kp2` description is unusually candid about the design:
Rate P responds to the nose's angular velocity independent of setpoint, and
although it's nominally a P term they use it "practically as the D (Derivative)
component for our PID loop, working as a damper for the Angle P component."
Recommended 0.1–0.6, with 0.7–1.2+ possible but flagged for side effects, and
advice to scale Angle P and Rate P together to preserve ride feel.

**Motor limits and ramps** — same quantities sub-command 100 pushes live

| Name | cDefine | Wire | Range | Default |
|---|---|---|---|---|
| `max_current` | `BIKE_MAX_CURRENT` | int16 | 0–150 | 150 |
| `max_current_brake` | `BIKE_MAX_CURRENT_BRAKE` | int16 | −150–0 | −90 |
| `ramp_alpha_pos` | `BIKE_RAMP_ALPHA_POS` | ×10000 | 0.0001–1 | 0.003 |
| `ramp_alpha_neg` | `BIKE_RAMP_ALPHA_NEG` | ×10000 | 0.0001–1 | 0.008 |
| `ramp_alpha_return` | `BIKE_RAMP_ALPHA_RETURN` | ×10000 | 0.0001–1 | 0.008 |
| `balance_pitch` | `BIKE_BALANCE_PITCH` | ×100 | 5–55 | 30 |

`ramp_alpha_return` has no counterpart in `createRideModeCommand` — the live
command carries only pos and neg. The persisted config has a third ramp the
live path can't set.

**Button and tilt**

| Name | cDefine | Wire | Range | Default |
|---|---|---|---|---|
| `button_function` | `BIKE_BUTTON_FUNCTION` | enum | Wheelie / Turbo / Horn / Disabled | 2 (Horn) |
| `throttle_tilt_enabled` | `BIKE_THROTTLE_TILT_ENABLED` | bool | — | 1 |
| `throttle_tilt_angle_limit` | `BIKE_THROTTLE_TILT_ANGLE_LIMIT` | ×100 | −20–20 | 5 |
| `brake_tilt_angle_limit` | `BIKE_BRAKE_TILT_ANGLE_LIMIT` | ×100 | −20–20 | −5 |
| `persist_wheelie_throttle` | `BIKE_PERSIST_WHEELIE_THROTTLE` | bool | — | 0 |
| `persist_wheelie_brake` | `BIKE_PERSIST_WHEELIE_BRAKE` | bool | — | 0 |

`button_function` enum labels are `['Wheelie', 'Turbo', 'Horn', 'Disabled']`,
matching the `{wheelie:0, turbo:1, horn:2, disabled:3}` map in sub-command 102.
Its description: Wheelie triggers a wheelie at any time; Turbo accelerates as
fast as possible while maintaining current angle; Horn triggers motor haptics;
Disabled assigns nothing.

`throttle_tilt_enabled` description: while the button is held in Wheelie mode,
the bike's angle can be modulated with throttle and regen brake within the
configured tilt angles.

Note the config default for `button_function` is **2 (Horn)** while
`DEFAULT_SELECTED_FUNCTION` in the app is **`'wheelie'` (0)** — another
app/firmware default mismatch.

**Anti-loop**

| Name | cDefine | Wire | Range | Default |
|---|---|---|---|---|
| `anti_loop_enabled` | `BIKE_ANTI_LOOP_ENABLED` | bool | — | 1 |
| `anti_loop_use_button_angle` | `BIKE_ANTI_LOOP_USE_BUTTON_ANGLE` | bool | — | 1 |
| `anti_loop_angle` | `BIKE_ANTI_LOOP_ANGLE` | ×100 | 5–55 | 30 |

When `anti_loop_use_button_angle` is set, anti-loop references the wheelie
button's configured angle instead of `anti_loop_angle`.

**Horn and limit**

| Name | cDefine | Wire | Range | Default |
|---|---|---|---|---|
| `horn_strength` | `BIKE_HORN_STRENGTH` | ×100 | 0–100 | 4 |
| `horn_frequency` | `BIKE_HORN_FREQUENCY` | uint16 | 200–1000 | 494 |
| `speed_limit` | `BIKE_SPEED_LIMIT` | uint8 | 0–35 | 19 |

`horn_frequency`'s 200–1000 Hz range brackets the note table's 208–988 Hz
exactly.

---

## 7. Telemetry for the bike screen

Two loops, both driven off `loopTime` reported by `COMM_GET_VALUES_SETUP`:

```
bikeRTDataInterval   = loopTime
bikeSlowDataInterval = max(loopTime, 1000)
```

`getRTFloatData(ctx, mode = 1)` sends:

```
36  102  10  <mode>
```

i.e. `GET_ALL_DATA` with a mode selector. All call sites in this build pass
only `ctx`, so **mode is always 1**. The response echoes the mode back as its
first byte, and the parser adds `tachAbs`, `tempMosfet`, `tempMotor`,
`tempBattery` only when `mode >= 2` — meaning the temperature fields exist in
the protocol but this build never requests them through this path. It gets
temperatures from plain `COMM_GET_VALUES` instead.

`getAllFloatData` additionally issues a plain `COMM_GET_VALUES_SETUP` and
derives `batteryPercent` (rounded to 2 dp), `batteryVolts` (1 dp), and
`wattHoursRemaining` for the dashboard.

Field layout of the `GET_ALL_DATA` response is in the main document, §6.

---

## 8. Bike-layer issues

| # | Issue |
|---|---|
| 1 | `BIKE_WHEELIE_CONFIGURE` (101) defined but never sent; 2 of 3 wheelie modes `disabled: true` |
| 2 | Ramp slider position 10 short-circuits to `alpha = 1` (byte 248); `RAMP_VALUES[9] = 0.1` is unreachable, giving a 0.06 → 1.0 jump |
| 3 | `hornStrengthUiToWireByte` multiplies by 10, `hornStrengthFirmwareToUi` doesn't divide — asymmetric round-trip |
| 4 | `coastingBrakeStrength` tracked in UI state but absent from the 13-byte `BIKE_FEATURE_CONFIGURE` payload |
| 5 | `speed_limit` firmware default 19 vs app `DEFAULT_SPEED_LIMIT` 20 |
| 6 | `button_function` firmware default 2 (Horn) vs app `DEFAULT_SELECTED_FUNCTION` `'wheelie'` (0) |
| 7 | `speedLimitMphToWireByte` guards on `n <= 0`, not `n < SPEED_LIMIT_MIN`; 1–4 mph reach the wire |
| 8 | `ramp_alpha_return` exists in the persisted config but has no live-command equivalent |
| 9 | Ride-mode current resolution is 10 A per nibble — the UI can present finer values than the wire can carry |
