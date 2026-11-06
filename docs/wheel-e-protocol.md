# wheel-e protocol notes

This document describes the wheel-e contract implemented by Wheelie Fun Display. The reference is [buestad/wheel-e commit `69382e6`](https://github.com/buestad/wheel-e/tree/69382e619bb696ffe54ef899df90fd2115039ba2), whose package name is `Wheel-E`. Protocol compatibility is established with replies from the connected controller, independently of its Bluetooth name.

## Supported contract

| Operation | VESC/application command | Display behavior |
| --- | --- | --- |
| Motor-controller identification | `COMM_FW_VERSION` (`0`) | Requires a valid controller firmware identity; cached routes must also match the saved UUID. |
| Package identification | `COMM_CUSTOM_APP_DATA` (`36`), interface `101`, `INFO` (`0`), version `2` | Requires a complete reply identifying `Wheel-E`. |
| Standard motor telemetry | `COMM_GET_VALUES_SETUP` (`47`) | Supplies speed, battery, voltage, currents, temperatures and controller fault. |
| Package telemetry | Interface `101`, `REALTIME_DATA` (`33`) | Requires the exact float32/state/pitch/roll request described below. |
| Bike lock/unlock | Interface `101`, `LOCK` (`12`) | Sends an explicit boolean and confirms the resulting state through fresh telemetry. |
| Tuning, configuration, lights and remote throttle | Other wheel-e commands | Not exposed or transmitted by this display. Use VESC Tool for those features. |

The implementation lives in [the integration](../firmware/components/bike/integration.cpp), [controller discovery](../firmware/components/bike/include/bike/controller_discovery.hpp) and [lock policy](../firmware/components/bike/include/bike/lock_policy.hpp). These notes distinguish the upstream protocol from the display's deliberately smaller feature set.

## Transport and envelope

The byte sequences below are complete **VESC payloads**, before serial length/CRC framing or native CAN fragmentation. They can travel through the shared BLE, UART or CAN transport. See [transport internals](transports.md) for framing, CAN forwarding and controller routing.

A wheel-e application payload begins:

```text
24 65 CC ...
│  │  └─ wheel-e command, hexadecimal
│  └──── interface 101 (0x65)
└─────── COMM_CUSTOM_APP_DATA, 36 (0x24)
```

The upstream command pages omit this three-byte prefix when describing command-specific offsets. Every offset in this document includes it. Multi-byte integers use big-endian byte order. Float32 telemetry uses the upstream `buffer_append_float32_auto` format: IEEE 754 binary32 in big-endian order, with subnormal values flushed to zero by the encoder. [Command envelope](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/index.md), [numeric encoders](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/conf/buffer.c).

Interface `101` is shared with Float/Refloat-family packages. Its presence, or a compatible telemetry response alone, is not a unique wheel-e identity. The display therefore requires the package-name check below before enabling wheel-e branding and features.

## Discovery and package identity

**Auto** checks supported packages on discovered motor controllers. A manual **wheel-e** selection still performs verification. The relevant sequence is:

1. Obtain a valid motor-controller firmware response, excluding non-motor adapters.
2. Request `INFO` version 2 and verify the package name.
3. Request modern `REALTIME_DATA` and validate the exact response format.
4. Enable the wheel-e integration only after both package checks succeed.

The standard firmware request `[0]` and the wheel-e application request `[36, 101, 0, 2, 0]` are different messages. The first identifies VESC firmware; the second identifies the installed package.

The display stores bounded NVS route hints to accelerate later connections. A hint changes query order only: firmware UUID, package identity and telemetry are checked again. A missing controller, mismatched identity, removed package or failed probe returns to full discovery. See [vehicle integrations](integrations.md) for Auto and manual selection behavior.

### INFO version 2

Request:

```text
24 65 00 02 00
         │  └─ flags = 0
         └──── INFO response version requested = 2
```

The referenced handler returns this **61-byte** VESC payload:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 1 | `36`: VESC custom application data |
| 1 | 1 | `101`: package interface |
| 2 | 1 | `0`: INFO command |
| 3 | 1 | `2`: actual INFO response version |
| 4 | 1 | Request flags echoed; `0` for the display's request |
| 5 | 20 | Package name, zero-padded fixed-width string |
| 25 | 3 | Package major, minor and patch version, one byte each |
| 28 | 20 | Version suffix, zero-padded fixed-width string |
| 48 | 4 | Build Git hash prefix |
| 52 | 4 | System tick rate in Hz |
| 56 | 4 | Package capability flags |
| 60 | 1 | Extra flags |

The display accepts a case-insensitive exact `Wheel-E` name with zero padding, version `2`, echoed flags `0`, a nonzero tick rate and the complete layout. `Float`, `Refloat`, longer names with a `Wheel-E` prefix, legacy version-1 replies and truncated replies do not establish wheel-e identity. INFO capability bits are not used to expose additional controls.

The reference build's tick rate is 10,000 Hz. Package version strings are diagnostic information; successful version-2 identity and modern telemetry replies are the compatibility requirements. [INFO specification](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/INFO.md), [handler](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L2652), [package name](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/package_name).

## REALTIME_DATA, command 33

The display requests float32 encoding and three fields from mask1: state flags at bit 1, pitch at bit 17 and roll at bit 19. Their combined mask is `0x000A0002`. Mask2 is zero.

Request, **12 bytes**:

```text
24 65 21 01 00 0A 00 02 00 00 00 00
         │  └─────────┘ └─────────┘
         │     mask1       mask2
         └─ control_flags = 1: float32
```

Response, **28 bytes**:

| Offset | Bytes | Field | Interpretation |
| --- | --- | --- | --- |
| 0 | 3 | Command prefix | `[36, 101, 33]` |
| 3 | 1 | Control flags | Exactly `1` |
| 4 | 4 | Mask1 | Exactly `0x000A0002` |
| 8 | 4 | Mask2 | Exactly `0` |
| 12 | 4 | Package time | Unsigned ticks; divide by INFO tick rate for seconds |
| 16 | 4 | State flags | Packed unsigned integer described below |
| 20 | 4 | Pitch | Float32 degrees |
| 24 | 4 | Roll | Float32 degrees |

Selected fields follow mask-bit order. No float16 variant, additional mask fields or legacy compact telemetry layout is accepted by this decoder. The display rejects incorrect headers, flags or masks, incomplete/extended bodies, unknown run states, non-finite angles and angles outside ±180°. Rejected packets leave the last valid sample unchanged. [REALTIME_DATA specification](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/REALTIME_DATA.md), [response encoder](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L2404).

### State flags

| Bits | Field | Display use |
| --- | --- | --- |
| 29–28 | Package mode | Not currently projected into the UI |
| 26–24 | Run state | Normalized using the table below |
| 23–22 | Footpad state | Not currently projected into the UI |
| 21 | Charging | Overrides the normalized state to `14`; blocks locking |
| 20 | Fatal error | Overrides the normalized state to `255`; blocks locking |
| 17 | Darkride | Not currently projected into the UI |
| 16 | Wheelslip | Not currently projected into the UI |
| 15–12 | Setpoint adjustment type | Not currently projected into the UI |
| 11–8 | Stop condition | Not currently projected into the UI |
| 7–0 | Beep reason | Not currently projected into the UI |

| Raw run state | Upstream name | Display's internal state |
| --- | --- | --- |
| 0 | DISABLED | 15 |
| 1 | STARTUP | 11 |
| 2 | READY | 0 |
| 3 | RUNNING | 1 |
| 4 | THROTTLE | 16 |
| 5 | CRUISE | 17 |

These are modern wheel-e states, not the four-bit legacy Float compatibility states. Keeping THROTTLE and CRUISE distinct allows the lock policy to reject cruise while considering stationary throttle mode. If charging and fatal error are both set, fatal error takes precedence. [State definitions](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/state.h), [flag encoder](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L2278).

## Standard telemetry and freshness

After verification, the display polls standard VESC setup telemetry (`47`) together with wheel-e realtime data (`33`) at 5 Hz. Standard telemetry supplies signed speed in m/s, voltage, battery percentage, motor/battery current, Celsius temperatures and controller fault. The UI converts speed and temperature into the independently selected display units.

Package data supplies pitch, roll and run state. Standard telemetry is not a substitute for a missing package-identity or package-telemetry reply. Auto may instead select generic VESC when no supported package verifies; that integration has standard read-only telemetry and no wheel-e lock or tuning features.

Setup and package freshness expire independently after 1,500 ms, measured from local receive time. The package timestamp is not used to schedule polling or establish freshness. A connection/route change clears prior telemetry and pending commands. Details are in [telemetry parsing](../firmware/components/bike/telemetry.cpp) and [architecture](architecture.md).

## LOCK, command 12

The request contains one boolean byte, with no PIN or acknowledgement field:

```text
24 65 0C 01    lock:   disabled = true
24 65 0C 00    unlock: disabled = false
```

The upstream handler acts only when the run state is not RUNNING. It reloads the persisted configuration, updates its disabled flag, changes the package state and writes configuration back to EEPROM. Unlocking from DISABLED enters STARTUP. The handler does not send a dedicated success response. This persistence also means temporary controller-side tuning is not retained by the lock handler. [LOCK handler](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L1754), [state transition](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/state.c#L63).

The display applies stricter write guards: current-session setup and package telemetry must both be fresh, speed magnitude below 0.2 m/s, motor-current magnitude below 2 A and controller fault zero. RUNNING, CRUISE, charging and fatal-error states are rejected. Stationary THROTTLE can pass these guards.

A queued write is not confirmation. The display requires fresh telemetry from the same session, received more than 400 ms after the request: DISABLED confirms locking; a permitted stopped non-disabled state confirms unlocking. A five-second timeout, disconnect or stale data leaves the operation unconfirmed. Commands are not automatically replayed after reconnect. The six-digit [display PIN](pins-and-locking.md) is local access control, separate from this wheel-e boolean command.

## Compatibility limits

The supported contract requires INFO version 2 identifying `Wheel-E` and modern REALTIME_DATA command 33 with the exact format above. Older wheel-e releases without this identity response remain unsupported by the wheel-e integration. Other interface-101 packages remain eligible for generic read-only VESC telemetry through Auto, but do not receive wheel-e branding or write capabilities.

Upstream marks `GET_RTDATA` (`1`) and `GET_ALLDATA` (`10`) deprecated. Internal realtime commands `31`/`32` do not provide the compatibility contract used here. This display does not send WFB-specific ride commands `100`/`103` to interface 101, and does not implement wheel-e configuration or throttle commands. [Upstream command index](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/index.md), [command IDs and dispatch](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c#L1581).

Portable tests cover identity validation, probe order, cached-hint re-verification, response sizes and masks, state normalization and lock confirmation. Actual controller firmware, phone interoperability and physical lock behavior still need hardware acceptance; source compatibility and simulator checks do not establish those results. See [planned work](../TODO.md).
