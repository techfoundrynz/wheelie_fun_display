# Wheelie (`com.thefloatlife.Wheelie`) — VESC communications API

VESC communications reference for Wheelie app version 1.0.0.

| | |
|---|---|
| Package | `com.thefloatlife.Wheelie` |
| Version | 1.0.0 (`versionCode` 6, iOS `buildNumber` 9) |

Entries marked **inferred** describe unconfirmed encodings or behavior and
require device verification before enabling writes. Later app or controller
versions may change the protocol behavior documented here.

---

## 1. Package compatibility

The app expects a compatible custom package to be installed on the controller;
it does not provide a `.vescpkg` or controller firmware installation path.

The app carries the custom-config schema described in §6. It can read and write
that configuration without fetching the package, but refuses unsupported
signatures with `"Package Version Unsupported or failed to get config"`.

There is no supported Lisp source upload path. `COMM_LISP_PRINT` replies are
explicitly discarded.

---

## 2. Transport — BLE

The GATT profile is the standard VESC one:

| Constant | UUID | Role |
|---|---|---|
| `VESC_SERVICE_UUID` | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` | Nordic UART Service |
| `VESC_CHARACTERISTICS_RX_UUID` | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` | write, app -> board |
| `VESC_CHARACTERISTICS_TX_UUID` | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` | notify, board -> app |

This is byte-for-byte the NUS profile that `nrf51_vesc` and VESC Express
expose - no custom service, no renamed characteristics, no vendor base UUID.
Any tool that speaks stock VESC BLE will connect. All of this app's
customisation lives above the transport, inside `COMM_CUSTOM_APP_DATA`
payloads.

After `discoverAllServicesAndCharacteristicsForDevice` the app walks
`servicesForDevice` and pushes `'vesc'` into a capability array if the NUS
service is present; that tag gates binding the `vescMessageHandler` listener.

Device discovery filters on advertised name — helper `bleDefaultNameIncludesWfb`.

### Write path — `writeBuffer(deviceId, packet)`

```
chunkSize = manager.mtu - 3
for each chunk:
    writeCharacteristicWithoutResponseForDevice(
        deviceId, VESC_SERVICE_UUID, VESC_CHARACTERISTICS_RX_UUID,
        chunk.toString('base64'))
