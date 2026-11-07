# Vehicle integrations

`VehicleIntegration` is a portable interface with separate implementations for WFB, wheel-e and generic VESC. It owns package polling, telemetry decoding, command encoding, the brand name and supported-feature flags. BLE framing, transport, preferences and display rendering remain shared. The UI and command callbacks both gate unsupported features.

| Integration | Package | Dashboard | Ride presets / speed limit | Bike lock |
| --- | --- | --- | --- | --- |
| Wheelie Fun Bike | 102 | VESC setup plus WFB compact telemetry | Supported | LOCK 12; firmware compatibility needs hardware verification |
| wheel-e | 101 | VESC setup plus REALTIME_DATA 33 | Not exposed; use VESC Tool | LOCK 12 |
| VESC | None | Standard VESC setup telemetry | Not exposed | Not exposed; local screen PIN still works |

Choose **Settings > Vehicle > Vehicle integration** before selecting a device. **Auto** is the default and probes for both WFB and wheel-e packages on discovered motor controllers. A valid package reply determines the integration; BLE names only affect discovery ranking. Auto falls back to read-only generic VESC only after the supported package probes find no match. Explicit WFB or wheel-e selections still require a matching package reply.

Forget the connected bike to select another integration; the decoder and pending writes are invalidated with the session. The BLE peer and selected policy are persisted together after UART notification subscription succeeds. Auto remains Auto after discovery and probes again on reconnect; the detected integration does not silently replace the selected policy. A saved BLE association proves the UART link, not package compatibility; failed package probes cannot enable custom features. Matching BLE names rank ahead of other UART candidates, rather than excluding them.

The header, window title and About page use the selected vehicle brand. WFB retains its supplied artwork; wheel-e and generic VESC use text wordmarks. Wheel-e does not display or send WFB's proprietary commands 100/103.

## wheel-e protocol reference

See the dedicated [wheel-e protocol notes](wheel-e-protocol.md) for identity verification, byte layouts, state flags, locking and compatibility limits.

Implemented against [buestad/wheel-e at 69382e6](https://github.com/buestad/wheel-e/tree/69382e619bb696ffe54ef899df90fd2115039ba2), specifically [command handlers](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/main.c), [REALTIME_DATA](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/doc/commands/REALTIME_DATA.md) and [state definitions](https://github.com/buestad/wheel-e/blob/69382e619bb696ffe54ef899df90fd2115039ba2/src/state.h).

The application payload is `[36, 101, command, ...]` inside the ordinary VESC frame. Interface 101 is shared with Float/Refloat-family packages, so discovery first requires a complete INFO version-2 reply naming `Wheel-E` before verifying realtime telemetry. Other interface-101 packages do not receive wheel-e branding or write features. Poll 33 requests float32 values with mask1 `0x000a0002` (state flags, pitch, roll), mask2 zero. Responses must echo the exact requested format and masks and contain the complete body; invalid or truncated packets leave telemetry unchanged. Standard command 47 supplies speed, current, battery, temperatures and controller fault.

Raw wheel-e states differ from the WFB compatibility states. DISABLED maps to display state 15, STARTUP to 11, READY to 0, RUNNING to 1, THROTTLE to 16 and CRUISE to 17. Charging and fatal-error flags prevent locking. THROTTLE may be locked only with fresh stationary telemetry, zero controller fault and motor current below 2 A; RUNNING and CRUISE are rejected. LOCK uses `[36, 101, 12, 1/0]`. No WFB configuration is sent to package 101.

Protocol fixtures cover both package IDs, masks, endianness, state normalization, truncated replies and unsupported command rejection. A real wheel-e vehicle and phone are still required to verify radio interoperability, telemetry values and lock behavior.