await Promise.all(all chunk promises)
```

Note the chunks are issued **concurrently** and awaited together, not
sequentially. Ordering depends on the platform BLE queue preserving submission
order; on a stack that reorders, a multi-chunk packet would arrive scrambled.

Android permissions requested: `BLUETOOTH`, `BLUETOOTH_ADMIN`, `BLUETOOTH_SCAN`,
`BLUETOOTH_CONNECT`, `ACCESS_FINE_LOCATION`, `ACCESS_COARSE_LOCATION`,
`FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_CONNECTED_DEVICE`, `WAKE_LOCK`.
iOS declares the `bluetooth-central` background mode. There is a
`VescBackgroundService` that keeps the connection alive under a foreground
service.

---

## 3. Framing — `generatePacket(payload, canId?)`

Standard VESC serial framing:

```
short (len <= 256):   0x02  len:u8       payload  crc16:u16be  0x03
long  (len >  256):   0x03  len:u16be    payload  crc16:u16be  0x03
```

CAN forwarding is applied at frame-build time: if `canId` is supplied and
`payload[0] !== COMM_FORWARD_CAN`, the two bytes `[34, canId]` are prepended.
The send path (`writeData`) does the same automatically from
`_primaryCANDeviceId`, so a caller normally doesn't pass `canId` explicitly.

### CRC

CRC-16/CCITT (XMODEM), init `0x0000`, table-driven, byte-at-a-time:

```js
crc = table[((crc >> 8) ^ byte) & 0xFF] ^ (crc << 8)   // truncated to u16
```

Matches `crc.c` in `vedderb/bldc`.

### Two framing bugs worth knowing about

1. **Off-by-one on the length-form boundary.** The branch is `len <= 256` for
   the short form; upstream uses `len <= 255`. A payload of exactly 256 bytes is
   emitted with a single length byte of `256 & 0xFF = 0`, which the firmware
   will read as a zero-length packet. Payloads of exactly 256 bytes are
   therefore unusable.
2. **No 3-byte length form.** Upstream supports start byte `0x04` with a
   24-bit length for payloads > 65535. Not implemented here — irrelevant for
   the commands this app sends, but a limit if you're reusing the code.

### `VescBuffer`

A reimplementation of VESC's `buffer.c`. Readers:
`readUInt8`, `readInt8`, `readUInt16`, `readInt16`, `readUInt32`, `readInt32`,
`readDouble16(scale)`, `readDouble32(scale)`, `readFloat32`,
`readDouble32Auto`, `readString`, `slice`, `size`, `raw`.
Writers mirror these, plus `writeDouble32_Old` retained for compatibility.

`readDouble16(scale)` / `readDouble32(scale)` are the scaled-integer forms
(read a signed int, divide by scale). `readDouble32Auto` is the
float32-packed-in-uint32 form.

---

## 4. Packet ID enum

127 `COMM_*` IDs are defined. **IDs 0–90 and 92–101 match upstream
`bldc/datatypes.h` exactly.** Beyond that this app targets a fork and the
numbering diverges. Full table:

| ID | Name | | ID | Name |
|---|---|---|---|---|
| 0 | `COMM_FW_VERSION` | | 51 | `COMM_GET_VALUES_SETUP_SELECTIVE` |
| 1 | `COMM_JUMP_TO_BOOTLOADER` | | 52 | `COMM_EXT_NRF_PRESENT` |
| 2 | `COMM_ERASE_NEW_APP` | | 53 | `COMM_EXT_NRF_ESB_SET_CH_ADDR` |
| 3 | `COMM_WRITE_NEW_APP_DATA` | | 54 | `COMM_EXT_NRF_ESB_SEND_DATA` |
| 4 | `COMM_GET_VALUES` | | 55 | `COMM_EXT_NRF_ESB_RX_DATA` |
| 5 | `COMM_SET_DUTY` | | 56 | `COMM_EXT_NRF_SET_ENABLED` |
| 6 | `COMM_SET_CURRENT` | | 57 | `COMM_DETECT_MOTOR_FLUX_LINKAGE_OPENLOOP` |
| 7 | `COMM_SET_CURRENT_BRAKE` | | 58 | `COMM_DETECT_APPLY_ALL_FOC` |
| 8 | `COMM_SET_RPM` | | 59 | `COMM_JUMP_TO_BOOTLOADER_ALL_CAN` |
| 9 | `COMM_SET_POS` | | 60 | `COMM_ERASE_NEW_APP_ALL_CAN` |
| 10 | `COMM_SET_HANDBRAKE` | | 61 | `COMM_WRITE_NEW_APP_DATA_ALL_CAN` |
| 11 | `COMM_SET_DETECT` | | 62 | `COMM_PING_CAN` |
| 12 | `COMM_SET_SERVO_POS` | | 63 | `COMM_APP_DISABLE_OUTPUT` |
| 13 | `COMM_SET_MCCONF` | | 64 | `COMM_TERMINAL_CMD_SYNC` |
| 14 | `COMM_GET_MCCONF` | | 65 | `COMM_GET_IMU_DATA` |
| 15 | `COMM_GET_MCCONF_DEFAULT` | | 66 | `COMM_BM_CONNECT` |
| 16 | `COMM_SET_APPCONF` | | 67 | `COMM_BM_ERASE_FLASH_ALL` |
| 17 | `COMM_GET_APPCONF` | | 68 | `COMM_BM_WRITE_FLASH` |
| 18 | `COMM_GET_APPCONF_DEFAULT` | | 69 | `COMM_BM_REBOOT` |
| 19 | `COMM_SAMPLE_PRINT` | | 70 | `COMM_BM_DISCONNECT` |
| 20 | `COMM_TERMINAL_CMD` | | 71 | `COMM_BM_MAP_PINS_DEFAULT` |
| 21 | `COMM_PRINT` | | 72 | `COMM_BM_MAP_PINS_NRF5X` |
| 22 | `COMM_ROTOR_POSITION` | | 73 | `COMM_ERASE_BOOTLOADER` |
| 23 | `COMM_EXPERIMENT_SAMPLE` | | 74 | `COMM_ERASE_BOOTLOADER_ALL_CAN` |
| 24 | `COMM_DETECT_MOTOR_PARAM` | | 75 | `COMM_PLOT_INIT` |
| 25 | `COMM_DETECT_MOTOR_R_L` | | 76 | `COMM_PLOT_DATA` |
| 26 | `COMM_DETECT_MOTOR_FLUX_LINKAGE` | | 77 | `COMM_PLOT_ADD_GRAPH` |
| 27 | `COMM_DETECT_ENCODER` | | 78 | `COMM_PLOT_SET_GRAPH` |
| 28 | `COMM_DETECT_HALL_FOC` | | 79 | `COMM_GET_DECODED_BALANCE` |
| 29 | `COMM_REBOOT` | | 80 | `COMM_BM_MEM_READ` |
| 30 | `COMM_ALIVE` | | 81 | `COMM_WRITE_NEW_APP_DATA_LZO` |
| 31 | `COMM_GET_DECODED_PPM` | | 82 | `COMM_WRITE_NEW_APP_DATA_ALL_CAN_LZO` |
| 32 | `COMM_GET_DECODED_ADC` | | 83 | `COMM_BM_WRITE_FLASH_LZO` |
| 33 | `COMM_GET_DECODED_CHUK` | | 84 | `COMM_SET_CURRENT_REL` |
| 34 | `COMM_FORWARD_CAN` | | 85 | `COMM_CAN_FWD_FRAME` |
| 35 | `COMM_SET_CHUCK_DATA` | | 86 | `COMM_SET_BATTERY_CUT` |
| 36 | `COMM_CUSTOM_APP_DATA` | | 87 | `COMM_SET_BLE_NAME` |
| 37 | `COMM_NRF_START_PAIRING` | | 88 | `COMM_SET_BLE_PIN` |
| 38 | `COMM_GPD_SET_FSW` | | 89 | `COMM_SET_CAN_MODE` |
| 39 | `COMM_GPD_BUFFER_NOTIFY` | | 90 | `COMM_GET_IMU_CALIBRATION` |
| 40 | `COMM_GPD_BUFFER_SIZE_LEFT` | | 92 | `COMM_GET_CUSTOM_CONFIG_XML` |
| 41 | `COMM_GPD_FILL_BUFFER` | | 93 | `COMM_GET_CUSTOM_CONFIG` |
| 42 | `COMM_GPD_OUTPUT_SAMPLE` | | 94 | `COMM_GET_CUSTOM_CONFIG_DEFAULT` |
| 43 | `COMM_GPD_SET_MODE` | | 95 | `COMM_SET_CUSTOM_CONFIG` |
| 44 | `COMM_GPD_FILL_BUFFER_INT8` | | 96 | `COMM_BMS_GET_VALUES` |
| 45 | `COMM_GPD_FILL_BUFFER_INT16` | | 97 | `COMM_BMS_SET_CHARGE_ALLOWED` |
| 46 | `COMM_GPD_SET_BUFFER_INT_SCALE` | | 98 | `COMM_BMS_SET_BALANCE_OVERRIDE` |
| 47 | `COMM_GET_VALUES_SETUP` | | 99 | `COMM_BMS_RESET_COUNTERS` |
| 48 | `COMM_SET_MCCONF_TEMP` | | 100 | `COMM_BMS_FORCE_BALANCE` ⚠ |
| 49 | `COMM_SET_MCCONF_TEMP_SETUP` | | 100 | `COMM_GET_MCCONF_TEMP` ⚠ |
| 50 | `COMM_GET_VALUES_SELECTIVE` | | 101 | `COMM_BMS_ZERO_CURRENT_OFFSET` |

Continuing:

| ID | Name | Upstream | Note |
|---|---|---|---|
| 111 | `COMM_JUMP_TO_BOOTLOADER_HW` | 102 | shifted |
| 112 | `COMM_ERASE_NEW_APP_HW` | 103 | shifted |
| 113 | `COMM_WRITE_NEW_APP_DATA_HW` | 104 | shifted |
| 114 | `COMM_ERASE_BOOTLOADER_HW` | 105 | shifted |
| 115 | `COMM_JUMP_TO_BOOTLOADER_ALL_CAN_HW` | 106 | shifted |
| 116 | `COMM_ERASE_NEW_APP_ALL_CAN_HW` | 107 | shifted |
| 117 | `COMM_WRITE_NEW_APP_DATA_ALL_CAN_HW` | 108 | shifted |
| 118 | `COMM_ERASE_BOOTLOADER_ALL_CAN_HW` | 109 | shifted |
| 119 | `COMM_SET_ODOMETER` | 110 | shifted |
| 135 | `COMM_LISP_PRINT` | 135 | matches |
| 138 | `COMM_LISP_REPL_CMD` | 138 | matches |
| 139 | `COMM_LISP_STREAM_CODE` | 139 | matches |
| 149 | `COMM_SET_APPCONF_NO_STORE` | 149 | matches |
| 150 | `COMM_EBMS_STORE_CONF` | — | vendor ext (upstream 150 is `COMM_GET_GNSS`) |
| 151 | `COMM_EBMS_GET_CELLS` | — | vendor ext |
| 152 | `COMM_EBMS_GET_AUX` | — | vendor ext |
| 153 | `COMM_EBMS_GET_EXP_TEMP` | — | vendor ext |
| 154 | `COMM_EBMS_SET_MCCONF` | — | vendor ext |
| 155 | `COMM_EBMS_GET_MCCONF` | — | vendor ext |
| 156 | `COMM_EBMS_GET_MCCONF_DEFAULT` | — | vendor ext |
| 157 | `COMM_EBMS_GET_VALUES` | — | vendor ext |
| 161 | `COMM_LOCK_SETPIN` | — | vendor ext |
| 162 | `COMM_WRITE_LOCK` | — | vendor ext |
| 163 | `COMM_LOCK_STATUS` | — | vendor ext |

### ⚠ Duplicate ID 100

The enum assigns `COMM_GET_MCCONF_TEMP = 100` (upstream value is **91**) while
also assigning `COMM_BMS_FORCE_BALANCE = 100`. In a TypeScript numeric enum
this compiles fine but the reverse map `enum[100]` resolves to whichever was
declared last. Since IDs 92–99 and 101 all match upstream exactly, `91` is
almost certainly what was intended and `100` is a typo. If you're
interoperating, treat `COMM_GET_MCCONF_TEMP` as 91 on the wire and expect this
app to send 100.

Two other observations:

- `COMM_GET_CUSTOM_CONFIG_XML` (92) is defined but never sent — the app carries
  the config schema locally instead (§6).
- A 14-element array of "movement-capable" commands is built from
  `COMM_JUMP_TO_BOOTLOADER`, `COMM_SET_DUTY`, `COMM_SET_CURRENT`,
  `COMM_SET_CURRENT_BRAKE`, `COMM_SET_RPM`, `COMM_SET_POS`,
  `COMM_SET_HANDBRAKE`, `COMM_SET_DETECT`, `COMM_SET_SERVO_POS`, `COMM_REBOOT`,
  `COMM_ALIVE`, `COMM_FORWARD_CAN`, `COMM_SET_CHUCK_DATA`,
  `COMM_CUSTOM_APP_DATA` — purpose not confirmed (*inferred*: a gating or
  keep-alive list).

Also defined: `HW_TYPE_VESC = 0`, `HW_TYPE_VESC_BMS = 1`,
`HW_TYPE_CUSTOM_MODULE = 2`, and the full upstream `FAULT_CODE_*` set
(including `FAULT_CODE_FLASH_CORRUPTION`, `..._APP_CFG`, `..._MC_CFG`).

---

## 5. Inbound dispatch — `processVESCMessage(packetId, buffer)`

Only these IDs are decoded; anything else logs `Unhandled packet type "N"`.

| Packet | Decoder |
|---|---|
| `COMM_FW_VERSION` | `getVescFWVersion` |
| `COMM_GET_VALUES` | `getVescValues` |
| `COMM_GET_VALUES_SETUP` | `getVescValuesSetup` |
| `COMM_GET_MCCONF` | `getVescMotorConf` |
| `COMM_GET_MCCONF_DEFAULT` | `getVescMotorConfDefault` |
| `COMM_GET_APPCONF` | `getVescAppConf` |
| `COMM_GET_APPCONF_DEFAULT` | `getVescAppConfDefault` |
| `COMM_CUSTOM_APP_DATA` | `getVescAppData` → `getFloatData` |
| `COMM_GET_CUSTOM_CONFIG` | `getFloatConf` |
| `COMM_BMS_GET_VALUES` | `getVescBMSData` |
| `COMM_PING_CAN` | `getVescPingCAN` |
| `COMM_GET_IMU_DATA` | `getVescIMUData` |
| `COMM_DETECT_APPLY_ALL_FOC` | `getVescDetectAllFOCReceived` |
| `COMM_SET_MCCONF` | `getVescSetMcconf` (ack) |
| `COMM_SET_APPCONF` / `..._NO_STORE` | `getVescSetAppconf` (ack) |
| `COMM_PRINT` | logged as `Terminal: …` **and shown in a native `Alert`** |
| `COMM_LISP_PRINT` | silently dropped |

The `COMM_PRINT` handler decodes the payload as ASCII and pops a modal alert
titled "Terminal Response:". Board-controlled text reaching a UI dialog
unfiltered is worth flagging if you care about that class of issue.

`COMM_GET_VALUES_SETUP` and `COMM_BMS_GET_VALUES` each appear **twice** in the
comparison chain with identical handlers — the second branch is dead code.

### `COMM_FW_VERSION` (0) response

```
u8   major
u8   minor
str  hardware name (NUL-terminated)
u8[] uuid (remaining bytes)
```

### `COMM_GET_VALUES` (4) response

| Field | Read | Scale |
|---|---|---|
| `tempMosfet` | `readDouble16` | 10 |
| `tempMotor` | `readDouble16` | 10 |
| `currentMotor` | `readDouble32` | 100 |
| `currentBattery` | `readDouble32` | 100 |
| `id` | `readDouble32` | 100 |
| `iq` | `readDouble32` | 100 |
| `dutyCycle` | `readDouble16` | 1000 |
| `erpm` | `readDouble32` | 1 |
| `voltage` | `readDouble16` | 10 |
| `ampHoursConsumed` | `readDouble32` | 10000 |
| `ampHoursCharged` | `readDouble32` | 10000 |
| `wattHoursConsumed` | `readDouble32` | 10000 |
| `wattHoursCharged` | `readDouble32` | 10000 |
| `tachValue` | `readInt32` | — |
| `tachAbs` | `readInt32` | — |
| `faultCode` | `readInt8` | — |

### `COMM_GET_VALUES_SETUP` (47) response

Same prefix through `currentBattery`, then:

| Field | Read | Scale / transform |
|---|---|---|
| `dutyCycle` | `readDouble16` | 1000 |
| `erpm` | `readDouble32` | 1 |
| `speed` | `readDouble32` | ×3.5897435 (m/s → mph) |
| `voltage` | `readDouble16` | 10 |
| `batteryLevel` | `readDouble16` | 1000 |
| `ampHoursConsumed` … `wattHoursCharged` | `readDouble32` | 10000 |
| `tachValue`, `tachAbs` | `readInt32` | — |
| `pidPos` | `readDouble32` | 1000000 |
| `faultCode` | `readInt8` | — |
| `controllerId` | `readInt8` | — |
| `numVescs` | `readInt8` | — |
| `wattHoursRemaining` | `readDouble32` | 10000 |
| `odometer` | `readUInt32` | — |
| `loopTime` | `readUInt32` | — |

### `COMM_GET_IMU_DATA` (65) response

```
u16 mask
```
Then, for each set bit in ascending order, one `float32`:
`0x001 roll`, `0x002 pitch`, `0x004 yaw`, `0x008 accX`, `0x010 accY`,
`0x020 accZ`, `0x040 gyroX`, `0x080 gyroY`, `0x100 gyroZ`, `0x200 magX`, …

### `COMM_PING_CAN` (62) response

`u8[]` — one byte per CAN device ID found, length = buffer size.

---

## 6. Custom app data — the WFB package layer

This is the app-specific layer, carried inside `COMM_CUSTOM_APP_DATA` (36).

```
COMM_CUSTOM_APP_DATA  appId:u8  subCommand:u8  [payload…]
```

`AppPacketTypes.WFB = 102`. `getVescAppData` reads the first two bytes and
**returns `null` unless `appId === 102`** — so this app will not talk to a stock
Float package board (which uses app ID 101) or a Refloat board.

### Sub-commands (`FloatPacketTypes`)

| ID | Name | Notes |
|---|---|---|
| 0 | `GET_INFO` | |
| 1 | `GET_RTDATA` | |
| 2 | `SET_TUNE` | |
| 3 | `SET_DEFAULT_TUNE` | |
| 4 | `SAVE_TUNE` | |
| 5 | `RESTORE_TUNE` | |
| 6 | `TUNE_OTHER` | |
| 7 | `MOVE` | |
| 8 | `BOOSTER` | |
| 9 | `PRINT_INFO` | |
| 10 | `GET_ALL_DATA` | primary telemetry poll |
| 11 | `EXPERIMENT` | |
| 12 | `LOCK` | |
| 22 | `FLYWHEEL` | |
| 25 | `LIGHT_INFO` | |
| 26 | `LIGHT_CTRL` | |
| 27 | `LCM_INFO` | |
| 100 | `BIKE_RIDE_CONFIGURE` | vendor |
| 101 | `BIKE_WHEELIE_CONFIGURE` | vendor |
| 102 | `BIKE_FEATURE_CONFIGURE` | vendor |
| 103 | `BIKE_SPEED_LIMIT_CONFIGURE` | vendor |

IDs 0–27 track the Float package's `FLOAT_COMMAND_*` layout; 100–103 are
specific to this fork.

The app's telemetry loop is `getRTFloatData(ctx, canId)` →
`getData(COMM_CUSTOM_APP_DATA, WFB, GET_ALL_DATA, canId)`, and
`getAllFloatData` additionally issues a plain `COMM_GET_VALUES_SETUP`.

### `GET_ALL_DATA` (10) response

Requires `buffer.size() >= 35`, else the parse is abandoned.

```
u8  mode
```

Then, always:

| Field | Encoding |
|---|---|
| `pidOutput` | `readDouble16(10)` |
| `pitchAngle` | `readDouble16(10)` |
| `rollAngle` | `readDouble16(10)` |
| `state` | `u8 & 0x0F` |
| `setpointAdjustmentType` | same byte `>> 4` |
| `switchState` | `u8 & 0x0F` |
| `adc1` | `u8 / 50` |
| `adc2` | `u8 / 50` |
| `setPoint` | `(u8 - 128) / 5` |
| `atr` | `(u8 - 128) / 5` |
| `brakeTilt` | `(u8 - 128) / 5` |
| `torqueTilt` | `(u8 - 128) / 5` |
| `turnTilt` | `(u8 - 128) / 5` |
| `inputTilt` | `(u8 - 128) / 5` |
| `truePitchAngle` | `readDouble16(10)` |
| `appliedBoosterCurrent` | `u8 - 128` |
| `voltage` | `readDouble16(10)` |
| `erpm` | `readInt16` |
| `speed` | `readDouble16(10)` × 3.5897435 |
| `currentMotor` | `readDouble16(10)` |
| `currentBattery` | `readDouble16(10)` |
| `dutyCycle` | `(u8 - 128) / 100` |
| `id` | `u8 / 3` |

After reading `switchState` the app normalises it: raw `2` → `FULL`,
raw `1` → `ADC1` or `ADC2` depending on which of `adc1`/`adc2` is larger,
anything else → `OFF`.

If `mode >= 2`, four more fields:

| Field | Encoding |
|---|---|
| `tachAbs` | `readFloat32` |
| `tempMosfet` | `u8 / 2` |
| `tempMotor` | `u8 / 2` |
| `tempBattery` | `u8` |

Then, unconditionally:

| Field | Encoding |
|---|---|
| `odometer` | (*inferred* — `readUInt32`) |
| `ampHoursConsumed` | `readDouble16` |
| `ampHoursCharged` | `readDouble16` |
| `wattHoursConsumed` | `readDouble16` |
| `wattHoursCharged` | `readDouble16` |
| `batteryLevel` | `u8 / 2` |

### Enums used by the telemetry

**`state`** (low nibble):
`0 BOOT`, `1 RUNNING`, `2 RUNNING_TILTBACK`, `3 RUNNING_WHEELSLIP`,
`4 RUNNING_UPSIDEDOWN`, `5 RUNNING_FLYWHEEL`, `6 STOP_ANGLE_PITCH`,
`7 STOP_ANGLE_ROLL`, `8 STOP_SWITCH_HALF`, `9 STOP_SWITCH_FULL`,
`11 STARTUP`, `12 STOP_REVERSE`, `13 STOP_QUICKSTOP`, `14 CHARGING`,
`15 DISABLED`. (No `10`.)

**`setpointAdjustmentType`** (high nibble):
`0 CENTERING`, `1 REVERSESTOP`, `2 TILTBACK_NONE`, `3 TILTBACK_DUTY`,
`4 TILTBACK_HV`, `5 TILTBACK_LV`, `6 TILTBACK_TEMP`.

**`switchState`**: `0 OFF`, `1 ADC1`, `2 ADC2`, `3 FULL`.

**LED type**: `0 NONE`, `1 RGB`, `2 RGBW`, `3 EXTERNAL`.

**Ride mode**: `0 DutyCycle`, `1 Standard`.

---

## 7. Custom config — `COMM_GET_CUSTOM_CONFIG` / `COMM_SET_CUSTOM_CONFIG`

The custom-config interface uses the local parameter schema.

`getFloatConf(buffer)` reads:

```
u8   (discarded)
u32  signature
```

then looks the signature up in a version map. The map has exactly **one entry,
version `1`** — so this build supports a single WFB config revision, and an
unrecognised signature yields `null`, surfacing to the user as
`"Package Version Unsupported or failed to get config"`. The signature value
itself is computed at runtime from the parameter table (VESC Tool-style hash).
Its numeric value is not specified here and must be verified before writing configuration.

Per version the app holds three generated artefacts:
`configPropertyMetadata` (the schema), a parser built by `getParser(metadata)`,
a serialiser built by `getEncoder(metadata)`, and `configPageLayout` for the UI.
This is exactly the code-gen output that VESC Tool produces from a package's
config XML — embedded rather than fetched.

### The `bike_config` parameter table (27 entries)

`vTx` is the wire type; `scale` is `vTxDoubleScale` (value is transmitted as an
integer of `value × scale`). Negative bounds below use their signed values.

| Name | cDefine | Type | vTx | Scale | Range | Default |
|---|---|---|---|---|---|---|
| `config_name` | — | string | — | — | — | `bike_config` |
| `hw_name` | — | label | — | — | — | "WFB Cfg" |
| `package_disable` | `PACKAGE_DISABLE` | bool | — | — | — | 1 |
| `kp` | `WHEELIE_KP` | double | 7 | 10 | 0 – 40 | 30 |
| `kp2` | `WHEELIE_KP2` | double | 7 | 100 | 0 – 3 | 1 |
| `mahony_kp` | `WHEELIE_MAHONY_KP` | double | 7 | 100 | 0 – 3 | 2 |
| `kp_brake` | `WHEELIE_KP_BRAKE` | double | 7 | 10 | 0.2 – 2 | 1.5 |
| `kp2_brake` | `WHEELIE_KP2_BRAKE` | double | 7 | 10 | 0 – 2 | 1.5 |
| `max_current` | `BIKE_MAX_CURRENT` | int | 4 | — | 0 – 150 | 150 |
| `max_current_brake` | `BIKE_MAX_CURRENT_BRAKE` | int | 4 | — | −150 – 0 | −90 |
| `ramp_alpha_pos` | `BIKE_RAMP_ALPHA_POS` | double | 7 | 10000 | 0.0001 – 1 | 0.003 |
| `ramp_alpha_neg` | `BIKE_RAMP_ALPHA_NEG` | double | 7 | 10000 | 0.0001 – 1 | 0.008 |
| `ramp_alpha_return` | `BIKE_RAMP_ALPHA_RETURN` | double | 7 | 10000 | 0.0001 – 1 | 0.008 |
| `balance_pitch` | `BIKE_BALANCE_PITCH` | double | 7 | 100 | 5 – 55 | 30 |
| `button_function` | `BIKE_BUTTON_FUNCTION` | enum | — | — | — | 2 |
| `throttle_tilt_enabled` | — | bool | — | — | — | — |
| `throttle_tilt_angle_limit` | `BIKE_THROTTLE_TILT_ANGLE_LIMIT` | double | 7 | 100 | −20 – 20 | 5 |
| `brake_tilt_angle_limit` | `BIKE_BRAKE_TILT_ANGLE_LIMIT` | double | 7 | 100 | −20 – 20 | −5 |
| `anti_loop_enabled` | `BIKE_ANTI_LOOP_ENABLED` | bool | — | — | — | 1 |
| `anti_loop_use_button_angle` | — | bool | — | — | — | — |
| `anti_loop_angle` | `BIKE_ANTI_LOOP_ANGLE` | double | 7 | 100 | 5 – 55 | 30 |
| `horn_strength` | `BIKE_HORN_STRENGTH` | double | 7 | 100 | 0 – 100 | 4 |
| `horn_frequency` | `BIKE_HORN_FREQUENCY` | int | 3 | — | 200 – 1000 | 494 |
| `lift_resistance_kp2` | — | double | — | — | — | — |
| `speed_limit` | `BIKE_SPEED_LIMIT` | int | 1 | — | 0 – 35 | 19 |
| `persist_wheelie_throttle` | `BIKE_PERSIST_WHEELIE_THROTTLE` | bool | — | — | — | 0 |
| `persist_wheelie_brake` | `BIKE_PERSIST_WHEELIE_BRAKE` | bool | — | — | — | 0 |

The declared JS types for the whole set are:
`config_name: string`, `hw_name: undefined`, `package_disable: boolean`,
and the remainder `number` except the five booleans
(`throttle_tilt_enabled`, `anti_loop_enabled`, `anti_loop_use_button_angle`,
`persist_wheelie_throttle`, `persist_wheelie_brake`).

`hw_name` carries the description "This is the VESC Logging and Communication
Module."

---

## 8. Client command scheduling

Outbound commands are matched to responses through command callbacks. While
callbacks are outstanding in serial command mode, the sender backs off in
10 ms increments before writing the next command.

---

## 9. Summary of defects found

| # | Issue | Impact |
|---|---|---|
| 1 | `COMM_GET_MCCONF_TEMP = 100` (should be 91), colliding with `COMM_BMS_FORCE_BALANCE = 100` | Wrong ID on the wire; enum reverse-map ambiguity |
| 2 | Short-frame boundary is `len <= 256`, should be `<= 255` | 256-byte payloads emit a length byte of 0 |
| 3 | `_HW` bootloader IDs shifted +9 vs upstream (111–118 vs 102–109) | Incompatible with stock firmware — intentional if the fork matches |
| 4 | BLE chunks written concurrently via `Promise.all` | Multi-chunk packets rely on stack-preserved ordering |
| 5 | `COMM_PRINT` payload rendered in a native `Alert` unfiltered | Board-controlled text reaches a UI dialog |
| 6 | Duplicate dead branches for `COMM_GET_VALUES_SETUP` / `COMM_BMS_GET_VALUES` | Cosmetic |
| 7 | No 3-byte length frame form | Payloads > 65535 unsupported |
